/***********************************************************************
RemoteServer - Class to connect remote bathymetry and water level
viewers to an Augmented Reality Sandbox.
Copyright (c) 2019-2025 Oliver Kreylos

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

#ifndef REMOTESERVER_INCLUDED
#define REMOTESERVER_INCLUDED

#include <vector>
#include <Threads/Thread.h>
#include <Threads/TripleBuffer.h>
#include <Threads/EventDispatcher.h>
#include <Comm/ListeningTCPSocket.h>
#include <Comm/TCPPipe.h>
#include <Geometry/Point.h>
#include <Geometry/Vector.h>
#include <Geometry/OrthonormalTransformation.h>
#include <GL/gl.h>
#include <Vrui/Types.h>

#include "Types.h"
#include "Pixel.h"

/* Forward declarations: */
class GLContextData;
class Sandbox;

class RemoteServer
	{
	/* Embedded classes: */
	private:
	struct GridBuffers // Structure representing a triplet of grids
		{
		/* Elements: */
		public:
		GLfloat* bathymetry;
		GLfloat* waterLevel;
		GLfloat* snowHeight;
		
		/* Constructors and destructors: */
		GridBuffers(void)
			:bathymetry(0),waterLevel(0),snowHeight(0)
			{
			}
		~GridBuffers(void)
			{
			delete[] bathymetry;
			delete[] waterLevel;
			delete[] snowHeight;
			}
		
		/* Methods: */
		void init(const GLsizei gridSize[2]) // Initializes the grids
			{
			bathymetry=new GLfloat[(gridSize[1]-1)*(gridSize[0]-1)];
			waterLevel=new GLfloat[gridSize[1]*gridSize[0]];
			snowHeight=new GLfloat[gridSize[1]*gridSize[0]];
			}
		};
	
	struct Client // Structure representing a remote client
		{
		/* Embedded classes: */
		public:
		enum ClientStates // States of the client communication state machine
			{
			START=0,INTRA,INTER
			};
		
		/* Elements: */
		RemoteServer* server; // Pointer to remote server to simplify event handling
		Comm::TCPPipe clientPipe; // Pipe connected to the remote client
		Threads::EventDispatcher::ListenerKey listenerKey; // Key with which this client is listening for I/O events
		ClientStates state; // Client's protocol state
		Vrui::Point position; // Client's current position in grid space
		Vrui::Vector direction; // Client's current viewing direction in grid space
		
		/* Constructors and destructors: */
		Client(RemoteServer* sServer); // Connects a remote client from a pending incoming connection on the listening socket
		};
	
	/* Elements: */
	Sandbox* sandbox; // Pointer to the sandbox object
	GLsizei gridSize[2]; // Width and height of the water table's cell-centered quantity grid
	GLfloat cellSize[2]; // Width and height of each water table cell
	GLfloat elevationRange[2]; // Minimum and maximum valid elevations
	GLfloat eScale,eOffset; // Quantization factors for grids
	Threads::EventDispatcher dispatcher; // Dispatcher for events on the listening socket and any connected client sockets
	Threads::Thread communicationThread; // Thread to handle communication with connected clients in the background
	Comm::ListeningTCPSocket listenSocket; // Socket on which to listen for incoming remote connections
	std::vector<Client*> clients; // List of currently connected clients
	unsigned int numClients; // Number of connected clients in streaming state
	Threads::TripleBuffer<std::vector<Vrui::ONTransform> > clientPositions; // Triple buffer of lists of positions/orientations of connected clients
	double requestInterval; // Time interval between requests for new property grids
	double nextRequestTime; // Application time at which to request the next property grids
	Threads::TripleBuffer<GridBuffers> grids; // Triple buffer of arrays to receive property grids
	Pixel* bathymetry[2]; // Pair of buffers for the current quantized bathymetry grid
	Pixel* waterLevel[2]; // Pair of buffers for the current quantized water grid
	Pixel* snowHeight[2]; // Pair of buffers for the current quantized snow grid
	int currentGrid; // Index of the current buffer pair
	
	/* Private methods: */
	void quantizeGrid(GLsizei width,GLsizei height,const GLfloat* source,Pixel* dest); // Quantizes a float-valued source grid into a Pixel-valued destination grid
	void disconnectClient(Client* client,bool removeListener); // Disconnects the given client after a communications error
	static void newConnectionCallback(Threads::EventDispatcher::IOEvent& event); // Callback called when a connection attempt is made at the listening socket
	static void clientMessageCallback(Threads::EventDispatcher::IOEvent& event); // Callback called when a message is received from a connected client
	void* communicationThreadMethod(void); // Method handling communication with connected clients in the background
	static void readBackCallback(GLfloat* bathymetryBuffer,GLfloat* waterLevelBuffer,GLfloat* snowHeightBuffer,void* userData); // Callback called when new property grids have been read back from the GPU
	
	/* Constructors and destructors: */
	public:
	RemoteServer(Sandbox* sSandbox,int listenPortId,double sRequestInterval); // Creates a remote server for the given water table and listening port ID
	~RemoteServer(void);
	
	/* Methods: */
	void frame(double applicationTime); // Called from the AR Sandbox's frame method
	void glRenderAction(const PTransform& projection,const OGTransform& modelview,GLContextData& contextData) const; // Renders the remote server's current state
	};

#endif
