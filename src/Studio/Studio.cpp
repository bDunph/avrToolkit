#include "Studio.hpp"

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <assert.h>
#include <math.h>
#include <cmath>
#include <iostream>
#include <random>

#include "stb_image.h"

#ifdef __APPLE__ 
#include "GLFW/glfw3.h"
#define TRAINING_SET_SIZE trainingData.trainingSet.size()
#elif _WIN32 
#include "glfw3.h"
#define TRAINING_SET_SIZE trainingSet.size()
#endif

#include "SystemInfo.hpp"
#include "ShaderManager.hpp"

#define PI 3.14159265359

#ifndef _countof
#define _countof(x) (sizeof(x)/sizeof((x)[0]))
#endif

//*******************************************************************************************
// Setup 
//*******************************************************************************************
bool Studio::setup(std::string csd, GLuint shaderProg)
{
	// bool to indicate first loop through update and draw functions to 
	// set initial paramaters
	m_bFirstLoop = true;

//*****************************Audio Setup********************************************************

	CsoundSession* csSession = PCsoundSetup(csd);
	
//********* send values from avr to csound *******************//

	if(!BSoundSourceSetup(csSession, NUM_SOUND_SOURCES))
	{
		std::cout << "Studio::setup sound sources not set up" << std::endl;
		return false;
	}

	const char* sineVal = "sineControlVal";
	if(session->GetChannelPtr(m_cspSineControlVal, sineVal, CSOUND_INPUT_CHANNEL | CSOUND_CONTROL_CHANNEL) != 0)
	{
		std::cout << "GetChannelPtr could not get the sineControlVal value" << std::endl;
		return false;
	}


//********* get return values from csound *******************//

	// example return value - RMS
	//m_fPrevRms = 0.0f;
	//const char* rmsOut = "rmsOut";
	//if(session->GetChannelPtr(m_pRmsOut, rmsOut, CSOUND_OUTPUT_CHANNEL | CSOUND_CONTROL_CHANNEL) != 0)
	//{
	//	std::cout << "Csound output value rmsOut not available" << std::endl;
	//	return false;
	//}
	
//************************************************************************************************

//*************************************Visual Setup***********************************************

	// Set up quad to use for raymarching
	RaymarchQuadSetup(shaderProg);
	
	// shader uniforms
	m_gliSineControlValLoc = glGetUniformLocation(shaderProg, "sineControlVal");
	
	modelMatrix = glm::mat4(1.0f);

//************************************************************************************************
	return true;
}
//*******************************************************************************************


//*******************************************************************************************
// Update 
//*******************************************************************************************
void Studio::update(glm::mat4 viewMat, glm::vec3 camPos, MachineLearning& machineLearning, glm::vec3 controllerWorldPos_0, glm::vec3 controllerWorldPos_1, glm::quat controllerQuat_0, glm::quat controllerQuat_1, PBOInfo& pboInfo, glm::vec3 translateVec){

	// For return values from shader
	// vec4 for each fragment is returned in the order ABGR
	// you have to wait until the 2nd frame to read from the buffer 
	//if(!m_bFirstLoop)
	//std::cout << (double)pboInfo.pboPtr[0] << std::endl;

	m_vec3Translation = translateVec;
	
	glm::vec4 viewerPosCameraSpace = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

	// example sound source at origin
	SoundSourceData soundSource1;
	soundSource1.position = viewerPosCameraSpace;
	m_vSoundSources.push_back(soundSource1);

	SoundSourceUpdate(m_vSoundSources, viewMat);

	//example control signal - sine function
	//sent to shader and csound
	sineControlVal = sin(glfwGetTime() * 0.33f);

	*m_cspSineControlVal = (MYFLT)sineControlVal;
}
//*********************************************************************************************


//*********************************************************************************************
// Draw 
//*********************************************************************************************
void Studio::draw(glm::mat4 projMat, glm::mat4 viewMat, glm::mat4 eyeMat, RaymarchData& raymarchData, GLuint mengerProg)
{
	DrawStart(projMat, eyeMat, viewMat, mengerProg);
	
	glUniform1f(m_gliSineControlValLoc, sineControlVal);

	DrawEnd();

	// update first loop switch
	m_bFirstLoop = false;
}
//*********************************************************************************************

















//*********************************************************************************************
//*********************************************************************************************
//*********************************************************************************************
CsoundSession* Studio::PCsoundSetup(std::string _csdName)
{
	std::string csdName = "";
	if(!_csdName.empty()) csdName = _csdName;
	session = new CsoundSession(csdName);

#ifdef _WIN32
	session->SetOption("-b -32"); 
	session->SetOption("-B 2048");
#endif
	session->StartThread();
	session->PlayScore();

	return session;
}

