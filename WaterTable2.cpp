/***********************************************************************
WaterTable2 - Class to simulate water flowing over a surface using
improved water flow simulation based on Saint-Venant system of partial
differenctial equations.
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

#include "WaterTable2.h"

#include <stdio.h>
#include <string>
#include <Math/Math.h>
#include <Geometry/AffineCombiner.h>
#include <Geometry/Vector.h>
#include <GL/gl.h>
#include <GL/GLMiscTemplates.h>
#include <GL/Extensions/GLARBDrawBuffers.h>
#include <GL/Extensions/GLARBFragmentShader.h>
#include <GL/Extensions/GLARBTextureFloat.h>
#include <GL/Extensions/GLARBTextureRectangle.h>
#include <GL/Extensions/GLARBTextureRg.h>
#include <GL/Extensions/GLARBVertexShader.h>
#include <GL/Extensions/GLEXTFramebufferObject.h>
#include <GL/GLContextData.h>
#include <GL/GLTransformationWrappers.h>

#include "TextureTracker.h"
#include "DepthImageRenderer.h"
#include "PropertyGridCreator.h"
#include "ShaderHelper.h"

// DEBUGGING
#include <iostream>

namespace {

/****************
Helper functions:
****************/

GLfloat* makeBuffer(unsigned int width,unsigned int height,int numComponents,float c0 =0.0f,float c1 =0.0f,float c2 =0.0f,float c3 =0.0f)
	{
	/* Create the buffer: */
	GLfloat* buffer=new GLfloat[height*width*numComponents];
	
	/* Copy the buffer fill components: */
	GLfloat fill[4];
	fill[0]=c0;
	fill[1]=c1;
	fill[2]=c2;
	fill[3]=c3;
	
	/* Fill the buffer: */
	GLfloat* bPtr=buffer;
	for(unsigned int y=0;y<height;++y)
		for(unsigned int x=0;x<width;++x,bPtr+=numComponents)
			for(int i=0;i<numComponents;++i)
				bPtr[i]=fill[i];
	
	return buffer;
	}

void sampleNearest(void)
	{
	/* Set up for nearest-neighbor sampling: */
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP);
	}

void sampleLinear(void)
	{
	/* Set up for linear sampling: */
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	}

}

/*********************************************
Methods of class WaterTable2::BufferedTexture:
*********************************************/

template <int numSlotsParam>
inline
WaterTable2::BufferedTexture<numSlotsParam>::BufferedTexture(
	GLenum sTextureTarget)
	:textureTarget(sTextureTarget),
	 current(0)
	{
	/* Generate the buffer slots' OpenGL texture objects: */
	glGenTextures(numSlots,textureObjects);
	}

template <int numSlotsParam>
inline
WaterTable2::BufferedTexture<numSlotsParam>::~BufferedTexture(
	void)
	{
	/* Destroy the OpenGL texture objects: */
	glDeleteTextures(numSlots,textureObjects);
	}

template <int numSlotsParam>
inline
void
WaterTable2::BufferedTexture<numSlotsParam>::init(
	unsigned int width,
	unsigned int height,
	int numComponents,
	GLenum internalFormat,
	GLenum externalFormat,
	float c0,
	float c1,
	float c2,
	float c3)
	{
	/* Create a temporary buffer to upload initial texture data: */
	GLfloat* buffer=makeBuffer(width,height,numComponents,c0,c1,c2,c3);
	
	/* Create and intialize the buffer slots' textures: */
	for(int i=0;i<numSlots;++i)
		{
		glBindTexture(textureTarget,textureObjects[i]);
		
		/* Set up the texture for nearest-neighbor sampling: */
		sampleNearest();
		linears[i]=false;
		
		/* Upload the initial texture data: */
		glTexImage2D(textureTarget,0,internalFormat,width,height,0,externalFormat,GL_FLOAT,buffer);
		}
	
	/* Release the temporary buffer: */
	delete[] buffer;
	}

template <int numSlotsParam>
inline
void
WaterTable2::BufferedTexture<numSlotsParam>::setSamplingMode(
	int slot,
	bool linear)
	{
	/* Check if the sampling mode is different from what's requested: */
	if(linears[slot]!=linear)
		{
		/* Set the requested sampling mode: */
		if(linear)
			sampleLinear();
		else
			sampleNearest();
		
		/* Remember the new sampling mode: */
		linears[slot]=linear;
		}
	}

template <int numSlotsParam>
inline
void
WaterTable2::BufferedTexture<numSlotsParam>::bind(TextureTracker& textureTracker,
	Shader& shader,
	int slot,
	bool linear)
	{
	/* Bind the texture slot of the given index to the given shader: */
	shader.uploadUniform(textureTracker.bindTexture(textureTarget,textureObjects[slot]));
	
	/* Set the requested sampling mode: */
	setSamplingMode(slot,linear);
	}

template <int numSlotsParam>
inline
GLint
WaterTable2::BufferedTexture<numSlotsParam>::bindCurrent(TextureTracker& textureTracker,
	bool linear)
	{
	/* Bind the current texture slot to the given texture tracker: */
	GLint textureUnit=textureTracker.bindTexture(textureTarget,textureObjects[current]);
	
	/* Set the requested sampling mode: */
	setSamplingMode(current,linear);
	
	return textureUnit;
	}

/**************************************
Methods of class WaterTable2::DataItem:
**************************************/

WaterTable2::DataItem::DataItem(void)
	:bathymetry(GL_TEXTURE_RECTANGLE_ARB),
	 bathymetryVersion(0),
	 snow(GL_TEXTURE_RECTANGLE_ARB),
	 quantity(GL_TEXTURE_RECTANGLE_ARB),
	 derivativeTextureObject(0),
	 maxStepSize(GL_TEXTURE_RECTANGLE_ARB),
	 waterTextureObject(0),
	 bathymetryFramebufferObject(0),derivativeFramebufferObject(0),maxStepSizeFramebufferObject(0),integrationFramebufferObject(0),waterFramebufferObject(0)
	{
	}

