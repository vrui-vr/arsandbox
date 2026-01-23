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

#include "SandboxClient.h"

#include <string>
#include <stdexcept>
#include <iostream>
#include <Misc/PrintInteger.h>
#include <Misc/FunctionCalls.h>
#include <Misc/MessageLogger.h>
#include <Comm/TCPPipe.h>
#include <Math/Math.h>
#include <Geometry/LinearUnit.h>
#include <GL/gl.h>
#include <GL/GLMaterialTemplates.h>
#include <GL/GLMiscTemplates.h>
#include <GL/GLLightTracker.h>
#include <GL/GLContextData.h>
#include <GL/Extensions/GLARBDepthClamp.h>
#include <GL/Extensions/GLARBDepthTexture.h>
#include <GL/Extensions/GLARBFragmentShader.h>
#include <GL/Extensions/GLARBShadow.h>
#include <GL/Extensions/GLARBTextureFloat.h>
#include <GL/Extensions/GLARBTextureRectangle.h>
#include <GL/Extensions/GLARBTextureRg.h>
#include <GL/Extensions/GLARBVertexBufferObject.h>
#include <GL/Extensions/GLARBVertexShader.h>
#include <GL/GLModels.h>
#include <GL/GLGeometryWrappers.h>
#include <GL/GLTransformationWrappers.h>
#include <Vrui/Viewer.h>
#include <Vrui/CoordinateManager.h>
#include <Vrui/Lightsource.h>
#include <Vrui/LightsourceManager.h>
#include <Vrui/ToolManager.h>
#include <Vrui/DisplayState.h>

#include "TextureTracker.h"
#include "ElevationColorMap.h"

/****************************************************
Static eleemnts of class SandboxClient::TeleportTool:
****************************************************/

SandboxClient::TeleportToolFactory* SandboxClient::TeleportTool::factory=0;

/********************************************
Methods of class SandboxClient::TeleportTool:
********************************************/

void SandboxClient::TeleportTool::applyNavState(void) const
	{
	/* Compose and apply the navigation transformation: */
	Vrui::NavTransform nav=physicalFrame;
	nav*=Vrui::NavTransform::rotate(Vrui::Rotation::rotateZ(azimuth));
	nav*=Geometry::invert(surfaceFrame);
	Vrui::setNavigationTransformation(nav);
	}

void SandboxClient::TeleportTool::initNavState(void)
	{
	/* Calculate the main viewer's current head and foot positions: */
	Point headPos=Vrui::getMainViewer()->getHeadPosition();
	footPos=Vrui::calcFloorPoint(headPos);
	headHeight=Geometry::dist(headPos,footPos);
	
	/* Set up a physical navigation frame around the main viewer's current head position: */
	calcPhysicalFrame(headPos);
	
	/* Calculate the initial environment-aligned surface frame in navigation coordinates: */
	surfaceFrame=Vrui::getInverseNavigationTransformation()*physicalFrame;
	Vrui::NavTransform newSurfaceFrame=surfaceFrame;
	
	/* Align the initial frame with the application's surface and calculate Euler angles: */
	AlignmentData ad(surfaceFrame,newSurfaceFrame,Vrui::getMeterFactor()*Scalar(0.25),Vrui::getMeterFactor());
	Scalar elevation,roll;
	align(ad,azimuth,elevation,roll);
	
	/* Move the physical frame to the foot position, and adjust the surface frame accordingly: */
	newSurfaceFrame*=Geometry::invert(physicalFrame)*Vrui::NavTransform::translate(footPos-headPos)*physicalFrame;
	physicalFrame.leftMultiply(Vrui::NavTransform::translate(footPos-headPos));
	
	/* Apply the initial navigation state: */
	surfaceFrame=newSurfaceFrame;
	applyNavState();
	}

void SandboxClient::TeleportTool::initClass(void)
	{
	/* Create a factory object for the teleporting tool class: */
	factory=new TeleportToolFactory("TeleportTool","Teleport",Vrui::getToolManager()->loadClass("SurfaceNavigationTool"),*Vrui::getToolManager());
	
	/* Set the teleport tool class' input layout: */
	factory->setNumButtons(2);
	factory->setButtonFunction(0,"Toggle");
	factory->setButtonFunction(1,"Teleport");
	
	/* Register the teleport tool class with Vrui's tool manager: */
	Vrui::getToolManager()->addClass(factory,Vrui::ToolManager::defaultToolFactoryDestructor);
	}

SandboxClient::TeleportTool::TeleportTool(const Vrui::ToolFactory* factory,const Vrui::ToolInputAssignment& inputAssignment)
	:Vrui::SurfaceNavigationTool(factory,inputAssignment),
	 cast(false)
	{
	sphereRenderer.setVariableRadius();
	cylinderRenderer.setVariableRadius();
	}

SandboxClient::TeleportTool::~TeleportTool(void)
	{
	}

const Vrui::ToolFactory* SandboxClient::TeleportTool::getFactory(void) const
	{
	return factory;
	}

void SandboxClient::TeleportTool::buttonCallback(int buttonSlotIndex,Vrui::InputDevice::ButtonCallbackData* cbData)
	{
	switch(buttonSlotIndex)
		{
		case 0:
			if(cbData->newButtonState) // Button has just been pressed
				{
				/* Act depending on this tool's current state: */
				if(isActive())
					{
					if(!cast)
						{
						/* Deactivate this tool: */
						deactivate();
						}
					}
				else
					{
					/* Try activating this tool: */
					if(activate())
						{
						/* Initialize the navigation state: */
						initNavState();
						}
					}
				}
			
			break;
		
		case 1:
			if(isActive())
				{
				if(cbData->newButtonState)
					cast=true;
				else
					{
					/* Teleport to the end of the cast arc if there is one: */
					if(!castArc.empty())
						surfaceFrame.leftMultiply(Vrui::NavTransform::translate(castArc.back()-surfaceFrame.getOrigin()));
					
					cast=false;
					}
				}
			
			break;
		}
	}

void SandboxClient::TeleportTool::frame(void)
	{
	if(isActive())
		{
		/* Calculate the new head and foot positions: */
		Point newHead=Vrui::getMainViewer()->getHeadPosition();
		Point newFootPos=Vrui::calcFloorPoint(newHead);
		headHeight=Geometry::dist(newHead,newFootPos);
		
		/* Create a physical navigation frame around the new foot position: */
		calcPhysicalFrame(newFootPos);
		
		/* Calculate the movement from walking: */
		Vector move=newFootPos-footPos;
		footPos=newFootPos;
		
		/* Transform the movement vector from physical space to the physical navigation frame: */
		move=physicalFrame.inverseTransform(move);
		
		/* Rotate by the current azimuth angle: */
		move=Vrui::Rotation::rotateZ(-azimuth).transform(move);
		
		/* Move the surface frame: */
		Vrui::NavTransform newSurfaceFrame=surfaceFrame;
		newSurfaceFrame*=Vrui::NavTransform::translate(move);
		
		/* Re-align the surface frame with the surface: */
		AlignmentData ad(surfaceFrame,newSurfaceFrame,Vrui::getMeterFactor()*Scalar(0.25),Vrui::getMeterFactor());
		align(ad);
		
		/* Apply the newly aligned surface frame: */
		surfaceFrame=newSurfaceFrame;
		applyNavState();
		
		if(cast)
			{
			/* Establish boundaries of the castable area: */
			Scalar xMin=application->bDomain.min[0];
			Scalar xMax=application->bDomain.max[0];
			Scalar yMin=application->bDomain.min[1];
			Scalar yMax=application->bDomain.max[1];
			
			/* Cast an arc from the current input device position: */
			castArc.clear();
			Point cp=Vrui::getInverseNavigationTransformation().transform(getButtonDevicePosition(1));
			Vector cv=Vrui::getInverseNavigationTransformation().transform(getButtonDeviceRayDirection(1)*(Vrui::getMeterFactor()*Scalar(10)));
			Vector ca(0,0,-Vrui::getInverseNavigationTransformation().getScaling()*Vrui::getMeterFactor()*Scalar(9.81));
			
			/* Check if the cast is potentially valid: */
			if((cp[0]>=xMin||cv[0]>Scalar(0))&&(cp[0]<=xMax||cv[0]<Scalar(0))&&(cp[1]>=yMin||cv[1]>Scalar(0))&&(cp[1]<=yMax||cv[1]<Scalar(0)))
				{
				castArc.push_back(cp);
				Scalar stepSize(0.05);
				for(int i=0;i<100;++i)
					{
					Point cpn=cp+cv*stepSize;
					
					/* Limit casting to the valid bathymetry area: */
					Vector normal=Vector::zero;
					Scalar lambda(1);
					if(cp[0]>=xMin&&cpn[0]<xMin)
						{
						Scalar l=(xMin-cp[0])/(cpn[0]-cp[0]);
						if(lambda>l)
							{
							normal=Vector(1,0,0);
							lambda=l;
							}
						}
					if(cp[0]<=xMax&&cpn[0]>xMax)
						{
						Scalar l=(xMax-cp[0])/(cpn[0]-cp[0]);
						if(lambda>l)
							{
							normal=Vector(-1,0,0);
							lambda=l;
							}
						}
					if(cp[1]>=yMin&&cpn[1]<yMin)
						{
						Scalar l=(yMin-cp[1])/(cpn[1]-cp[1]);
						if(lambda>l)
							{
							normal=Vector(0,1,0);
							lambda=l;
							}
						}
					if(cp[1]<=yMax&&cpn[1]>yMax)
						{
						Scalar l=(yMax-cp[1])/(cpn[1]-cp[1]);
						if(lambda>l)
							{
							normal=Vector(0,-1,0);
							lambda=l;
							}
						}
					
					/* Intersect the arc with the bathymetry: */
					Scalar l=application->intersectLine(cp,cpn);
					if(lambda>l)
						{
						normal=Vector::zero;
						lambda=l;
						}
					
					if(lambda<Scalar(1))
						{
						cpn=Geometry::affineCombination(cp,cpn,lambda);
						
						/* Stop casting if the arc hit the ground; otherwise, reflect the arc: */
						if(normal==Vector::zero)
							{
							castArc.push_back(cpn);
							break;
							}
						else
							{
							// cv-=normal*(Scalar(2)*(cv*normal)); // Fully elastic reflection
							cv-=normal*(cv*normal); // Fully inelastic reflection
							}
						}
					
					castArc.push_back(cpn);
					cp=cpn;
					cv+=ca*(stepSize*lambda);
					}
				}
			}
		}
	}

