/***********************************************************************
SurfaceRenderer - Class to render a surface defined by a regular grid in
depth image space.
Copyright (c) 2012-2026 Oliver Kreylos

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

#include "SurfaceRenderer.h"

#include <string>
#include <vector>
#include <Misc/PrintInteger.h>
#include <Misc/MessageLogger.h>
#include <Threads/FunctionCalls.h>
#include <GL/gl.h>
#include <GL/GLMiscTemplates.h>
#include <GL/GLVertexArrayParts.h>
#include <GL/Extensions/GLARBFragmentShader.h>
#include <GL/Extensions/GLARBTextureFloat.h>
#include <GL/Extensions/GLARBTextureRectangle.h>
#include <GL/Extensions/GLARBTextureRg.h>
#include <GL/Extensions/GLARBVertexShader.h>
#include <GL/Extensions/GLEXTFramebufferObject.h>
#include <GL/GLLightTracker.h>
#include <GL/GLClipPlaneTracker.h>
#include <GL/GLContextData.h>
#include <GL/GLTransformationWrappers.h>
#include <GL/GLGeometryVertex.h>

#include "TextureTracker.h"
#include "DepthImageRenderer.h"
#include "ElevationColorMap.h"
#include "DEM.h"
#include "WaterTable2.h"
#include "ShaderHelper.h"
#include "Config.h"

/******************************************
Methods of class SurfaceRenderer::DataItem:
******************************************/

SurfaceRenderer::DataItem::DataItem(void)
	:contourLineFramebufferObject(0),contourLineDepthBufferObject(0),contourLineColorTextureObject(0),contourLineVersion(0),
	 surfaceSettingsVersion(0),lightTrackerVersion(0),clipPlaneTrackerVersion(0),clipping(false)
	{
	}

SurfaceRenderer::DataItem::~DataItem(void)
	{
	/* Release all allocated buffers and textures: */
	glDeleteFramebuffersEXT(1,&contourLineFramebufferObject);
	glDeleteRenderbuffersEXT(1,&contourLineDepthBufferObject);
	glDeleteTextures(1,&contourLineColorTextureObject);
	}

/********************************
Methods of class SurfaceRenderer:
********************************/

void SurfaceRenderer::shaderSourceFileChanged(IO::FileMonitor::Event& event)
	{
	/* Invalidate the single-pass surface shader: */
	++surfaceSettingsVersion;
	}

