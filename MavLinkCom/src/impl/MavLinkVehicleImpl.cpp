// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "Utils.hpp"
#include "MavLinkVehicleImpl.hpp"
#include "MavLinkConnection.hpp"
#include "../serial_com/SerialPort.hpp"
#include "../serial_com/UdpClientPort.hpp"
#include <exception>
#include <cstring>
using namespace mavlink_utils;

using namespace mavlinkcom_impl;
#define PACKET_PAYLOAD 253 //hard coded in MavLink code - do not change

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

enum PX4_CUSTOM_MAIN_MODE
{
    PX4_CUSTOM_MAIN_MODE_NONE = 0,
    PX4_CUSTOM_MAIN_MODE_MANUAL = 1,
    PX4_CUSTOM_MAIN_MODE_ALTCTL = 2,
    PX4_CUSTOM_MAIN_MODE_POSCTL = 3,
    PX4_CUSTOM_MAIN_MODE_AUTO = 4,
    PX4_CUSTOM_MAIN_MODE_ACRO = 5,
    PX4_CUSTOM_MAIN_MODE_OFFBOARD = 6,
    PX4_CUSTOM_MAIN_MODE_STABILIZED = 7,
    PX4_CUSTOM_MAIN_MODE_RATTITUDE = 8
};

enum PX4_CUSTOM_SUB_MODE_AUTO
{
    PX4_CUSTOM_SUB_MODE_AUTO_NONE = 0,
    PX4_CUSTOM_SUB_MODE_AUTO_READY = 1,
    PX4_CUSTOM_SUB_MODE_AUTO_TAKEOFF = 2,
    PX4_CUSTOM_SUB_MODE_AUTO_LOITER = 3,
    PX4_CUSTOM_SUB_MODE_AUTO_MISSION = 4,
    PX4_CUSTOM_SUB_MODE_AUTO_RTL = 5,
    PX4_CUSTOM_SUB_MODE_AUTO_LAND = 6,
    PX4_CUSTOM_SUB_MODE_AUTO_RTGS = 7,
    PX4_CUSTOM_SUB_MODE_AUTO_FOLLOW_TARGET = 8
};

// ------------------------------------------------------------------------------
//   Defines
// ------------------------------------------------------------------------------

/**
* Defines for mavlink_set_position_target_local_ned_t.type_mask
*
* Bitmask to indicate which dimensions should be ignored by the vehicle
*
* a value of 0b0000000000000000 or 0b0000001000000000 indicates that none of
* the setpoint dimensions should be ignored.
*
* If bit 10 is set the floats afx afy afz should be interpreted as force
* instead of acceleration.
*
* Mapping:
* bit 1: x,
* bit 2: y,
* bit 3: z,
* bit 4: vx,
* bit 5: vy,
* bit 6: vz,
* bit 7: ax,
* bit 8: ay,
* bit 9: az,
* bit 10: is force setpoint,
* bit 11: yaw,
* bit 12: yaw rate
* remaining bits unused
*
* Combine bitmasks with bitwise &
*
* Example for position and yaw angle:
* uint16_t type_mask =
*     MAVLINK_MSG_SET_POSITION_TARGET_POSITION &
*     MAVLINK_MSG_SET_POSITION_TARGET_YAW_ANGLE;
*/

// bit number  876543210987654321
#define MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_POSITION 0x7 // if 0x7 bits are on the PX4 ignores position.
#define MAVLINK_MSG_SET_POSITION_TARGET_ALT_HOLD 0x3 // if 0x4 bit can be used to do z altitude hold.
#define MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_VELOCITY 0x38 // if 0x38 bits are on on the PX4 ignores velocity.
#define MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_ACCELERATION 0x1C0 // if 0x1C0 bits are on the PX4 ignores acceleration.
#define MAVLINK_MSG_SET_POSITION_TARGET_FORCE 0x200 // if 0x200 is on the PX4 interprets acceleration as a force.
#define MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_YAW_ANGLE 0x400 // if 0x400 is on the PX4 ignores the yaw
#define MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_YAW_RATE 0x800 // if 0x800 is on the PX4 ignores the yaw rate.
#define MAVLINK_MSG_SET_POSITION_TARGET_TAKEOFF 0x1000
#define MAVLINK_MSG_SET_POSITION_TARGET_LAND 0x2000
#define MAVLINK_MSG_SET_POSITION_TARGET_LOITER 0x3000
#define MAVLINK_MSG_SET_POSITION_TARGET_IDLE 0x4000

MavLinkVehicleImpl::MavLinkVehicleImpl(int localSystemId, int localComponentId)
    : MavLinkNodeImpl(localSystemId, localComponentId)
{
}

MavLinkVehicleImpl::~MavLinkVehicleImpl()
{
}

AsyncResult<MavLinkHomePosition> MavLinkVehicleImpl::waitForHomePosition()
{
    Utils::log("Waiting for home position...");

    std::shared_ptr<MavLinkConnection> con = ensureConnection();
    AsyncResult<MavLinkHomePosition> result([=](int subscription) {
        con->unsubscribe(subscription);
    });

    this->setMessageInterval(static_cast<int>(MavLinkMessageIds::MAVLINK_MSG_ID_HOME_POSITION), 1);

    int subscription = con->subscribe([=](std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& m) {
        unused(connection);
        if (m.msgid == static_cast<uint8_t>(MavLinkMessageIds::MAVLINK_MSG_ID_HOME_POSITION)) {
            MavLinkHomePosition pos;
            pos.decode(m);
            result.setResult(pos);
        }
    });

    return result;
}

