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
#include <Math/Math.h>

#include "IntraFrameDecompressor.h"
#include "InterFrameDecompressor.h"

/******************************************
Methods of class RemoteClient::GridBuffers:
******************************************/

RemoteClient::GridBuffers::GridBuffers(void)
	:bathymetry(0),waterLevel(0),snowHeight(0)
	{
	}

RemoteClient::GridBuffers::~GridBuffers(void)
	{
	/* Release all allocated resources: */
	delete[] bathymetry;
	delete[] waterLevel;
	delete[] snowHeight;
	}

void RemoteClient::GridBuffers::init(const Size& gridSize)
	{
	bathymetry=new GridScalar[(gridSize[1]-1)*(gridSize[0]-1)]; // Bathymetry grid is vertex-centered and smaller by one in both directions
	waterLevel=new GridScalar[gridSize[1]*gridSize[0]]; // Water level grid is cell-centered
	snowHeight=new GridScalar[gridSize[1]*gridSize[0]]; // Snow height grid is cell-centered
	}

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
	Pixel* qbPtr=bathymetry[currentBuffer];
	for(GridScalar* bPtr=gb.bathymetry;bPtr!=bEnd;++bPtr,++qbPtr)
		*bPtr=GridScalar(*qbPtr)*eScale+eOffset;
	
	/* Un-quantize the water level grid: */
	GridScalar* wlEnd=gb.waterLevel+gridSize.volume();
	Pixel* qwlPtr=waterLevel[currentBuffer];
	for(GridScalar* wlPtr=gb.waterLevel;wlPtr!=wlEnd;++wlPtr,++qwlPtr)
		*wlPtr=GridScalar(*qwlPtr)*eScale+eOffset;
	
	/* Un-quantize the snow height grid: */
	GridScalar* shEnd=gb.snowHeight+gridSize.volume();
	Pixel* qshPtr=snowHeight[currentBuffer];
	for(GridScalar* shPtr=gb.snowHeight;shPtr!=shEnd;++shPtr,++qshPtr)
		*shPtr=GridScalar(*qshPtr)*eScale+eOffset;
	
	/* Post the new set of grids: */
	grids.postNewValue();
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
		
		/* Initialize the grid buffers: */
		for(int i=0;i<3;++i)
			grids.getBuffer(i).init(gridSize);
		
		/* Read the initial set of grids using intra-frame decompression: */
		IntraFrameDecompressor decompressor(*pipe);
		currentBuffer=0;
		decompressor.decompressFrame(bathymetrySize[0],bathymetrySize[1],bathymetry[currentBuffer]);
		decompressor.decompressFrame(gridSize[0],gridSize[1],waterLevel[currentBuffer]);
		decompressor.decompressFrame(gridSize[0],gridSize[1],snowHeight[currentBuffer]);
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
		
		/* Re-throw the exception: */
		throw;
		}
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

RemoteClient::GridBox RemoteClient::getDomain(void) const
	{
	/* The cell-centered grids extend from (0, 0), but can only be evaluated from cell center to cell center: */
	GridBox result;
	for(int i=0;i<2;++i)
		{
		result.min[i]=GridScalar(0.5)*cellSize[i];
		result.max[i]=(GridScalar(gridSize[i])-GridScalar(0.5))*cellSize[i];
		}
	
	return result;
	}

RemoteClient::GridBox RemoteClient::getBathymetryDomain(void) const
	{
	/* The vertex-centered bathymetry grid extends from (1, 1), and can only be evaluated from vertex to vertex: */
	GridBox result;
	for(int i=0;i<2;++i)
		{
		result.min[i]=cellSize[i];
		result.max[i]=GridScalar(bathymetrySize[i])*cellSize[i];
		}
	
	return result;
	}

void RemoteClient::processUpdate(void)
	{
	/* Create an inter-frame decompressor and connect it to the TCP pipe: */
	InterFrameDecompressor decompressor(*pipe);
	
	/* Receive and decompress the quantized property grids into the intermediate buffers: */
	int newBuffer=1-currentBuffer;
	decompressor.decompressFrame(bathymetrySize[0],bathymetrySize[1],bathymetry[currentBuffer],bathymetry[newBuffer]);
	decompressor.decompressFrame(gridSize[0],gridSize[1],waterLevel[currentBuffer],waterLevel[newBuffer]);
	decompressor.decompressFrame(gridSize[0],gridSize[1],snowHeight[currentBuffer],snowHeight[newBuffer]);
	currentBuffer=newBuffer;
	
	/* Un-quantize the received property grids: */
	unquantizeGrids();
	}

