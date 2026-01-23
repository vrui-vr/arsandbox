/***********************************************************************
SandboxClient - Vrui application connect to a remote AR Sandbox and
render its bathymetry and water level.
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

#ifndef SANDBOXCLIENT_INCLUDED
#define SANDBOXCLIENT_INCLUDED

#include <Threads/EventDispatcherThread.h>
#include <Geometry/Point.h>
#include <Geometry/Vector.h>
#include <Geometry/Box.h>
#include <Geometry/OrthogonalTransformation.h>
#include <GL/gl.h>
#include <GL/GLObject.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <GL/GLSphereRenderer.h>
#include <GL/GLCylinderRenderer.h>
#include <GL/GLGeometryVertex.h>
#include <Vrui/Application.h>
#include <Vrui/TransparentObject.h>
#include <Vrui/GenericToolFactory.h>
#include <Vrui/SurfaceNavigationTool.h>

#include "RemoteClient.h"
#include "Shader.h"

/* Forward declarations: */
class GLLightTracker;
namespace Vrui {
class Lightsource;
}
class ElevationColorMap;

class SandboxClient:public Vrui::Application,public GLObject,public Vrui::TransparentObject
	{
	/* Embedded classes: */
	private:
	typedef Vrui::Scalar Scalar;
	typedef Vrui::Point Point;
	typedef Vrui::Vector Vector;
	typedef Geometry::Box<Scalar,2> GridBox;
	
	class TeleportTool;
	typedef Vrui::GenericToolFactory<TeleportTool> TeleportToolFactory;
	
	class TeleportTool:public Vrui::SurfaceNavigationTool,public Vrui::Application::Tool<SandboxClient>
		{
		friend class Vrui::GenericToolFactory<TeleportTool>;
		
		/* Elements: */
		private:
		static TeleportToolFactory* factory; // Pointer to the factory object for this class
		
		/* Transient navigation state: */
		Point footPos; // Position of the main viewer's foot on the last frame
		Scalar headHeight; // Height of viewer's head above the foot point
		Vrui::NavTransform surfaceFrame; // Current local coordinate frame aligned to the surface in navigation coordinates
		Scalar azimuth; // Current azimuth of view relative to local coordinate frame
		
		bool cast; // Flag if the teleport button is currently pressed
		std::vector<Point> castArc; // The cast arc
		GLSphereRenderer sphereRenderer;
		GLCylinderRenderer cylinderRenderer;
		
		/* Private methods: */
		void applyNavState(void) const; // Sets the navigation transformation based on the tool's current navigation state
		void initNavState(void); // Initializes the tool's navigation state when it is activated
		
			/* Constructors and destructors: */
		public:
		static void initClass(void); // Initializes the teleport tool class's factory class
		TeleportTool(const Vrui::ToolFactory* factory,const Vrui::ToolInputAssignment& inputAssignment);
		virtual ~TeleportTool(void);
		
		/* Methods from class Vrui::Tool: */
		virtual const Vrui::ToolFactory* getFactory(void) const;
		virtual void buttonCallback(int buttonSlotIndex,Vrui::InputDevice::ButtonCallbackData* cbData);
		virtual void frame(void);
		virtual void display(GLContextData& contextData) const;
		};
	
	typedef GLGeometry::Vertex<void,0,void,0,void,GLfloat,2> Vertex; // Type for grid rendering template vertices
	
	struct DataItem:public GLObject::DataItem
		{
		/* Elements: */
		public:
		GLuint bathymetryTexture; // ID of texture object holding bathymetry vertex elevations
		GLuint waterTexture; // ID of texture object holding water surface vertex elevations
		GLuint snowTexture; // ID of texture object holding snow heights
		unsigned int textureVersion; // Version number of bathymetry and water grids stored in textures
		GLuint depthTexture; // ID of the depth texture used for water opacity calculation
		Size depthTextureSize; // Current size of the depth texture image
		GLuint bathymetryVertexBuffer; // ID of vertex buffer object holding bathymetry's template vertices
		GLuint bathymetryIndexBuffer; // ID of index buffer object holding bathymetry's triangles
		GLuint waterVertexBuffer; // ID of vertex buffer object holding water surface's template vertices
		GLuint waterIndexBuffer; // ID of index buffer object holding water surface's triangles
		Shader bathymetryShader; // Shader to render the bathymetry
		Shader opaqueWaterShader; // Shader to render the water surface's back side during the opaque rendering pass
		Shader transparentWaterShader; // Shader to render the water surface's front side during the transparent rendering pass
		Shader snowShader; // Shader to render the snow surface
		unsigned int lightStateVersion; // Version number for current lighting state reflected in the bathymetry and water surface shader programs
		
		/* Constructors and destructors: */
		DataItem(void);
		virtual ~DataItem(void);
		};
	
	/* Elements: */
	RemoteClient* remoteClient; // Client object connected to the remote AR Sandbox server
	bool connected; // Flag if the remote AR Sandbox server is still connected
	Size gSize; // Size of the remote AR Sandbox's cell-centered property grids
	Size bSize; // Size of the remote AR Sandbox's bathymetry grid
	Scalar cellSize[2]; // Cell size of the remote AR Sandbox's property grids
	GridBox bDomain; // Domain of the bathymetry grid
	GLfloat quantFactor; // Un-quantization factor for the remote AR Sandbox's property grids
	ElevationColorMap* elevationColorMap; // The elevation color map
	Threads::EventDispatcherThread dispatcher; // Dispatcher for events on the TCP pipe
	Vrui::Lightsource* sun; // Light source representing the sun
	unsigned int gridVersion; // Version number of most-recently received grids from the remote AR Sandbox
	bool underwater; // Flag if the main viewer's head is currently under water
	bool undersnow; // Flag if the main viewer's head is currently under snow
	
	/* Private methods: */
	Scalar intersectLine(const Point& p0,const Point& p1) const; // Returns the intersection parameter of a line segment with the bathymetry; returns 1.0 if there is no intersection
	static void serverMessageCallback(Threads::EventDispatcher::IOEvent& event); // Callback called when a message arrives from the remote AR Sandbox
	void alignSurfaceFrame(Vrui::SurfaceNavigationTool::AlignmentData& alignmentData); // Aligns the surface frame of a surface navigation tool with the bathymetry surface
	void compileShaders(DataItem* dataItem,const GLLightTracker& lightTracker) const; // Compiles the bathymetry and water surface shader programs based on current lighting state
	
	/* Constructors and destructors: */
	public:
	SandboxClient(int& argc,char**& argv);
	virtual ~SandboxClient(void);
	
	/* Methods from class Vrui::Application: */
	virtual void toolCreationCallback(Vrui::ToolManager::ToolCreationCallbackData* cbData);
	virtual void frame(void);
	virtual void display(GLContextData& contextData) const;
	virtual void resetNavigation(void);
	
	/* Methods from class GLObject: */
	virtual void initContext(GLContextData& contextData) const;
	
	/* Methods from class Vrui::TransparentObject: */
	virtual void glRenderActionTransparent(GLContextData& contextData) const;
	};

#endif