WaterTable2::DataItem::~DataItem(void)
	{
	/* Delete all allocated textures and buffers: */
	glDeleteTextures(1,&derivativeTextureObject);
	glDeleteTextures(1,&waterTextureObject);
	glDeleteFramebuffersEXT(1,&bathymetryFramebufferObject);
	glDeleteFramebuffersEXT(1,&derivativeFramebufferObject);
	glDeleteFramebuffersEXT(1,&maxStepSizeFramebufferObject);
	glDeleteFramebuffersEXT(1,&integrationFramebufferObject);
	glDeleteFramebuffersEXT(1,&waterFramebufferObject);
	}

/****************************
Methods of class WaterTable2:
****************************/

void WaterTable2::calcTransformations(void)
	{
	/* Calculate the combined modelview and projection matrix to render depth images into the bathymetry grid: */
	{
	bathymetryPmv=PTransform::identity;
	PTransform::Matrix& bpmvm=bathymetryPmv.getMatrix();
	Scalar hw=Math::div2(cellSize[0]);
	Scalar left=domain.min[0]+hw;
	Scalar right=domain.max[0]-hw;
	Scalar hh=Math::div2(cellSize[1]);
	Scalar bottom=domain.min[1]+hh;
	Scalar top=domain.max[1]-hh;
	Scalar near=-domain.max[2];
	Scalar far=-domain.min[2];
	bpmvm(0,0)=Scalar(2)/(right-left);
	bpmvm(0,3)=-(right+left)/(right-left);
	bpmvm(1,1)=Scalar(2)/(top-bottom);
	bpmvm(1,3)=-(top+bottom)/(top-bottom);
	bpmvm(2,2)=Scalar(-2)/(far-near);
	bpmvm(2,3)=-(far+near)/(far-near);
	bathymetryPmv*=baseTransform;
	}
	
	/* Calculate the combined modelview and projection matrix to render water-adding geometry into the water texture: */
	{
	waterAddPmv=PTransform::identity;
	PTransform::Matrix& wapmvm=waterAddPmv.getMatrix();
	Scalar left=domain.min[0];
	Scalar right=domain.max[0];
	Scalar bottom=domain.min[1];
	Scalar top=domain.max[1];
	Scalar near=-domain.max[2]*Scalar(5);
	Scalar far=-domain.min[2];
	wapmvm(0,0)=Scalar(2)/(right-left);
	wapmvm(0,3)=-(right+left)/(right-left);
	wapmvm(1,1)=Scalar(2)/(top-bottom);
	wapmvm(1,3)=-(top+bottom)/(top-bottom);
	wapmvm(2,2)=Scalar(-2)/(far-near);
	wapmvm(2,3)=-(far+near)/(far-near);
	waterAddPmv*=baseTransform;
	}
	
	/* Convert the water addition matrix to column-major OpenGL format: */
	GLfloat* wapPtr=waterAddPmvMatrix;
	for(int j=0;j<4;++j)
		for(int i=0;i<4;++i,++wapPtr)
			*wapPtr=GLfloat(waterAddPmv.getMatrix()(i,j));
	
	/* Calculate a transformation from camera space into water texture space: */
	waterTextureTransform=PTransform::identity;
	PTransform::Matrix& wttm=waterTextureTransform.getMatrix();
	wttm(0,0)=Scalar(size[0])/(domain.max[0]-domain.min[0]);
	wttm(0,3)=wttm(0,0)*-domain.min[0];
	wttm(1,1)=Scalar(size[1])/(domain.max[1]-domain.min[1]);
	wttm(1,3)=wttm(1,1)*-domain.min[1];
	waterTextureTransform*=baseTransform;
	
	/* Convert the water texture transform to column-major OpenGL format: */
	GLfloat* wttmPtr=waterTextureTransformMatrix;
	for(int j=0;j<4;++j)
		for(int i=0;i<4;++i,++wttmPtr)
			*wttmPtr=GLfloat(wttm(i,j));
	}

GLfloat WaterTable2::calcDerivative(GLContextData& contextData,TextureTracker& textureTracker,int quantityTextureIndex,bool calcMaxStepSize) const
	{
	/* Retrieve the context data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/*********************************************************************
	Step 1: Calculate partial spatial derivatives, partial fluxes across
	cell boundaries, and the temporal derivative.
	*********************************************************************/
	
	/* Set up the derivative computation frame buffer: */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->derivativeFramebufferObject);
	glViewport(size);
	
	/* Set up the temporal derivative computation shader: */
	Shader* derivativeShader=&dataItem->derivativeShaders[mode];
	derivativeShader->use();
	textureTracker.reset();
	derivativeShader->uploadUniform2v(1,cellSize);
	derivativeShader->uploadUniform(theta);
	derivativeShader->uploadUniform(g);
	derivativeShader->uploadUniform(epsilon);
	derivativeShader->uploadUniform2v(1,maxPropagationSpeed);
	dataItem->bathymetry.bind(textureTracker,*derivativeShader,dataItem->bathymetry.current,false);
	dataItem->quantity.bind(textureTracker,*derivativeShader,quantityTextureIndex,false);
	if(mode==Engineering)
		derivativeShader->uploadUniform(propertyGridCreator->bindPropertyGridTexture(contextData,textureTracker));
	
	/* Run the temporal derivative computation: */
	glBegin(GL_QUADS);
	glVertex2i(0,0);
	glVertex2i(size[0],0);
	glVertex2i(size[0],size[1]);
	glVertex2i(0,size[1]);
	glEnd();
	
	/*********************************************************************
	Step 2: Gather the maximum step size by reducing the maximum step size
	texture.
	*********************************************************************/
	
	GLfloat stepSize=maxStepSize;
	
	if(calcMaxStepSize)
		{
		/* Install the maximum step size reduction shader: */
		dataItem->maxStepSizeShader.use();
		
		/* Bind the maximum step size computation frame buffer: */
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->maxStepSizeFramebufferObject);
		
		/* Reduce the maximum step size texture in a sequence of half-reduction steps: */
		Size reducedSize=size;
		dataItem->maxStepSize.current=0;
		while(reducedSize[0]>1||reducedSize[1]>1)
			{
			/* Set up the maximum step size reduction shader for this reduction step: */
			dataItem->maxStepSizeShader.resetUniforms();
			textureTracker.reset();
			
			/* Set up the simulation frame buffer for maximum step size reduction: */
			glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT+(1-dataItem->maxStepSize.current));
			
			/* Reduce the viewport by a factor of two: */
			Size nextReducedSize((reducedSize[0]+1)/2,(reducedSize[1]+1)/2);
			glViewport(nextReducedSize);
			dataItem->maxStepSizeShader.uploadUniform(GLfloat(reducedSize[0]-1),GLfloat(reducedSize[1]-1));
			
			/* Bind the current max step size texture: */
			dataItem->maxStepSize.bind(textureTracker,dataItem->maxStepSizeShader,dataItem->maxStepSize.current,false);
			
			/* Run the reduction step: */
			glBegin(GL_QUADS);
			
			/* We're using size[] instead of nextReducedSize[] here because the vertex shader will scale properly: */
			glVertex2i(0,0);
			glVertex2i(size[0],0);
			glVertex2i(size[0],size[1]);
			glVertex2i(0,size[1]);
			
			glEnd();
			
			/* Go to the next step: */
			reducedSize=nextReducedSize;
			dataItem->maxStepSize.current=1-dataItem->maxStepSize.current;
			}
		
		/* Read the final value written into the last reduced 1x1 frame buffer: */
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT+dataItem->maxStepSize.current);
		glReadPixels(0,0,1,1,GL_LUMINANCE,GL_FLOAT,&stepSize);
		
		/* Limit the step size to the client-specified range: */
		stepSize=Math::min(stepSize,maxStepSize);
		}
	
	return stepSize;
	}