void SandboxClient::TeleportTool::display(GLContextData& contextData) const
	{
	if(isActive()&&cast&&!castArc.empty())
		{
		/* Draw the cast arc: */
		Vrui::goToNavigationalSpace(contextData);
		Scalar radius=Vrui::getInchFactor()*Scalar(1)*Vrui::getInverseNavigationTransformation().getScaling();
		
		glMaterialAmbientAndDiffuse(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(0.0f,1.0f,0.0f));
		glMaterialSpecular(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(0.333f,0.333f,0.333f));
		glMaterialShininess(GLMaterialEnums::FRONT,32.0f);
		glMaterialEmission(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(1.0f,0.0f,0.0f));
		
		sphereRenderer.enable(Vrui::getNavigationTransformation().getScaling(),contextData);
		glBegin(GL_POINTS);
		for(std::vector<Point>::const_iterator caIt=castArc.begin();caIt!=castArc.end();++caIt)
			glVertex4f((*caIt)[0],(*caIt)[1],(*caIt)[2],radius);
		glVertex4f(castArc.back()[0],castArc.back()[1],castArc.back()[2],Vrui::getMeterFactor()*Scalar(0.125)*Vrui::getInverseNavigationTransformation().getScaling());
		glEnd();
		sphereRenderer.disable(contextData);
		
		cylinderRenderer.enable(Vrui::getNavigationTransformation().getScaling(),contextData);
		glBegin(GL_LINE_STRIP);
		for(std::vector<Point>::const_iterator caIt=castArc.begin();caIt!=castArc.end();++caIt)
			glVertex4f((*caIt)[0],(*caIt)[1],(*caIt)[2],radius);
		glEnd();
		cylinderRenderer.disable(contextData);
		
		glPopMatrix();
		}
	}

/****************************************
Methods of class SandboxClient::DataItem:
****************************************/

SandboxClient::DataItem::DataItem(void)
	:bathymetryTexture(0),waterTexture(0),snowTexture(0),textureVersion(0),
	 depthTexture(0),depthTextureSize(0,0),
	 bathymetryVertexBuffer(0),bathymetryIndexBuffer(0),
	 waterVertexBuffer(0),waterIndexBuffer(0),
	 lightStateVersion(0)
	{
	/* Initialize required OpenGL extensions: */
	GLARBDepthClamp::initExtension();
	GLARBDepthTexture::initExtension();
	GLARBFragmentShader::initExtension();
	GLARBShaderObjects::initExtension();
	GLARBShadow::initExtension();
	GLARBTextureFloat::initExtension();
	GLARBTextureRectangle::initExtension();
	GLARBTextureRg::initExtension();
	GLARBVertexBufferObject::initExtension();
	GLARBVertexShader::initExtension();
	Shader::initExtensions();
	TextureTracker::initExtensions();
	
	/* Create texture objects: */
	GLuint textures[4];
	glGenTextures(4,textures);
	bathymetryTexture=textures[0];
	waterTexture=textures[1];
	snowTexture=textures[2];
	depthTexture=textures[3];
	
	/* Create buffer objects: */
	GLuint buffers[4];
	glGenBuffersARB(4,buffers);
	bathymetryVertexBuffer=buffers[0];
	bathymetryIndexBuffer=buffers[1];
	waterVertexBuffer=buffers[2];
	waterIndexBuffer=buffers[3];
	}

SandboxClient::DataItem::~DataItem(void)
	{
	/* Destroy texture objects: */
	GLuint textures[4];
	textures[0]=bathymetryTexture;
	textures[1]=waterTexture;
	textures[2]=waterTexture;
	textures[3]=depthTexture;
	glDeleteTextures(4,textures);
	
	/* Destroy buffer objects: */
	GLuint buffers[4];
	buffers[0]=bathymetryVertexBuffer;
	buffers[1]=bathymetryIndexBuffer;
	buffers[2]=waterVertexBuffer;
	buffers[3]=waterIndexBuffer;
	glDeleteBuffersARB(4,buffers);
	}

/******************************
Methods of class SandboxClient:
******************************/

SandboxClient::Scalar SandboxClient::intersectLine(const SandboxClient::Point& p0,const SandboxClient::Point& p1) const
	{
	/* Convert the points to grid coordinates: */
	Point gp0(p0[0]/cellSize[0]-Scalar(1),p0[1]/cellSize[1]-Scalar(1),p0[2]);
	Point gp1(p1[0]/cellSize[0]-Scalar(1),p1[1]/cellSize[1]-Scalar(1),p1[2]);
	Vector gd=gp1-gp0;
	
	/* Clip the line segment against the grid's boundaries: */
	Scalar l0(0);
	Scalar l1(1);
	for(int i=0;i<2;++i)
		{
		/* Clip against the lower boundary: */
		Scalar b(0);
		if(gp0[i]<b)
			{
			if(gp1[i]>b)
				l0=Math::max(l0,(b-gp0[i])/gd[i]);
			else
				return Scalar(1);
			}
		else if(gp1[i]<b)
			{
			if(gp0[i]>b)
				l1=Math::min(l1,(b-gp0[i])/gd[i]);
			else
				return Scalar(1);
			}
		
		/* Clip against the upper boundary: */
		b=Scalar(bSize[i]-1);
		if(gp0[i]>b)
			{
			if(gp1[i]<b)
				l0=Math::max(l0,(b-gp0[i])/gd[i]);
			else
				return Scalar(1);
			}
		else if(gp1[i]>b)
			{
			if(gp0[i]<b)
				l1=Math::min(l1,(b-gp0[i])/gd[i]);
			else
				return Scalar(1);
			}
		}
	if(l0>=l1)
		return Scalar(1);
	
	/* Find the grid cell containing the first point: */
	Point gp=Geometry::affineCombination(gp0,gp1,l0);
	unsigned int cp[2];
	for(int i=0;i<2;++i)
		cp[i]=Math::clamp(int(Math::floor(gp[i])),int(0),int(bSize[i])-2);
	Scalar cl0=l0;
	while(cl0<l1)
		{
		/* Calculate the line parameter where the line segment leaves the current cell: */
		Scalar cl1=l1;
		int exit=-1;
		for(int i=0;i<2;++i)
			{
			Scalar el=cl1;
			if(gp0[i]<gp1[i])
				el=(Scalar(cp[i]+1)-gp0[i])/gd[i];
			else if(gp0[i]>gp1[i])
				el=(Scalar(cp[i])-gp0[i])/gd[i];
			if(cl1>el)
				{
				cl1=el;
				exit=i;
				}
			}
		
		/* Intersect the line segment with the surface inside the current cell: */
		const RemoteClient::GridScalar* cell=remoteClient->getBathymetryGrid()+(cp[1]*bSize[0]+cp[0]);
		Scalar c0=cell[0];
		Scalar c1=cell[1];
		Scalar c2=cell[bSize[0]];
		Scalar c3=cell[bSize[0]+1];
		Scalar cx0=Scalar(cp[0]);
		Scalar cx1=Scalar(cp[0]+1);
		Scalar cy0=Scalar(cp[1]);
		Scalar cy1=Scalar(cp[1]+1);
		Scalar fxy=c0-c1+c3-c2;
		Scalar fx=(c1-c0)*cy1-(c3-c2)*cy0;
		Scalar fy=(c2-c0)*cx1-(c3-c1)*cx0;
		Scalar f=(c0*cx1-c1*cx0)*cy1-(c2*cx1-c3*cx0)*cy0;
		Scalar a=fxy*gd[0]*gd[1];
		Scalar bc0=(fxy*gp0[1]+fx);
		Scalar bc1=(fxy*gp0[0]+fy);
		Scalar b=bc0*gd[0]+bc1*gd[1]-gd[2];
		Scalar c=bc0*gp0[0]+bc1*gp0[1]-gp0[2]-fxy*gp0[0]*gp0[1]+f;
		Scalar il=cl1;
		if(a!=Scalar(0))
			{
			/* Solve the quadratic equation and use the smaller valid solution: */
			Scalar det=b*b-Scalar(4)*a*c;
			if(det>=Scalar(0))
				{
				det=Math::sqrt(det);
				if(a>Scalar(0))
					{
					/* Test the smaller intersection first: */
					il=b>=Scalar(0)?(-b-det)/(Scalar(2)*a):(Scalar(2)*c)/(-b+det);
					if(il<cl0)
						il=b>=Scalar(0)?(Scalar(2)*c)/(-b-det):(-b+det)/(Scalar(2)*a);
					}
				else
					{
					/* Test the smaller intersection first: */
					il=b>=Scalar(0)?(Scalar(2)*c)/(-b-det):(-b+det)/(Scalar(2)*a);
					if(il<cl0)
						il=b>=Scalar(0)?(-b-det)/(Scalar(2)*a):(Scalar(2)*c)/(-b+det);
					}
				}
			}
		else
			{
			/* Solve the linear equation: */
			il=-c/b;
			}
		
		/* Check if the intersection is valid: */
		if(il>=cl0&&il<cl1)
			return il;
		
		/* Go to the next cell: */
		if(exit>=0)
			{
			if(gd[exit]<Scalar(0))
				--cp[exit];
			else
				++cp[exit];
			}
		cl0=cl1;
		}
	
	return Scalar(1);
	}

