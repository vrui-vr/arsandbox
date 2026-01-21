/***********************************************************************
RemoteClient - Class to receive elevation, water level, and snow level
grids from an Augmented Reality Sandbox server.
Copyright (c) 2019-2026 Oliver Kreylos

This file is part of the Augmented Reality Sandbox (SARndbox).

The Augmented Reality Sandbox is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Augmented Reality Sandbox is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Augmented Reality Sandbox; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#include "RemoteClient.h"

#include <stdexcept>
#include <Misc/SizedTypes.h>
#include <Misc/StdError.h>
#include <Comm/TCPPipe.h>

#include "IntraFrameDecompressor.h"
#include "InterFrameDecompressor.h"

/******************************************
Methods of class RemoteClient::GridBuffers:
******************************************/

/*****************************
Methods of class RemoteClient:
*****************************/

void RemoteClient::unquantizeGrids(void)
	{
	/* Start a new set of grids: */
	GridBuffers& gb=grids.startNewValue();
	
	/* Calculate elevation quantization factors: */
	GridScalar eScale=(elevationRange[1]-elevationRange[0])/GridScalar(65535);
	GridScalar eOffset=elevationRange[0];
	
	/* Un-quantize the bathymetry grid: */
	GridScalar* bEnd=gb.bathymetry+bathymetrySize.volume();
	Pixel* qbPtr=bathymetry[currentGrid];
	for(GridScalar* bPtr=gb.bathymetry;bPtr!=bEnd;++bPtr,++qbPtr)
		*bPtr=GridScalar(*qbPtr)*eScale+eOffset;
	
	/* Un-quantize the water level grid: */
	GridScalar* wlEnd=gb.waterLevel+gridSize.volume();
	Pixel* qwlPtr=waterLevel[currentGrid];
	for(GridScalar* wlPtr=gb.waterLevel;wlPtr!=wlEnd;++wlPtr,++qwlPtr)
		*wlPtr=GridScalar(*qwlPtr)*eScale+eOffset;
	
	/* Un-quantize the snow height grid: */
	GridScalar* shEnd=gb.snowHeight+gridSize.volume();
	Pixel* qshPtr=snowHeight[currentGrid];
	for(GridScalar* shPtr=gb.snowHeight;shPtr!=shEnd;++shPtr,++qshPtr)
		*shPtr=GridScalar(*qshPtr)*eScale+eOffset;
	
	/* Post the new set of grids: */
	grids.postNewValue();
	}

void RemoteClient::receiveGridsInter(void)
	{
	/* Create an inter-frame decompressor and connect it to the TCP pipe: */
	InterFrameDecompressor decompressor(*pipe);
	
	/* Receive and decompress the quantized property grids into the intermediate buffers: */
	int newGrid=1-currentGrid;
	decompressor.decompressFrame(bathymetrySize[0],bathymetrySize[1],bathymetry[currentGrid],bathymetry[newGrid]);
	decompressor.decompressFrame(gridSize[0],gridSize[1],waterLevel[currentGrid],waterLevel[newGrid]);
	decompressor.decompressFrame(gridSize[0],gridSize[1],snowHeight[currentGrid],snowHeight[newGrid]);
	currentGrid=newGrid;
	
	/* Un-quantize the received property grids: */
	unquantizeGrids();
	}

