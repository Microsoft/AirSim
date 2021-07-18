// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef MavLinkCom_MavLinkVehicleImpl_hpp
#define MavLinkCom_MavLinkVehicleImpl_hpp

#define MAVLINK_PACKED

#include "VehicleState.hpp"
#include "Utils.hpp"
#include "MavLinkNodeImpl.hpp"
#include "MavLinkMessages.hpp"

#include "../serial_com/Port.h"
#include "string"
#include <map>
#include <vector>
#include <mutex>
#include <memory>
#include "AsyncResult.hpp"

using namespace mavlinkcom;

namespace mavlinkcom_impl
{
class MavLinkVehicleImpl : public MavLinkNodeImpl
{
public:
    MavLinkVehicleImpl(int localSystemId, int localComponentId);
    ~MavLinkVehicleImpl();

public:
    AsyncResult<bool> armDisarm(bool arm);
    AsyncResult<bool> takeoff(float z = -2.5, float pitch = 0, float yaw = 0);
    AsyncResult<bool> land(float yaw, float lat = 0, float lon = 0, float altitude = 0);
    AsyncResult<bool> returnToHome();
    AsyncResult<bool> loiter();
    AsyncResult<bool> setPositionHoldMode();
    AsyncResult<bool> setStabilizedFlightMode();
    AsyncResult<bool> setHomePosition(float lat = 0, float lon = 0, float alt = 0);
    AsyncResult<bool> setMissionMode();
    AsyncResult<MavLinkHomePosition> waitForHomePosition();
    AsyncResult<bool> allowFlightControlOverUsb();
    AsyncResult<bool> waitForAltitude(float z, float dz, float dvz);
    AsyncResult<bool> setMode(int modeFlags, int customMode = 0, int customSubMode = 0, bool waitForAck = true);

    // request OFFBOARD control.
    void requestControl();
    // release OFFBOARD control
    void releaseControl();

    // return true if we still have offboard control (can lose this if user flips the switch).
    bool hasOffboardControl();
    // send this to keep offboard control but do no movement.
    void offboardIdle();

    // offboard control methods.
    bool isLocalControlSupported();
    void moveToLocalPosition(float x, float y, float z, bool isYaw, float yawOrRate);
    void moveToGlobalPosition(float lat, float lon, float alt, bool isYaw, float yawOrRate);
    void moveByLocalVelocity(float vx, float vy, float vz, bool isYaw, float yawOrRate);
    void moveByLocalVelocityWithAltHold(float vx, float vy, float z, bool isYaw, float yawOrRate);

    // low level control, only use this one if you really know what you are doing!!
    bool isAttitudeControlSupported();

    // Move drone by directly controlling the attitude of the drone (units are degrees).
    // If the rollRate, pitchRate and yawRate are all zero then you will get the default rates provided by the drone.
    void moveByAttitude(float roll, float pitch, float yaw, float rollRate, float pitchRate, float yawRate, float thrust);
    void writeMessage(MavLinkMessageBase& message, bool update_stats = true);

    int getVehicleStateVersion();
    const VehicleState& getVehicleState();

    uint32_t getTimeStamp();

private:
    virtual void handleMessage(std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& message);
    void resetCommandParams(MavLinkCommandLong& cmd);
    void updateReadStats(const MavLinkMessage& msg);
    void checkOffboard();
    bool getRcSwitch(int channel, float threshold);

private:
    std::mutex state_mutex_;
    int state_version_ = 0;
    bool control_requested_ = false;
    bool control_request_sent_ = false;
    int requested_mode_ = 0;
    int previous_mode_ = 0;
    // this latch is reset even time we receive a heartbeat, this is useful for operations that we
    // want to throttle to the heartbeat rate.
    bool heartbeat_throttle_ = false;
    VehicleState vehicle_state_;
};
}

#endif