void SandboxClient::serverMessageCallback(Threads::EventDispatcher::IOEvent& event)
	{
	SandboxClient* thisPtr=static_cast<SandboxClient*>(event.getUserData());
	
	try
		{
		/* Let the remote client process the update message: */
		thisPtr->remoteClient->processUpdate();
		}
	catch(const std::runtime_error&)
		{
		/* Show an error message and disconnect from the remote AR Sandbox: */
		Misc::sourcedUserError(__PRETTY_FUNCTION__,"Disconnected from remote AR Sandbox");
		event.removeListener();
		thisPtr->connected=false;
		}
	
	/* Request a new frame: */
	Vrui::requestUpdate();
	}

void SandboxClient::alignSurfaceFrame(Vrui::SurfaceNavigationTool::AlignmentData& alignmentData)
	{
	/* Get the frame's base point: */
	Point base=alignmentData.surfaceFrame.getOrigin();
	
	/* Snap the base point to the currently locked bathymetry grid: */
	base[2]=remoteClient->calcBathymetry(base[0],base[1]);
	
	/* Align the frame with the bathymetry surface's x and y directions: */
	alignmentData.surfaceFrame=Vrui::NavTransform(base-Point::origin,Vrui::Rotation::identity,alignmentData.surfaceFrame.getScaling());
	}