WaterTable2::WaterTable2(const Size& sSize,const GLfloat sCellSize[2])
	:size(sSize),
	 depthImageRenderer(0),
	 baseTransform(ONTransform::identity),
	 mode(Traditional),
	 propertyGridCreator(0),
	 dryBoundary(true)
	{
	/* Initialize the water table cell size: */
	for(int i=0;i<2;++i)
		cellSize[i]=sCellSize[i];
	
	/* Calculate a simulation domain: */
	for(int i=0;i<2;++i)
		{
		domain.min[i]=Scalar(0);
		domain.max[i]=Scalar(size[i])*Scalar(cellSize[i]);
		}
	
	/* Calculate the water table transformations: */
	calcTransformations();
	
	/* Initialize simulation parameters: */
	theta=1.3f;
	g=9.81f;
	epsilon=0.01f*Math::max(Math::max(cellSize[0],cellSize[1]),1.0f);
	maxPropagationSpeed[1]=maxPropagationSpeed[0]=1.0e10; // Ridiculously large
	attenuation=127.0f/128.0f; // 31.0f/32.0f;
	maxStepSize=1.0f;
	
	/* Initialize snow pack simulation: */
	snowLine=1000.0f;
	snowMelt=0.1f;
	
	/* Initialize the water deposit amount: */
	waterDeposit=0.0f;
	}

WaterTable2::WaterTable2(const Size& sSize,const DepthImageRenderer* sDepthImageRenderer,const Point basePlaneCorners[4])
	:size(sSize),
	 depthImageRenderer(sDepthImageRenderer),
	 mode(Traditional),
	 propertyGridCreator(0),
	 dryBoundary(true)
	{
	/* Project the corner points to the base plane and calculate their centroid: */
	const Plane& basePlane=depthImageRenderer->getBasePlane();
	Point bpc[4];
	Point::AffineCombiner centroidC;
	for(int i=0;i<4;++i)
		{
		bpc[i]=basePlane.project(basePlaneCorners[i]);
		centroidC.addPoint(bpc[i]);
		}
	Point baseCentroid=centroidC.getPoint();
	
	/* Calculate the transformation from camera space to upright elevation model space: */
	typedef Point::Vector Vector;
	Vector z=basePlane.getNormal();
	Vector x=(bpc[1]-bpc[0])+(bpc[3]-bpc[2]);
	Vector y=z^x;
	baseTransform=ONTransform::translateFromOriginTo(baseCentroid);
	baseTransform*=ONTransform::rotate(ONTransform::Rotation::fromBaseVectors(x,y));
	baseTransform.doInvert();
	
	/* Calculate the domain of upright elevation model space: */
	domain=Box::empty;
	#if 0
	/* Calculate the bounding box of the four base plane corners: */
	for(int i=0;i<4;++i)
		domain.addPoint(baseTransform.transform(bpc[i]));
	#else
	/* Fit the domain into the four base plane corners: */
	Point bbpc[4];
	for(int i=0;i<4;++i)
		bbpc[i]=baseTransform.transform(bpc[i]);
	domain.min[0]=Math::max(bbpc[0][0],bbpc[2][0]);
	domain.max[0]=Math::min(bbpc[1][0],bbpc[3][0]);
	domain.min[1]=Math::max(bbpc[0][1],bbpc[1][1]);
	domain.max[1]=Math::min(bbpc[2][1],bbpc[3][1]);
	#endif
	domain.min[2]=Scalar(-20);
	domain.max[2]=Scalar(100);
	
	/* Calculate the grid's cell size: */
	for(int i=0;i<2;++i)
		cellSize[i]=GLfloat((domain.max[i]-domain.min[i])/Scalar(size[i]));
	
	// DEBUGGING
	// std::cout<<cellSize[0]<<" x "<<cellSize[1]<<std::endl;
	
	/* Calculate the water table transformations: */
	calcTransformations();
	
	/* Initialize simulation parameters: */
	theta=1.3f;
	g=9.81f;
	epsilon=0.01f*Math::max(Math::max(cellSize[0],cellSize[1]),1.0f);
	maxPropagationSpeed[1]=maxPropagationSpeed[0]=1.0e10; // Ridiculously large
	attenuation=127.0f/128.0f; // 31.0f/32.0f;
	maxStepSize=1.0f;
	
	/* Initialize the water deposit amount: */
	waterDeposit=0.0f;
	}

WaterTable2::~WaterTable2(void)
	{
	}