AsyncResult<bool> MavLinkVehicleImpl::allowFlightControlOverUsb()
{
    MavLinkParameter p;
    p.name = "CBRK_USB_CHK";
    p.value = 197848;
    return setParameter(p);
}

void MavLinkVehicleImpl::handleMessage(std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& msg)
{
    MavLinkNodeImpl::handleMessage(connection, msg);

    //status messages should usually be only sent by actual PX4. However if someone else is sending it to, we should listen it.
    //in future it would be good to have ability to add system IDs we are interested in
    //if (msg.sysid != getTargetSystemId())
    //{
    //	// we only care about messages from our intended remote node.
    //	return;
    //}

    switch (msg.msgid) {
    case MavLinkHeartbeat::kMessageId: { // MAVLINK_MSG_ID_HEARTBEAT:
        heartbeat_throttle_ = false;

        MavLinkHeartbeat heartbeat;
        heartbeat.decode(msg);

        vehicle_state_.mode = heartbeat.base_mode;

        bool armed = (heartbeat.base_mode & static_cast<uint8_t>(MAV_MODE_FLAG::MAV_MODE_FLAG_SAFETY_ARMED)) != 0;
        std::lock_guard<std::mutex> guard(state_mutex_);
        if (vehicle_state_.controls.armed != armed) {
            state_version_++;
            vehicle_state_.controls.armed = armed;
        }
        if (heartbeat.autopilot == static_cast<uint8_t>(MAV_AUTOPILOT::MAV_AUTOPILOT_PX4)) {
            int custom = (heartbeat.custom_mode >> 16);
            int mode = (custom & 0xff);
            int submode = (custom >> 8);

            bool isOffboard = (mode == PX4_CUSTOM_MAIN_MODE_OFFBOARD);
            if (isOffboard) {
                vehicle_state_.controls.offboard = isOffboard;
                Utils::log("MavLinkVehicle: confirmed offboard mode\n");
            }
            else if (vehicle_state_.controls.offboard != isOffboard) {
                vehicle_state_.controls.offboard = isOffboard;
                Utils::log("MavLinkVehicle: is no longer in offboard mode\n");
            }
            if (!isOffboard && (requested_mode_ != custom) && (custom != previous_mode_)) {
                if (control_request_sent_) {
                    // user may have changed modes on us! So we need to honor that and not
                    // try and take it back.
                    vehicle_state_.controls.offboard = false;
                    control_requested_ = false;
                    control_request_sent_ = false;

                    Utils::log(Utils::stringf("MavLinkVehicle: detected mode change (mode=%d, submode=%d), will stop trying to do offboard control\n",
                                              mode,
                                              submode));
                }
            }
            previous_mode_ = custom;
        }
        break;
    }
    case MavLinkAttitude::kMessageId: { // MAVLINK_MSG_ID_ATTITUDE:
        MavLinkAttitude att;
        att.decode(msg);

        std::lock_guard<std::mutex> guard(state_mutex_);
        state_version_++;
        updateReadStats(msg);
        vehicle_state_.attitude.roll = att.roll;
        vehicle_state_.attitude.pitch = att.pitch;
        vehicle_state_.attitude.yaw = att.yaw;
        vehicle_state_.attitude.roll_rate = att.rollspeed;
        vehicle_state_.attitude.pitch_rate = att.pitchspeed;
        vehicle_state_.attitude.yaw_rate = att.yawspeed;
        vehicle_state_.attitude.updated_on = att.time_boot_ms;
        //Utils::logMessage("Received attitude, acc=[%2.2f %2.2f %2.2f]", att.pitch, att.roll, att.yaw);
        break;
    }
    case MavLinkControlSystemState::kMessageId: { // CONTROL_SYSTEM_STATE:
        MavLinkControlSystemState cnt;
        cnt.decode(msg);

        std::lock_guard<std::mutex> guard(state_mutex_);
        state_version_++;
        updateReadStats(msg);
        vehicle_state_.local_est.acc.x = cnt.x_acc;
        vehicle_state_.local_est.acc.y = cnt.x_acc;
        vehicle_state_.local_est.acc.z = cnt.x_acc;
        //Utils::logMessage("Received attitude, acc=[%2.2f %2.2f %2.2f]", att.pitch, att.roll, att.yaw);
        break;
    }
    case MavLinkLocalPositionNed::kMessageId: { // MAVLINK_MSG_ID_LOCAL_POSITION_NED:
        MavLinkLocalPositionNed value;
        value.decode(msg);
        std::lock_guard<std::mutex> guard(state_mutex_);
        state_version_++;
        updateReadStats(msg);
        vehicle_state_.local_est.pos.x = value.x;
        vehicle_state_.local_est.pos.y = value.y;
        vehicle_state_.local_est.pos.z = value.z;
        vehicle_state_.local_est.lin_vel.x = value.vx;
        vehicle_state_.local_est.lin_vel.y = value.vy;
        vehicle_state_.local_est.lin_vel.z = value.vz;
        vehicle_state_.local_est.updated_on = value.time_boot_ms;
        break;
    }
    case MavLinkGlobalPositionInt::kMessageId: { // MAVLINK_MSG_ID_GLOBAL_POSITION_INT:
        MavLinkGlobalPositionInt pos;
        pos.decode(msg);
        std::lock_guard<std::mutex> guard(state_mutex_);
        state_version_++;
        updateReadStats(msg);
        vehicle_state_.global_est.pos.lat = static_cast<float>(pos.lat) / 1E7f;
        vehicle_state_.global_est.pos.lon = static_cast<float>(pos.lon) / 1E7f;
        vehicle_state_.global_est.pos.alt = static_cast<float>(pos.alt) / 1E3f;
        vehicle_state_.global_est.vel.x = pos.vx;
        vehicle_state_.global_est.vel.y = pos.vy;
        vehicle_state_.global_est.vel.z = pos.vz;
        vehicle_state_.global_est.alt_ground = pos.relative_alt;
        vehicle_state_.global_est.heading = static_cast<float>(pos.hdg) / 100;
        vehicle_state_.global_est.updated_on = pos.time_boot_ms;
        break;
    }
    case MavLinkRcChannelsScaled::kMessageId: {
        MavLinkRcChannelsScaled ch;
        ch.decode(msg);
        std::lock_guard<std::mutex> guard(state_mutex_);
        state_version_++;
        updateReadStats(msg);
        int port = ch.port;
        // we can store up to 16 channels in rc_channels_scaled.
        if (port < 2) {
            int offset = port * 8;
            vehicle_state_.rc.rc_channels_scaled[0 + offset] = ch.chan1_scaled;
            vehicle_state_.rc.rc_channels_scaled[1 + offset] = ch.chan2_scaled;
            vehicle_state_.rc.rc_channels_scaled[2 + offset] = ch.chan3_scaled;
            vehicle_state_.rc.rc_channels_scaled[3 + offset] = ch.chan4_scaled;
            vehicle_state_.rc.rc_channels_scaled[4 + offset] = ch.chan5_scaled;
            vehicle_state_.rc.rc_channels_scaled[5 + offset] = ch.chan6_scaled;
            vehicle_state_.rc.rc_channels_scaled[6 + offset] = ch.chan7_scaled;
            vehicle_state_.rc.rc_channels_scaled[7 + offset] = ch.chan8_scaled;
        }

        break;
    }
    case MavLinkRcChannels::kMessageId: { // MAVLINK_MSG_ID_RC_CHANNELS:
        MavLinkRcChannels ch;
        ch.decode(msg);

        vehicle_state_.rc.rc_channels_count = ch.chancount;
        vehicle_state_.rc.rc_signal_strength = ch.rssi;
        vehicle_state_.rc.updated_on = ch.time_boot_ms;

        break;
    }
    case MavLinkServoOutputRaw::kMessageId: { // MAVLINK_MSG_ID_SERVO_OUTPUT_RAW:
        // The RAW values of the servo outputs
        MavLinkServoOutputRaw servo;
        servo.decode(msg);
        std::lock_guard<std::mutex> guard(state_mutex_);
        state_version_++;
        updateReadStats(msg);
        vehicle_state_.servo.servo_raw[0] = servo.servo1_raw;
        vehicle_state_.servo.servo_raw[1] = servo.servo2_raw;
        vehicle_state_.servo.servo_raw[2] = servo.servo3_raw;
        vehicle_state_.servo.servo_raw[3] = servo.servo4_raw;
        vehicle_state_.servo.servo_raw[4] = servo.servo5_raw;
        vehicle_state_.servo.servo_raw[5] = servo.servo6_raw;
        vehicle_state_.servo.servo_raw[6] = servo.servo7_raw;
        vehicle_state_.servo.servo_raw[7] = servo.servo8_raw;
        vehicle_state_.servo.servo_port = servo.port;
        vehicle_state_.servo.updated_on = servo.time_usec;

        break;
    }
    case MavLinkVfrHud::kMessageId: { // MAVLINK_MSG_ID_VFR_HUD:
        // Metrics typically displayed on a HUD for fixed wing aircraft
        MavLinkVfrHud vfrhud;
        vfrhud.decode(msg);
        std::lock_guard<std::mutex> guard(state_mutex_);
        state_version_++;
        updateReadStats(msg);
        vehicle_state_.vfrhud.true_airspeed = vfrhud.airspeed;
        vehicle_state_.vfrhud.groundspeed = vfrhud.groundspeed;
        vehicle_state_.vfrhud.altitude = vfrhud.alt;
        vehicle_state_.vfrhud.climb_rate = vfrhud.climb;
        vehicle_state_.vfrhud.heading = vfrhud.heading;
        vehicle_state_.vfrhud.throttle = vfrhud.throttle;
        break;
    }
    case MavLinkHighresImu::kMessageId: { // MAVLINK_MSG_ID_HIGHRES_IMU:
        // The IMU readings in SI units in NED body frame
        break;
    }
    case MavLinkAltitude::kMessageId: { // MAVLINK_MSG_ID_ALTITUDE:
        MavLinkAltitude altitude;
        altitude.decode(msg);
        std::lock_guard<std::mutex> guard(state_mutex_);
        state_version_++;
        updateReadStats(msg);
        vehicle_state_.altitude.altitude_amsl = altitude.altitude_amsl;
        vehicle_state_.altitude.altitude_local = altitude.altitude_local;
        vehicle_state_.altitude.altitude_monotonic = altitude.altitude_monotonic;
        vehicle_state_.altitude.bottom_clearance = altitude.bottom_clearance;
        vehicle_state_.altitude.altitude_relative = altitude.altitude_relative;
        vehicle_state_.altitude.altitude_terrain = altitude.altitude_terrain;
        vehicle_state_.altitude.updated_on = altitude.time_usec;
        break;
    }
    case MavLinkSysStatus::kMessageId: {
        //printSystemStatus(&msg);
        break;
    }
    case MavLinkHomePosition::kMessageId: { // MAVLINK_MSG_ID_HOME_POSITION:
        MavLinkHomePosition home;
        home.decode(msg);
        std::lock_guard<std::mutex> guard(state_mutex_);
        state_version_++;
        updateReadStats(msg);
        vehicle_state_.home.global_pos.lat = static_cast<float>(home.latitude) / 1E7f;
        vehicle_state_.home.global_pos.lon = static_cast<float>(home.longitude) / 1E7f;
        vehicle_state_.home.global_pos.alt = static_cast<float>(home.altitude) / 1E3f;
        vehicle_state_.home.approach.x = home.approach_x;
        vehicle_state_.home.approach.y = home.approach_y;
        vehicle_state_.home.approach.z = home.approach_z;
        vehicle_state_.home.local_pose.pos.x = home.x;
        vehicle_state_.home.local_pose.pos.y = home.y;
        vehicle_state_.home.local_pose.pos.z = home.z;
        vehicle_state_.home.is_set = true;
        Utils::copy(home.q, vehicle_state_.home.local_pose.q);
        break;
    }
    case MavLinkBatteryStatus::kMessageId: { // MAVLINK_MSG_ID_BATTERY_STATUS
        // todo: use this to determine when we need to do emergency landing...
        break;
    }
    case MavLinkAttitudeTarget::kMessageId: { // MAVLINK_MSG_ID_ATTITUDE_TARGET
        // Reports the current commanded attitude of the vehicle as specified by the autopilot
        break;
    }
    case MavLinkExtendedSysState::kMessageId: { // MAVLINK_MSG_ID_EXTENDED_SYS_STATE:
        // Provides state for additional features
        // The general system state
        MavLinkExtendedSysState extstatus;
        extstatus.decode(msg);
        bool landed = extstatus.landed_state == static_cast<int>(MAV_LANDED_STATE::MAV_LANDED_STATE_ON_GROUND);
        std::lock_guard<std::mutex> guard(state_mutex_);
        if (vehicle_state_.controls.landed != landed) {
            state_version_++;
            updateReadStats(msg);
            vehicle_state_.controls.landed = landed;
        }
        break;
    }
    case MavLinkActuatorControlTarget::kMessageId: { // MAVLINK_MSG_ID_ACTUATOR_CONTROL_TARGE
        break;
    }
    case MavLinkStatustext::kMessageId: { // MAVLINK_MSG_ID_STATUSTEXT:
        /*MavLinkStatustext statustext;
        statustext.decode(msg);
        Utils::logMessage("Received status sev=%d, text, %s",
            static_cast<int>(statustext.severity), statustext.text);*/
        break;
    }
    case MavLinkMessageInterval::kMessageId: { // MAVLINK_MSG_ID_MESSAGE_INTERVAL:
        MavLinkMessageInterval msgInt;
        msgInt.decode(msg);
        Utils::log(Utils::stringf("Received message interval for msg=%d, interval = %d",
                                  static_cast<int>(msgInt.message_id),
                                  msgInt.interval_us));
        break;
    }
    case MavLinkNamedValueInt::kMessageId: { // MAVLINK_MSG_ID_NAMED_VALUE_INT
        break;
    }
    case MavLinkHilControls::kMessageId: { // MAVLINK_MSG_ID_HIL_CONTROLS:
        MavLinkHilControls value;
        value.decode(msg);
        std::lock_guard<std::mutex> guard(state_mutex_);
        state_version_++;
        updateReadStats(msg);
        vehicle_state_.controls.actuator_controls[0] = value.roll_ailerons;
        vehicle_state_.controls.actuator_controls[1] = value.pitch_elevator;
        vehicle_state_.controls.actuator_controls[2] = value.yaw_rudder;
        vehicle_state_.controls.actuator_controls[3] = value.throttle;
        vehicle_state_.controls.actuator_controls[4] = value.aux1;
        vehicle_state_.controls.actuator_controls[5] = value.aux2;
        vehicle_state_.controls.actuator_controls[6] = value.aux3;
        vehicle_state_.controls.actuator_controls[7] = value.aux4;
        vehicle_state_.controls.actuator_mode = value.mode;
        vehicle_state_.controls.actuator_nav_mode = value.nav_mode;
        vehicle_state_.controls.updated_on = value.time_usec;
        break;
    }
    case MavLinkCommandAck::kMessageId: {
        // This one is tricky, we can't do sendCommandAndWaitForAck in this case because it takes too long
        // but we do want to know when we get the ack.  So this is async ACK processing!
        MavLinkCommandAck ack;
        ack.decode(msg);
        if (ack.command == MavCmdNavGuidedEnable::kCommandId) {
            MAV_RESULT ackResult = static_cast<MAV_RESULT>(ack.result);
            if (ackResult == MAV_RESULT::MAV_RESULT_TEMPORARILY_REJECTED) {
                Utils::log("### command MavCmdNavGuidedEnable result: MAV_RESULT_TEMPORARILY_REJECTED");
            }
            else if (ackResult == MAV_RESULT::MAV_RESULT_UNSUPPORTED) {
                Utils::log("### command MavCmdNavGuidedEnable result: MAV_RESULT_UNSUPPORTED");
                vehicle_state_.controls.offboard = false;
            }
            else if (ackResult == MAV_RESULT::MAV_RESULT_FAILED) {
                Utils::log("### command MavCmdNavGuidedEnable result: MAV_RESULT_FAILED");
                vehicle_state_.controls.offboard = false;
            }
            else if (ackResult == MAV_RESULT::MAV_RESULT_ACCEPTED) {
                Utils::log("### command MavCmdNavGuidedEnableresult: MAV_RESULT_ACCEPTED");
                vehicle_state_.controls.offboard = true;
            }
        }
        break;
    }
    case MavLinkAttPosMocap::kMessageId: {
        MavLinkAttPosMocap mocap;
        mocap.decode(msg);
        std::lock_guard<std::mutex> guard(state_mutex_);
        state_version_++;
        updateReadStats(msg);
        vehicle_state_.mocap.pose.pos.x = mocap.x;
        vehicle_state_.mocap.pose.pos.y = mocap.y;
        vehicle_state_.mocap.pose.pos.z = mocap.z;
        Utils::copy(mocap.q, vehicle_state_.mocap.pose.q);
        vehicle_state_.mocap.updated_on = mocap.time_usec;
        break;
    }
    default:
        break;
    }
}