void SandboxClient::compileShaders(SandboxClient::DataItem* dataItem,const GLLightTracker& lightTracker) const
	{
	/*********************************************************************
	Compile and link the bathymetry shader:
	*********************************************************************/
	
	{
	Shader& shader=dataItem->bathymetryShader;
	
	/* Create the vertex shader source code: */
	std::string vertexShaderDefines="\
	#extension GL_ARB_texture_rectangle : enable\n";
	std::string vertexShaderFunctions;
	std::string vertexShaderUniforms="\
	uniform sampler2DRect bathymetrySampler; // Sampler for the bathymetry texture\n\
	uniform vec2 bathymetryCellSize; // Cell size of the bathymetry grid\n";
	if(elevationColorMap!=0)
		{
		vertexShaderUniforms+="\
	uniform sampler1D elevationColorMapSampler; // Sampler for the elevation color map texture\n\
	uniform vec2 elevationColorMapScale; // Scale and offset to sample the elevation color map\n";
		}
	std::string vertexShaderVaryings="\
	varying float dist; // Eye-space distance to vertex for fogging\n";
	std::string vertexShaderMain="\
	void main()\n\
		{\n\
		/* Get the vertex's grid-space z coordinate from the bathymetry texture: */\n\
		vec4 vertexGc=gl_Vertex;\n\
		vertexGc.z=texture2DRect(bathymetrySampler,vertexGc.xy).r;\n\
		\n\
		/* Calculate the vertex's grid-space normal vector: */\n\
		vec3 normalGc;\n\
		normalGc.x=(texture2DRect(bathymetrySampler,vec2(vertexGc.x-1.0,vertexGc.y)).r-texture2DRect(bathymetrySampler,vec2(vertexGc.x+1.0,vertexGc.y)).r)*bathymetryCellSize.y;\n\
		normalGc.y=(texture2DRect(bathymetrySampler,vec2(vertexGc.x,vertexGc.y-1.0)).r-texture2DRect(bathymetrySampler,vec2(vertexGc.x,vertexGc.y+1.0)).r)*bathymetryCellSize.x;\n\
		normalGc.z=2.0*bathymetryCellSize.x*bathymetryCellSize.y;\n\
		\n\
		/* Transform the vertex and its normal vector from grid space to eye space for illumination: */\n\
		vertexGc.x=(vertexGc.x+0.5)*bathymetryCellSize.x;\n\
		vertexGc.y=(vertexGc.y+0.5)*bathymetryCellSize.y;\n\
		vec4 vertexEc=gl_ModelViewMatrix*vertexGc;\n\
		vec3 normalEc=normalize(gl_NormalMatrix*normalGc);\n\
		\n\
		/* Initialize the vertex color accumulators: */\n";
	if(elevationColorMap!=0)
		{
		vertexShaderMain+="\
		/* Look up the elevation color map value: */\n\
		vec4 elevationColor=texture1D(elevationColorMapSampler,vertexGc.z*elevationColorMapScale.x+elevationColorMapScale.y);\n\
		vec4 ambDiff=gl_LightModel.ambient*elevationColor;\n";
		}
	else
		{
		vertexShaderMain+="\
		vec4 ambDiff=gl_LightModel.ambient*gl_FrontMaterial.ambient;\n";
		}
	vertexShaderMain+="\
		vec4 spec=vec4(0.0,0.0,0.0,0.0);\n\
		\n\
		/* Accumulate all enabled light sources: */\n";
	
	/* Create light application functions for all enabled light sources: */
	for(int lightIndex=0;lightIndex<lightTracker.getMaxNumLights();++lightIndex)
		if(lightTracker.getLightState(lightIndex).isEnabled())
			{
			/* Create the light accumulation function: */
			vertexShaderFunctions+=lightTracker.createAccumulateLightFunction(lightIndex);
			
			/* Call the light application function from the bathymetry vertex shader's main function: */
			vertexShaderMain+="\
		accumulateLight";
			char liBuffer[12];
			vertexShaderMain.append(Misc::print(lightIndex,liBuffer+11));
			if(elevationColorMap!=0)
				vertexShaderMain+="(vertexEc,normalEc,elevationColor,elevationColor,gl_FrontMaterial.specular,gl_FrontMaterial.shininess,ambDiff,spec);\n";
			else
				vertexShaderMain+="(vertexEc,normalEc,gl_FrontMaterial.ambient,gl_FrontMaterial.diffuse,gl_FrontMaterial.specular,gl_FrontMaterial.shininess,ambDiff,spec);\n";
			}
	
	/* Finalize the vertex shader's main function: */
	vertexShaderMain+="\
		dist=length(vertexEc.xyz);\n\
		gl_FrontColor=ambDiff+spec;\n\
		gl_Position=gl_ModelViewProjectionMatrix*vertexGc;\n\
		}\n";
	
	/* Compile the vertex shader: */
	shader.addShader(glCompileVertexShaderFromStrings(5,vertexShaderDefines.c_str(),vertexShaderFunctions.c_str(),vertexShaderUniforms.c_str(),vertexShaderVaryings.c_str(),vertexShaderMain.c_str()));
	
	/* Create the fragment shader source code: */
	std::string fragmentShaderMain="\
	uniform vec4 waterColor; // Color of water surface for fogging\n\
	uniform float waterOpacity; // Opacity of water for fogging\n\
	\n\
	varying float dist; // Eye-space distance to vertex for fogging\n\
	\n\
	void main()\n\
		{\n\
		gl_FragColor=mix(waterColor,gl_Color,exp(-dist*waterOpacity));\n\
		}\n";
	
	/* Compile the fragment shader: */
	shader.addShader(glCompileFragmentShaderFromString(fragmentShaderMain.c_str()));
	
	/* Link the shader program: */
	shader.link();
	
	/* Retrieve the bathymetry shader program's uniform variable locations: */
	shader.setUniformLocation("bathymetrySampler");
	shader.setUniformLocation("bathymetryCellSize");
	shader.setUniformLocation("waterColor");
	shader.setUniformLocation("waterOpacity");
	if(elevationColorMap!=0)
		{
		shader.setUniformLocation("elevationColorMapSampler");
		shader.setUniformLocation("elevationColorMapScale");
		}
	}
	
	/*********************************************************************
	Compile and link the opaque water shader:
	*********************************************************************/
	
	{
	Shader& shader=dataItem->opaqueWaterShader;
	
	/* Create the vertex shader source code: */
	std::string vertexShaderDefines="\
	#extension GL_ARB_texture_rectangle : enable\n";
	std::string vertexShaderFunctions;
	std::string vertexShaderVaryings="\
	varying float vertexWaterDepth; // Water depth at a surface's vertex\n";
	std::string vertexShaderUniforms="\
	uniform sampler2DRect waterSampler; // Sampler for the water surface texture\n\
	uniform sampler2DRect bathymetrySampler; // Sampler for the bathymetry texture\n\
	uniform vec2 waterCellSize; // Cell size of the water surface grid\n";
	std::string vertexShaderMain="\
	void main()\n\
		{\n\
		/* Get the vertex's grid-space z coordinate from the water surface texture: */\n\
		vec4 vertexGc=gl_Vertex;\n\
		vertexGc.z=texture2DRect(waterSampler,vertexGc.xy).r;\n\
		\n\
		/* Calculate the vertex's grid-space normal vector: */\n\
		vec3 normalGc;\n\
		normalGc.x=(texture2DRect(waterSampler,vec2(vertexGc.x-1.0,vertexGc.y)).r-texture2DRect(waterSampler,vec2(vertexGc.x+1.0,vertexGc.y)).r)*waterCellSize.y;\n\
		normalGc.y=(texture2DRect(waterSampler,vec2(vertexGc.x,vertexGc.y-1.0)).r-texture2DRect(waterSampler,vec2(vertexGc.x,vertexGc.y+1.0)).r)*waterCellSize.x;\n\
		normalGc.z=1.0*waterCellSize.x*waterCellSize.y;\n\
		\n\
		/* Get the bathymetry elevation at the same location and calculate the vertex's water depth: */\n\
		float bathy=(texture2DRect(bathymetrySampler,vertexGc.xy-vec2(1.0,1.0)).r\n\
		            +texture2DRect(bathymetrySampler,vertexGc.xy-vec2(1.0,0.0)).r\n\
		            +texture2DRect(bathymetrySampler,vertexGc.xy-vec2(0.0,1.0)).r\n\
		            +texture2DRect(bathymetrySampler,vertexGc.xy-vec2(0.0,0.0)).r)*0.25;\n\
		vertexWaterDepth=vertexGc.z-bathy;\n\
		\n\
		/* Transform the vertex and its normal vector from grid space to eye space for illumination: */\n\
		vertexGc.x=(vertexGc.x-0.5)*waterCellSize.x;\n\
		vertexGc.y=(vertexGc.y-0.5)*waterCellSize.y;\n\
		vec4 vertexEc=gl_ModelViewMatrix*vertexGc;\n\
		vec3 normalEc=normalize(gl_NormalMatrix*normalGc);\n\
		\n\
		/* Initialize the vertex color accumulators: */\n\
		vec4 ambDiff=gl_LightModel.ambient*gl_FrontMaterial.ambient;\n\
		vec4 spec=vec4(0.0,0.0,0.0,0.0);\n\
		\n\
		/* Accumulate all enabled light sources: */\n";
	
	/* Create light application functions for all enabled light sources: */
	for(int lightIndex=0;lightIndex<lightTracker.getMaxNumLights();++lightIndex)
		if(lightTracker.getLightState(lightIndex).isEnabled())
			{
			/* Create the light accumulation function: */
			vertexShaderFunctions+=lightTracker.createAccumulateLightFunction(lightIndex);
			
			/* Call the light application function from the bathymetry vertex shader's main function: */
			vertexShaderMain+="\
			accumulateLight";
			char liBuffer[12];
			vertexShaderMain.append(Misc::print(lightIndex,liBuffer+11));
			vertexShaderMain+="(vertexEc,normalEc,gl_FrontMaterial.ambient,gl_FrontMaterial.diffuse,gl_FrontMaterial.specular,gl_FrontMaterial.shininess,ambDiff,spec);\n";
			}
	
	/* Finalize the vertex shader's main function: */
	vertexShaderMain+="\
		gl_FrontColor=vec4(ambDiff.xyz+spec.xyz,1.0);\n\
		gl_BackColor=gl_FrontColor;\n\
		gl_Position=gl_ModelViewProjectionMatrix*vertexGc;\n\
		}\n";
	
	/* Compile the vertex shader: */
	shader.addShader(glCompileVertexShaderFromStrings(5,vertexShaderDefines.c_str(),vertexShaderFunctions.c_str(),vertexShaderVaryings.c_str(),vertexShaderUniforms.c_str(),vertexShaderMain.c_str()));
	
	/* Create the fragment shader source code: */
	std::string fragmentShaderVaryings="\
	varying float vertexWaterDepth; // Water depth at a surface's vertex\n";
	std::string fragmentShaderUniforms="\
	uniform float waterDepthThreshold; // Depth threshold under which a vertex is considered dry\n";
	std::string fragmentShaderMain="\
	void main()\n\
		{\n\
		/* Discard the fragment if the ground underneath is actually dry: */\n\
		if(vertexWaterDepth<waterDepthThreshold)\n\
			discard;\n\
		gl_FragColor=gl_Color;\n\
		}\n";
	
	/* Compile the fragment shader: */
	shader.addShader(glCompileFragmentShaderFromStrings(3,fragmentShaderVaryings.c_str(),fragmentShaderUniforms.c_str(),fragmentShaderMain.c_str()));
	
	/* Link the shader program: */
	shader.link();
	
	/* Retrieve the shader program's uniform variable locations: */
	shader.setUniformLocation("waterSampler");
	shader.setUniformLocation("bathymetrySampler");
	shader.setUniformLocation("waterCellSize");
	shader.setUniformLocation("waterDepthThreshold");
	}
	
	/*********************************************************************
	Compile and link the transparent water shader:
	*********************************************************************/
	
	{
	Shader& shader=dataItem->transparentWaterShader;
	
	/* Create the vertex shader source code: */
	std::string vertexShaderDefines="\
	#extension GL_ARB_texture_rectangle : enable\n";
	std::string vertexShaderFunctions;
	std::string vertexShaderVaryings="\
	varying float vertexWaterDepth; // Water depth at a surface's vertex\n";
	std::string vertexShaderUniforms="\
	uniform sampler2DRect waterSampler; // Sampler for the water surface texture\n\
	uniform sampler2DRect bathymetrySampler; // Sampler for the bathymetry texture\n\
	uniform vec2 waterCellSize; // Cell size of the water surface grid\n";
	std::string vertexShaderMain="\
	void main()\n\
		{\n\
		/* Get the vertex's grid-space z coordinate from the water surface texture: */\n\
		vec4 vertexGc=gl_Vertex;\n\
		vertexGc.z=texture2DRect(waterSampler,vertexGc.xy).r;\n\
		\n\
		/* Calculate the vertex's grid-space normal vector: */\n\
		vec3 normalGc;\n\
		normalGc.x=(texture2DRect(waterSampler,vec2(vertexGc.x-1.0,vertexGc.y)).r-texture2DRect(waterSampler,vec2(vertexGc.x+1.0,vertexGc.y)).r)*waterCellSize.y;\n\
		normalGc.y=(texture2DRect(waterSampler,vec2(vertexGc.x,vertexGc.y-1.0)).r-texture2DRect(waterSampler,vec2(vertexGc.x,vertexGc.y+1.0)).r)*waterCellSize.x;\n\
		normalGc.z=1.0*waterCellSize.x*waterCellSize.y;\n\
		\n\
		/* Get the bathymetry elevation at the same location and calculate the vertex's water depth: */\n\
		float bathy=(texture2DRect(bathymetrySampler,vertexGc.xy-vec2(1.0,1.0)).r\n\
		            +texture2DRect(bathymetrySampler,vertexGc.xy-vec2(1.0,0.0)).r\n\
		            +texture2DRect(bathymetrySampler,vertexGc.xy-vec2(0.0,1.0)).r\n\
		            +texture2DRect(bathymetrySampler,vertexGc.xy-vec2(0.0,0.0)).r)*0.25;\n\
		vertexWaterDepth=vertexGc.z-bathy;\n\
		\n\
		/* Transform the vertex and its normal vector from grid space to eye space for illumination: */\n\
		vertexGc.x*=waterCellSize.x;\n\
		vertexGc.y*=waterCellSize.y;\n\
		vec4 vertexEc=gl_ModelViewMatrix*vertexGc;\n\
		vec3 normalEc=normalize(gl_NormalMatrix*normalGc);\n\
		\n\
		/* Initialize the vertex color accumulators: */\n\
		vec4 ambDiff=gl_LightModel.ambient*gl_FrontMaterial.ambient;\n\
		vec4 spec=vec4(0.0,0.0,0.0,0.0);\n\
		\n\
		/* Accumulate all enabled light sources: */\n";
	
	/* Create light application functions for all enabled light sources: */
	for(int lightIndex=0;lightIndex<lightTracker.getMaxNumLights();++lightIndex)
		if(lightTracker.getLightState(lightIndex).isEnabled())
			{
			/* Create the light accumulation function: */
			vertexShaderFunctions+=lightTracker.createAccumulateLightFunction(lightIndex);
			
			/* Call the light application function from the bathymetry vertex shader's main function: */
			vertexShaderMain+="\
			accumulateLight";
			char liBuffer[12];
			vertexShaderMain.append(Misc::print(lightIndex,liBuffer+11));
			vertexShaderMain+="(vertexEc,normalEc,gl_FrontMaterial.ambient,gl_FrontMaterial.diffuse,gl_FrontMaterial.specular,gl_FrontMaterial.shininess,ambDiff,spec);\n";
			}
	
	/* Finalize the vertex shader's main function: */
	vertexShaderMain+="\
		gl_FrontColor=vec4(ambDiff.xyz+spec.xyz,1.0);\n\
		gl_BackColor=gl_FrontColor;\n\
		gl_Position=gl_ModelViewProjectionMatrix*vertexGc;\n\
		}\n";
	
	/* Compile the vertex shader: */
	shader.addShader(glCompileVertexShaderFromStrings(5,vertexShaderDefines.c_str(),vertexShaderFunctions.c_str(),vertexShaderVaryings.c_str(),vertexShaderUniforms.c_str(),vertexShaderMain.c_str()));
	
	std::string fragmentShaderDefines="\
	#extension GL_ARB_texture_rectangle : enable\n";
	std::string fragmentShaderVaryings="\
	varying float vertexWaterDepth; // Water depth at a surface's vertex\n";
	std::string fragmentShaderUniforms="\
	uniform sampler2DRect depthSampler; // Sampler for the depth buffer texture\n\
	uniform mat4 depthMatrix; // Matrix to transform fragment coordinates to model space\n\
	uniform float waterOpacity; // Scale factor for fogging\n\
	uniform float waterDepthThreshold; // Depth threshold under which a vertex is considered dry\n";
	
	/* Create the fragment shader source code: */
	std::string fragmentShaderMain="\
	void main()\n\
		{\n\
		/* Discard the fragment if the ground underneath is actually dry: */\n\
		if(vertexWaterDepth<waterDepthThreshold)\n\
			discard;\n\
		\n\
		/* Transform the fragment currently in the pixel back to model space: */\n\
		vec4 oldFrag=depthMatrix*vec4(gl_FragCoord.xy,texture2DRect(depthSampler,gl_FragCoord.xy).x,1.0);\n\
		vec4 newFrag=depthMatrix*vec4(gl_FragCoord.xyz,1.0);\n\
		float modelDist=length(newFrag.xyz/newFrag.w-oldFrag.xyz/oldFrag.w);\n\
		// gl_FragColor=vec4(gl_Color.xyz,1.0-exp(-modelDist*waterOpacity));\n\
		gl_FragColor=vec4(vec3(0.2,0.5,0.8),1.0-exp(-modelDist*waterOpacity));\n\
		}\n";
	
	/* Compile the fragment shader: */
	shader.addShader(glCompileFragmentShaderFromStrings(4,fragmentShaderDefines.c_str(),fragmentShaderVaryings.c_str(),fragmentShaderUniforms.c_str(),fragmentShaderMain.c_str()));
	
	/* Link the shader program: */
	shader.link();
	
	/* Retrieve the shader program's uniform variable locations: */
	shader.setUniformLocation("waterSampler");
	shader.setUniformLocation("bathymetrySampler");
	shader.setUniformLocation("waterCellSize");
	shader.setUniformLocation("depthSampler");
	shader.setUniformLocation("depthMatrix");
	shader.setUniformLocation("waterOpacity");
	shader.setUniformLocation("waterDepthThreshold");
	}
	
	/*********************************************************************
	Compile and link the snow shader:
	*********************************************************************/
	
	{
	Shader& shader=dataItem->snowShader;
	
	/* Create the vertex shader source code: */
	std::string vertexShaderDefines="\
	#extension GL_ARB_texture_rectangle : enable\n";
	std::string vertexShaderFunctions;
	std::string vertexShaderVaryings="\
	varying float vertexSnowHeight; // Snow height at a surface's vertex\n";
	std::string vertexShaderUniforms="\
	uniform sampler2DRect snowSampler; // Sampler for the snow height texture\n\
	uniform sampler2DRect bathymetrySampler; // Sampler for the bathymetry texture\n\
	uniform vec2 waterCellSize; // Cell size of the water surface grid\n";
	std::string vertexShaderMain="\
	void main()\n\
		{\n\
		/* Get the vertex's snow height from the snow height texture: */\n\
		vertexSnowHeight=texture2DRect(snowSampler,gl_Vertex.xy).r;\n\
		\n\
		/* Get the bathymetry elevation at the same location and calculate the vertex's grid-space z coordinate: */\n\
		float b0=texture2DRect(bathymetrySampler,gl_Vertex.xy-vec2(1.0,1.0)).r;\n\
		float b1=texture2DRect(bathymetrySampler,gl_Vertex.xy-vec2(1.0,0.0)).r;\n\
		float b2=texture2DRect(bathymetrySampler,gl_Vertex.xy-vec2(0.0,1.0)).r;\n\
		float b3=texture2DRect(bathymetrySampler,gl_Vertex.xy-vec2(0.0,0.0)).r;\n\
		float bathy=(b0+b1+b2+b3)*0.25;\n\
		vec4 vertexGc=gl_Vertex;\n\
		vertexGc.z=vertexSnowHeight+bathy;\n\
		\n\
		/* Calculate the vertex's grid-space normal vector: */\n\
		float b4=texture2DRect(bathymetrySampler,gl_Vertex.xy-vec2(1.0,2.0)).r;\n\
		float b5=texture2DRect(bathymetrySampler,gl_Vertex.xy-vec2(0.0,2.0)).r;\n\
		float b6=texture2DRect(bathymetrySampler,gl_Vertex.xy-vec2(2.0,1.0)).r;\n\
		float b7=texture2DRect(bathymetrySampler,gl_Vertex.xy-vec2(-1.0,1.0)).r;\n\
		float b8=texture2DRect(bathymetrySampler,gl_Vertex.xy-vec2(2.0,0.0)).r;\n\
		float b9=texture2DRect(bathymetrySampler,gl_Vertex.xy-vec2(-1.0,0.0)).r;\n\
		float b10=texture2DRect(bathymetrySampler,gl_Vertex.xy-vec2(1.0,-1.0)).r;\n\
		float b11=texture2DRect(bathymetrySampler,gl_Vertex.xy-vec2(0.0,-1.0)).r;\n\
		vec3 normalGc;\n\
		float zxm=texture2DRect(snowSampler,vec2(vertexGc.x-1.0,vertexGc.y)).r+(b6+b0+b8+b2)*0.25;\n\
		float zxp=texture2DRect(snowSampler,vec2(vertexGc.x+1.0,vertexGc.y)).r+(b1+b7+b3+b9)*0.25;\n\
		normalGc.x=(zxm-zxp)*waterCellSize.y;\n\
		float zym=texture2DRect(snowSampler,vec2(vertexGc.x,vertexGc.y-1.0)).r+(b4+b5+b0+b1)*0.25;\n\
		float zyp=texture2DRect(snowSampler,vec2(vertexGc.x,vertexGc.y+1.0)).r+(b2+b3+b10+b11)*0.25;\n\
		normalGc.y=(zym-zyp)*waterCellSize.x;\n\
		normalGc.z=1.0*waterCellSize.x*waterCellSize.y;\n\
		\n\
		/* Transform the vertex and its normal vector from grid space to eye space for illumination: */\n\
		vertexGc.x*=waterCellSize.x;\n\
		vertexGc.y*=waterCellSize.y;\n\
		vec4 vertexEc=gl_ModelViewMatrix*vertexGc;\n\
		vec3 normalEc=normalize(gl_NormalMatrix*normalGc);\n\
		\n\
		/* Initialize the vertex color accumulators: */\n\
		vec4 ambDiff=gl_LightModel.ambient*gl_FrontMaterial.ambient;\n\
		vec4 spec=vec4(0.0,0.0,0.0,0.0);\n\
		\n\
		/* Accumulate all enabled light sources: */\n";
	
	/* Create light application functions for all enabled light sources: */
	for(int lightIndex=0;lightIndex<lightTracker.getMaxNumLights();++lightIndex)
		if(lightTracker.getLightState(lightIndex).isEnabled())
			{
			/* Create the light accumulation function: */
			vertexShaderFunctions+=lightTracker.createAccumulateLightFunction(lightIndex);
			
			/* Call the light application function from the bathymetry vertex shader's main function: */
			vertexShaderMain+="\
			accumulateLight";
			char liBuffer[12];
			vertexShaderMain.append(Misc::print(lightIndex,liBuffer+11));
			vertexShaderMain+="(vertexEc,normalEc,gl_FrontMaterial.ambient,gl_FrontMaterial.diffuse,gl_FrontMaterial.specular,gl_FrontMaterial.shininess,ambDiff,spec);\n";
			}
	
	/* Finalize the vertex shader's main function: */
	vertexShaderMain+="\
		gl_FrontColor=vec4(ambDiff.xyz+spec.xyz,1.0);\n\
		gl_BackColor=gl_FrontColor;\n\
		gl_Position=gl_ModelViewProjectionMatrix*vertexGc;\n\
		}\n";
	
	/* Compile the vertex shader: */
	shader.addShader(glCompileVertexShaderFromStrings(5,vertexShaderDefines.c_str(),vertexShaderFunctions.c_str(),vertexShaderVaryings.c_str(),vertexShaderUniforms.c_str(),vertexShaderMain.c_str()));
	
	/* Create the fragment shader source code: */
	std::string fragmentShaderVaryings="\
	varying float vertexSnowHeight; // Snow height at a surface's vertex\n";
	std::string fragmentShaderUniforms="\
	uniform float snowHeightThreshold; // Height threshold under which a vertex is considered uncovered\n";
	std::string fragmentShaderMain="\
	void main()\n\
		{\n\
		/* Discard the fragment if the ground underneath is actually uncovered: */\n\
		if(vertexSnowHeight<snowHeightThreshold)\n\
			discard;\n\
		gl_FragColor=gl_Color;\n\
		}\n";
	
	/* Compile the fragment shader: */
	shader.addShader(glCompileFragmentShaderFromStrings(3,fragmentShaderVaryings.c_str(),fragmentShaderUniforms.c_str(),fragmentShaderMain.c_str()));
	
	/* Link the shader program: */
	shader.link();
	
	/* Retrieve the shader program's uniform variable locations: */
	shader.setUniformLocation("snowSampler");
	shader.setUniformLocation("bathymetrySampler");
	shader.setUniformLocation("waterCellSize");
	shader.setUniformLocation("snowHeightThreshold");
	}
	
	/* Mark the shaders as up-to-date: */
	dataItem->lightStateVersion=lightTracker.getVersion();
	}