bool Studio::BSoundSourceSetup(CsoundSession* _session, int numSources)
{

	for(int i = 0; i < NUM_SOUND_SOURCES; i++)
	{
		std::string val1 = "azimuth" + std::to_string(i);
		const char* azimuth = val1.c_str();	
		if(session->GetChannelPtr(azimuthVals[i], azimuth, CSOUND_INPUT_CHANNEL | CSOUND_CONTROL_CHANNEL) != 0)
		{
			std::cout << "GetChannelPtr could not get the azimuth" << i << " input" << std::endl;
			return false;
		}

		std::string val2 = "elevation" + std::to_string(i);
		const char* elevation = val2.c_str();
		if(session->GetChannelPtr(elevationVals[i], elevation, CSOUND_INPUT_CHANNEL | CSOUND_CONTROL_CHANNEL) != 0)
		{
			std::cout << "GetChannelPtr could not get the elevation" << i << " input" << std::endl;
			return false;
		}	

		std::string val3 = "distance" + std::to_string(i);
		const char* distance = val3.c_str();
		if(session->GetChannelPtr(distanceVals[i], distance, CSOUND_INPUT_CHANNEL | CSOUND_CONTROL_CHANNEL) != 0)
		{
			std::cout << "GetChannelPtr could not get the distance" << i << " input" << std::endl;
			return false;
		}
	}

	return true;
}

void Studio::SoundSourceUpdate(std::vector<SoundSourceData>& soundSources, glm::mat4 _viewMat)
{
	for(int i = 0; i < soundSources.size(); i++)
	{
		// camera space positions
		soundSources[i].posCamSpace = _viewMat * modelMatrix * soundSources[i].position;

		// distance value
		soundSources[i].distCamSpace = sqrt(pow(soundSources[i].posCamSpace.x, 2) + pow(soundSources[i].posCamSpace.y, 2) + pow(soundSources[i].posCamSpace.z, 2));

		//azimuth in camera space
		float valX = soundSources[i].posCamSpace.x - soundSources[i].position.x;
		float valZ = soundSources[i].posCamSpace.z - soundSources[i].position.z;

		soundSources[i].azimuth = atan2(valX, valZ);
		soundSources[i].azimuth *= (180.0f/PI); 	

		//elevation in camera space
		float oppSide = soundSources[i].posCamSpace.y - soundSources[i].position.y;
		float sinVal = oppSide / soundSources[i].distCamSpace;
		soundSources[i].elevation = asin(sinVal);
		soundSources[i].elevation *= (180.0f/PI);		

		//send values to Csound pointers
		*azimuthVals[i] = (MYFLT)soundSources[i].azimuth;
		*elevationVals[i] = (MYFLT)soundSources[i].elevation;
		*distanceVals[i] = (MYFLT)soundSources[i].distCamSpace;
	}
	
	soundSources.clear();

}

void Studio::RaymarchQuadSetup(GLuint _shaderProg)
{
	float sceneVerts[] = {
		-1.0f, 1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		1.0f, 1.0f, 0.0f
	};
	m_uiNumSceneVerts = _countof(sceneVerts);

	unsigned int sceneIndices[] = {
		0, 1, 2,
		2, 3, 0
	};
	m_uiNumSceneIndices = _countof(sceneIndices);

	float groundRayTexCoords [] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
	};
	m_uiNumSceneTexCoords = _countof(groundRayTexCoords);	

	glGenVertexArrays(1, &m_uiglSceneVAO);

	glBindVertexArray(m_uiglSceneVAO);

	GLuint m_uiglSceneVBO;
	glGenBuffers(1, &m_uiglSceneVBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_uiglSceneVBO);
	glBufferData(GL_ARRAY_BUFFER, m_uiNumSceneVerts * sizeof(float), sceneVerts, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	GLuint m_uiglGroundTexCoords;
	glGenBuffers(1, &m_uiglGroundTexCoords);
	glBindBuffer(GL_ARRAY_BUFFER, m_uiglGroundTexCoords);
	glBufferData(GL_ARRAY_BUFFER, m_uiNumSceneTexCoords * sizeof(float), groundRayTexCoords, GL_STATIC_DRAW);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL); 

	glGenBuffers(1, &m_uiglIndexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_uiglIndexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_uiNumSceneIndices * sizeof(unsigned int), sceneIndices, GL_STATIC_DRAW);
	
	
	glBindVertexArray(0);
	glDisableVertexAttribArray(0);

	m_gliMVEPMatrixLocation = glGetUniformLocation(_shaderProg, "MVEPMat");
	m_gliInverseMVEPLocation = glGetUniformLocation(_shaderProg, "InvMVEP");
}

void Studio::DrawStart(glm::mat4 _projMat, glm::mat4 _eyeMat, glm::mat4 _viewMat, GLuint _shaderProg)
{
	modelMatrix = glm::translate(modelMatrix, m_vec3Translation);

	//matrices for raymarch shaders
	modelViewEyeProjectionMat = _projMat * _eyeMat * _viewMat * modelMatrix;
	inverseMVEPMat = glm::inverse(modelViewEyeProjectionMat);

	glBindVertexArray(m_uiglSceneVAO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_uiglIndexBuffer);
	glUseProgram(_shaderProg);

	glUniformMatrix4fv(m_gliMVEPMatrixLocation, 1, GL_FALSE, &modelViewEyeProjectionMat[0][0]);
	glUniformMatrix4fv(m_gliInverseMVEPLocation, 1, GL_FALSE, &inverseMVEPMat[0][0]);
}

void Studio::DrawEnd()
{
	glDrawElements(GL_TRIANGLES, m_uiNumSceneIndices * sizeof(unsigned int), GL_UNSIGNED_INT, (void*)0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void Studio::exit(){
	//stop csound
	session->StopPerformance();
	//close GL context and any other GL resources
	glfwTerminate();
}