RemoteClient::GridScalar RemoteClient::calcBathymetry(RemoteClient::GridScalar x,RemoteClient::GridScalar y) const
	{
	/* Convert the given position to bathymetry grid coordinates and clamp against the boundaries of the bathymetry grid: */
	GridScalar dx=x/cellSize[0]-GridScalar(1);
	int gx=Math::clamp(int(Math::floor(dx)),int(0),int(bathymetrySize[0])-2);
	dx=Math::clamp(dx-GridScalar(gx),GridScalar(0),GridScalar(1));
	GridScalar dy=y/cellSize[1]-GridScalar(1);
	int gy=Math::clamp(int(Math::floor(dy)),int(0),int(bathymetrySize[1])-2);
	dy=Math::clamp(dy-GridScalar(gy),GridScalar(0),GridScalar(1));
	
	/* Access the grid cell containing the given position in the currently locked bathymetry grid: */
	const GridScalar* cell=grids.getLockedValue().bathymetry+(gy*bathymetrySize[0]+gx);
	
	/* Calculate the bathymetry elevation at the given position via bilinear interpolation: */
	GridScalar b0=cell[0]*(GridScalar(1)-dx)+cell[1]*dx;
	cell+=bathymetrySize[0];
	GridScalar b1=cell[0]*(GridScalar(1)-dx)+cell[1]*dx;
	return b0*(GridScalar(1)-dy)+b1*dy;
	}

RemoteClient::GridScalar RemoteClient::calcWaterLevel(RemoteClient::GridScalar x,RemoteClient::GridScalar y) const
	{
	/* Convert the given position to cell-centered grid coordinates and clamp against the boundaries of the water level grid: */
	GridScalar dx=x/cellSize[0]-GridScalar(0.5);
	int gx=Math::clamp(int(Math::floor(dx)),int(0),int(gridSize[0])-2);
	dx=Math::clamp(dx-GridScalar(gx),GridScalar(0),GridScalar(1));
	GridScalar dy=y/cellSize[1]-GridScalar(0.5);
	int gy=Math::clamp(int(Math::floor(dy)),int(0),int(gridSize[1])-2);
	dy=Math::clamp(dy-GridScalar(gy),GridScalar(0),GridScalar(1));
	
	/* Access the grid cell containing the given position in the currently locked water level grid: */
	const GridScalar* cell=grids.getLockedValue().waterLevel+(gy*gridSize[0]+gx);
	
	/* Calculate the water level at the given position via bilinear interpolation: */
	GridScalar b0=cell[0]*(GridScalar(1)-dx)+cell[1]*dx;
	cell+=gridSize[0];
	GridScalar b1=cell[0]*(GridScalar(1)-dx)+cell[1]*dx;
	return b0*(GridScalar(1)-dy)+b1*dy;
	}

RemoteClient::GridScalar RemoteClient::calcSnowHeight(RemoteClient::GridScalar x,RemoteClient::GridScalar y) const
	{
	/* Convert the given position to cell-centered grid coordinates and clamp against the boundaries of the snow height grid: */
	GridScalar dx=x/cellSize[0]-GridScalar(0.5);
	int gx=Math::clamp(int(Math::floor(dx)),int(0),int(gridSize[0])-2);
	dx=Math::clamp(dx-GridScalar(gx),GridScalar(0),GridScalar(1));
	GridScalar dy=y/cellSize[1]-GridScalar(0.5);
	int gy=Math::clamp(int(Math::floor(dy)),int(0),int(gridSize[1])-2);
	dy=Math::clamp(dy-GridScalar(gy),GridScalar(0),GridScalar(1));
	
	/* Access the grid cell containing the given position in the currently locked snow height grid: */
	const GridScalar* cell=grids.getLockedValue().snowHeight+(gy*gridSize[0]+gx);
	
	/* Calculate the snow height at the given position via bilinear interpolation: */
	GridScalar b0=cell[0]*(GridScalar(1)-dx)+cell[1]*dx;
	cell+=gridSize[0];
	GridScalar b1=cell[0]*(GridScalar(1)-dx)+cell[1]*dx;
	return b0*(GridScalar(1)-dy)+b1*dy;
	}

void RemoteClient::sendViewer(const RemoteClient::Point3& headPos,const RemoteClient::Vector3& viewDir)
	{
	/* Write the message identifier: */
	pipe->write<Misc::UInt16>(0);
	
	/* Write the head position and view direction: */
	Geometry::Point<Misc::Float32,3> fhead(headPos);
	pipe->write(fhead.getComponents(),3);
	Geometry::Vector<Misc::Float32,3> fview(viewDir);
	pipe->write(fview.getComponents(),3);
	
	/* Send the message: */
	pipe->flush();
	}