void MavLinkVehicleImpl::writeMessage(MavLinkMessageBase& msg, bool update_stats)
{
    sendMessage(msg);
    if (update_stats) {
        vehicle_state_.stats.last_write_msg_id = msg.msgid;
        vehicle_state_.stats.last_write_msg_time = getTimeStamp();
    }
}

AsyncResult<bool> MavLinkVehicleImpl::armDisarm(bool arm)
{
    MavCmdComponentArmDisarm cmd{};
    cmd.Arm = arm ? 1.0f : 0.0f;
    return sendCommandAndWaitForAck(cmd);
}

AsyncResult<bool> MavLinkVehicleImpl::takeoff(float z, float pitch, float yaw)
{
    // careful here, we are doing a tricky conversion from local coordinates to global coordinates.
    float deltaZ = z - vehicle_state_.local_est.pos.z;
    float targetAlt = vehicle_state_.home.global_pos.alt - deltaZ;
    Utils::log(Utils::stringf("Take off to %f", targetAlt));
    MavCmdNavTakeoff cmd{};
    cmd.Pitch = pitch;
    cmd.Yaw = yaw;
    cmd.Latitude = INFINITY;
    cmd.Longitude = INFINITY;
    cmd.Altitude = targetAlt;
    return sendCommandAndWaitForAck(cmd);
}

