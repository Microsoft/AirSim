// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef MavLinkCom_VehicleState_hpp
#define MavLinkCom_VehicleState_hpp

#include <vector>
#include <string>

namespace mavlinkcom
{
typedef struct _VehicleState
{
    typedef unsigned long long uint64_t;
    typedef unsigned char uint8_t;

    struct GlobalPosition
    {
        float lat = 0, lon = 0, alt = 0;
    };
    struct Vector3
    {
        float x = 0, y = 0, z = 0; // in NED (north, east, down) coordinates
    };
    struct LocalPose
    {
        Vector3 pos; // in NEU (north, east, down) coordinates
        float q[4] = { 0 }; //qauternion
    };

    struct AttitudeState
    {
        float roll = 0, pitch = 0, yaw = 0, roll_rate = 0, yaw_rate = 0, pitch_rate = 0;
        uint64_t updated_on = 0;
    } attitude;

    struct GlobalState
    {
        GlobalPosition pos;
        Vector3 vel;
        int alt_ground = 0;
        float heading = 0;
        uint64_t updated_on = 0;
    } global_est;

    struct RCState
    {
        int16_t rc_channels_scaled[16] = { 0 };
        unsigned char rc_channels_count = 0;
        unsigned char rc_signal_strength = 0;
        uint64_t updated_on = 0;
    } rc;

    struct ServoState
    {
        unsigned int servo_raw[8] = { 0 };
        unsigned char servo_port = 0;
        uint64_t updated_on = 0;
    } servo;

    struct ControlState
    {
        float actuator_controls[16] = { 0 };
        unsigned char actuator_mode = 0, actuator_nav_mode = 0;
        bool landed = 0;
        bool armed = false;
        bool offboard = false;
        uint64_t updated_on = 0;
    } controls;

    struct LocalState
    {
        Vector3 pos; // in NEU (north, east, up) coordinates (positive Z goes upwards).
        Vector3 lin_vel;
        Vector3 acc;
        uint64_t updated_on;
    } local_est;

    struct MocapState
    {
        LocalPose pose;
        uint64_t updated_on = 0;
    } mocap;

    struct AltitudeState
    {
        uint64_t updated_on = 0;
        float altitude_monotonic = 0, altitude_amsl = 0, altitude_terrain = 0;
        float altitude_local = 0, altitude_relative = 0, bottom_clearance = 0;
    } altitude;

    struct VfrHud
    {
        float true_airspeed = 0; // in m/s
        float groundspeed = 0; // in m/s
        float altitude = 0; // MSL altitude in meters
        float climb_rate = 0; // in m/s
        int16_t heading = 0; // in degrees w.r.t. north
        uint16_t throttle = 0; // in percent, 0 to 100
    } vfrhud;

    struct HomeState
    {
        GlobalPosition global_pos;
        LocalPose local_pose;
        Vector3 approach; // in NEU (north, east, up) coordinates (positive Z goes upwards).
        bool is_set = false;
    } home;

    struct Stats
    { //mainly for debugging purposes
        int last_read_msg_id = 0;
        uint64_t last_read_msg_time = 0;
        int last_write_msg_id = 0;
        uint64_t last_write_msg_time = 0;
        std::string debug_msg;
    } stats;

    int mode = 0; // MAV_MODE_FLAG
} VehicleState;
}

#endif