void SurfaceRenderer::updateSinglePassSurfaceShader(const GLLightTracker& lt,const GLClipPlaneTracker& cpt,SurfaceRenderer::DataItem* dataItem) const
	{
	/* Access the single-pass surface shader: */
	Shader& shader=dataItem->heightMapShader;
	
	try
		{
		/*******************************************************************
		Assemble and compile the surface rendering vertex shader:
		*******************************************************************/
		
		/* Assemble the function and declaration strings: */
		std::string vertexFunctions="\
			#extension GL_ARB_texture_rectangle : enable\n";
		
		std::string vertexUniforms="\
			uniform sampler2DRect depthSampler; // Sampler for the depth image-space elevation texture\n\
			uniform mat4 depthProjection; // Transformation from depth image space to camera space\n\
			uniform mat4 projectionModelviewDepthProjection; // Transformation from depth image space to clip space\n";
		
		std::string vertexVaryings;
		
		/* Assemble the vertex shader's main function: */
		std::string vertexMain="\
			void main()\n\
				{\n\
				/* Get the vertex' depth image-space z coordinate from the texture: */\n\
				vec4 vertexDic=gl_Vertex;\n\
				vertexDic.z=texture2DRect(depthSampler,gl_Vertex.xy).r;\n\
				\n\
				/* Transform the vertex from depth image space to camera space and normalize it: */\n\
				vec4 vertexCc=depthProjection*vertexDic;\n\
				vertexCc/=vertexCc.w;\n\
				\n";
		
		if(dem!=0)
			{
			/* Add declarations for DEM matching: */
			vertexUniforms+="\
				uniform mat4 demTransform; // Transformation from camera space to DEM space\n\
				uniform sampler2DRect demSampler; // Sampler for the DEM texture\n\
				uniform float demDistScale; // Distance from surface to DEM at which the color map saturates\n";
			
			vertexVaryings+="\
				varying float demDist; // Scaled signed distance from surface to DEM\n";
			
			/* Add DEM matching code to vertex shader's main function: */
			vertexMain+="\
				/* Transform the camera-space vertex to scaled DEM space: */\n\
				vec4 vertexDem=demTransform*vertexCc;\n\
				\n\
				/* Calculate scaled DEM-surface distance: */\n\
				demDist=(vertexDem.z-texture2DRect(demSampler,vertexDem.xy).r)*demDistScale;\n\
				\n";
			}
		else
			{
			if(elevationColorMap!=0)
				{
				/* Add declarations for height mapping: */
				vertexUniforms+="\
					uniform vec4 heightColorMapPlaneEq; // Plane equation of the base plane in camera space, scaled for height map textures\n";
				
				vertexVaryings+="\
					varying float heightColorMapTexCoord; // Texture coordinate for the height color map\n";
				
				/* Add height mapping code to vertex shader's main function: */
				vertexMain+="\
					/* Plug camera-space vertex into the scaled and offset base plane equation: */\n\
					heightColorMapTexCoord=dot(heightColorMapPlaneEq,vertexCc);\n\
					\n";
				}
			
			if(drawDippingBed)
				{
				/* Add declarations for dipping bed rendering: */
				if(dippingBedFolded)
					{
					vertexUniforms+="\
						uniform float dbc[5]; // Dipping bed coefficients\n";
					}
				else
					{
					vertexUniforms+="\
						uniform vec4 dippingBedPlaneEq; // Plane equation of the dipping bed\n";
					}
				
				vertexVaryings+="\
					varying float dippingBedDistance; // Vertex distance to dipping bed\n";
				
				/* Add dipping bed code to vertex shader's main function: */
				if(dippingBedFolded)
					{
					vertexMain+="\
						/* Calculate distance from camera-space vertex to dipping bed equation: */\n\
						dippingBedDistance=vertexCc.z-(((1.0-dbc[3])+cos(dbc[0]*vertexCc.x)*dbc[3])*sin(dbc[1]*vertexCc.y)*dbc[2]+dbc[4]);\n\
						\n";
					}
				else
					{
					vertexMain+="\
						/* Plug camera-space vertex into the dipping bed equation: */\n\
						dippingBedDistance=dot(dippingBedPlaneEq,vertexCc);\n\
						\n";
					}
				}
			}
		
		/* Check if there are any enabled clipping planes: */
		int numClipPlanes=cpt.getNumEnabledClipPlanes();
		
		/* Check if we need the vertex's eye-space position: */
		if(illuminate||numClipPlanes>0)
			{
			vertexUniforms+="\
				uniform mat4 modelview; // Transformation from camera space to eye space\n";
			
			vertexMain+="\
				/* Transform the vertex from depth image space to eye space: */\n\
				vec4 vertexEc=modelview*vertexCc;\n";
			}
		
		if(illuminate)
			{
			/* Add declarations for illumination: */
			vertexUniforms+="\
				uniform mat4 tangentModelviewDepthProjection; // Transformation from depth image space to eye space for tangent planes\n";
			
			vertexVaryings+="\
				varying vec4 diffColor,specColor; // Diffuse and specular colors, interpolated separately for correct highlights\n";
			
			/* Add illumination code to vertex shader's main function: */
			vertexMain+="\
				/* Calculate the vertex' tangent plane equation in depth image space: */\n\
				vec4 tangentDic;\n\
				tangentDic.x=texture2DRect(depthSampler,vec2(vertexDic.x-1.0,vertexDic.y)).r-texture2DRect(depthSampler,vec2(vertexDic.x+1.0,vertexDic.y)).r;\n\
				tangentDic.y=texture2DRect(depthSampler,vec2(vertexDic.x,vertexDic.y-1.0)).r-texture2DRect(depthSampler,vec2(vertexDic.x,vertexDic.y+1.0)).r;\n\
				tangentDic.z=2.0;\n\
				tangentDic.w=-dot(vertexDic.xyz,tangentDic.xyz)/vertexDic.w;\n\
				\n\
				/* Transform the vertex's tangent plane from depth image space to eye space: */\n\
				vec3 normalEc=normalize((tangentModelviewDepthProjection*tangentDic).xyz);\n\
				\n\
				/* Initialize the color accumulators: */\n\
				diffColor=gl_LightModel.ambient*gl_FrontMaterial.ambient;\n\
				specColor=vec4(0.0,0.0,0.0,0.0);\n\
				\n";
			
			/* Call the appropriate light accumulation function for every enabled light source: */
			bool firstLight=true;
			for(int lightIndex=0;lightIndex<lt.getMaxNumLights();++lightIndex)
				if(lt.getLightState(lightIndex).isEnabled())
					{
					/* Create the light accumulation function: */
					vertexFunctions.push_back('\n');
					vertexFunctions+=lt.createAccumulateLightFunction(lightIndex);
					
					if(firstLight)
						{
						vertexMain+="\
							/* Call the light accumulation functions for all enabled light sources: */\n";
						firstLight=false;
						}
					
					/* Call the light accumulation function from vertex shader's main function: */
					vertexMain+="\
						accumulateLight";
					char liBuffer[12];
					vertexMain.append(Misc::print(lightIndex,liBuffer+11));
					vertexMain+="(vertexEc,normalEc,gl_FrontMaterial.ambient,gl_FrontMaterial.diffuse,gl_FrontMaterial.specular,gl_FrontMaterial.shininess,diffColor,specColor);\n";
					}
			if(!firstLight)
				vertexMain+="\
					\n";
			}
		
		if(waterTable!=0&&dem==0)
			{
			/* Add declarations for water handling: */
			vertexUniforms+="\
				uniform mat4 waterTransform; // Transformation from camera space to water level texture coordinate space\n";
			vertexVaryings+="\
				varying vec2 waterTexCoord; // Texture coordinate for water level texture\n";
			
			/* Add water handling code to vertex shader's main function: */
			vertexMain+="\
				/* Transform the vertex from camera space to water level texture coordinate space: */\n\
				waterTexCoord=(waterTransform*vertexCc).xy;\n\
				\n";
			}
		
		if(numClipPlanes>0)
			{
			/* Find the highest index of any enabled clip planes: */
			int maxCPIndex=-1;
			for(int i=0;i<cpt.getMaxNumClipPlanes();++i)
				if(cpt.getClipPlaneState(i).isEnabled()&&maxCPIndex<i)
					maxCPIndex=i;
			
			/* Add declarations for clipping: */
			vertexVaryings+="\
				varying float gl_ClipDistance[";
			char maxCPIndexBuffer[11];
			vertexVaryings+=Misc::print(maxCPIndex+1,maxCPIndexBuffer+10);
			vertexVaryings+="]; // Vertex clip plane distances for all enabled clipping planes\n";
			
			vertexMain+=cpt.createCalcClipDistances("vertexEc");
			}
		
		/* Finish the vertex shader's main function: */
		vertexMain+="\
				/* Transform vertex from depth image space to clip space: */\n\
				gl_Position=projectionModelviewDepthProjection*vertexDic;\n\
				}\n";
		
		/* Compile the vertex shader: */
		shader.addShader(glCompileVertexShaderFromStrings(7,vertexFunctions.c_str(),"\t\t\n",vertexUniforms.c_str(),"\t\t\n",vertexVaryings.c_str(),"\t\t\n",vertexMain.c_str()));
		
		/*******************************************************************
		Assemble and compile the surface rendering fragment shaders:
		*******************************************************************/
		
		/* Assemble the fragment shader's function declarations: */
		std::string fragmentDeclarations;
		
		/* Assemble the fragment shader's uniform and varying variables: */
		std::string fragmentUniforms;
		std::string fragmentVaryings;
		
		/* Assemble the fragment shader's main function: */
		std::string fragmentMain="\
			void main()\n\
				{\n";
		
		if(dem!=0)
			{
			/* Add declarations for DEM matching: */
			fragmentVaryings+="\
				varying float demDist; // Scaled signed distance from surface to DEM\n";
			
			/* Add DEM matching code to the fragment shader's main function: */
			fragmentMain+="\
				/* Calculate the fragment's color from a double-ramp function: */\n\
				vec4 baseColor;\n\
				if(demDist<0.0)\n\
					baseColor=mix(vec4(1.0,1.0,1.0,1.0),vec4(1.0,0.0,0.0,1.0),min(-demDist,1.0));\n\
				else\n\
					baseColor=mix(vec4(1.0,1.0,1.0,1.0),vec4(0.0,0.0,1.0,1.0),min(demDist,1.0));\n\
				\n";
			}
		else
			{
			if(elevationColorMap!=0)
				{
				/* Add declarations for height mapping: */
				fragmentUniforms+="\
					uniform sampler1D heightColorMapSampler;\n";
				fragmentVaryings+="\
					varying float heightColorMapTexCoord; // Texture coordinate for the height color map\n";
				
				/* Add height mapping code to the fragment shader's main function: */
				fragmentMain+="\
					/* Get the fragment's color from the height color map: */\n\
					vec4 baseColor=texture1D(heightColorMapSampler,heightColorMapTexCoord);\n\
					\n";
				}
			else
				{
				fragmentMain+="\
					/* Set the surface's base color to white: */\n\
					vec4 baseColor=vec4(1.0,1.0,1.0,1.0);\n\
					\n";
				}
			
			if(drawDippingBed)
				{
				/* Add declarations for dipping bed rendering: */
				fragmentUniforms+="\
					uniform float dippingBedThickness; // Thickness of dipping bed in camera-space units\n";
				
				fragmentVaryings+="\
					varying float dippingBedDistance; // Vertex distance to dipping bed plane\n";
				
				/* Add dipping bed code to fragment shader's main function: */
				fragmentMain+="\
					/* Check fragment's dipping plane distance against dipping bed thickness: */\n\
					float w=fwidth(dippingBedDistance)*1.0;\n\
					if(dippingBedDistance<0.0)\n\
						baseColor=mix(baseColor,vec4(1.0,0.0,0.0,1.0),smoothstep(-dippingBedThickness*0.5-w,-dippingBedThickness*0.5+w,dippingBedDistance));\n\
					else\n\
						baseColor=mix(vec4(1.0,0.0,0.0,1.0),baseColor,smoothstep(dippingBedThickness*0.5-w,dippingBedThickness*0.5+w,dippingBedDistance));\n\
					\n";
				}
			}
		
		if(drawContourLines)
			{
			/* Declare the contour line function: */
			fragmentDeclarations+="\
				void addContourLines(in vec2,inout vec4);\n";
			
			/* Compile the contour line shader: */
			shader.addShader(compileFragmentShader("SurfaceAddContourLines"));
			
			/* Call contour line function from fragment shader's main function: */
			fragmentMain+="\
				/* Modulate the base color by contour line color: */\n\
				addContourLines(gl_FragCoord.xy,baseColor);\n\
				\n";
			}
		
		if(illuminate)
			{
			/* Declare the illumination function: */
			fragmentDeclarations+="\
				void illuminate(inout vec4);\n";
			
			/* Compile the illumination shader: */
			shader.addShader(compileFragmentShader("SurfaceIlluminate"));
			
			/* Call illumination function from fragment shader's main function: */
			fragmentMain+="\
				/* Apply illumination to the base color: */\n\
				illuminate(baseColor);\n\
				\n";
			}
		
		if(waterTable!=0&&dem==0)
			{
			/* Declare the water handling functions: */
			fragmentDeclarations+="\
				void addWaterColor(in vec2,inout vec4);\n\
				void addWaterColorAdvected(inout vec4);\n";
			
			/* Compile the water handling shader: */
			shader.addShader(compileFragmentShader("SurfaceAddWaterColor"));
			
			/* Call water coloring function from fragment shader's main function: */
			if(advectWaterTexture)
				{
				fragmentMain+="\
					/* Modulate the base color with water color: */\n\
					addWaterColorAdvected(baseColor);\n\
					\n";
				}
			else
				{
				fragmentMain+="\
					/* Modulate the base color with water color: */\n\
					addWaterColor(gl_FragCoord.xy,baseColor);\n\
					\n";
				}
			}
		
		/* Finish the fragment shader's main function: */
		fragmentMain+="\
			/* Assign the final color to the fragment: */\n\
			gl_FragColor=baseColor;\n\
			}\n";
		
		/* Compile the fragment shader: */
		shader.addShader(glCompileFragmentShaderFromStrings(7,fragmentDeclarations.c_str(),"\t\t\n",fragmentUniforms.c_str(),"\t\t\n",fragmentVaryings.c_str(),"\t\t\n",fragmentMain.c_str()));
		
		/* Link the shader program: */
		shader.link();
		
		/*******************************************************************
		Query the shader program's uniform locations:
		*******************************************************************/
		
		/* Override the shader's number of uniform variables to avoid problems if variables aren't used in a specific external shader: */
		shader.setNumUniforms(16);
		
		/* Query common uniform variables: */
		shader.setUniformLocation("depthSampler");
		shader.setUniformLocation("depthProjection");
		if(dem!=0)
			{
			/* Query DEM matching uniform variables: */
			shader.setUniformLocation("demTransform");
			shader.setUniformLocation("demSampler");
			shader.setUniformLocation("demDistScale");
			}
		else if(elevationColorMap!=0)
			{
			/* Query height color mapping uniform variables: */
			shader.setUniformLocation("heightColorMapPlaneEq");
			shader.setUniformLocation("heightColorMapSampler");
			}
		if(drawContourLines)
			{
			shader.setUniformLocation("pixelCornerElevationSampler");
			shader.setUniformLocation("contourLineFactor");
			}
		if(drawDippingBed)
			{
			if(dippingBedFolded)
				shader.setUniformLocation("dbc");
			else
				shader.setUniformLocation("dippingBedPlaneEq");
			shader.setUniformLocation("dippingBedThickness");
			}
		if(illuminate||numClipPlanes>0)
			{
			/* Query illumination and/or clipping uniform variables: */
			shader.setUniformLocation("modelview");
			}
		if(illuminate)
			{
			/* Query illumination uniform variables: */
			shader.setUniformLocation("tangentModelviewDepthProjection");
			}
		if(waterTable!=0&&dem==0)
			{
			/* Query water handling uniform variables: */
			shader.setUniformLocation("waterTransform");
			shader.setUniformLocation("bathymetrySampler");
			shader.setUniformLocation("snowSampler");
			shader.setUniformLocation("quantitySampler");
			shader.setUniformLocation("waterCellSize");
			shader.setUniformLocation("waterOpacity");
			shader.setUniformLocation("waterAnimationTime");
			}
		shader.setUniformLocation("projectionModelviewDepthProjection");
		}
	catch(...)
		{
		/* Clean up and re-throw the exception: */
		shader.clearLinkList();
		throw;
		}
	}

