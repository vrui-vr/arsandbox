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

#ifndef REMOTECLIENT_INCLUDED
#define REMOTECLIENT_INCLUDED

#include <Threads/TripleBuffer.h>

#include "Types.h"
#include "Pixel.h"

/* Forward declarations: */
namespace Comm {
class TCPPipe;
}

class RemoteClient
	{
	/* Embedded classes: */
	public:
	typedef float GridScalar; // Type for values stored in property grids
	
	private:
	struct GridBuffers // Structure representing a triplet of property grids
		{
		/* Elements: */
		public:
		GridScalar* bathymetry;
		GridScalar* waterLevel;
		GridScalar* snowHeight;
		
		/* Constructors and destructors: */
		GridBuffers(void) // Creates an uninitialized grid buffer
			:bathymetry(0),waterLevel(0),snowHeight(0)
			{
			}
		~GridBuffers(void)
			{
			/* Release all allocated resources: */
			delete[] bathymetry;
			delete[] waterLevel;
			delete[] snowHeight;
			}
		
		/* Methods: */
		void init(const Size& gridSize) // Initializes the property grids based on a given grid size
			{
			bathymetry=new GridScalar[(gridSize[1]-1)*(gridSize[0]-1)]; // Bathymetry grid is vertex-centered and smaller by one in both directions
			waterLevel=new GridScalar[gridSize[1]*gridSize[0]]; // Water level grid is cell-centered
			snowHeight=new GridScalar[gridSize[1]*gridSize[0]]; // Snow height grid is cell-centered
			}
		};
	
	/* Elements: */
	Comm::TCPPipe* pipe; // TCP pipe connected to the remote AR Sandbox
	Size gridSize; // Width and height of the cell-centered water level and snow height grids
	GridScalar cellSize[2]; // Width and height of each grid cell
	Size bathymetrySize; // Width and height of the vertex-centered bathymetry grid (one smaller than cell-centered grids)
	GridScalar elevationRange[2]; // Minimum and maximum valid elevations in the property grids
	Pixel* bathymetry[2]; // Pair of intermediate buffers holding quantized bathymetry grids received from the server
	Pixel* waterLevel[2]; // Pair of intermediate buffers holding quantized water level grids received from the server
	Pixel* snowHeight[2]; // Pair of intermediate buffers holding quantized snow height grids received from the server
	int currentBuffer; // Index of the current intermediate grid buffers
	Threads::TripleBuffer<GridBuffers> grids; // Triple buffer of property grids
	unsigned int gridVersion; // Version number of currently locked property grids
	
	/* Private methods: */
	void unquantizeGrids(void); // Un-quantizes the current property grids received from the remote AR Sandbox
	void receiveGridsInter(void); // Receives a set of property grids from the server using inter-frame compression
	
	/* Constructors and destructors: */
	public:
	RemoteClient(const char* serverHostName,int serverPort); // Creates a remote client connected to an AR Sandbox server listening on the given port on the given host
	~RemoteClient(void); // Destroys the remote client
	
	/* Methods: */
	const Size& getGridSize(void) const // Returns the width and height of the cell-centered water level and snow height grids
		{
		return gridSize;
		}
	const GridScalar* getCellSize(void) const // Returns the cell size of the property grids as a two-element array
		{
		return cellSize;
		}
	const Size& getBathymetrySize(void) const // Returns the width and height of the vertex-centered bathymetry grid
		{
		return bathymetrySize;
		}
	const GridScalar* getElevationRange(void) const // Returns the minimum and maximum elevations in the property grids as a two-element array
		{
		return elevationRange;
		}
	bool lockNewGrids(void); // Locks the most recently received property grids; returns true if the grids have been updated since the last call
	unsigned int getGridVersion(void) const // Returns a monotonically increasing version number for the currently locked grids
		{
		return gridVersion;
		}
	const GridScalar* getBathymetry(void) const // Returns a pointer to the first element of the currently locked bathymetry grid
		{
		return grids.getLockedValue().bathymetry;
		}
	const GridScalar* getWaterLevel(void) const // Returns a pointer to the first element of the currently locked water level grid
		{
		return grids.getLockedValue().waterLevel;
		}
	const GridScalar* getSnowHeight(void) const // Returns a pointer to the first element of the currently locked snow height grid
		{
		return grids.getLockedValue().snowHeight;
		}
	};

#endif