SandboxClient::SandboxClient(int& argc,char**& argv)
	:Vrui::Application(argc,argv),
	 remoteClient(0),connected(false),
	 elevationColorMap(0),
	 sun(0),
	 gridVersion(0),
	 underwater(false),undersnow(false)
	{
	/* Parse the command line: */
	const char* serverHostName=0;
	int serverPort=26000;
	const char* elevationColorMapName=0;
	for(int argi=1;argi<argc;++argi)
		{
		if(argv[argi][0]=='-')
			{
			if(strcasecmp(argv[argi]+1,"hm")==0)
				{
				if(argi+1<argc&&argv[argi+1][0]!='-')
					{
					++argi;
					elevationColorMapName=argv[argi];
					}
				else
					std::cerr<<"SandboxClient: Missing height map name"<<std::endl;
				}
			else
				std::cerr<<"SandboxClient: Ignoring command line option "<<argv[argi]<<std::endl;
			}
		else if(serverHostName==0)
			serverHostName=argv[argi];
		else
			std::cerr<<"SandboxClient: Ignoring command line argument "<<argv[argi]<<std::endl;
		}
	
	/* Connect to the remote AR Sandbox: */
	if(serverHostName==0)
		throw Misc::makeStdErr(__PRETTY_FUNCTION__,"No server name provided");
	try
		{
		remoteClient=new RemoteClient(serverHostName,serverPort);
		connected=true;
		}
	catch(const std::runtime_error& err)
		{
		throw Misc::makeStdErr(__PRETTY_FUNCTION__,"Unable to connect to remote AR Sandbox on %s:%d due to exception %s",serverHostName,serverPort,err.what());
		}
	
	/* Extract the remote AR Sandbox's cell-centered and bathymetry grid sizes, property grid cell size, and bathymetry extents: */
	gSize=remoteClient->getGridSize();
	bSize=remoteClient->getBathymetrySize();
	for(int i=0;i<2;++i)
		cellSize[i]=Scalar(remoteClient->getCellSize()[i]);
	bDomain=GridBox(remoteClient->getBathymetryDomain());
	quantFactor=GLfloat(remoteClient->getElevationRange()[1]-remoteClient->getElevationRange()[0])/65535.0f;
	
	/* Load a requested elevation color map: */
	if(elevationColorMapName!=0)
		{
		try
			{
			elevationColorMap=new ElevationColorMap(elevationColorMapName);
			}
		catch(const std::runtime_error& err)
			{
			std::cerr<<"SandboxClient: Unable to load height map "<<elevationColorMapName<<" due to exception "<<err.what()<<std::endl;
			}
		}
	
	/* Start listening on the remote client's TCP pipe: */
	dispatcher.addIOEventListener(remoteClient->getPipe().getFd(),Threads::EventDispatcher::Read,serverMessageCallback,this);
	dispatcher.startThread();
	
	/* Set the linear unit to scale the AR Sandbox correctly: */
	Vrui::getCoordinateManager()->setUnit(Geometry::LinearUnit(Geometry::LinearUnit::METER,1.0));
	
	/* Create a light source and disable all viewers' headlights: */
	sun=Vrui::getLightsourceManager()->createLightsource(false);
	sun->enable();
	sun->getLight().position=GLLight::Position(-0.3,0.4,1.0,0.0);
	for(int i=0;i<Vrui::getNumViewers();++i)
		Vrui::getViewer(i)->setHeadlightState(false);
	
	/* Adjust the backplane distance: */
	Scalar minBackplaneDist=Geometry::dist(bDomain.min,bDomain.max)*Scalar(1.25);
	Vrui::setBackplaneDist(Math::max(minBackplaneDist,Vrui::getBackplaneDist()));
	
	/* Create tool classes: */
	TeleportTool::initClass();
	}