void WaterTable2::initContext(GLContextData& contextData) const
	{
	/* Initialize required OpenGL extensions: */
	GLARBDrawBuffers::initExtension();
	GLARBFragmentShader::initExtension();
	GLARBTextureFloat::initExtension();
	GLARBTextureRectangle::initExtension();
	GLARBTextureRg::initExtension();
	GLARBVertexShader::initExtension();
	GLEXTFramebufferObject::initExtension();
	Shader::initExtensions();
	
	/* Create a data item and add it to the context: */
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);
	
	/* Create the vertex-centered bathymetry texture, replacing the outermost layer of cells with ghost cells: */
	dataItem->bathymetry.init(size[0]-1,size[1]-1,1,GL_R32F,GL_LUMINANCE,domain.min[2]);
	
	/* Create the cell-centered snow height texture: */
	dataItem->snow.init(size[0],size[1],1,GL_R32F,GL_LUMINANCE,0.0f);
	
	/* Create the cell-centered quantity state texture: */
	dataItem->quantity.init(size[0],size[1],3,GL_RGB32F,GL_RGB,domain.min[2],0.0f,0.0f);
	
	{
	/* Create the cell-centered temporal derivative texture: */
	glGenTextures(1,&dataItem->derivativeTextureObject);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->derivativeTextureObject);
	sampleNearest();
	GLfloat* qt=makeBuffer(size[0],size[1],3,0.0f,0.0f,0.0f);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_RGB32F,size[0],size[1],0,GL_RGB,GL_FLOAT,qt);
	delete[] qt;
	}
	
	/* Create the cell-centered maximum step size gathering texture: */
	dataItem->maxStepSize.init(size[0],size[1],1,GL_R32F,GL_LUMINANCE,10000.0f);
	
	{
	/* Create the cell-centered water texture: */
	glGenTextures(1,&dataItem->waterTextureObject);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->waterTextureObject);
	sampleNearest();
	GLfloat* w=makeBuffer(size[0],size[1],1,0.0f);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_R32F,size[0],size[1],0,GL_LUMINANCE,GL_FLOAT,w);
	delete[] w;
	}
	
	/* Protect the newly-created textures: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	
	/* Save the currently bound frame buffer: */
	GLint currentFrameBuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&currentFrameBuffer);
	
	{
	/* Create the bathymetry rendering frame buffer: */
	glGenFramebuffersEXT(1,&dataItem->bathymetryFramebufferObject);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->bathymetryFramebufferObject);
	
	/* Attach the bathymetry textures to the bathymetry rendering frame buffer: */
	for(int i=0;i<2;++i)
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT+i,GL_TEXTURE_RECTANGLE_ARB,dataItem->bathymetry.textureObjects[i],0);
	
	/* Active buffers will be set up during rendering: */
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	}
	
	{
	/* Create the temporal derivative computation frame buffer: */
	glGenFramebuffersEXT(1,&dataItem->derivativeFramebufferObject);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->derivativeFramebufferObject);
	
	/* Attach the derivative and maximum step size textures to the temporal derivative computation frame buffer: */
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_RECTANGLE_ARB,dataItem->derivativeTextureObject,0);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT1_EXT,GL_TEXTURE_RECTANGLE_ARB,dataItem->maxStepSize.textureObjects[0],0);
	GLenum drawBuffers[2]={GL_COLOR_ATTACHMENT0_EXT,GL_COLOR_ATTACHMENT1_EXT};
	glDrawBuffersARB(2,drawBuffers);
	glReadBuffer(GL_NONE);
	}
	
	{
	/* Create the maximum step size computation frame buffer: */
	glGenFramebuffersEXT(1,&dataItem->maxStepSizeFramebufferObject);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->maxStepSizeFramebufferObject);
	
	/* Attach the maximum step size textures to the maximum step size computation frame buffer: */
	for(int i=0;i<2;++i)
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT+i,GL_TEXTURE_RECTANGLE_ARB,dataItem->maxStepSize.textureObjects[i],0);
	
	/* Active buffers will be set up during rendering: */
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	}
	
	{
	/* Create the integration step frame buffer: */
	glGenFramebuffersEXT(1,&dataItem->integrationFramebufferObject);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->integrationFramebufferObject);
	
	/* Attach the conserved quantity textures to the integration step frame buffer: */
	for(int i=0;i<3;++i)
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT+i,GL_TEXTURE_RECTANGLE_ARB,dataItem->quantity.textureObjects[i],0);
	
	/* Attach the snow height textures to the integration step frame buffer: */
	for(int i=0;i<2;++i)
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT+3+i,GL_TEXTURE_RECTANGLE_ARB,dataItem->snow.textureObjects[i],0);
	
	/* Active buffers will be set up during rendering: */
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	}
	
	{
	/* Create the water frame buffer: */
	glGenFramebuffersEXT(1,&dataItem->waterFramebufferObject);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->waterFramebufferObject);
	
	/* Attach the water texture to the water frame buffer: */
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_RECTANGLE_ARB,dataItem->waterTextureObject,0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	glReadBuffer(GL_NONE);
	}
	
	/* Restore the previously bound frame buffer: */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,currentFrameBuffer);
	
	/* Create a simple vertex shader to render quads in pixel space: */
	static const char* vertexShaderSourceTemplate="void main(){gl_Position=vec4(gl_Vertex.x*%f-1.0,gl_Vertex.y*%f-1.0,0.0,1.0);}";
	char vertexShaderSource[256];
	snprintf(vertexShaderSource,sizeof(vertexShaderSource),vertexShaderSourceTemplate,2.0/double(size[0]),2.0/double(size[1]));
	GLhandleARB vertexShader=glCompileVertexShaderFromString(vertexShaderSource);
	
	/* Create the bathymetry update shader: */
	dataItem->bathymetryShader.addShader(vertexShader,false);
	dataItem->bathymetryShader.addShader(compileFragmentShader("Water2BathymetryUpdateShader"));
	dataItem->bathymetryShader.link();
	dataItem->bathymetryShader.setUniformLocation("oldBathymetrySampler");
	dataItem->bathymetryShader.setUniformLocation("newBathymetrySampler");
	dataItem->bathymetryShader.setUniformLocation("quantitySampler");
	
	/* Create the water adaptation shader: */
	dataItem->waterAdaptShader.addShader(vertexShader,false);
	dataItem->waterAdaptShader.addShader(compileFragmentShader("Water2WaterAdaptShader"));
	dataItem->waterAdaptShader.link();
	dataItem->waterAdaptShader.setUniformLocation("bathymetrySampler");
	dataItem->waterAdaptShader.setUniformLocation("newQuantitySampler");
	
	/* Create the "traditional" temporal derivative computation shader: */
	dataItem->derivativeShaders[0].addShader(vertexShader,false);
	dataItem->derivativeShaders[0].addShader(compileFragmentShader("Water2SlopeAndFluxAndDerivativeShader"));
	dataItem->derivativeShaders[0].link();
	dataItem->derivativeShaders[0].setUniformLocation("cellSize");
	dataItem->derivativeShaders[0].setUniformLocation("theta");
	dataItem->derivativeShaders[0].setUniformLocation("g");
	dataItem->derivativeShaders[0].setUniformLocation("epsilon");
	dataItem->derivativeShaders[0].setUniformLocation("maxPropagationSpeed");
	dataItem->derivativeShaders[0].setUniformLocation("bathymetrySampler");
	dataItem->derivativeShaders[0].setUniformLocation("quantitySampler");
	
	/* Create the "engineering" temporal derivative computation shader: */
	dataItem->derivativeShaders[1].addShader(vertexShader,false);
	dataItem->derivativeShaders[1].addShader(compileFragmentShader("Water2EngineeringSlopeAndFluxAndDerivativeShader"));
	dataItem->derivativeShaders[1].link();
	dataItem->derivativeShaders[1].setUniformLocation("cellSize");
	dataItem->derivativeShaders[1].setUniformLocation("theta");
	dataItem->derivativeShaders[1].setUniformLocation("g");
	dataItem->derivativeShaders[1].setUniformLocation("epsilon");
	dataItem->derivativeShaders[1].setUniformLocation("maxPropagationSpeed");
	dataItem->derivativeShaders[1].setUniformLocation("bathymetrySampler");
	dataItem->derivativeShaders[1].setUniformLocation("quantitySampler");
	dataItem->derivativeShaders[1].setUniformLocation("gridPropertySampler");
	
	/* Create the maximum step size gathering shader: */
	dataItem->maxStepSizeShader.addShader(vertexShader,false);
	dataItem->maxStepSizeShader.addShader(compileFragmentShader("Water2MaxStepSizeShader"));
	dataItem->maxStepSizeShader.link();
	dataItem->maxStepSizeShader.setUniformLocation("fullTextureSize");
	dataItem->maxStepSizeShader.setUniformLocation("maxStepSizeSampler");
	
	/* Create the boundary condition shader: */
	dataItem->boundaryShader.addShader(vertexShader,false);
	dataItem->boundaryShader.addShader(compileFragmentShader("Water2BoundaryShader"));
	dataItem->boundaryShader.link();
	dataItem->boundaryShader.setUniformLocation("bathymetrySampler");
	
	/* Create the "traditional" Euler integration step shader: */
	dataItem->eulerStepShaders[0].addShader(vertexShader,false);
	dataItem->eulerStepShaders[0].addShader(compileFragmentShader("Water2EulerStepShader"));
	dataItem->eulerStepShaders[0].link();
	dataItem->eulerStepShaders[0].setUniformLocation("stepSize");
	dataItem->eulerStepShaders[0].setUniformLocation("attenuation");
	dataItem->eulerStepShaders[0].setUniformLocation("quantitySampler");
	dataItem->eulerStepShaders[0].setUniformLocation("derivativeSampler");
	
	/* Create the "engineering" Euler integration step shader: */
	dataItem->eulerStepShaders[1].addShader(vertexShader,false);
	dataItem->eulerStepShaders[1].addShader(compileFragmentShader("Water2EngineeringEulerStepShader"));
	dataItem->eulerStepShaders[1].link();
	dataItem->eulerStepShaders[1].setUniformLocation("stepSize");
	dataItem->eulerStepShaders[1].setUniformLocation("quantitySampler");
	dataItem->eulerStepShaders[1].setUniformLocation("derivativeSampler");
	
	/* Create the "traditional" Runge-Kutta integration step shader: */
	dataItem->rungeKuttaStepShaders[0].addShader(vertexShader,false);
	dataItem->rungeKuttaStepShaders[0].addShader(compileFragmentShader("Water2RungeKuttaStepShader"));
	dataItem->rungeKuttaStepShaders[0].link();
	dataItem->rungeKuttaStepShaders[0].setUniformLocation("stepSize");
	dataItem->rungeKuttaStepShaders[0].setUniformLocation("attenuation");
	dataItem->rungeKuttaStepShaders[0].setUniformLocation("quantitySampler");
	dataItem->rungeKuttaStepShaders[0].setUniformLocation("quantityStarSampler");
	dataItem->rungeKuttaStepShaders[0].setUniformLocation("derivativeSampler");
	
	/* Create the "engineering" Runge-Kutta integration step shader: */
	dataItem->rungeKuttaStepShaders[1].addShader(vertexShader,false);
	dataItem->rungeKuttaStepShaders[1].addShader(compileFragmentShader("Water2EngineeringRungeKuttaStepShader"));
	dataItem->rungeKuttaStepShaders[1].link();
	dataItem->rungeKuttaStepShaders[1].setUniformLocation("stepSize");
	dataItem->rungeKuttaStepShaders[1].setUniformLocation("quantitySampler");
	dataItem->rungeKuttaStepShaders[1].setUniformLocation("quantityStarSampler");
	dataItem->rungeKuttaStepShaders[1].setUniformLocation("derivativeSampler");
	
	/* Create the water adder rendering shader: */
	dataItem->waterAddShader.addShader(compileVertexShader("Water2WaterAddShader"));
	dataItem->waterAddShader.addShader(compileFragmentShader("Water2WaterAddShader"));
	dataItem->waterAddShader.link();
	dataItem->waterAddShader.setUniformLocation("pmv");
	dataItem->waterAddShader.setUniformLocation("stepSize");
	
	/* Create the water shader: */
	dataItem->waterShader.addShader(vertexShader,false);
	dataItem->waterShader.addShader(compileFragmentShader("Water2WaterUpdateShader"));
	dataItem->waterShader.link();
	dataItem->waterShader.setUniformLocation("bathymetrySampler");
	dataItem->waterShader.setUniformLocation("snowSampler");
	dataItem->waterShader.setUniformLocation("quantitySampler");
	dataItem->waterShader.setUniformLocation("waterSampler");
	dataItem->waterShader.setUniformLocation("snowLine");
	dataItem->waterShader.setUniformLocation("snowMelt");
	
	/* Delete the shared vertex shader: */
	glDeleteObjectARB(vertexShader);
	}

