#pragma once

#ifdef OPTIXPLUGIN_EXPORTS  
#define OPTIXPLUGIN_API __declspec(dllexport)   
#else  
#define OPTIXPLUGIN_API __declspec(dllimport)   
#endif  

using namespace optix::prime;

// -------------------------------------------------------------------------------------
// Optix Data Structures
// -------------------------------------------------------------------------------------

class OptixSensorBase
{
public:
	optix::Matrix4x4 localToWorldTranslationMatrix;

	float sensorDepth;
	float sensorHeight;
	float sensorRadius;

	float pointGap;
	float totalPoints;
};

extern "C"
{
	void GenerateRaysOnDevice(float4* rays, OptixSensorBase sensor, int startIndex);
}

// -------------------------------------------------------------------------------------
// Optix Variables
// -------------------------------------------------------------------------------------

typedef intptr_t ItemListHandle;

static RTPcontexttype contextType = RTP_CONTEXT_TYPE_CPU;
static RTPbuffertype bufferType = RTP_BUFFER_TYPE_HOST;

static Context context;

static std::vector<Model> objectModels;
static std::vector<SimpleMatrix4x3> objectTranslationMatrices;
static std::vector<int> objectEnabledStatus;
static int totalObjects;

static std::vector<Ray> baseRays;
static Buffer<Ray> raysBuffer(0, bufferType, LOCKED);

// -------------------------------------------------------------------------------------
// Init Optix Functions
// -------------------------------------------------------------------------------------

extern "C" OPTIXPLUGIN_API void SetAllObjectsFromUnity
(
	int totalTrans, int* meshVertexCounts, float** meshVerticies, optix::Matrix4x4* transformationMatrices, int* transformEnabled
);

extern "C" OPTIXPLUGIN_API void SetAllSensorsFromUnity
(
	int sensorCount, OptixSensorBase* sensors
);

// -------------------------------------------------------------------------------------
// Optix Update Functions
// -------------------------------------------------------------------------------------

extern "C" OPTIXPLUGIN_API void TranslateAllSensorsFromUnity
(
	int sensorCount, OptixSensorBase* sensors
);

extern "C" OPTIXPLUGIN_API void UpdateGameObjectEnabledFromUnity
(
	int transformCount, int* transformIndices, int* transformsEnabled
);


extern "C" OPTIXPLUGIN_API void UpdateGameObjectMatrixFromUnity
(
	int matrixCount, int* matrixIndex, optix::Matrix4x4* transformationMatrices
);

// -------------------------------------------------------------------------------------
// Optix Execute Functions
// -------------------------------------------------------------------------------------

extern "C" OPTIXPLUGIN_API bool SensorFireAndReturnHitPositions
(
	ItemListHandle* hItems, float3** itemsFound, int* itemCount
);

extern "C" OPTIXPLUGIN_API bool SensorFireAndReturnHitCount
(
	int* itemCount, OptixSensorBase* sensors, int sensorCount
);

extern "C" OPTIXPLUGIN_API bool CheckSingleRayHit
(
	float3 origin, float3 direction, float depth
);

extern "C" OPTIXPLUGIN_API float3 ReturnSingleRayHit
(
	float3 origin, float3 direction, float depth
);

// -------------------------------------------------------------------------------------
// Optix Cleanup Functions
// -------------------------------------------------------------------------------------

extern "C" OPTIXPLUGIN_API bool ReleaseItems(ItemListHandle hItems);

// -------------------------------------------------------------------------------------
// Helper Functions
// -------------------------------------------------------------------------------------

void CreateInstances
(
	std::vector<RTPmodel>& instances, std::vector<SimpleMatrix4x3>& matrixes
);

void CreateRaysFromSensorBounds
(
	OptixSensorBase* sensors, int sensorCount
);

void TranslateRays
(
	OptixSensorBase* sensors, int sensorCount
);

SimpleMatrix4x3 GetSimpleMatrixFromUnityMatrix
(
	const optix::Matrix4x4 M
);