AsyncResult<bool> MavLinkVehicleImpl::waitForAltitude(float z, float dz, float dvz)
{
    std::shared_ptr<MavLinkConnection> con = ensureConnection();
    AsyncResult<bool> result([=](int subscription) {
        con->unsubscribe(subscription);
    });

    int subscription = con->subscribe([=](std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& m) {
        unused(connection);
        if (m.msgid == static_cast<uint8_t>(MavLinkLocalPositionNed::kMessageId)) {
            MavLinkLocalPositionNed pos;
            pos.decode(m);

            if (fabs(pos.z - z) <= dz && fabs(pos.vz) < dvz) {
                result.setResult(true);
            }
        }
    });

    return result;
}

AsyncResult<bool> MavLinkVehicleImpl::land(float yaw, float lat, float lon, float altitude)
{
    MavCmdNavLand cmd{};
    cmd.AbortAlt = 0; // use system default.
    cmd.YawAngle = yaw;
    cmd.Latitude = lat;
    cmd.Longitude = lon;
    cmd.Altitude = altitude;
    return sendCommandAndWaitForAck(cmd);
}

AsyncResult<bool> MavLinkVehicleImpl::returnToHome()
{
    MavCmdNavReturnToLaunch cmd{};
    return sendCommandAndWaitForAck(cmd);
}

