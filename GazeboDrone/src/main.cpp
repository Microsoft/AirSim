/*
 * Copyright (C) 2012 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <gazebo/transport/transport.hh>
#include <gazebo/msgs/msgs.hh>
#include <gazebo/gazebo_client.hh>
#include "common/common_utils/StrictMode.hpp"
STRICT_MODE_OFF
#ifndef RPCLIB_MSGPACK
#define RPCLIB_MSGPACK clmdep_msgpack
#endif // !RPCLIB_MSGPACK
#include "rpc/rpc_error.h"
STRICT_MODE_ON

#include "vehicles/multirotor/api/MultirotorRpcLibClient.hpp"
#include <chrono>

#include <iostream>

constexpr int NWIDTH = 7;
static constexpr int MESSAGE_THROTTLE = 100;

using namespace msr::airlib;

msr::airlib::MultirotorRpcLibClient client;

void cbLocalPose(ConstPosesStampedPtr& msg)
{
    std::cout << std::fixed;
    std::cout << std::setprecision(3);
    static int count = 0;
    for (int i = 0; i < msg->pose_size(); i++) {
        auto x = msg->pose(i).position().x();
        auto y = msg->pose(i).position().y();
        auto z = msg->pose(i).position().z();
        auto ow = msg->pose(i).orientation().w();
        auto ox = msg->pose(i).orientation().x();
        auto oy = msg->pose(i).orientation().y();
        auto oz = msg->pose(i).orientation().z();
        if (count % MESSAGE_THROTTLE == 0) {
            std::cout << "local (" << std::setw(2) << i << ") ";
            std::cout << std::left << std::setw(32) << msg->pose(i).name();
            std::cout << " x: " << std::right << std::setw(NWIDTH) << x;
            std::cout << " y: " << std::right << std::setw(NWIDTH) << y;
            std::cout << " z: " << std::right << std::setw(NWIDTH) << z;

            std::cout << " ow: " << std::right << std::setw(NWIDTH) << ow;
            std::cout << " ox: " << std::right << std::setw(NWIDTH) << ox;
            std::cout << " oy: " << std::right << std::setw(NWIDTH) << oy;
            std::cout << " oz: " << std::right << std::setw(NWIDTH) << oz;
            std::cout << std::endl;
        }
        if (i == msg->pose_size() - 1) {
            msr::airlib::Vector3r p(x, -y, -z);
            msr::airlib::Quaternionr o(ow, ox, -oy, -oz);

            client.simSetVehiclePose(Pose(p, o), true);
        }
    }
    if (count % MESSAGE_THROTTLE == 0) {
        std::cout << std::endl;
    }

    ++count;
}

void cbGobalPose(ConstPosesStampedPtr& msg)
{
    std::cout << std::fixed;
    std::cout << std::setprecision(4);
    static int count = 0;
    if (count % MESSAGE_THROTTLE) {
        ++count;
        return;
    }
    ++count;

    for (int i = 0; i < msg->pose_size(); i++) {
        std::cout << "global (" << i << ") ";
        std::cout << std::left << std::setw(32) << msg->pose(i).name();
        std::cout << " x: " << std::right << std::setfill(' ') << std::setw(NWIDTH) << msg->pose(i).position().x();
        std::cout << " y: " << std::right << std::setfill(' ') << std::setw(NWIDTH) << msg->pose(i).position().y();
        std::cout << " z: " << std::right << std::setfill(' ') << std::setw(NWIDTH) << msg->pose(i).position().z();

        std::cout << " ow: " << std::right << std::setfill(' ') << std::setw(NWIDTH) << msg->pose(i).orientation().w();
        std::cout << " ox: " << std::right << std::setfill(' ') << std::setw(NWIDTH) << msg->pose(i).orientation().x();
        std::cout << " oy: " << std::right << std::setfill(' ') << std::setw(NWIDTH) << msg->pose(i).orientation().y();
        std::cout << " oz: " << std::right << std::setfill(' ') << std::setw(NWIDTH) << msg->pose(i).orientation().z();
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

int main(int _argc, char** _argv)
{

    client.confirmConnection();

    // Load gazebo
    gazebo::client::setup(_argc, _argv);

    // Create our node for communication
    gazebo::transport::NodePtr node(new gazebo::transport::Node());
    node->Init();

    // Listen to Gazebo topics
    gazebo::transport::SubscriberPtr sub_pose1 = node->Subscribe("~/pose/local/info", cbLocalPose);
    gazebo::transport::SubscriberPtr sub_pose2 = node->Subscribe("~/pose/info", cbGobalPose);

    while (true)
        gazebo::common::Time::MSleep(10);

    // Make sure to shut everything down.
    gazebo::client::shutdown();
}