SandboxClient::~SandboxClient(void)
	{
	/* Disconnect from the remote AR Sandbox: */
	dispatcher.stopThread();
	delete remoteClient;
	
	/* Release allocated resources: */
	delete elevationColorMap;
	}

void SandboxClient::toolCreationCallback(Vrui::ToolManager::ToolCreationCallbackData* cbData)
	{
	/* Check if the new tool is a surface navigation tool: */
	Vrui::SurfaceNavigationTool* surfaceNavigationTool=dynamic_cast<Vrui::SurfaceNavigationTool*>(cbData->tool);
	if(surfaceNavigationTool!=0)
		{
		/* Set the new tool's alignment function: */
		surfaceNavigationTool->setAlignFunction(Misc::createFunctionCall(this,&SandboxClient::alignSurfaceFrame));
		}
	
	/* Call the base class method: */
	Vrui::Application::toolCreationCallback(cbData);
	}

void SandboxClient::frame(void)
	{
	/* Lock the most recent grid buffers and update the grid version number if there are new grids: */
	if(remoteClient->lockNewGrids())
		++gridVersion;
	
	/* Retrieve the main viewer's head position in grid coordinates: */
	Point head=Vrui::getHeadPosition();
	GridBox::Point head2(head[0],head[1]);
	
	/* Check if the head is underwater and/or under snow: */
	underwater=false;
	undersnow=false;
	if(bDomain.contains(head2))
		{
		/* Compare the head's elevation to the currently locked water level: */
		underwater=head[2]<=remoteClient->calcWaterLevel(head2[0],head2[1]);
		
		/* Compare the head's elevation to the currently locked bathymetry and snow height: */
		undersnow=head[2]<=remoteClient->calcBathymetry(head2[0],head2[1])+remoteClient->calcSnowHeight(head2[0],head2[1]);
		}
	
	if(connected)
		{
		try
			{
			/* Send the current head position and view direction to the remote AR Sandbox: */
			remoteClient->sendViewer(head,Vrui::getViewDirection());
			}
		catch(const std::runtime_error&)
			{
			/* Show an error message and disconnect from the remote AR Sandbox: */
			Misc::sourcedUserError(__PRETTY_FUNCTION__,"Disconnected from remote AR Sandbox");
			connected=false;
			}
		}
	}