void SurfaceRenderer::renderPixelCornerElevations(const SurfaceRenderer::Rect& viewport,const PTransform& projectionModelview,GLContextData& contextData,TextureTracker& textureTracker,SurfaceRenderer::DataItem* dataItem) const
	{
	/* Save the currently-bound frame buffer and clear color: */
	GLint currentFrameBuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&currentFrameBuffer);
	GLfloat currentClearColor[4];
	glGetFloatv(GL_COLOR_CLEAR_VALUE,currentClearColor);
	
	/* Check if the contour line rendering frame buffer needs to be created: */
	if(dataItem->contourLineFramebufferObject==0)
		{
		/* Initialize the frame buffer: */
		dataItem->contourLineFramebufferSize=Size(0,0);
		glGenFramebuffersEXT(1,&dataItem->contourLineFramebufferObject);
		glGenRenderbuffersEXT(1,&dataItem->contourLineDepthBufferObject);
		glGenTextures(1,&dataItem->contourLineColorTextureObject);
		}
	
	/* Bind the contour line rendering frame buffer object: */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->contourLineFramebufferObject);
	
	/* Check if the contour line frame buffer needs to be resized: */
	if(dataItem->contourLineFramebufferSize[0]!=viewport.size[0]+1U||dataItem->contourLineFramebufferSize[1]!=viewport.size[1]+1U)
		{
		/* Remember if the render buffers must still be attached to the frame buffer: */
		bool mustAttachBuffers=dataItem->contourLineFramebufferSize==Size(0,0);
		
		/* Update the frame buffer size: */
		for(int i=0;i<2;++i)
			dataItem->contourLineFramebufferSize[i]=viewport.size[i]+1U;
		
		/* Resize the topographic contour line rendering depth buffer: */
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,dataItem->contourLineDepthBufferObject);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,GL_DEPTH_COMPONENT,dataItem->contourLineFramebufferSize);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,0);
		
		/* Resize the topographic contour line rendering color texture: */
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->contourLineColorTextureObject);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_R32F,dataItem->contourLineFramebufferSize,0,GL_LUMINANCE,GL_UNSIGNED_BYTE,0);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
		
		if(mustAttachBuffers)
			{
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,dataItem->contourLineDepthBufferObject);
			glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_RECTANGLE_ARB,dataItem->contourLineColorTextureObject,0);
			glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
			glReadBuffer(GL_NONE);
			}
		}
	
	/* Extend the viewport to render the corners of all pixels: */
	glViewport(dataItem->contourLineFramebufferSize);
	glClearColor(0.0f,0.0f,0.0f,1.0f);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	
	/* Shift the projection matrix by half a pixel to render the corners of the final pixels: */
	PTransform shiftedProjectionModelview=projectionModelview;
	PTransform::Matrix& spmm=shiftedProjectionModelview.getMatrix();
	Scalar xs=Scalar(viewport.size[0])/Scalar(viewport.size[0]+1);
	Scalar ys=Scalar(viewport.size[1])/Scalar(viewport.size[1]+1);
	for(int j=0;j<4;++j)
		{
		spmm(0,j)*=xs;
		spmm(1,j)*=ys;
		}
	
	/* Render the surface elevation into the half-pixel offset frame buffer: */
	depthImageRenderer->renderElevation(shiftedProjectionModelview,contextData,textureTracker);
	
	/* Restore the original viewport: */
	glViewport(viewport);
	
	/* Restore the original clear color and frame buffer binding: */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,currentFrameBuffer);
	glClearColor(currentClearColor[0],currentClearColor[1],currentClearColor[2],currentClearColor[3]);
	}

