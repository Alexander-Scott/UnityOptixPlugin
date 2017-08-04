/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 //-----------------------------------------------------------------------------
 //
 //  Minimal OptiX Prime C++ wrapper usage demonstration  
 //
 //-----------------------------------------------------------------------------

#include "primeCommon.h" 
#include <optix_prime/optix_primepp.h>
#include <optixu/optixu_math_namespace.h>
#include <optixu/optixu_matrix_namespace.h> 

#include <sutil.h>
#include <d3d11.h>
#include <assert.h>

#include "OptixPlugin.h"

#include "PlatformBase.h"
#include "RenderAPI.h"

using namespace optix::prime;

// External Optix Functions
// -------------------------------------------------------------------------------------

// Sets the models and matrices of each object from Unity for use in Optix later. Called on application initalisation.
OPTIXPLUGIN_API void SetAllObjectsFromUnity(int totalTrans, int* meshVertexCounts, float** meshVerticies, optix::Matrix4x4* transformationMatrices, int* transformEnabled)
{
	contextType = RTP_CONTEXT_TYPE_CPU;
	bufferType = RTP_BUFFER_TYPE_HOST;

	context = Context::create(contextType); 

	totalObjects = totalTrans; 
	 
	objectModels.resize(totalObjects);
	objectTranslationMatrices.resize(totalObjects);
	objectEnabledStatus.resize(totalObjects);

	for (int i = 0; i < totalObjects; ++i) // For every mesh in meshVerticies
	{
		// Create a float3 formatted buffer (each triplet of floats in the array is now a vector3 in the order x,y,z)
		BufferDesc bufferDesc = context->createBufferDesc(RTP_BUFFER_FORMAT_VERTEX_FLOAT3, RTP_BUFFER_TYPE_HOST, meshVerticies[i]);
		bufferDesc->setRange(0, meshVertexCounts[i]);

		// Create an Optix Prime Model from the float3's
		objectModels[i] = context->createModel();
		objectModels[i]->setTriangles(bufferDesc);
		objectModels[i]->update(0);

		// Convert the Matrix4x4 translation matrix into the format required by Optix Prime (Matrix4x3) and then store it
		objectTranslationMatrices[i] = getSimpleMatrixFromUnityMatrix(transformationMatrices[i]);

		objectEnabledStatus[i] = transformEnabled[i];
	}
}

OPTIXPLUGIN_API void SetAllSensorsFromUnity(int sensorCount, OptixSensorBase* sensors)
{
	raysBuffer.free();
	
	CreateRaysFromSensorBounds(raysBuffer, sensors, sensorCount);
}

// Fires rays at every point in a sensors view bounds and then returns the hit position back to Unity
OPTIXPLUGIN_API bool SensorFireAndReturnHitPositions(ItemListHandle * hItems, float3 ** itemsFound, int * itemCount)
{
	std::vector<RTPmodel> instances;
	std::vector<SimpleMatrix4x3> matrixes;
	CreateInstances(instances, matrixes);

	if (instances.size() == 0)
		return true;

	// Create our scene from the models and their translation matrices
	Model scene = context->createModel();
	scene->setInstances(instances.size(), RTP_BUFFER_TYPE_HOST, &instances[0],
		RTP_BUFFER_FORMAT_TRANSFORM_FLOAT4x3, RTP_BUFFER_TYPE_HOST, &matrixes[0]);
	scene->update(0);

	Buffer<Hit> hitsBuffer(raysBuffer.count(), bufferType, LOCKED); // Buffer that will hold all the hit positions (if there are any)

	Query query = scene->createQuery(RTP_QUERY_TYPE_CLOSEST); // Query type is set to closest as all we want to know is if and where a intersection occurs
	query->setRays(raysBuffer.count(), Ray::format, raysBuffer.type(), raysBuffer.ptr());
	query->setHits(hitsBuffer.count(), Hit::format, hitsBuffer.type(), hitsBuffer.ptr());
	query->execute(0);

	auto hitPositions = new std::vector<float3>; // Allocate these on the heap as they will be returned to Unity and deleted later
	const Hit* hits = hitsBuffer.hostPtr();
	const Ray* rays = raysBuffer.hostPtr();

	for (int iHit = 0; iHit < hitsBuffer.count(); iHit++)
	{
		// hit.t is the distance from the origin that the ray intersected with something. Anything above 0 is an intersection
		if (hits[iHit].t > 0.0f)
		{
			float3 hitPoint = rays[iHit].origin + rays[iHit].dir * hits[iHit].t; // Calculate the actual intersection point using the ray that caused the hit and the distance along the ray
			hitPositions->push_back(hitPoint); // Add to the vector to be returned to Unity
		}
	}

	*hItems = reinterpret_cast<ItemListHandle>(hitPositions); // Cast this to a handle so it's easier for us to manage
	*itemsFound = hitPositions->data();
	*itemCount = hitPositions->size();

	return true;
}

