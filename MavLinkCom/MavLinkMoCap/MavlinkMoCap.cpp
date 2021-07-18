// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// MavlinkMoCap.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <chrono>
#include <thread>
#include "NPTrackingTools.h"
#include "MavLinkConnection.hpp"
#include "MavLinkVehicle.hpp"
#include "MavLinkMessages.hpp"

using namespace mavlinkcom;

void CheckResult(NPRESULT result)
{
    if (result != NPRESULT_SUCCESS) {
        // Treat all errors as failure conditions.
        printf("Error: %s\n", TT_GetResultString(result));
        exit(1);
    }
}

void mavlink_euler_to_quaternion(float roll, float pitch, float yaw, float quaternion[4])
{
    float cosPhi_2 = cosf(roll / 2);
    float sinPhi_2 = sinf(roll / 2);
    float cosTheta_2 = cosf(pitch / 2);
    float sinTheta_2 = sinf(pitch / 2);
    float cosPsi_2 = cosf(yaw / 2);
    float sinPsi_2 = sinf(yaw / 2);
    quaternion[0] = (cosPhi_2 * cosTheta_2 * cosPsi_2 +
                     sinPhi_2 * sinTheta_2 * sinPsi_2);
    quaternion[1] = (sinPhi_2 * cosTheta_2 * cosPsi_2 -
                     cosPhi_2 * sinTheta_2 * sinPsi_2);
    quaternion[2] = (cosPhi_2 * sinTheta_2 * cosPsi_2 +
                     sinPhi_2 * cosTheta_2 * sinPsi_2);
    quaternion[3] = (cosPhi_2 * cosTheta_2 * sinPsi_2 -
                     sinPhi_2 * sinTheta_2 * cosPsi_2);
}

class PortAddress
{
public:
    std::string addr;
    int port;
};

PortAddress endPoint = { "127.0.0.1", 14590 };
std::string bodyName = "Quadrocopter"; // expected name.
std::string project;

bool ParseCommandLine(int argc, char* argv[])
{

    // parse command line
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (arg[0] == '-' || arg[0] == '/') {
            std::string option(arg + 1);
            std::vector<std::string> parts = Utils::split(parts, ":,", 2);
            if (lower == "server") {
                if (parts.size() > 1) {
                    endPoint.addr = parts[1];
                    if (parts.size() > 2) {
                        endPoint.port = atoi(parts[2].c_str());
                    }
                }
            }
            else if (lower == "body") {
                if (parts.size() > 1) {
                    bodyName = parts[1];
                }
            }
            else if (lower == "project") {
                if (parts.size() > 1) {
                    project = parts[1];
                    if (parts.size() > 2) {
                        project += ":";
                        project += parts[2];
                    }
                }
            }
            else if (lower == "?" || lower == "h" || lower == "help") {
                return false;
            }
            else {
                printf("### Error: unexpected argument: %s\n", arg);
                return false;
            }
        }
    }
    return true;
}

void PrintUsage()
{
    printf("Usage: MavLinkMoCap options\n");
    printf("Connects PX4 to ATT_POS_MOCAP messages from OptiTrack system.\n");
    printf("Using a project you created already using OptiTrack Motive\n");
    printf("Options: \n");
    printf("    -project:d:/path/to/motive/project.ttp\n");
    printf("    -server:ipaddr[:port]]     - connect to drone via this udp address (default 127.0.0.1:14590)\n");
    printf("    -body:name                 - specify name of rigid body to track (default 'Quadrocopter')\n");
}

int main(int argc, char* argv[])
{
    int rc = 0;

    if (!ParseCommandLine(argc, argv)) {
        PrintUsage();
        return 1;
    }
    if (project == "") {
        printf("error: please specify the Motive project to load.\n");
        PrintUsage();
        return 1;
    }

    // motive gives a weird error if the project is not found, so we look for it.
    FILE* ptr = fopen(project.c_str(), "rb");
    if (ptr == nullptr) {
        int rc = errno;
        printf("error: cannot open project file '%s', rc=%d\n", project.c_str(), rc);
        PrintUsage();
        return 1;
    }
    fclose(ptr);

    printf("Initializing NaturalPoint Devices\n");
    TT_Initialize();

    // Do an update to pick up any recently-arrived cameras.
    TT_Update();

    printf("Loading Project: %s\n", project.c_str());
    CheckResult(TT_LoadProject(project.c_str()));

    // List all detected cameras.
    printf("Cameras:\n");
    for (int i = 0; i < TT_CameraCount(); i++) {
        printf("\t%s\n", TT_CameraName(i));
    }
    printf("\n");

    // List all defined rigid bodies.
    printf("Rigid Bodies:\n");
    for (int i = 0; i < TT_RigidBodyCount(); i++) {
        printf("\t%s\n", TT_RigidBodyName(i));
    }
    printf("\n");

    printf("Starting MOCAP server at %s:%d\n", endPoint.addr.c_str(), endPoint.port);

    std::shared_ptr<MavLinkConnection> proxyConnection = MavLinkConnection::connectLocalUdp("optitrack", endPoint.addr, endPoint.port);

    auto start = std::chrono::system_clock::now();

    int body = -1;
    printf("Looking for Motive RigidBody named '%s'\n", bodyName.c_str());

    while (true) {

        if (TT_Update() == NPRESULT_SUCCESS) {
            float yaw, pitch, roll;
            float x, y, z;
            float qx, qy, qz, qw;

            if (body == -1) {
                for (int i = 0; i < TT_RigidBodyCount(); i++) {
                    const char* found = TT_RigidBodyName(i);
                    if (strcmp(found, bodyName.c_str()) == 0) {
                        printf("Found '%s'\n", bodyName.c_str());
                        printf("Waiting for body tracking data...\n");
                        body = i;
                        break;
                    }
                }
            }
            if (body >= 0) {
                TT_RigidBodyLocation(body, &x, &y, &z, &qx, &qy, &qz, &qw, &yaw, &pitch, &roll);

                if (TT_IsRigidBodyTracked(body)) {
                    auto now = std::chrono::system_clock::now();
                    auto duration = now - start;
                    // throttle to 50 messages per second.
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() > 20) {
                        if (x < -10 || x > 10) {
                            printf("X out of range: %f\n", x);
                        }
                        else if (y < -10 || y > 10) {
                            printf("Y out of range: %f\n", y);
                        }
                        else if (z < -10 || z > 10) {
                            printf("Z out of range: %f\n", z);
                        }
                        else {
                            printf("Pos (%.3f, %.3f, %.3f) Orient (%.1f, %.1f, %.1f)\n",
                                   x,
                                   y,
                                   z,
                                   yaw,
                                   pitch,
                                   roll);

                            MavLinkAttPosMocap pos;
                            // OptiTrack uses 'y' axis for vertical.
                            pos.x = x;
                            pos.y = z;
                            pos.z = -y; // convert to NED coordinates.
                            pos.time_usec = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
                            pos.compid = 1;
                            pos.sysid = 166;
                            mavlink_euler_to_quaternion(roll, pitch, yaw, pos.q);
                            proxyConnection->sendMessage(pos);
                            start = now;
                        }
                    }
                }
            }
        }
    }

    return 0;
}