SurfaceRenderer::SurfaceRenderer(const DepthImageRenderer* sDepthImageRenderer)
	:depthImageRenderer(sDepthImageRenderer),
	 depthImageSize(depthImageRenderer->getDepthImageSize()),
	 drawContourLines(true),contourLineFactor(1.0f),
	 elevationColorMap(0),
	 drawDippingBed(false),dippingBedFolded(false),
	 dippingBedPlane(Plane::Vector(0,0,1),0.0f),dippingBedThickness(1),
	 dem(0),demDistScale(1.0f),
	 illuminate(false),
	 waterTable(0),advectWaterTexture(false),waterOpacity(2.0f),
	 surfaceSettingsVersion(1),
	 animationTime(0.0)
	{
	/* Check if the depth projection matrix retains right-handedness: */
	const PTransform& depthProjection=depthImageRenderer->getDepthProjection();
	Point p1=depthProjection.transform(Point(0,0,0));
	Point p2=depthProjection.transform(Point(1,0,0));
	Point p3=depthProjection.transform(Point(0,1,0));
	Point p4=depthProjection.transform(Point(0,0,1));
	bool depthProjectionInverts=((p2-p1)^(p3-p1))*(p4-p1)<Scalar(0);
	
	/* Calculate the transposed tangent plane depth projection: */
	tangentDepthProjection=Geometry::invert(depthProjection);
	if(depthProjectionInverts)
		tangentDepthProjection*=PTransform::scale(PTransform::Scale(-1,-1,-1));
	
	/* Monitor the external shader source files: */
	fileMonitor.addPath((std::string(CONFIG_SHADERDIR)+std::string("/SurfaceAddContourLines.fs")).c_str(),IO::FileMonitor::Modified,*Threads::createFunctionCall(this,&SurfaceRenderer::shaderSourceFileChanged));
	fileMonitor.addPath((std::string(CONFIG_SHADERDIR)+std::string("/SurfaceIlluminate.fs")).c_str(),IO::FileMonitor::Modified,*Threads::createFunctionCall(this,&SurfaceRenderer::shaderSourceFileChanged));
	fileMonitor.addPath((std::string(CONFIG_SHADERDIR)+std::string("/SurfaceAddWaterColor.fs")).c_str(),IO::FileMonitor::Modified,*Threads::createFunctionCall(this,&SurfaceRenderer::shaderSourceFileChanged));
	fileMonitor.startPolling();
	}