OPTIXPLUGIN_API bool SensorFireAndReturnHitCount(int * itemCount)
{
	std::vector<RTPmodel> instances;
	std::vector<SimpleMatrix4x3> matrixes;
	CreateInstances(instances, matrixes);

	if (instances.size() == 0)
		return true;

	// Create our scene from the models and their translation matrices
	Model scene = context->createModel();
	scene->setInstances(instances.size(), RTP_BUFFER_TYPE_HOST, &instances[0],
		RTP_BUFFER_FORMAT_TRANSFORM_FLOAT4x3, RTP_BUFFER_TYPE_HOST, &matrixes[0]);
	scene->update(0);

	Buffer<Hit> hitsBuffer(raysBuffer.count(), bufferType, LOCKED); // Buffer that will hold all the hit positions (if there are any)

	Query query = scene->createQuery(RTP_QUERY_TYPE_CLOSEST); // Query type is set to closest as all we want to know is if and where a intersection occurs
	query->setRays(raysBuffer.count(), Ray::format, raysBuffer.type(), raysBuffer.ptr());
	query->setHits(hitsBuffer.count(), Hit::format, hitsBuffer.type(), hitsBuffer.ptr());
	query->execute(0);

	const Hit* hits = hitsBuffer.hostPtr();

	for (int iHit = 0; iHit < hitsBuffer.count(); iHit++)
	{
		// hit.t is the distance from the origin that the ray intersected with something. Anything above 0 is an intersection
		if (hits[iHit].t > 0.0f)
		{
			itemCount[0]++;
		}
	}

	return true;
}