bool MavLinkVehicleImpl::getRcSwitch(int channel, float threshold)
{
    if (threshold < -1 || threshold > 1) {
        auto msg = Utils::stringf("RC channel threshold is out of range, expecting -1 < threshold < 1, but got %f", threshold);
        throw std::runtime_error(msg);
    }
    // if threshold < 0 then the threshold is inverted.
    if (channel > 0 && channel < 18) {
        std::lock_guard<std::mutex> guard(state_mutex_);
        int16_t position = vehicle_state_.rc.rc_channels_scaled[channel - 1];
        // RC channel 1 value scaled, (-100%) -10000, (0%) 0, (100%) 10000, (invalid) INT16_MAX.
        // Convert it to a floating point number between -1 and 1.
        float value = static_cast<float>(position) / 10000.0f;
        if (threshold < 0) {
            return value < threshold;
        }
        else {
            return value > threshold;
        }
    }
    return false;
}

void MavLinkVehicleImpl::requestControl()
{
    // PX4 expects the move commands to happen IMMEDIATELY after this call, so we don't actually request control here
    // until the move commands start happening.
    control_requested_ = true;
}

// return true if user calls requestControl and has not called releaseControl.
bool MavLinkVehicleImpl::hasOffboardControl()
{
    return control_requested_;
}