void SurfaceRenderer::initContext(GLContextData& contextData) const
	{
	/* Initialize required OpenGL extensions: */
	GLARBFragmentShader::initExtension();
	GLARBTextureRectangle::initExtension();
	GLARBTextureRg::initExtension();
	GLARBVertexShader::initExtension();
	GLEXTFramebufferObject::initExtension();
	Shader::initExtensions();
	TextureTracker::initExtensions();
	
	/* Create a data item and add it to the context: */
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);
	
	/* Create the height map render shader: */
	updateSinglePassSurfaceShader(*contextData.getLightTracker(),*contextData.getClipPlaneTracker(),dataItem);
	dataItem->surfaceSettingsVersion=surfaceSettingsVersion;
	dataItem->lightTrackerVersion=contextData.getLightTracker()->getVersion();
	dataItem->clipPlaneTrackerVersion=contextData.getClipPlaneTracker()->getVersion();
	
	/* Create the global ambient height map render shader: */
	dataItem->globalAmbientHeightMapShader.addShader(compileVertexShader("SurfaceGlobalAmbientHeightMapShader"));
	dataItem->globalAmbientHeightMapShader.addShader(compileFragmentShader("SurfaceGlobalAmbientHeightMapShader"));
	dataItem->globalAmbientHeightMapShader.link();
	dataItem->globalAmbientHeightMapShader.setUniformLocation("depthSampler");
	dataItem->globalAmbientHeightMapShader.setUniformLocation("depthProjection");
	dataItem->globalAmbientHeightMapShader.setUniformLocation("basePlane");
	dataItem->globalAmbientHeightMapShader.setUniformLocation("pixelCornerElevationSampler");
	dataItem->globalAmbientHeightMapShader.setUniformLocation("contourLineFactor");
	dataItem->globalAmbientHeightMapShader.setUniformLocation("heightColorMapSampler");
	dataItem->globalAmbientHeightMapShader.setUniformLocation("heightColorMapTransformation");
	dataItem->globalAmbientHeightMapShader.setUniformLocation("waterLevelSampler");
	dataItem->globalAmbientHeightMapShader.setUniformLocation("waterLevelTextureTransformation");
	dataItem->globalAmbientHeightMapShader.setUniformLocation("waterOpacity");
	
	/* Create the shadowed illuminated height map render shader: */
	dataItem->shadowedIlluminatedHeightMapShader.addShader(compileVertexShader("SurfaceShadowedIlluminatedHeightMapShader"));
	dataItem->shadowedIlluminatedHeightMapShader.addShader(compileFragmentShader("SurfaceShadowedIlluminatedHeightMapShader"));
	dataItem->shadowedIlluminatedHeightMapShader.link();
	dataItem->shadowedIlluminatedHeightMapShader.setUniformLocation("depthSampler");
	dataItem->shadowedIlluminatedHeightMapShader.setUniformLocation("depthProjection");
	dataItem->shadowedIlluminatedHeightMapShader.setUniformLocation("tangentDepthProjection");
	dataItem->shadowedIlluminatedHeightMapShader.setUniformLocation("basePlane");
	dataItem->shadowedIlluminatedHeightMapShader.setUniformLocation("pixelCornerElevationSampler");
	dataItem->shadowedIlluminatedHeightMapShader.setUniformLocation("contourLineFactor");
	dataItem->shadowedIlluminatedHeightMapShader.setUniformLocation("heightColorMapSampler");
	dataItem->shadowedIlluminatedHeightMapShader.setUniformLocation("heightColorMapTransformation");
	dataItem->shadowedIlluminatedHeightMapShader.setUniformLocation("waterLevelSampler");
	dataItem->shadowedIlluminatedHeightMapShader.setUniformLocation("waterLevelTextureTransformation");
	dataItem->shadowedIlluminatedHeightMapShader.setUniformLocation("waterOpacity");
	dataItem->shadowedIlluminatedHeightMapShader.setUniformLocation("shadowTextureSampler");
	dataItem->shadowedIlluminatedHeightMapShader.setUniformLocation("shadowProjection");
	}

void SurfaceRenderer::setDrawContourLines(bool newDrawContourLines)
	{
	drawContourLines=newDrawContourLines;
	++surfaceSettingsVersion;
	}

void SurfaceRenderer::setContourLineDistance(GLfloat newContourLineDistance)
	{
	/* Set the new contour line factor: */
	contourLineFactor=1.0f/newContourLineDistance;
	}

void SurfaceRenderer::setElevationColorMap(ElevationColorMap* newElevationColorMap)
	{
	/* Check if setting this elevation color map invalidates the shader: */
	if(dem==0&&((newElevationColorMap!=0&&elevationColorMap==0)||(newElevationColorMap==0&&elevationColorMap!=0)))
		++surfaceSettingsVersion;
	
	/* Set the elevation color map: */
	elevationColorMap=newElevationColorMap;
	}

void SurfaceRenderer::setDrawDippingBed(bool newDrawDippingBed)
	{
	drawDippingBed=newDrawDippingBed;
	++surfaceSettingsVersion;
	}

void SurfaceRenderer::setDippingBedPlane(const SurfaceRenderer::Plane& newDippingBedPlane)
	{
	/* Set the dipping bed mode to planar: */
	if(dippingBedFolded)
		{
		dippingBedFolded=false;
		++surfaceSettingsVersion;
		}
	
	/* Set the dipping bed's plane equation: */
	dippingBedPlane=newDippingBedPlane;
	}