void WaterTable2::setElevationRange(Scalar newMin,Scalar newMax)
	{
	/* Set the new elevation range: */
	domain.min[2]=newMin;
	domain.max[2]=newMax;
	
	/* Recalculate the water table transformations: */
	calcTransformations();
	}

void WaterTable2::setMode(WaterTable2::Mode newMode)
	{
	mode=newMode;
	}

void WaterTable2::setAttenuation(GLfloat newAttenuation)
	{
	attenuation=newAttenuation;
	}

void WaterTable2::setPropertyGridCreator(PropertyGridCreator* newPropertyGridCreator)
	{
	propertyGridCreator=newPropertyGridCreator;
	}

void WaterTable2::forceMinStepSize(GLfloat newMinStepSize)
	{
	/* Calculate the maximum propagation speeds in x and y: */
	for(int i=0;i<2;++i)
		maxPropagationSpeed[i]=cellSize[i]/(2.0f*newMinStepSize);
	}

void WaterTable2::setMaxStepSize(GLfloat newMaxStepSize)
	{
	maxStepSize=newMaxStepSize;
	}

void WaterTable2::addRenderFunction(const AddWaterFunction* newRenderFunction)
	{
	/* Store the new render function: */
	renderFunctions.push_back(newRenderFunction);
	}