void MavLinkVehicleImpl::releaseControl()
{
    control_requested_ = false;
    control_request_sent_ = false;
    vehicle_state_.controls.offboard = false;
    setMode(static_cast<int>(MAV_MODE_FLAG::MAV_MODE_FLAG_CUSTOM_MODE_ENABLED),
            static_cast<int>(PX4_CUSTOM_MAIN_MODE_POSCTL),
            0,
            false);
}

void MavLinkVehicleImpl::checkOffboard()
{
    if (!control_requested_) {
        throw std::runtime_error("You must call requestControl first.");
    }

    if (control_requested_ && !vehicle_state_.controls.offboard) {
        // Ok, now's the time to actually request it since the caller is about to send MavLinkSetPositionTargetGlobalInt, but
        // PX4 will reject this thinking 'offboard_control_loss_timeout' because we haven't actually sent any offboard messages
        // yet.  I know the PX4 protocol is kind of weird.  So we prime the pump here with some dummy messages that tell the
        // drone to stay where it is, this will reset the 'offboard_control_loss_timeout', then we should be able to get control.

        // send a few to make sure it gets through...
        for (int i = 0; i < 3; i++) {
            offboardIdle();
        }

        Utils::log("MavLinkVehicleImpl::checkOffboard: sending MavCmdNavGuidedEnable \n");
        // Note: we can't wait for ACK here, I've tried it.  The ACK takes too long to get back to
        // us by which time the PX4 times out offboard mode!!
        setMode(static_cast<int>(MAV_MODE_FLAG::MAV_MODE_FLAG_CUSTOM_MODE_ENABLED),
                static_cast<int>(PX4_CUSTOM_MAIN_MODE_OFFBOARD),
                0,
                false);
        control_request_sent_ = true;
        // assume this was successful, we'll find out if so in the next heartbeat.
        vehicle_state_.controls.offboard = true;
    }
}