void SurfaceRenderer::setDippingBedCoeffs(const GLfloat newDippingBedCoeffs[5])
	{
	/* Set the dipping bed mode to folded: */
	if(!dippingBedFolded)
		{
		dippingBedFolded=true;
		++surfaceSettingsVersion;
		}
	
	/* Set the dipping bed's coefficients: */
	for(int i=0;i<5;++i)
		dippingBedCoeffs[i]=newDippingBedCoeffs[i];
	}

void SurfaceRenderer::setDippingBedThickness(GLfloat newDippingBedThickness)
	{
	dippingBedThickness=newDippingBedThickness;
	}

void SurfaceRenderer::setDem(DEM* newDem)
	{
	/* Check if setting this DEM invalidates the shader: */
	if((newDem!=0&&dem==0)||(newDem==0&&dem!=0))
		++surfaceSettingsVersion;
	
	/* Set the new DEM: */
	dem=newDem;
	}

void SurfaceRenderer::setDemDistScale(GLfloat newDemDistScale)
	{
	demDistScale=newDemDistScale;
	}

void SurfaceRenderer::setIlluminate(bool newIlluminate)
	{
	illuminate=newIlluminate;
	++surfaceSettingsVersion;
	}

void SurfaceRenderer::setWaterTable(WaterTable2* newWaterTable)
	{
	waterTable=newWaterTable;
	++surfaceSettingsVersion;
	}

void SurfaceRenderer::setAdvectWaterTexture(bool newAdvectWaterTexture)
	{
	advectWaterTexture=false; // newAdvectWaterTexture;
	++surfaceSettingsVersion;
	}

void SurfaceRenderer::setWaterOpacity(GLfloat newWaterOpacity)
	{
	/* Set the new opacity factor: */
	waterOpacity=newWaterOpacity;
	}

void SurfaceRenderer::setAnimationTime(double newAnimationTime)
	{
	/* Set the new animation time: */
	animationTime=newAnimationTime;
	
	/* Poll the file monitor: */
	fileMonitor.processEvents();
	}

void SurfaceRenderer::renderSinglePass(const SurfaceRenderer::Rect& viewport,const PTransform& projection,const OGTransform& modelview,GLContextData& contextData,TextureTracker& textureTracker) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Calculate the required matrices: */
	PTransform projectionModelview=projection;
	projectionModelview*=modelview;
	
	/* Check if contour line rendering is enabled: */
	if(drawContourLines)
		{
		/* Run the first rendering pass to create a half-pixel offset texture of surface elevations: */
		renderPixelCornerElevations(viewport,projectionModelview,contextData,textureTracker,dataItem);
		}
	else if(dataItem->contourLineFramebufferObject!=0)
		{
		/* Delete the contour line rendering frame buffer: */
		glDeleteFramebuffersEXT(1,&dataItem->contourLineFramebufferObject);
		dataItem->contourLineFramebufferObject=0;
		glDeleteRenderbuffersEXT(1,&dataItem->contourLineDepthBufferObject);
		dataItem->contourLineDepthBufferObject=0;
		glDeleteTextures(1,&dataItem->contourLineColorTextureObject);
		dataItem->contourLineColorTextureObject=0;
		}
	
	/* Check if the single-pass surface shader is outdated: */
	if(dataItem->surfaceSettingsVersion!=surfaceSettingsVersion||(illuminate&&dataItem->lightTrackerVersion!=contextData.getLightTracker()->getVersion())||dataItem->clipPlaneTrackerVersion!=contextData.getClipPlaneTracker()->getVersion())
		{
		/* Rebuild the shader: */
		try
			{
			updateSinglePassSurfaceShader(*contextData.getLightTracker(),*contextData.getClipPlaneTracker(),dataItem);
			dataItem->clipping=contextData.getClipPlaneTracker()->getNumEnabledClipPlanes()>0;
			}
		catch(const std::runtime_error& err)
			{
			Misc::formattedUserError("SurfaceRenderer::renderSinglePass: Caught exception %s while rebuilding surface shader",err.what());
			}
		
		/* Mark the shader as up-to-date: */
		dataItem->surfaceSettingsVersion=surfaceSettingsVersion;
		dataItem->lightTrackerVersion=contextData.getLightTracker()->getVersion();
		dataItem->clipPlaneTrackerVersion=contextData.getClipPlaneTracker()->getVersion();
		}
	
	/* Install the single-pass surface shader: */
	dataItem->heightMapShader.use();
	textureTracker.reset();
	
	/* Bind the current depth image texture: */
	dataItem->heightMapShader.uploadUniform(depthImageRenderer->bindDepthTexture(contextData,textureTracker));
	
	/* Upload the depth projection matrix: */
	depthImageRenderer->uploadDepthProjection(dataItem->heightMapShader);
	
	if(dem!=0)
		{
		/* Upload the DEM transformation: */
		dem->uploadDemTransform(dataItem->heightMapShader);
		
		/* Bind the DEM texture: */
		dataItem->heightMapShader.uploadUniform(dem->bindTexture(contextData,textureTracker));
		
		/* Upload the DEM distance scale factor: */
		dataItem->heightMapShader.uploadUniform(GLfloat(Scalar(1)/(demDistScale*dem->getVerticalScale())));
		}
	else if(elevationColorMap!=0)
		{
		/* Upload the texture mapping plane equation: */
		elevationColorMap->uploadTexturePlane(dataItem->heightMapShader);
		
		/* Bind the height color map texture: */
		dataItem->heightMapShader.uploadUniform(elevationColorMap->bindTexture(contextData,textureTracker));
		}
	
	if(drawContourLines)
		{
		/* Bind the pixel corner elevation texture: */
		dataItem->heightMapShader.uploadUniform(textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->contourLineColorTextureObject));
		
		/* Upload the contour line distance factor: */
		dataItem->heightMapShader.uploadUniform(contourLineFactor);
		}
	
	if(drawDippingBed)
		{
		if(dippingBedFolded)
			{
			/* Upload the dipping bed coefficients: */
			dataItem->heightMapShader.uploadUniform1v(5,dippingBedCoeffs);
			}
		else
			{
			/* Upload the dipping bed plane equation: */
			GLfloat planeEq[4];
			for(int i=0;i<3;++i)
				planeEq[i]=dippingBedPlane.getNormal()[i];
			planeEq[3]=-dippingBedPlane.getOffset();
			dataItem->heightMapShader.uploadUniform4v(1,planeEq);
			}
		
		/* Upload the dipping bed thickness: */
		dataItem->heightMapShader.uploadUniform(dippingBedThickness);
		}
	
	if(illuminate||dataItem->clipping)
		{
		/* Upload the modelview matrix: */
		dataItem->heightMapShader.uploadUniform(modelview);
		}
	
	if(illuminate)
		{
		/* Calculate and upload the transposed tangent-plane modelview depth projection matrix: */
		PTransform tangentModelviewDepthProjection=tangentDepthProjection;
		tangentModelviewDepthProjection*=Geometry::invert(modelview);
		const Scalar* tmdpPtr=tangentModelviewDepthProjection.getMatrix().getEntries();
		GLfloat matrix[16];
		GLfloat* mPtr=matrix;
		for(int i=0;i<16;++i,++tmdpPtr,++mPtr)
				*mPtr=GLfloat(*tmdpPtr);
		dataItem->heightMapShader.uploadUniformMatrix4(1,GL_FALSE,matrix);
		}
	
	if(waterTable!=0&&dem==0)
		{
		/* Upload the water table texture coordinate matrix: */
		waterTable->uploadWaterTextureTransform(dataItem->heightMapShader);
		
		/* Bind the bathymetry texture: */
		dataItem->heightMapShader.uploadUniform(waterTable->bindBathymetryTexture(contextData,textureTracker,true));
		
		/* Bind the snow height texture: */
		dataItem->heightMapShader.uploadUniform(waterTable->bindSnowTexture(contextData,textureTracker,true));
		
		/* Bind the conserved quantities texture: */
		dataItem->heightMapShader.uploadUniform(waterTable->bindQuantityTexture(contextData,textureTracker,true));
		
		/* Upload the water grid cell size for normal vector calculation: */
		dataItem->heightMapShader.uploadUniform2v(1,waterTable->getCellSize());
		
		/* Upload the water opacity factor: */
		dataItem->heightMapShader.uploadUniform(waterOpacity);
		
		/* Upload the water animation time: */
		dataItem->heightMapShader.uploadUniform(GLfloat(animationTime));
		}
	
	/* Upload the combined projection, modelview, and depth unprojection matrix: */
	PTransform projectionModelviewDepthProjection=projectionModelview;
	projectionModelviewDepthProjection*=depthImageRenderer->getDepthProjection();
	dataItem->heightMapShader.uploadUniform(projectionModelviewDepthProjection);
	
	/* Draw the surface template: */
	depthImageRenderer->renderSurfaceTemplate(contextData);
	}