void WaterTable2::removeRenderFunction(const AddWaterFunction* removeRenderFunction)
	{
	/* Find the given render function in the list and remove it: */
	for(std::vector<const AddWaterFunction*>::iterator rfIt=renderFunctions.begin();rfIt!=renderFunctions.end();++rfIt)
		if(*rfIt==removeRenderFunction)
			{
			/* Remove the list element: */
			renderFunctions.erase(rfIt);
			break;
			}
	}

void WaterTable2::setSnowLine(GLfloat newSnowLine)
	{
	snowLine=newSnowLine;
	}

void WaterTable2::setSnowMelt(GLfloat newSnowMelt)
	{
	snowMelt=newSnowMelt;
	}

void WaterTable2::setWaterDeposit(GLfloat newWaterDeposit)
	{
	waterDeposit=newWaterDeposit;
	}

void WaterTable2::setDryBoundary(bool newDryBoundary)
	{
	dryBoundary=newDryBoundary;
	}

void WaterTable2::updateBathymetry(GLContextData& contextData,TextureTracker& textureTracker) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Check if the current bathymetry texture is outdated: */
	if(dataItem->bathymetryVersion!=depthImageRenderer->getDepthImageVersion())
		{
		/* Retrieve the current and new buffer slots for the bathymetry and quantity textures: */
		int oldBathymetry=dataItem->bathymetry.current;
		int newBathymetry=1-oldBathymetry;
		int oldQuantity=dataItem->quantity.current;
		int newQuantity=1-oldQuantity;
		
		/* Save relevant OpenGL state: */
		glPushAttrib(GL_VIEWPORT_BIT);
		GLint currentFrameBuffer;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&currentFrameBuffer);
		GLfloat currentClearColor[4];
		glGetFloatv(GL_COLOR_CLEAR_VALUE,currentClearColor);
		
		/* Bind the bathymetry rendering frame buffer and clear it: */
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->bathymetryFramebufferObject);
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT+newBathymetry);
		glViewport(getBathymetrySize());
		glClearColor(GLfloat(domain.min[2]),0.0f,0.0f,1.0f);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		
		/* Render the surface into the bathymetry grid: */
		depthImageRenderer->renderElevation(bathymetryPmv,contextData,textureTracker);
		
		/* Set up the integration frame buffer to update the conserved quantities based on bathymetry changes: */
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->integrationFramebufferObject);
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT+newQuantity);
		glViewport(size);
		
		/* Set up the bathymetry update shader: */
		dataItem->bathymetryShader.use();
		textureTracker.reset();
		dataItem->bathymetry.bind(textureTracker,dataItem->bathymetryShader,oldBathymetry,false);
		dataItem->bathymetry.bind(textureTracker,dataItem->bathymetryShader,newBathymetry,false);
		dataItem->quantity.bind(textureTracker,dataItem->bathymetryShader,oldQuantity,false);
		
		/* Run the bathymetry update: */
		glBegin(GL_QUADS);
		glVertex2i(0,0);
		glVertex2i(size[0],0);
		glVertex2i(size[0],size[1]);
		glVertex2i(0,size[1]);
		glEnd();
		
		/* Update the bathymetry and quantity grids: */
		dataItem->bathymetry.current=newBathymetry;
		dataItem->bathymetryVersion=depthImageRenderer->getDepthImageVersion();
		dataItem->quantity.current=newQuantity;
		
		/* Restore OpenGL state: */
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,currentFrameBuffer);
		glClearColor(currentClearColor[0],currentClearColor[1],currentClearColor[2],currentClearColor[3]);
		glPopAttrib();
		}
	}