void SandboxClient::display(GLContextData& contextData) const
	{
	/* Retrieve the context data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	if(undersnow)
		{
		/* Draw a white ping pong ball around the current eye position: */
		glPushAttrib(GL_ENABLE_BIT);
		glDisable(GL_LIGHTING);
		
		glPushMatrix();
		const Vrui::DisplayState& ds=Vrui::getDisplayState(contextData);
		glLoadMatrix(ds.modelviewPhysical);
		glTranslate(ds.eyePosition-Vrui::Point::origin);
		glColor3f(1.0f,1.0f,1.0f);
		glFrontFace(GL_CW);
		glDrawCube(12.0*Vrui::getInchFactor());
		glFrontFace(GL_CCW);
		
		glPopMatrix();
		glPopAttrib();
		
		return;
		}
	
	/* Set up OpenGL state: */
	glPushAttrib(GL_ENABLE_BIT);
	
	/* Create a texture tracker: */
	TextureTracker textureTracker;
	
	/* Update the shader programs if necessary: */
	const GLLightTracker& lightTracker=*contextData.getLightTracker();
	if(dataItem->lightStateVersion!=lightTracker.getVersion())
		compileShaders(dataItem,lightTracker);
	
	/* Activate the bathymetry shader: */
	glMaterialAmbientAndDiffuse(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(0.6f,0.4f,0.1f));
	#if 0
	glMaterialSpecular(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(1.0f,1.0f,1.0f));
	glMaterialShininess(GLMaterialEnums::FRONT,32.0f);
	#else
	glMaterialSpecular(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(0.0f,0.0f,0.0f));
	glMaterialShininess(GLMaterialEnums::FRONT,0.0f);
	#endif
	dataItem->bathymetryShader.use();
	textureTracker.reset();
	
	/* Render the locked bathymetry grid: */
	dataItem->bathymetryShader.uploadUniform(textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->bathymetryTexture));
	if(dataItem->textureVersion!=gridVersion)
		{
		/* Upload the new bathymetry grid: */
		glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,bSize,GL_RED,GL_FLOAT,remoteClient->getBathymetryGrid());
		}
	dataItem->bathymetryShader.uploadUniform(GLfloat(cellSize[0]),GLfloat(cellSize[1]));
	dataItem->bathymetryShader.uploadUniform(0.2f,0.5f,0.8f,1.0f);
	
	GLfloat waterOpacity(Vrui::getInverseNavigationTransformation().getScaling()*Scalar(0.25));
	dataItem->bathymetryShader.uploadUniform(underwater?waterOpacity:0.0f);
	
	if(elevationColorMap!=0)
		{
		/* Upload the elevation color map: */
		dataItem->bathymetryShader.uploadUniform(elevationColorMap->bindTexture(contextData,textureTracker));
		GLfloat scale[2];
		scale[0]=GLfloat(GLdouble(1)/(elevationColorMap->getScalarRangeMax()-elevationColorMap->getScalarRangeMin()));
		scale[1]=-scale[0]*GLfloat(elevationColorMap->getScalarRangeMin());
		dataItem->bathymetryShader.uploadUniform(scale[0],scale[1]);
		}
	
	/* Bind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->bathymetryVertexBuffer);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->bathymetryIndexBuffer);
	
	/* Draw the bathymetry: */
	{
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	GLuint* indexPtr=0;
	for(unsigned int y=1;y<bSize[1];++y,indexPtr+=bSize[0]*2)
		glDrawElements(GL_QUAD_STRIP,bSize[0]*2,GL_UNSIGNED_INT,indexPtr);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	}
	
	/* Activate the water surface shader: */
	glMaterialAmbientAndDiffuse(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(0.2f,0.5f,0.8f));
	glMaterialSpecular(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(1.0f,1.0f,1.0f));
	glMaterialShininess(GLMaterialEnums::FRONT,64.0f);
	dataItem->opaqueWaterShader.use();
	textureTracker.reset();
	
	/* Render the locked water surface grid: */
	dataItem->opaqueWaterShader.uploadUniform(textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->waterTexture));
	if(dataItem->textureVersion!=gridVersion)
		{
		/* Upload the new water surface grid: */
		glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,gSize,GL_RED,GL_FLOAT,remoteClient->getWaterLevelGrid());
		}
	dataItem->opaqueWaterShader.uploadUniform(textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->bathymetryTexture));
	
	dataItem->opaqueWaterShader.uploadUniform(GLfloat(cellSize[0]),GLfloat(cellSize[1]));
	dataItem->opaqueWaterShader.uploadUniform(quantFactor);
	
	/* Bind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->waterVertexBuffer);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->waterIndexBuffer);
	
	/* Draw the back side of the water surface: */
	glCullFace(GL_FRONT);
	{
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	GLuint* indexPtr=0;
	for(unsigned int y=1;y<gSize[1];++y,indexPtr+=gSize[0]*2)
		glDrawElements(GL_QUAD_STRIP,gSize[0]*2,GL_UNSIGNED_INT,indexPtr);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	}
	glCullFace(GL_BACK);
	
	/* Activate the snow surface shader: */
	glMaterialAmbientAndDiffuse(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(1.0f,1.0f,1.0f));
	glMaterialSpecular(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(1.0f,1.0f,1.0f));
	glMaterialShininess(GLMaterialEnums::FRONT,24.0f);
	dataItem->snowShader.use();
	textureTracker.reset();
	
	/* Render the locked snow height grid: */
	dataItem->snowShader.uploadUniform(textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->snowTexture));
	if(dataItem->textureVersion!=gridVersion)
		{
		/* Upload the new snow height grid: */
		glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,gSize,GL_RED,GL_FLOAT,remoteClient->getSnowHeightGrid());
		}
	dataItem->snowShader.uploadUniform(textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->bathymetryTexture));
	
	dataItem->snowShader.uploadUniform(GLfloat(cellSize[0]),GLfloat(cellSize[1]));
	dataItem->snowShader.uploadUniform(quantFactor);
	
	/* Bind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->waterVertexBuffer);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->waterIndexBuffer);
	
	/* Draw the snow surface: */
	{
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	GLuint* indexPtr=0;
	for(unsigned int y=1;y<gSize[1];++y,indexPtr+=gSize[0]*2)
		glDrawElements(GL_QUAD_STRIP,gSize[0]*2,GL_UNSIGNED_INT,indexPtr);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	}
	
	/* Protect the buffers and textures and deactivate the shaders: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	Shader::unuse();
	
	/* Mark the textures as up-to-date: */
	dataItem->textureVersion=gridVersion;
	
	/* Restore OpenGL state: */
	glPopAttrib();
	}

void SandboxClient::resetNavigation(void)
	{
	Vrui::NavTransform nav=Vrui::NavTransform::translateFromOriginTo(Vrui::getDisplayCenter());
	
	/* Scale to the AR Sandbox's true size: */
	nav*=Vrui::NavTransform::scale(Vrui::getMeterFactor());
	
	/* Align the bathymetry surface horizontally: */
	Vrui::Vector z=Vrui::getUpDirection();
	Vrui::Vector y=Vrui::getForwardDirection();
	y.orthogonalize(z);
	Vrui::Vector x=y^z;
	nav*=Vrui::NavTransform::rotate(Vrui::Rotation::fromBaseVectors(x,y));
	
	/* Lock the most recent grid buffers: */
	if(remoteClient->lockNewGrids())
		++gridVersion;
	
	/* Evaluate the bathymetry grid at the grid center: */
	GridBox::Point mid=Geometry::mid(bDomain.min,bDomain.max);
	Scalar midZ=remoteClient->calcBathymetry(mid[0],mid[1]);
	
	/* Center on a point some distance above the center of the grid: */
	nav*=Vrui::NavTransform::translateToOriginFrom(Vrui::Point(mid[0],mid[1],midZ+Scalar(2)));
	
	Vrui::setNavigationTransformation(nav);
	}