OPTIXPLUGIN_API bool CheckSingleRayHit(float3 origin, float3 direction, float depth)
{
	std::vector<RTPmodel> instances;
	std::vector<SimpleMatrix4x3> matrixes;
	CreateInstances(instances, matrixes);

	if (instances.size() == 0)
		return false;

	// Create our scene from the models and their translation matrices
	Model scene = context->createModel();
	scene->setInstances(instances.size(), RTP_BUFFER_TYPE_HOST, &instances[0],
		RTP_BUFFER_FORMAT_TRANSFORM_FLOAT4x3, RTP_BUFFER_TYPE_HOST, &matrixes[0]);
	scene->update(0);

	Buffer<Ray> raysBuffer(0, bufferType, LOCKED); // Buffer that will hold the ray that Optix will fire
	raysBuffer.alloc(1);

	Ray r = { origin, 0.0f, direction, depth };
	raysBuffer.ptr()[0] = r;

	Buffer<Hit> hitsBuffer(raysBuffer.count(), bufferType, LOCKED); // Buffer that will hold all the hit positions (if there are any)

	Query query = scene->createQuery(RTP_QUERY_TYPE_CLOSEST); // Query type is set to closest as all we want to know is if and where a intersection occurs
	query->setRays(raysBuffer.count(), Ray::format, raysBuffer.type(), raysBuffer.ptr());
	query->setHits(hitsBuffer.count(), Hit::format, hitsBuffer.type(), hitsBuffer.ptr());
	query->execute(0);

	if (hitsBuffer.ptr()[0].t > 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

OPTIXPLUGIN_API float3 ReturnSingleRayHit(float3 origin, float3 direction, float depth)
{
	std::vector<RTPmodel> instances;
	std::vector<SimpleMatrix4x3> matrixes;
	CreateInstances(instances, matrixes);

	if (instances.size() == 0)
		return make_float3(0, 0, 0);

	// Create our scene from the models and their translation matrices
	Model scene = context->createModel();
	scene->setInstances(instances.size(), RTP_BUFFER_TYPE_HOST, &instances[0],
		RTP_BUFFER_FORMAT_TRANSFORM_FLOAT4x3, RTP_BUFFER_TYPE_HOST, &matrixes[0]);
	scene->update(0);

	Buffer<Ray> raysBuffer(0, bufferType, LOCKED); // Buffer that will hold the ray that Optix will fire
	raysBuffer.alloc(1);

	Ray r = { origin, 0.0f, direction, depth };
	raysBuffer.ptr()[0] = r;

	Buffer<Hit> hitsBuffer(raysBuffer.count(), bufferType, LOCKED); // Buffer that will hold all the hit positions (if there are any)

	Query query = scene->createQuery(RTP_QUERY_TYPE_CLOSEST); // Query type is set to closest as all we want to know is if and where a intersection occurs
	query->setRays(raysBuffer.count(), Ray::format, raysBuffer.type(), raysBuffer.ptr());
	query->setHits(hitsBuffer.count(), Hit::format, hitsBuffer.type(), hitsBuffer.ptr());
	query->execute(0);

	if (hitsBuffer.ptr()[0].t > 0)
	{
		return raysBuffer.ptr()[0].origin + raysBuffer.ptr()[0].dir * hitsBuffer.ptr()[0].t;
	}
	else
	{
		return make_float3(0, 0, 0);
	}
}

OPTIXPLUGIN_API void UpdateTransformEnabled(int transformCount, int* transformIndex, int* transformsEnabled)
{
	for (int i = 0; i < transformCount; ++i)
	{
		objectEnabledStatus[transformIndex[i]] = transformsEnabled[i];
	}
}

// Updates multiple model's translation matrices
OPTIXPLUGIN_API void UpdateMatrices(int matrixCount, int* matrixIndex, optix::Matrix4x4* transformationMatrices)
{
	for (int iMatrix = 0; iMatrix < matrixCount; iMatrix++)
	{
		objectTranslationMatrices[matrixIndex[iMatrix]] = getSimpleMatrixFromUnityMatrix(transformationMatrices[iMatrix]);
	}
}

// Deletes all the items allocated on the heap after they have finished being used by Unity
OPTIXPLUGIN_API bool ReleaseItems(ItemListHandle hItems)
{
	auto items = reinterpret_cast<std::vector<float3>*>(hItems);
	delete items;

	return true;
}

// Helper Functions
// -------------------------------------------------------------------------------------

void CreateInstances(std::vector<RTPmodel>& instances, std::vector<SimpleMatrix4x3>& matrixes)
{
	instances.reserve(totalObjects);
	matrixes.reserve(totalObjects);

	for (int i = 0; i < totalObjects; i++)
	{
		if (objectEnabledStatus[i == 1])
		{
			instances.push_back(objectModels[i]->getRTPmodel());
			matrixes.push_back(objectTranslationMatrices[i]);
		}
	}
}

// Generates rays across a sensor bounds based on certain parameters such as sensor radius, height and ray gap
void CreateRaysFromSensorBounds(Buffer<Ray>& raysBuffer, OptixSensorBase* sensors, int sensorCount)
{
	float rows = 0;
	float columns = 0;

	for (int iSensor = 0; iSensor < sensorCount; iSensor++)
	{
		rows += ceilf(sensors[iSensor].sensorHeight / sensors[iSensor].pointGap);
		columns += ceilf(sensors[iSensor].sensorRadius / sensors[iSensor].pointGap);
	}

	raysBuffer.alloc(columns * rows); // Calculate and allocate the number of rays we will be generating

	if (raysBuffer.type() == RTP_BUFFER_TYPE_HOST)
	{
		Ray* rays = raysBuffer.ptr();

		int idx = 0;
		for (int iSensor = 0; iSensor < sensorCount; iSensor++)
		{
			// For every row and column within the sensor bounds
			for (float iHeight = sensors[iSensor].sensorHeight / 2; iHeight > -sensors[iSensor].sensorHeight / 2; iHeight -= sensors[iSensor].pointGap)
			{
				for (float iRadius = -sensors[iSensor].sensorRadius / 2; iRadius < sensors[iSensor].sensorRadius / 2; iRadius += sensors[iSensor].pointGap, idx++)
				{
					// Calculate the centre point of the sensor bounds based on origin, direction and depth
					float3 centre = ((sensors[iSensor].sensorPosition + (sensors[iSensor].sensorDirection * sensors[iSensor].sensorDepth)) - sensors[iSensor].sensorPosition);

					// Create a direction vector from origin to the centre + the current height index value
					float4 targetDir = make_float4((sensors[iSensor].sensorPosition + centre + make_float3(0, iHeight, 0)) - sensors[iSensor].sensorPosition)
						* optix::Matrix4x4::rotate((iRadius * M_PI) / 180, make_float3(0, 1, 0)); // Multiply this vector by a rotation matrix to rotate the direction by radius index value in the y axis

					// Normalise the value and convert back to a float3 for the Ray structure
					float3 targetDirection = optix::normalize(make_float3(targetDir));

					Ray r = { sensors[iSensor].sensorPosition, 0.0f, targetDirection, sensors[iSensor].sensorDepth };

					if (idx < raysBuffer.count()) // This avoids an error where idx becomes larger than raysbuffer somehow?!?!?!?
					{
						rays[idx] = r;
					}
				}
			}
		}
	}
	else
	{
		// CUDA
	}
}

// This function is used to convert the Matrix4x4 from Unity to the format required by Optix Prime
SimpleMatrix4x3 getSimpleMatrixFromUnityMatrix(const optix::Matrix4x4 M)
{
	return SimpleMatrix4x3(
		M[0], M[4], M[8], M[12],
		M[1], M[5], M[9], M[13],
		M[2], M[6], M[10], M[14]
	);
}

// Unity Graphics
// -------------------------------------------------------------------------------------

static float g_Time;
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTimeFromUnity(float t) { g_Time = t; }

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

static RenderAPI* s_CurrentAPI = NULL;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	// Create graphics API implementation upon initialization
	if (eventType == kUnityGfxDeviceEventInitialize)
	{
		assert(s_CurrentAPI == NULL);
		s_DeviceType = s_Graphics->GetRenderer();
		s_CurrentAPI = CreateRenderAPI(s_DeviceType);
	}

	// Let the implementation process the device related events
	if (s_CurrentAPI)
	{
		s_CurrentAPI->ProcessDeviceEvent(eventType, s_UnityInterfaces);
	}

	// Cleanup graphics API implementation upon shutdown
	if (eventType == kUnityGfxDeviceEventShutdown)
	{
		delete s_CurrentAPI;
		s_CurrentAPI = NULL;
		s_DeviceType = kUnityGfxRendererNull;
	}
}

static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
	// Unknown / unsupported graphics device type? Do nothing
	if (s_CurrentAPI == NULL)
		return;

	// Use D3D11 buffer here
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return OnRenderEvent;
}