void WaterTable2::updateBathymetry(const GLfloat* bathymetryGrid,GLContextData& contextData,TextureTracker& textureTracker) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Retrieve the current and new buffer slots for the bathymetry and quantity textures: */
	int oldBathymetry=dataItem->bathymetry.current;
	int newBathymetry=1-oldBathymetry;
	int oldQuantity=dataItem->quantity.current;
	int newQuantity=1-oldQuantity;
	
	/* Save relevant OpenGL state: */
	glPushAttrib(GL_VIEWPORT_BIT);
	GLint currentFrameBuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&currentFrameBuffer);
	
	/* Set up the integration frame buffer to update the conserved quantities based on bathymetry changes: */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->integrationFramebufferObject);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT+newQuantity);
	glViewport(size);
	
	/* Set up the bathymetry update shader: */
	dataItem->bathymetryShader.use();
	textureTracker.reset();
	dataItem->bathymetry.bind(textureTracker,dataItem->bathymetryShader,oldBathymetry,false);
	
	/* Bind and upload the given new bathymetry grid: */
	dataItem->bathymetry.bind(textureTracker,dataItem->bathymetryShader,newBathymetry,false);
	glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,getBathymetrySize(),GL_LUMINANCE,GL_FLOAT,bathymetryGrid);
	
	dataItem->quantity.bind(textureTracker,dataItem->bathymetryShader,oldQuantity,false);
	
	/* Run the bathymetry update: */
	glBegin(GL_QUADS);
	glVertex2i(0,0);
	glVertex2i(size[0],0);
	glVertex2i(size[0],size[1]);
	glVertex2i(0,size[1]);
	glEnd();

	/* Update the bathymetry and quantity grids: */
	dataItem->bathymetry.current=newBathymetry;
	dataItem->quantity.current=newQuantity;

	/* Restore OpenGL state: */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,currentFrameBuffer);
	glPopAttrib();
	}

void WaterTable2::setWaterLevel(const GLfloat* waterGrid,GLContextData& contextData,TextureTracker& textureTracker) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Retrieve the current and new buffer slots for the quantity texture: */
	int oldQuantity=dataItem->quantity.current;
	int newQuantity=1-oldQuantity;
	
	/* Save relevant OpenGL state: */
	glPushAttrib(GL_VIEWPORT_BIT);
	GLint currentFrameBuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&currentFrameBuffer);
	
	/* Set up the integration frame buffer to adapt the new water level to the current bathymetry: */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->integrationFramebufferObject);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT+newQuantity);
	glViewport(size);
	
	/* Set up the water adaptation shader: */
	dataItem->waterAdaptShader.use();
	textureTracker.reset();
	dataItem->bathymetry.bind(textureTracker,dataItem->waterAdaptShader,dataItem->bathymetry.current,false);
	dataItem->quantity.bind(textureTracker,dataItem->waterAdaptShader,oldQuantity,false);
	
	/* Upload the given water level grid into the quantity texture: */
	glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,size,GL_RED,GL_FLOAT,waterGrid);
	
	/* Run the water adaptation shader: */
	glBegin(GL_QUADS);
	glVertex2i(0,0);
	glVertex2i(size[0],0);
	glVertex2i(size[0],size[1]);
	glVertex2i(0,size[1]);
	glEnd();

	/* Update the quantity grid: */
	dataItem->quantity.current=newQuantity;

	/* Restore OpenGL state: */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,currentFrameBuffer);
	glPopAttrib();
	}