#if 0

void SurfaceRenderer::renderGlobalAmbientHeightMap(GLuint heightColorMapTexture,GLContextData& contextData) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Check if contour line rendering is enabled: */
	if(drawContourLines)
		{
		/* Run the first rendering pass to create a half-pixel offset texture of surface elevations: */
		glPrepareContourLines(contextData);
		}
	else if(dataItem->contourLineFramebufferObject!=0)
		{
		/* Delete the contour line rendering frame buffer: */
		glDeleteFramebuffersEXT(1,&dataItem->contourLineFramebufferObject);
		dataItem->contourLineFramebufferObject=0;
		glDeleteRenderbuffersEXT(1,&dataItem->contourLineDepthBufferObject);
		dataItem->contourLineDepthBufferObject=0;
		glDeleteTextures(1,&dataItem->contourLineColorTextureObject);
		dataItem->contourLineColorTextureObject=0;
		}
	
	/* Bind the global ambient height map shader: */
	dataItem->globalAmbientHeightMapShader.use();
	
	/* Bind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	
	/* Set up the depth image texture: */
	if(usePreboundDepthTexture)
		dataItem->globalAmbientHeightMapShader.uploadUniform(0,0);
	else
		{
		dataItem->globalAmbientHeightMapShader.bindTexture(0,GL_TEXTURE_RECTANGLE_ARB,dataItem->depthTexture,0);
		
		/* Check if the texture is outdated: */
		if(dataItem->depthTextureVersion!=depthImageVersion)
			{
			/* Upload the new depth texture: */
			glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,0,0,size[0],size[1],GL_LUMINANCE,GL_FLOAT,depthImage.getBuffer());
			
			/* Mark the depth texture as current: */
			dataItem->depthTextureVersion=depthImageVersion;
			}
		}
	
	/* Upload the depth projection matrix: */
	glUniformMatrix4fvARB(dataItem->globalAmbientHeightMapShader.getUniformLocation(1),1,GL_FALSE,depthProjectionMatrix);
	
	/* Upload the base plane equation: */
	dataItem->globalAmbientHeightMapShader.uploadUniform4v(2,1,basePlaneEq);
	
	/* Bind the pixel corner elevation texture: */
	dataItem->globalAmbientHeightMapShader.bindTexture(1,GL_TEXTURE_RECTANGLE_ARB,dataItem->contourLineColorTextureObject,3);
	
	/* Upload the contour line distance factor: */
	dataItem->globalAmbientHeightMapShader.uploadUniform(4,contourLineFactor);
	
	/* Bind the height color map texture: */
	dataItem->globalAmbientHeightMapShader.bindTexture(2,GL_TEXTURE_1D,heightColorMapTexture,5);
	
	/* Upload the height color map texture coordinate transformation: */
	dataItem->globalAmbientHeightMapShader.uploadUniform(6,heightMapScale,heightMapOffset);
	
	if(waterTable!=0)
		{
		/* Bind the water level texture: */
		glActiveTextureARB(GL_TEXTURE3_ARB);
		//waterTable->bindWaterLevelTexture(contextData);
		dataItem->globalAmbientHeightMapShader.uploadUniform(7,3);
		
		/* Upload the water table texture coordinate matrix: */
		glUniformMatrix4fvARB(dataItem->globalAmbientHeightMapShader.getUniformLocation(8),1,GL_FALSE,waterTable->getWaterTextureMatrix());
		
		/* Upload the water opacity factor: */
		dataItem->globalAmbientHeightMapShader.uploadUniform(9,waterOpacity);
		}
	
	/* Draw the surface: */
	typedef GLGeometry::Vertex<void,0,void,0,void,float,3> Vertex;
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	for(unsigned int y=1;y<size[1];++y)
		glDrawElements(GL_QUAD_STRIP,size[0]*2,GL_UNSIGNED_INT,static_cast<const GLuint*>(0)+(y-1)*size[0]*2);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	
	/* Unbind all textures and buffers: */
	if(waterTable!=0)
		{
		glActiveTextureARB(GL_TEXTURE3_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
		}
	glActiveTextureARB(GL_TEXTURE2_ARB);
	glBindTexture(GL_TEXTURE_1D,0);
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	if(!usePreboundDepthTexture)
		{
		glActiveTextureARB(GL_TEXTURE0_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
		}
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	
	/* Unbind the global ambient height map shader: */
	glUseProgramObjectARB(0);
	}

void SurfaceRenderer::renderShadowedIlluminatedHeightMap(GLuint heightColorMapTexture,GLuint shadowTexture,const PTransform& shadowProjection,GLContextData& contextData) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Bind the shadowed illuminated height map shader: */
	dataItem->shadowedIlluminatedHeightMapShader.use();
	
	/* Bind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	
	/* Set up the depth image texture: */
	if(usePreboundDepthTexture)
		dataItem->shadowedIlluminatedHeightMapShader.uploadUniform(0,0);
	else
		{
		dataItem->shadowedIlluminatedHeightMapShader.bindTexture(0,GL_TEXTURE_RECTANGLE_ARB,dataItem->depthTexture,0);
		
		/* Check if the texture is outdated: */
		if(dataItem->depthTextureVersion!=depthImageVersion)
			{
			/* Upload the new depth texture: */
			glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,0,0,size[0],size[1],GL_LUMINANCE,GL_FLOAT,depthImage.getBuffer());
			
			/* Mark the depth texture as current: */
			dataItem->depthTextureVersion=depthImageVersion;
			}
		}
	
	/* Upload the depth projection matrix: */
	glUniformMatrix4fvARB(dataItem->shadowedIlluminatedHeightMapShader.getUniformLocation(1),1,GL_FALSE,depthProjectionMatrix);
	
	/* Upload the tangent-plane depth projection matrix: */
	glUniformMatrix4fvARB(dataItem->shadowedIlluminatedHeightMapShader.getUniformLocation(2),1,GL_FALSE,tangentDepthProjectionMatrix);
	
	/* Upload the base plane equation: */
	dataItem->shadowedIlluminatedHeightMapShader.uploadUniform4v(3,1,basePlaneEq);
	
	/* Bind the pixel corner elevation texture: */
	dataItem->shadowedIlluminatedHeightMapShader.bindTexture(1,GL_TEXTURE_RECTANGLE_ARB,dataItem->contourLineColorTextureObject,4);
	
	/* Upload the contour line distance factor: */
	dataItem->shadowedIlluminatedHeightMapShader.uploadUniform(5,contourLineFactor);
	
	/* Bind the height color map texture: */
	dataItem->shadowedIlluminatedHeightMapShader.bindTexture(2,GL_TEXTURE_1D,heightColorMapTexture,6);
	
	/* Upload the height color map texture coordinate transformation: */
	dataItem->shadowedIlluminatedHeightMapShader.uploadUniform(7,heightMapScale,heightMapOffset);
	
	if(waterTable!=0)
		{
		/* Bind the water level texture: */
		glActiveTextureARB(GL_TEXTURE3_ARB);
		//waterTable->bindWaterLevelTexture(contextData);
		dataItem->shadowedIlluminatedHeightMapShader.uploadUniform(8,3);
		
		/* Upload the water table texture coordinate matrix: */
		glUniformMatrix4fvARB(dataItem->shadowedIlluminatedHeightMapShader.getUniformLocation(9),1,GL_FALSE,waterTable->getWaterTextureMatrix());
		
		/* Upload the water opacity factor: */
		dataItem->shadowedIlluminatedHeightMapShader.uploadUniform(10,waterOpacity);
		}
	
	/* Bind the shadow texture: */
	dataItem->shadowedIlluminatedHeightMapShader.bindTexture(4,GL_TEXTURE_2D,shadowTexture,11);
	
	/* Upload the combined shadow viewport, shadow projection and modelview, and depth projection matrix: */
	PTransform spdp(1.0);
	spdp.getMatrix()(0,0)=0.5;
	spdp.getMatrix()(0,3)=0.5;
	spdp.getMatrix()(1,1)=0.5;
	spdp.getMatrix()(1,3)=0.5;
	spdp.getMatrix()(2,2)=0.5;
	spdp.getMatrix()(2,3)=0.5;
	spdp*=shadowProjection;
	spdp*=depthProjection;
	dataItem->shadowedIlluminatedHeightMapShader.uploadUniform(12,spdp);
	
	/* Draw the surface: */
	typedef GLGeometry::Vertex<void,0,void,0,void,float,3> Vertex;
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	for(unsigned int y=1;y<size[1];++y)
		glDrawElements(GL_QUAD_STRIP,size[0]*2,GL_UNSIGNED_INT,static_cast<const GLuint*>(0)+(y-1)*size[0]*2);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	
	/* Unbind all textures and buffers: */
	if(waterTable!=0)
		{
		glActiveTextureARB(GL_TEXTURE3_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
		}
	glActiveTextureARB(GL_TEXTURE2_ARB);
	glBindTexture(GL_TEXTURE_1D,0);
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	if(!usePreboundDepthTexture)
		{
		glActiveTextureARB(GL_TEXTURE0_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
		}
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	
	/* Unbind the shadowed illuminated height map shader: */
	glUseProgramObjectARB(0);
	}

#endif