RemoteClient::RemoteClient(const char* serverHostName,int serverPort)
	:pipe(0)
	{
	/* Initialize resources: */
	for(int i=0;i<2;++i)
		{
		bathymetry[i]=0;
		waterLevel[i]=0;
		snowHeight[i]=0;
		}
	
	try
		{
		/* Connect to the AR Sandbox server: */
		pipe=new Comm::TCPPipe(serverHostName,serverPort);
		pipe->ref();
		
		/* Send an endianness token to the server: */
		pipe->write<Misc::UInt32>(0x12345678U);
		pipe->flush();
		
		/* Receive an endianness token from the server: */
		Misc::UInt32 token=pipe->read<Misc::UInt32>();
		if(token==0x78563412U)
			pipe->setSwapOnRead(true);
		else if(token!=0x12345678U)
			throw Misc::makeStdErr(__PRETTY_FUNCTION__,"Invalid response from remote AR Sandbox");
		
		/* Receive the remote AR Sandbox's property grid size, cell size, and elevation range: */
		for(int i=0;i<2;++i)
			{
			gridSize[i]=pipe->read<Misc::UInt32>();
			cellSize[i]=pipe->read<Misc::Float32>();
			bathymetrySize[i]=gridSize[i]-1;
			}
		for(int i=0;i<2;++i)
			elevationRange[i]=pipe->read<Misc::Float32>();
		
		/* Initialize the quantized grid buffers: */
		for(int i=0;i<2;++i)
			{
			bathymetry[i]=new Pixel[bathymetrySize.volume()];
			waterLevel[i]=new Pixel[gridSize.volume()];
			snowHeight[i]=new Pixel[gridSize.volume()];
			}
		currentGrid=0;
		
		/* Initialize the grid buffers: */
		for(int i=0;i<3;++i)
			grids.getBuffer(i).init(gridSize);
		
		/* Read the initial set of grids using intra-frame decompression: */
		IntraFrameDecompressor decompressor(*pipe);
		decompressor.decompressFrame(bathymetrySize[0],bathymetrySize[1],bathymetry[currentGrid]);
		decompressor.decompressFrame(gridSize[0],gridSize[1],waterLevel[currentGrid]);
		decompressor.decompressFrame(gridSize[0],gridSize[1],snowHeight[currentGrid]);
		unquantizeGrids();
		}
	catch(const std::runtime_error& err)
		{
		/* Disconnect from the server: */
		delete pipe;
		
		/* Release all allocated resources: */
		for(int i=0;i<2;++i)
			{
			delete[] bathymetry[i];
			delete[] waterLevel[i];
			delete[] snowHeight[i];
			}
		}
	
	
	try
		{
		/* Receive the remote AR Sandbox's water table grid size, cell size, and elevation range: */
		for(int i=0;i<2;++i)
			{
			gridSize[i]=pipe->read<Misc::UInt32>();
			cellSize[i]=pipe->read<Misc::Float32>();
			bathymetrySize[i]=gridSize[i]-1;
			}
		for(int i=0;i<2;++i)
			elevationRange[i]=pipe->read<Misc::Float32>();
		
		/* Initialize the quantized grid buffers: */
		for(int i=0;i<2;++i)
			{
			bathymetry[i]=new Pixel[bathymetrySize[1]*bathymetrySize[0]];
			waterLevel[i]=new Pixel[gridSize[1]*gridSize[0]];
			snowHeight[i]=new Pixel[gridSize[1]*gridSize[0]];
			}
		currentGrid=0;
		
		/* Initialize the grid buffers: */
		for(int i=0;i<3;++i)
			grids.getBuffer(i).init(gridSize);
		
		/* Read the initial set of grids using intra-frame decompression: */
		IntraFrameDecompressor decompressor(*pipe);
		decompressor.decompressFrame(bathymetrySize[0],bathymetrySize[1],bathymetry[currentGrid]);
		decompressor.decompressFrame(gridSize[0],gridSize[1],waterLevel[currentGrid]);
		decompressor.decompressFrame(gridSize[0],gridSize[1],snowHeight[currentGrid]);
		unquantizeGrids();
		}
	catch(const std::runtime_error& err)
		{
		/* Disconnect from the remote AR Sandbox: */
		delete pipe;
		
		/* Re-throw the exception: */
		throw;
		}
	
	/* Start listening on the TCP pipe: */
	dispatcher.addIOEventListener(pipe->getFd(),Threads::EventDispatcher::Read,serverMessageCallback,this);
	communicationThread.start(this,&SandboxClient::communicationThreadMethod);
	}

RemoteClient::~RemoteClient(void)
	{
	/* Release allocated resources: */
	for(int i=0;i<2;++i)
		{
		delete[] bathymetry[i];
		delete[] waterLevel[i];
		delete[] snowHeight[i];
		}
	}