GLfloat WaterTable2::runSimulationStep(bool forceStepSize,GLContextData& contextData,TextureTracker& textureTracker) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Save relevant OpenGL state: */
	glPushAttrib(GL_COLOR_BUFFER_BIT|GL_VIEWPORT_BIT);
	GLint currentFrameBuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&currentFrameBuffer);
	
	/*********************************************************************
	Step 1: Calculate temporal derivative of most recent quantities.
	*********************************************************************/
	
	GLfloat stepSize=calcDerivative(contextData,textureTracker,dataItem->quantity.current,!forceStepSize);
	
	// DEBUGGING
	// std::cout<<stepSize<<' ';
	
	/*********************************************************************
	Step 2: Perform the tentative Euler integration step.
	*********************************************************************/
	
	/* Set up the Euler step integration frame buffer: */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->integrationFramebufferObject);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT+2);
	glViewport(size);
	
	/* Set up the Euler integration step shader: */
	Shader* eulerStepShader=&dataItem->eulerStepShaders[mode];
	eulerStepShader->use();
	textureTracker.reset();
	eulerStepShader->uploadUniform(stepSize);
	if(mode==Traditional)
		eulerStepShader->uploadUniform(Math::pow(attenuation,stepSize));
	dataItem->quantity.bind(textureTracker,*eulerStepShader,dataItem->quantity.current,false);
	eulerStepShader->uploadUniform(textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->derivativeTextureObject));
	
	/* Run the Euler integration step: */
	glBegin(GL_QUADS);
	glVertex2i(0,0);
	glVertex2i(size[0],0);
	glVertex2i(size[0],size[1]);
	glVertex2i(0,size[1]);
	glEnd();
	
	/*********************************************************************
	Step 3: Calculate temporal derivative of intermediate quantities.
	*********************************************************************/
	
	calcDerivative(contextData,textureTracker,2,false);
	
	/*********************************************************************
	Step 4: Perform the final Runge-Kutta integration step.
	*********************************************************************/
	
	/* Set up the Runge-Kutta step integration frame buffer: */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->integrationFramebufferObject);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT+(1-dataItem->quantity.current));
	glViewport(size);
	
	/* Set up the Runge-Kutta integration step shader: */
	Shader* rungeKuttaStepShader=&dataItem->rungeKuttaStepShaders[mode];
	rungeKuttaStepShader->use();
	textureTracker.reset();
	rungeKuttaStepShader->uploadUniform(stepSize);
	if(mode==Traditional)
		rungeKuttaStepShader->uploadUniform(Math::pow(attenuation,stepSize));
	dataItem->quantity.bind(textureTracker,*rungeKuttaStepShader,dataItem->quantity.current,false);
	dataItem->quantity.bind(textureTracker,*rungeKuttaStepShader,2,false);
	rungeKuttaStepShader->uploadUniform(textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->derivativeTextureObject));
	
	/* Run the Runge-Kutta integration step: */
	glBegin(GL_QUADS);
	glVertex2i(0,0);
	glVertex2i(size[0],0);
	glVertex2i(size[0],size[1]);
	glVertex2i(0,size[1]);
	glEnd();
	
	if(dryBoundary)
		{
		/* Set up the boundary condition shader to enforce dry boundaries: */
		dataItem->boundaryShader.use();
		textureTracker.reset();
		dataItem->bathymetry.bind(textureTracker,dataItem->boundaryShader,dataItem->bathymetry.current,false);
		
		/* Run the boundary condition shader on the outermost layer of pixels: */
		//glColorMask(GL_TRUE,GL_FALSE,GL_FALSE,GL_FALSE);
		glBegin(GL_LINE_LOOP);
		glVertex2f(0.5f,0.5f);
		glVertex2f(GLfloat(size[0])-0.5f,0.5f);
		glVertex2f(GLfloat(size[0])-0.5f,GLfloat(size[1])-0.5f);
		glVertex2f(0.5f,GLfloat(size[1])-0.5f);
		glEnd();
		//glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
		}
	
	/* Update the current quantities: */
	dataItem->quantity.current=1-dataItem->quantity.current;
	
	if(waterDeposit!=0.0f||!renderFunctions.empty())
		{
		/* Save OpenGL state: */
		GLfloat currentClearColor[4];
		glGetFloatv(GL_COLOR_CLEAR_VALUE,currentClearColor);
		
		/*******************************************************************
		Step 5: Render all water sources and sinks additively into the water
		texture.
		*******************************************************************/
		
		/* Set up and clear the water frame buffer: */
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->waterFramebufferObject);
		glViewport(size);
		glClearColor(waterDeposit*stepSize,0.0f,0.0f,0.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		
		/* Enable additive rendering: */
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE,GL_ONE);
		
		/* Set up the water adding shader: */
		dataItem->waterAddShader.use();
		dataItem->waterAddShader.uploadUniformMatrix4(1,GL_FALSE,waterAddPmvMatrix);
		dataItem->waterAddShader.uploadUniform(stepSize);
		
		/* Call all render functions: */
		for(std::vector<const AddWaterFunction*>::const_iterator rfIt=renderFunctions.begin();rfIt!=renderFunctions.end();++rfIt)
			(**rfIt)(contextData);
		
		/* Restore OpenGL state: */
		glDisable(GL_BLEND);
		glClearColor(currentClearColor[0],currentClearColor[1],currentClearColor[2],currentClearColor[3]);
		
		/*******************************************************************
		Step 6: Update the conserved quantities based on the water texture.
		*******************************************************************/
		
		/* Set up the integration frame buffer to update the conserved quantities based on the water texture: */
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->integrationFramebufferObject);
		GLenum drawBuffers[2];
		drawBuffers[0]=GL_COLOR_ATTACHMENT0_EXT+(1-dataItem->quantity.current);
		drawBuffers[1]=GL_COLOR_ATTACHMENT0_EXT+3+(1-dataItem->snow.current);
		glDrawBuffersARB(2,drawBuffers);
		glViewport(size);
		
		/* Set up the water update shader: */
		dataItem->waterShader.use();
		textureTracker.reset();
		dataItem->bathymetry.bind(textureTracker,dataItem->waterShader,dataItem->bathymetry.current,false);
		dataItem->snow.bind(textureTracker,dataItem->waterShader,dataItem->snow.current,false);
		dataItem->quantity.bind(textureTracker,dataItem->waterShader,dataItem->quantity.current,false);
		dataItem->waterShader.uploadUniform(textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->waterTextureObject));
		dataItem->waterShader.uploadUniform(snowLine);
		dataItem->waterShader.uploadUniform(snowMelt*stepSize);
		
		/* Run the water update: */
		glBegin(GL_QUADS);
		glVertex2i(0,0);
		glVertex2i(size[0],0);
		glVertex2i(size[0],size[1]);
		glVertex2i(0,size[1]);
		glEnd();
		
		/* Update the snow height and current quantities: */
		dataItem->snow.current=1-dataItem->snow.current;
		dataItem->quantity.current=1-dataItem->quantity.current;
		}
	
	/* Restore OpenGL state: */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,currentFrameBuffer);
	glPopAttrib();
	
	/* Return the Runge-Kutta step's step size: */
	return stepSize;
	}

void WaterTable2::uploadWaterTextureTransform(Shader& shader) const
	{
	/* Upload the matrix to the given shader: */
	shader.uploadUniformMatrix4(1,GL_FALSE,waterTextureTransformMatrix);
	}

GLint WaterTable2::bindBathymetryTexture(GLContextData& contextData,TextureTracker& textureTracker,bool linearSampling) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Bind the current bathymetry texture to the given texture tracker and return the index of the used texture unit: */
	return dataItem->bathymetry.bindCurrent(textureTracker,linearSampling);
	}

GLint WaterTable2::bindSnowTexture(GLContextData& contextData,TextureTracker& textureTracker,bool linearSampling) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Bind the current snow height texture to the given texture tracker and return the index of the used texture unit: */
	return dataItem->snow.bindCurrent(textureTracker,linearSampling);
	}

GLint WaterTable2::bindQuantityTexture(GLContextData& contextData,TextureTracker& textureTracker,bool linearSampling) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Bind the current conserved quantities texture to the given texture tracker and return the index of the used texture unit: */
	return dataItem->quantity.bindCurrent(textureTracker,linearSampling);
	}

void WaterTable2::readBathymetryTexture(GLContextData& contextData,TextureTracker& textureTracker,GLfloat* buffer) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Bind the current bathymetry texture: */
	textureTracker.reset();
	textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->bathymetry.textureObjects[dataItem->bathymetry.current]);
	
	/* Read the texture image into the given buffer: */
	glGetTexImage(GL_TEXTURE_RECTANGLE_ARB,0,GL_RED,GL_FLOAT,buffer);
	}

void WaterTable2::readSnowTexture(GLContextData& contextData,TextureTracker& textureTracker,GLfloat* buffer) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Bind the current snow height texture: */
	textureTracker.reset();
	textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->snow.textureObjects[dataItem->snow.current]);
	
	/* Read the texture image into the given buffer: */
	glGetTexImage(GL_TEXTURE_RECTANGLE_ARB,0,GL_RED,GL_FLOAT,buffer);
	}

void WaterTable2::readQuantityTexture(GLContextData& contextData,TextureTracker& textureTracker,GLenum components,GLfloat* buffer) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Bind the current conserved quantities texture: */
	textureTracker.reset();
	textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->quantity.textureObjects[dataItem->quantity.current]);
	
	/* Read the requested components of the texture image into the given buffer: */
	glGetTexImage(GL_TEXTURE_RECTANGLE_ARB,0,components,GL_FLOAT,buffer);
	}
