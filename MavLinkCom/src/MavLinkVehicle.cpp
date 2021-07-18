// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "MavLinkVehicle.hpp"
#include "impl/MavLinkVehicleImpl.hpp"
#include <memory>

using namespace mavlinkcom;
using namespace mavlinkcom_impl;

MavLinkVehicle::MavLinkVehicle(int localSystemId, int localComponentId)
{
    pImpl.reset(new MavLinkVehicleImpl(localSystemId, localComponentId));
}

MavLinkVehicle::MavLinkVehicle()
{
}

MavLinkVehicle::~MavLinkVehicle()
{
    pImpl = nullptr;
}

AsyncResult<bool> MavLinkVehicle::armDisarm(bool arm)
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->armDisarm(arm);
}

AsyncResult<bool> MavLinkVehicle::takeoff(float z, float pitch, float yaw)
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->takeoff(z, pitch, yaw);
}

AsyncResult<bool> MavLinkVehicle::waitForAltitude(float z, float dz, float dvz)
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->waitForAltitude(z, dz, dvz);
}

AsyncResult<bool> MavLinkVehicle::land(float yaw, float lat, float lon, float altitude)
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->land(yaw, lat, lon, altitude);
}

AsyncResult<bool> MavLinkVehicle::returnToHome()
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->returnToHome();
}

AsyncResult<bool> MavLinkVehicle::setMode(int modeFlags, int customMode, int customSubMode, bool waitForAck)
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->setMode(modeFlags, customMode, customSubMode, waitForAck);
}

bool MavLinkVehicle::isLocalControlSupported()
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->isLocalControlSupported();
}

void MavLinkVehicle::moveToLocalPosition(float x, float y, float z, bool isYaw, float yawOrRate)
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    ptr->moveToLocalPosition(x, y, z, isYaw, yawOrRate);
}

void MavLinkVehicle::moveToGlobalPosition(float lat, float lon, float alt, bool isYaw, float yawOrRate)
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    ptr->moveToGlobalPosition(lat, lon, alt, isYaw, yawOrRate);
}

void MavLinkVehicle::moveByLocalVelocity(float vx, float vy, float vz, bool isYaw, float yawOrRate)
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    ptr->moveByLocalVelocity(vx, vy, vz, isYaw, yawOrRate);
}

void MavLinkVehicle::moveByLocalVelocityWithAltHold(float vx, float vy, float z, bool isYaw, float yawOrRate)
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    ptr->moveByLocalVelocityWithAltHold(vx, vy, z, isYaw, yawOrRate);
}

bool MavLinkVehicle::isAttitudeControlSupported()
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->isAttitudeControlSupported();
}

void MavLinkVehicle::moveByAttitude(float roll, float pitch, float yaw, float rollRate, float pitchRate, float yawRate, float thrust)
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    ptr->moveByAttitude(roll, pitch, yaw, rollRate, pitchRate, yawRate, thrust);
}

void MavLinkVehicle::writeMessage(MavLinkMessageBase& message, bool update_stats)
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    ptr->writeMessage(message, update_stats);
}

AsyncResult<bool> MavLinkVehicle::loiter()
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->loiter();
}

AsyncResult<bool> MavLinkVehicle::setPositionHoldMode()
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->setPositionHoldMode();
}

void MavLinkVehicle::requestControl()
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    ptr->requestControl();
}

void MavLinkVehicle::releaseControl()
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    ptr->releaseControl();
}

// return true if we still have offboard control (can lose this if user flips the switch).
bool MavLinkVehicle::hasOffboardControl()
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->hasOffboardControl();
}

AsyncResult<bool> MavLinkVehicle::setStabilizedFlightMode()
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->setStabilizedFlightMode();
}

AsyncResult<bool> MavLinkVehicle::setHomePosition(float lat, float lon, float alt)
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->setHomePosition(lat, lon, alt);
}

AsyncResult<bool> MavLinkVehicle::allowFlightControlOverUsb()
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->allowFlightControlOverUsb();
}

AsyncResult<bool> MavLinkVehicle::setMissionMode()
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->setMissionMode();
}

AsyncResult<MavLinkHomePosition> MavLinkVehicle::waitForHomePosition()
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->waitForHomePosition();
}

uint32_t MavLinkVehicle::getTimeStamp()
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->getTimeStamp();
}

int MavLinkVehicle::getVehicleStateVersion()
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->getVehicleStateVersion();
}

const VehicleState& MavLinkVehicle::getVehicleState()
{
    auto ptr = static_cast<MavLinkVehicleImpl*>(pImpl.get());
    return ptr->getVehicleState();
}

//MavLinkVehicle::MavLinkVehicle() = default;
//MavLinkVehicle::MavLinkVehicle(MavLinkVehicle&&) = default;
