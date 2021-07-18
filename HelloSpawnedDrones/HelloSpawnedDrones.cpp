// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "common/common_utils/StrictMode.hpp"
STRICT_MODE_OFF
#ifndef RPCLIB_MSGPACK
#define RPCLIB_MSGPACK clmdep_msgpack
#endif // !RPCLIB_MSGPACK
#include "rpc/rpc_error.h"
STRICT_MODE_ON

#include "vehicles/multirotor/api/MultirotorRpcLibClient.hpp"
#include "common/common_utils/FileSystem.hpp"
#include <iostream>
#include <chrono>
#include <cstdlib>
#include <thread>

void runSingleClient(uint16_t port, int ordinal)
{
    using namespace msr::airlib;
    const char host[] = "localhost";
    float timeout_s = 60;

    try {
        MultirotorRpcLibClient* client = new MultirotorRpcLibClient(host, port, timeout_s);
        std::cout << "Confirming connections..." << std::endl;
        client->confirmConnection();

        std::string vehicle_name = "UAV_" + std::to_string(ordinal);
        std::cout << "Vehicle name:" << vehicle_name << std::endl;

        Pose pose(Vector3r(0, 5.0f * (ordinal + 1), 0), Quaternionr(0, 0, 0, 0));
        client->simAddVehicle(vehicle_name, "simpleflight", pose, "");

        // This is a bit crude, but give it a moment to settle on the ground, else takeoff will fail
        std::this_thread::sleep_for(std::chrono::duration<double>(2));

        // moveByVelocityZ is an offboard operation, so we need to set offboard mode.
        client->enableApiControl(true, vehicle_name);
        client->armDisarm(true, vehicle_name);

        auto ground_pos = client->getMultirotorState(vehicle_name).getPosition();
        float groundZ = ground_pos.z(); // current position (NED coordinate system).

        float takeoff_timeout = 5;
        std::cout << "Initiating takeoff for " << vehicle_name << "..." << std::endl;
        client->takeoffAsync(takeoff_timeout, vehicle_name)->waitOnLastTask();
        std::cout << "Completed takeoff for " << vehicle_name << "..." << std::endl;

        const float speed = 3.0f;

        // switch to explicit hover mode so that this is the fallback when
        // move* commands are finished.
        std::cout << "Initiating hover for " << vehicle_name << "..." << std::endl;
        client->hoverAsync(vehicle_name)->waitOnLastTask();
        std::cout << "Completed hover for " << vehicle_name << "..." << std::endl;

        auto position = client->getMultirotorState(vehicle_name).getPosition();
        float duration = 1;
        float z = position.z(); // current position (NED coordinate system).

        // Altitude difference between each platform, in meters
        const float altitude_delta = 1.0f;

        z -= ordinal * altitude_delta;
        float timeout = 10.0f;
        client->moveToZAsync(z, speed, timeout, YawMode(), -1.0f, 1.0f, vehicle_name)->waitOnLastTask();

        std::cout << "Completed move to z " << z << " for " << vehicle_name << "..." << std::endl;
        std::cout << "Flying in a 10m box pattern at 3 m/s velocity" << std::endl;

        const float size = 5.0f;
        duration = size / speed;
        DrivetrainType drivetrain = DrivetrainType::ForwardOnly;
        YawMode yaw_mode(true, 0);

        position = client->getMultirotorState(vehicle_name).getPosition();
        std::cout << "Position of " << port << ": " << position << std::endl;
        z = position.z(); // current position (NED coordinate system).

        std::cout << "moveByVelocityZ(" << speed << ", 0, " << z << "," << duration << ")" << std::endl;
        client->moveByVelocityZAsync(speed, 0, z, duration, drivetrain, yaw_mode, vehicle_name);
        std::this_thread::sleep_for(std::chrono::duration<double>(duration));
        std::cout << "moveByVelocityZ(0, " << speed << "," << z << "," << duration << ")" << std::endl;
        client->moveByVelocityZAsync(0, speed, z, duration, drivetrain, yaw_mode, vehicle_name);
        std::this_thread::sleep_for(std::chrono::duration<double>(duration));
        std::cout << "moveByVelocityZ(" << -speed << ", 0, " << z << "," << duration << ")" << std::endl;
        client->moveByVelocityZAsync(-speed, 0, z, duration, drivetrain, yaw_mode, vehicle_name);
        std::this_thread::sleep_for(std::chrono::duration<double>(duration));
        std::cout << "moveByVelocityZ(0, " << -speed << "," << z << "," << duration << ")" << std::endl;
        client->moveByVelocityZAsync(0, -speed, z, duration, drivetrain, yaw_mode, vehicle_name);

        std::this_thread::sleep_for(std::chrono::duration<double>(duration));

        client->moveToZAsync(groundZ - 0.5f, speed, timeout, YawMode(), -1.0f, 1.0f, vehicle_name)->waitOnLastTask();

        std::cout << "Hovering..." << std::endl;
        client->hoverAsync(vehicle_name)->waitOnLastTask();

        client->enableApiControl(true, vehicle_name);

        std::cout << "Landing..." << std::endl;
        client->landAsync(timeout, vehicle_name)->waitOnLastTask();
        std::this_thread::sleep_for(std::chrono::duration<double>(5));

        std::cout << "Disarming..." << std::endl;
        client->armDisarm(false, vehicle_name);

        std::cout << "Done!..." << std::endl;

        delete client;

        std::this_thread::sleep_for(std::chrono::duration<double>(50));
    }
    catch (rpc::rpc_error& e) {
        std::string msg = e.get_error().as<std::string>();
        std::cout << "Exception raised by the API, something went wrong." << std::endl
                  << msg << std::endl;
    }
}

int main(int argc, char* argv[])
{
    using namespace msr::airlib;

    uint16_t rpc_port = 41451;
    int num_platforms = 1;

    std::cout << "argc is " << argc << std::endl;
    if (argc > 1) {
        std::cout << "Num plats string: " << argv[1] << std::endl;
        num_platforms = static_cast<uint16_t>(std::stoi(argv[1]));
    }

    std::cout << "First port is " << rpc_port << std::endl;
    std::cout << "Num platforms: " << num_platforms << std::endl;
    std::cout << "Making clients..." << std::endl;

    try {
        std::cout << "Press Enter to begin..." << std::endl;
        std::cin.get();

        std::vector<std::thread> clientThreads;

        // Count down, so the first one can easily go the highest (without knowing count)
        int client_ordinal = num_platforms - 1;
        for (int i = 0; i < num_platforms; i++) {
            clientThreads.push_back(std::thread(runSingleClient, rpc_port, client_ordinal));
            client_ordinal--;

            std::this_thread::sleep_for(std::chrono::duration<double>(0.1));
        }

        for (auto& toJoin : clientThreads) {
            toJoin.join();
        }
    }
    catch (rpc::rpc_error& e) {
        std::string msg = e.get_error().as<std::string>();
        std::cout << "Exception raised by the API, something went wrong." << std::endl
                  << msg << std::endl;
    }

    return 0;
}
