#pragma once

#include "AirSimStructs.hpp"
#include "UnityImageCapture.h"
#include "vehicles/car/api/CarApiBase.hpp"


#ifdef _WIN32
	#define EXPORT __declspec(dllexport)
#elif defined(__linux__) || defined(__APPLE__)
	#define EXPORT __attribute__((visibility("default")))
#endif
/*
* Defines all the functions that can be called into Unity
*/

//Function pointers to hold the addresses of the functions that are defined in Unity
extern bool(*SetPose)(AirSimPose pose, bool ignoreCollision, const char* vehicleName);
extern AirSimPose(*GetPose)(const char* vehicleName);
extern AirSimCollisionInfo(*GetCollisionInfo)(const char* vehicleName);
extern AirSimRCData(*GetRCData)(const char* vehicleName);
extern AirSimImageResponse(*GetSimImages)(AirSimImageRequest request, const char* vehicleName);
extern bool(*SetRotorSpeed)(int rotorIndex, RotorInfo rotorInfo, const char* vehicleName);
extern bool(*SetEnableApi)(bool enableApi, const char* vehicleName);
extern bool(*SetCarApiControls)(msr::airlib::CarApiBase::CarControls controls, const char* vehicleName);
extern AirSimCarState(*GetCarState)(const char* vehicleName);
extern AirSimCameraInfo(*GetCameraInfo)(const char* cameraName, const char* vehicleName);
extern bool(*SetCameraPose)(const char* cameraName, AirSimPose pose, const char* vehicleName);
extern bool(*SetCameraFoV)(const char* cameraName, const float fov_degrees, const char* vehicleName);
extern bool(*SetCameraDistortionParam)(const char* cameraName, const char* paramName, const float value, const char* vehicleName);
extern bool(*GetCameraDistortionParams)(const char* cameraName, const char* vehicleName);
extern bool(*SetSegmentationObjectId)(const char* meshName, int objectId, bool isNameRegex);
extern int(*GetSegmentationObjectId)(const char* meshName);
extern bool(*PrintLogMessage) (const char* message, const char* messageParam, const char* vehicleName, int severity);
extern UnityTransform(*GetTransformFromUnity)(const char* vehicleName);
extern bool(*Reset)();
extern AirSimVector(*GetVelocity)(const char* vehicleName);
extern RayCastHitResult(*GetRayCastHit)(AirSimVector startVec, AirSimVector endVec, const char* vehicleName);
extern bool(*Pause)(float timeScale);

// PInvoke call to initialize the function pointers. This function is called from Unity.

extern "C" EXPORT void InitVehicleManager(
	bool(*setPose)(AirSimPose pose, bool ignoreCollision, const char* vehicleName),
	AirSimPose(*getPose)(const char* vehicleName),
	AirSimCollisionInfo(*getCollisionInfo)(const char* vehicleName),
	AirSimRCData(*getDroneRCData)(const char* vehicleName),
	AirSimImageResponse(*getSimImages)(AirSimImageRequest request, const char* vehicleName),
	bool(*setRotorSpeed)(int rotorIndex, RotorInfo rotorInfo, const char* vehicleName),
	bool(*setEnableApi)(bool enableApi, const char* vehicleName),
	bool(*setCarApiControls)(msr::airlib::CarApiBase::CarControls controls, const char* vehicleName),
	AirSimCarState(*getCarState)(const char* vehicleName),
	AirSimCameraInfo(*getCameraInfo)(const char* cameraName, const char* vehicleName),
	bool(*setCameraPose)(const char* cameraName, AirSimPose pose, const char* vehicleName),
	bool(*setCameraFoV)(const char* cameraName, const float fov_degrees, const char* vehicleName),
	bool(*setDistortionParam)(const char* cameraName, const char* paramName, const float value, const char* vehicleName),
	bool(*getDistortionParams)(const char* cameraName, const char* vehicleName),
	bool(*setSegmentationObjectId)(const char* meshName, int objectId, bool isNameRegex),
	int(*getSegmentationObjectId)(const char* meshName),
	bool(*printLogMessage) (const char* message, const char* messageParam, const char* vehicleName, int severity),
	UnityTransform(*getTransformFromUnity)(const char* vehicleName),
	bool(*reset)(),
	AirSimVector(*getVelocity)(const char* vehicleName),
	RayCastHitResult(*getRayCastHit)(AirSimVector startVec, AirSimVector endVec, const char* vehicleName),
	bool(*pause)(float timeScale)
);