uint32_t MavLinkVehicleImpl::getTimeStamp()
{
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

void MavLinkVehicleImpl::offboardIdle()
{
    MavLinkSetPositionTargetLocalNed msg;
    msg.time_boot_ms = getTimeStamp();
    msg.target_system = getTargetSystemId();
    msg.target_component = getTargetComponentId();
    msg.coordinate_frame = static_cast<uint8_t>(MAV_FRAME::MAV_FRAME_LOCAL_NED);

    msg.type_mask = MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_POSITION |
                    MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_ACCELERATION |
                    MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_POSITION |
                    MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_YAW_RATE |
                    MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_YAW_ANGLE |
                    MAVLINK_MSG_SET_POSITION_TARGET_IDLE;

    writeMessage(msg);
}

void MavLinkVehicleImpl::moveToGlobalPosition(float lat, float lon, float alt, bool isYaw, float yawOrRate)
{
    checkOffboard();

    MavLinkSetPositionTargetGlobalInt msg;
    msg.target_component = getTimeStamp();
    msg.target_system = getTargetSystemId();
    msg.target_component = getTargetComponentId();
    msg.afx = msg.afy = msg.afz = 0;
    msg.vx = msg.vy = msg.vz = 0;
    msg.coordinate_frame = static_cast<uint8_t>(MAV_FRAME::MAV_FRAME_GLOBAL_INT);

    msg.type_mask = MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_VELOCITY |
                    MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_ACCELERATION;

    if (isYaw) {
        msg.type_mask |= MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_YAW_RATE;
        msg.yaw = yawOrRate;
        msg.yaw_rate = 0;
    }
    else {
        msg.type_mask |= MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_YAW_ANGLE;
        msg.yaw = 0;
        msg.yaw_rate = yawOrRate;
    }

    msg.lat_int = static_cast<int>(lat * 1E7);
    msg.lon_int = static_cast<int>(lon * 1E7);
    msg.alt = alt;

    writeMessage(msg);
}

AsyncResult<bool> MavLinkVehicleImpl::setMode(int mode, int customMode, int customSubMode, bool waitForAck)
{
    // this mode change take precedence over offboard mode.
    if (customMode != static_cast<int>(PX4_CUSTOM_MAIN_MODE_OFFBOARD)) {
        control_requested_ = false;
        control_request_sent_ = false;
    }

    if ((vehicle_state_.mode & static_cast<int>(MAV_MODE_FLAG::MAV_MODE_FLAG_HIL_ENABLED)) != 0) {
        mode |= static_cast<int>(MAV_MODE_FLAG::MAV_MODE_FLAG_HIL_ENABLED); // must preserve this flag.
    }
    if ((vehicle_state_.mode & static_cast<uint8_t>(MAV_MODE_FLAG::MAV_MODE_FLAG_SAFETY_ARMED)) != 0) {
        mode |= static_cast<int>(MAV_MODE_FLAG::MAV_MODE_FLAG_SAFETY_ARMED); // must preserve this flag.
    }
    requested_mode_ = (customMode & 0xff) + ((customSubMode & 0xff) << 8);
    MavCmdDoSetMode cmd{};
    cmd.Mode = static_cast<float>(mode);
    cmd.CustomMode = static_cast<float>(customMode);
    cmd.CustomSubmode = static_cast<float>(customSubMode);
    if (waitForAck) {
        return sendCommandAndWaitForAck(cmd);
    }
    else {
        sendCommand(cmd);
        return AsyncResult<bool>::Completed(true);
    }
}

AsyncResult<bool> MavLinkVehicleImpl::setPositionHoldMode()
{
    return setMode(static_cast<int>(MAV_MODE_FLAG::MAV_MODE_FLAG_CUSTOM_MODE_ENABLED),
                   static_cast<int>(PX4_CUSTOM_MAIN_MODE_POSCTL));
}

AsyncResult<bool> MavLinkVehicleImpl::setStabilizedFlightMode()
{
    return setMode(static_cast<int>(MAV_MODE_FLAG::MAV_MODE_FLAG_STABILIZE_ENABLED));
}

AsyncResult<bool> MavLinkVehicleImpl::setHomePosition(float lat, float lon, float alt)
{
    MavCmdDoSetHome cmd{};
    cmd.UseCurrent = 0;
    cmd.Latitude = lat;
    cmd.Longitude = lon;
    cmd.Altitude = alt;
    return sendCommandAndWaitForAck(cmd);
}

AsyncResult<bool> MavLinkVehicleImpl::setMissionMode()
{
    return setMode(static_cast<int>(MAV_MODE_FLAG::MAV_MODE_FLAG_CUSTOM_MODE_ENABLED) |
                       static_cast<int>(MAV_MODE_FLAG::MAV_MODE_FLAG_AUTO_ENABLED),
                   static_cast<int>(PX4_CUSTOM_MAIN_MODE_AUTO),
                   static_cast<int>(PX4_CUSTOM_SUB_MODE_AUTO_MISSION));
}

AsyncResult<bool> MavLinkVehicleImpl::loiter()
{
    return setMode(static_cast<int>(MAV_MODE_FLAG::MAV_MODE_FLAG_CUSTOM_MODE_ENABLED),
                   static_cast<int>(PX4_CUSTOM_MAIN_MODE_AUTO),
                   static_cast<int>(PX4_CUSTOM_SUB_MODE_AUTO_LOITER));
}

bool MavLinkVehicleImpl::isLocalControlSupported()
{
    MavLinkAutopilotVersion cap;
    assertNotPublishingThread();
    if (!getCapabilities().wait(2000, &cap)) {
        throw std::runtime_error("Two second timeout waiting for response to mavlink command MAV_CMD_REQUEST_AUTOPILOT_CAPABILITIES");
    }
    return (cap.capabilities & static_cast<int>(MAV_PROTOCOL_CAPABILITY::MAV_PROTOCOL_CAPABILITY_SET_POSITION_TARGET_LOCAL_NED)) != 0;
}

void MavLinkVehicleImpl::moveByLocalVelocity(float vx, float vy, float vz, bool isYaw, float yawOrRate)
{
    checkOffboard();

    MavLinkSetPositionTargetLocalNed msg;
    msg.time_boot_ms = getTimeStamp();
    msg.target_system = getTargetSystemId();
    msg.target_component = getTargetComponentId();
    msg.afx = msg.afy = msg.afz = 0;
    msg.x = msg.y = msg.z = 0;
    msg.vx = vx;
    msg.vy = vy;
    msg.vz = vz;
    msg.coordinate_frame = static_cast<uint8_t>(MAV_FRAME::MAV_FRAME_LOCAL_NED);

    msg.type_mask = MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_POSITION |
                    MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_ACCELERATION;

    if (isYaw) {
        msg.type_mask |= MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_YAW_RATE;
        msg.yaw = yawOrRate;
        msg.yaw_rate = 0;
    }
    else {
        msg.type_mask |= MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_YAW_ANGLE;
        msg.yaw = 0;
        msg.yaw_rate = yawOrRate;
    }

    writeMessage(msg);
}

void MavLinkVehicleImpl::moveByLocalVelocityWithAltHold(float vx, float vy, float z, bool isYaw, float yawOrRate)
{
    checkOffboard();

    MavLinkSetPositionTargetLocalNed msg;
    msg.time_boot_ms = getTimeStamp();
    msg.target_system = getTargetSystemId();
    msg.target_component = getTargetComponentId();
    msg.afx = msg.afy = msg.afz = 0;
    msg.x = msg.y = msg.z = z;
    msg.vx = vx;
    msg.vy = vy;
    msg.vz = 0;
    msg.coordinate_frame = static_cast<uint8_t>(MAV_FRAME::MAV_FRAME_LOCAL_NED);

    msg.type_mask = MAVLINK_MSG_SET_POSITION_TARGET_ALT_HOLD |
                    MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_ACCELERATION;

    if (isYaw) {
        msg.type_mask |= MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_YAW_RATE;
        msg.yaw = yawOrRate;
        msg.yaw_rate = 0;
    }
    else {
        msg.type_mask |= MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_YAW_ANGLE;
        msg.yaw = 0;
        msg.yaw_rate = yawOrRate;
    }

    writeMessage(msg);
}

void MavLinkVehicleImpl::moveToLocalPosition(float x, float y, float z, bool isYaw, float yawOrRate)
{
    checkOffboard();

    MavLinkSetPositionTargetLocalNed msg;
    msg.time_boot_ms = getTimeStamp();
    msg.target_system = getTargetSystemId();
    msg.target_component = getTargetComponentId();
    msg.afx = msg.afy = msg.afz = 0;
    msg.vx = msg.vy = msg.vz = 0;
    msg.x = x;
    msg.y = y;
    msg.z = z;
    msg.coordinate_frame = static_cast<uint8_t>(MAV_FRAME::MAV_FRAME_LOCAL_NED);

    msg.type_mask = MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_ACCELERATION |
                    MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_VELOCITY;
    if (isYaw) {
        msg.type_mask |= MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_YAW_RATE;
        msg.yaw = yawOrRate;
        msg.yaw_rate = 0;
    }
    else {
        msg.type_mask |= MAVLINK_MSG_SET_POSITION_TARGET_IGNORE_YAW_ANGLE;
        msg.yaw = 0;
        msg.yaw_rate = yawOrRate;
    }

    writeMessage(msg);
}

bool MavLinkVehicleImpl::isAttitudeControlSupported()
{
    MavLinkAutopilotVersion cap;
    assertNotPublishingThread();
    if (!getCapabilities().wait(5000, &cap)) {
        throw std::runtime_error("Five second timeout waiting for response to mavlink command MAV_CMD_REQUEST_AUTOPILOT_CAPABILITIES");
    }
    return (cap.capabilities & static_cast<int>(MAV_PROTOCOL_CAPABILITY::MAV_PROTOCOL_CAPABILITY_SET_ATTITUDE_TARGET)) != 0;
}

void MavLinkVehicleImpl::moveByAttitude(float roll, float pitch, float yaw, float rollRate, float pitchRate, float yawRate, float thrust)
{
    checkOffboard();

    MavLinkSetAttitudeTarget msg;
    msg.time_boot_ms = getTimeStamp();
    msg.target_system = getTargetSystemId();
    msg.target_component = getTargetComponentId();

    float radRoll = roll * M_PIf / 180.0f;
    float radPitch = pitch * M_PIf / 180.0f;
    float radYaw = yaw * M_PIf / 180.0f;
    mavlink_euler_to_quaternion(radRoll, radPitch, radYaw, msg.q);

    // thrust must be between -1 and 1.
    thrust = static_cast<float>(fmax(-1.0f, fmin(1.0f, thrust)));
    msg.thrust = thrust;
    msg.body_roll_rate = rollRate * M_PIf / 180.0f;
    msg.body_pitch_rate = pitchRate * M_PIf / 180.0f;
    msg.body_yaw_rate = yawRate * M_PIf / 180.0f;

    const uint8_t kIgnoreBodyRollRateBit = 1; //  mask bit 1: body roll rate,
    const uint8_t kIgnoreBodyPitchRateBit = 2; // bit 2: body pitch rate,
    const uint8_t kIgnoreBodyYawRateBit = 4; // bit 3: body yaw rate.
    const uint8_t kIgnoreBit4Reserved = 8; //  bit 4-bit 6: reserved
    const uint8_t kIgnoreBit5Reserved = 0x10;
    const uint8_t kIgnoreBit6Reserved = 0x20;
    const uint8_t kIgnoreThrottleBit = 0x40; // bit 7: throttle,
    const uint8_t kIgnoreAttitudeBit = 0x80; // bit 8: attitude

    msg.type_mask = 0;
    if (rollRate == 0) {
        msg.type_mask |= kIgnoreBodyRollRateBit;
    }
    if (pitchRate == 0) {
        msg.type_mask |= kIgnoreBodyPitchRateBit;
    }
    if (yawRate == 0) {
        msg.type_mask |= kIgnoreBodyYawRateBit;
    }
    writeMessage(msg);
}

void MavLinkVehicleImpl::resetCommandParams(MavLinkCommandLong& cmd)
{
    cmd.param1 = cmd.param2 = cmd.param3 = cmd.param4 = cmd.param5 = cmd.param6 = cmd.param7 = 0;
}

const VehicleState& MavLinkVehicleImpl::getVehicleState()
{
    std::lock_guard<std::mutex> guard(state_mutex_);
    return vehicle_state_; // return a snapshot.
}

int MavLinkVehicleImpl::getVehicleStateVersion()
{
    std::lock_guard<std::mutex> guard(state_mutex_);
    return state_version_;
}

void MavLinkVehicleImpl::updateReadStats(const MavLinkMessage& msg)
{
    vehicle_state_.stats.last_read_msg_id = msg.msgid;
    vehicle_state_.stats.last_read_msg_time = getTimeStamp();
}
