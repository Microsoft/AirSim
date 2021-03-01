//#pragma once

#include <thread>
#include "UnityUtilities.hpp"
#include "SimHUD/SimHUD.h"
#include "Logger.h"

#ifdef _WIN32
	#define EXPORT __declspec(dllexport)
#else
	#define EXPORT __attribute__((visibility("default")))
#endif

static SimHUD* key = nullptr;

void StartServerThread(std::string sim_mode_name, int port_number);

extern "C" EXPORT bool StartServer(char* sim_mode_name, int port_number)
{
	LOGGER->WriteLog("Starting server for : " + std::string(sim_mode_name));
	std::thread server_thread(StartServerThread, sim_mode_name, port_number);
	server_thread.detach();
	int waitCounter = 25; // waiting for maximum 5 seconds to start a server.
	while ((key == nullptr || !key->server_started_Successfully_) && waitCounter > 0)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		waitCounter--;
	}
	return key->server_started_Successfully_;
}

extern "C" EXPORT void StopServer()
{
	key->EndPlay();
	if (key != nullptr)
	{
		delete key;
		key = nullptr;
	}
		LOGGER->WriteLog("Server stopped");
}

extern "C" EXPORT void CallTick(float deltaSeconds)
{
	key->Tick(deltaSeconds);
}

extern "C" EXPORT void InvokeCollisionDetection(char* vehicle_name, AirSimUnity::AirSimCollisionInfo collision_info)
{
	auto simMode = key->GetSimMode();
	if (simMode)
	{
		auto vehicleApi = simMode->getVehicleSimApi(vehicle_name);
		if (vehicleApi)
		{
			msr::airlib::CollisionInfo collisionInfo = UnityUtilities::Convert_to_AirSimCollisioinInfo(collision_info);
			vehicleApi->OnCollision(collisionInfo);
		}
	}
}