void SandboxClient::initContext(GLContextData& contextData) const
	{
	/* Create the context data item and store it in the GLContextData object: */
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);
	
	/* Create the bathymetry elevation texture: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->bathymetryTexture);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_R32F,bSize,0,GL_RED,GL_FLOAT,0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	
	/* Create the water surface elevation texture: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->waterTexture);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_R32F,gSize,0,GL_RED,GL_FLOAT,0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	
	/* Create the snow height texture: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->snowTexture);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_R32F,gSize,0,GL_RED,GL_FLOAT,0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	
	/* Create the depth texture: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->depthTexture);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_COMPARE_MODE_ARB,GL_NONE);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	
	/* Upload the grid of bathymetry template vertices into the vertex buffer: */
	{
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->bathymetryVertexBuffer);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB,bSize.volume()*sizeof(Vertex),0,GL_STATIC_DRAW_ARB);
	Vertex* vPtr=static_cast<Vertex*>(glMapBufferARB(GL_ARRAY_BUFFER_ARB,GL_WRITE_ONLY_ARB));
	for(unsigned int y=0;y<bSize[1];++y)
		for(unsigned int x=0;x<bSize[0];++x,++vPtr)
			{
			/* Set the template vertex' position to the cell center's position: */
			vPtr->position[0]=GLfloat(x)+0.5f;
			vPtr->position[1]=GLfloat(y)+0.5f;
			}
	glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	
	/* Upload the bathymetry's triangle indices into the index buffer: */
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->bathymetryIndexBuffer);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,(bSize[1]-1)*bSize[0]*2*sizeof(GLuint),0,GL_STATIC_DRAW_ARB);
	GLuint* iPtr=static_cast<GLuint*>(glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,GL_WRITE_ONLY_ARB));
	for(unsigned int y=1;y<bSize[1];++y)
		for(unsigned int x=0;x<bSize[0];++x,iPtr+=2)
			{
			iPtr[0]=GLuint(y*bSize[0]+x);
			iPtr[1]=GLuint((y-1)*bSize[0]+x);
			}
	glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	}
	
	/* Upload the grid of water surface template vertices into the vertex buffer: */
	{
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->waterVertexBuffer);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB,gSize.volume()*sizeof(Vertex),0,GL_STATIC_DRAW_ARB);
	Vertex* vPtr=static_cast<Vertex*>(glMapBufferARB(GL_ARRAY_BUFFER_ARB,GL_WRITE_ONLY_ARB));
	for(unsigned int y=0;y<gSize[1];++y)
		for(unsigned int x=0;x<gSize[0];++x,++vPtr)
			{
			/* Set the template vertex' position to the cell center's position: */
			vPtr->position[0]=GLfloat(x)+0.5f;
			vPtr->position[1]=GLfloat(y)+0.5f;
			}
	glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	
	/* Upload the water surface's triangle indices into the index buffer: */
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->waterIndexBuffer);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,(gSize[1]-1)*gSize[0]*2*sizeof(GLuint),0,GL_STATIC_DRAW_ARB);
	GLuint* iPtr=static_cast<GLuint*>(glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,GL_WRITE_ONLY_ARB));
	for(unsigned int y=1;y<gSize[1];++y)
		for(unsigned int x=0;x<gSize[0];++x,iPtr+=2)
			{
			iPtr[0]=GLuint(y*gSize[0]+x);
			iPtr[1]=GLuint((y-1)*gSize[0]+x);
			}
	glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	}
	
	/* Create the initial bathymetry and water surface shader programs: */
	compileShaders(dataItem,*contextData.getLightTracker());
	}

void SandboxClient::glRenderActionTransparent(GLContextData& contextData) const
	{
	/* Bail out if the viewer is under water: */
	if(underwater)
		return;
	
	/* Retrieve Vrui's display state and the context data item: */
	const Vrui::DisplayState& ds=Vrui::getDisplayState(contextData);
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Create a texture tracker: */
	TextureTracker textureTracker;
	
	/* Go to navigational space: */
	Vrui::goToNavigationalSpace(contextData);
	
	/* Activate the water surface shader: */
	glMaterialAmbientAndDiffuse(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(0.2f,0.5f,0.8f));
	glMaterialSpecular(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(1.0f,1.0f,1.0f));
	glMaterialShininess(GLMaterialEnums::FRONT,64.0f);
	dataItem->transparentWaterShader.use();
	textureTracker.reset();
	
	/* Render the locked bathymetry and water surface grids: */
	dataItem->transparentWaterShader.uploadUniform(textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->waterTexture));
	dataItem->transparentWaterShader.uploadUniform(textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->bathymetryTexture));
	
	dataItem->transparentWaterShader.uploadUniform(GLfloat(cellSize[0]),GLfloat(cellSize[1]));
	
	/* Check if the depth texture needs to be resized: */
	dataItem->transparentWaterShader.uploadUniform(textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->depthTexture));
	if(dataItem->depthTextureSize!=ds.maxFrameSize)
		{
		/* Resize the depth texture: */
		dataItem->depthTextureSize=ds.maxFrameSize;
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_DEPTH_COMPONENT24_ARB,dataItem->depthTextureSize,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_BYTE,0);
		}
	
	/* Copy the current depth buffer from the current viewport into the depth texture: */
	glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,ds.viewport.offset,ds.viewport);
	
	/* Calculate the fragment unprojection matrix: */
	Vrui::PTransform depthTransform=Vrui::PTransform::identity;
	
	/* Start with the transformation from clip coordinates to device coordinates: */
	Vrui::PTransform::Matrix& depthMatrix=depthTransform.getMatrix();
	depthMatrix(0,0)=Scalar(0.5)*Scalar(ds.viewport.size[0]);
	depthMatrix(0,3)=Scalar(ds.viewport.offset[0])+depthMatrix(0,0);
	depthMatrix(1,1)=Scalar(0.5)*Scalar(ds.viewport.size[1]);
	depthMatrix(1,3)=Scalar(ds.viewport.offset[1])+depthMatrix(1,1);
	depthMatrix(2,2)=Scalar(0.5);
	depthMatrix(2,3)=Scalar(0.5);
	
	/* Concatenate the projection matrix: */
	depthTransform*=ds.projection;
	
	/* Concatenate the navigational-space modelview matrix: */
	depthTransform*=ds.modelviewNavigational;
	
	/* Invert the depth matrix and upload it to the shader: */
	depthTransform.doInvert();
	dataItem->transparentWaterShader.uploadUniform(depthTransform);
	
	dataItem->transparentWaterShader.uploadUniform(0.25f);
	dataItem->transparentWaterShader.uploadUniform(quantFactor);
	
	/* Bind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->waterVertexBuffer);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->waterIndexBuffer);
	
	/* Draw the water surface: */
	glEnable(GL_DEPTH_CLAMP);
	{
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	GLuint* indexPtr=0;
	for(unsigned int y=1;y<gSize[1];++y,indexPtr+=gSize[0]*2)
		glDrawElements(GL_QUAD_STRIP,gSize[0]*2,GL_UNSIGNED_INT,indexPtr);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	}
	glDisable(GL_DEPTH_CLAMP);
	
	/* Activate the opaque water surface shader: */
	glMaterialAmbientAndDiffuse(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(0.0f,0.0f,0.0f));
	glMaterialSpecular(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(1.0f,1.0f,1.0f));
	glMaterialShininess(GLMaterialEnums::FRONT,64.0f);
	dataItem->opaqueWaterShader.use();
	textureTracker.reset();
	
	dataItem->opaqueWaterShader.uploadUniform(textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->waterTexture));
	dataItem->opaqueWaterShader.uploadUniform(textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->bathymetryTexture));
	
	dataItem->opaqueWaterShader.uploadUniform(GLfloat(cellSize[0]),GLfloat(cellSize[1]));
	dataItem->opaqueWaterShader.uploadUniform(quantFactor);
	
	/* Draw the water surface: */
	glBlendFunc(GL_ONE,GL_ONE);
	{
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	GLuint* indexPtr=0;
	for(unsigned int y=1;y<gSize[1];++y,indexPtr+=gSize[0]*2)
		glDrawElements(GL_QUAD_STRIP,gSize[0]*2,GL_UNSIGNED_INT,indexPtr);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	}
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	
	/* Protect the buffers and textures and deactivate the shaders: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	glUseProgramObjectARB(0);
	
	/* Return to physical space: */
	glPopMatrix();
	}

/*************
Main function:
*************/

VRUI_APPLICATION_RUN(SandboxClient)
