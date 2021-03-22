// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_SimpleImuParams_hpp
#define msr_airlib_SimpleImuParams_hpp

#include "common/Common.hpp"
#include "common/EarthUtils.hpp"
#include "common/AirSimSettings.hpp"
#include <cmath>


namespace msr { namespace airlib {


// A description of the parameters:
// https://github.com/ethz-asl/kalibr/wiki/IMU-Noise-Model-and-Intrinsics
struct ImuSimpleParams {
    /* ref: Parameter values are for MPU 6000 IMU from InvenSense
    Design and Characterization of a Low Cost MEMS IMU Cluster for Precision Navigation
    Daniel R. Greenheck, 2009, sec 2.2, pp 17
    http://epublications.marquette.edu/cgi/viewcontent.cgi?article=1326&context=theses_open
    Datasheet:
    https://www.invensense.com/wp-content/uploads/2015/02/MPU-6000-Datasheet1.pdf
    For Allan Variance/Deviation plots see http://www.invensense.com/wp-content/uploads/2015/02/MPU-3300-Datasheet.pdf
    */
    Pose relative_pose;

    struct Gyroscope {
        //angule random walk (ARW)
        real_T arw = 0.30f / sqrt(3600.0f) * M_PIf / 180; //deg/sqrt(hour) converted to rad/sqrt(sec)
        //Bias Stability (tau = 500s)
        real_T tau = 500;
        real_T bias_stability = 4.6f / 3600 * M_PIf / 180; //deg/hr converted to rad/sec
        Vector3r turn_on_bias = Vector3r::Zero(); //assume calibration is done
    } gyro;

    struct Accelerometer {
        //velocity random walk (ARW)
        real_T vrw = 0.24f * EarthUtils::Gravity / 1.0E3f; //mg converted to m/s^2
        //Bias Stability (tau = 800s)
        real_T tau = 800;
        real_T bias_stability = 36.0f * 1E-6f * 9.80665f; //ug converted to m/s^2
        Vector3r turn_on_bias = Vector3r::Zero(); //assume calibration is done
    } accel;

    real_T min_sample_time = 1 / 1000.0f;   //internal IMU frequency

    void initializeFromSettings(const AirSimSettings::ImuSetting& settings)
    {
        gyro.arw = settings.gyro_arw;
        gyro.tau = settings.gyro_tau;
        gyro.bias_stability = settings.gyro_bias_stability;
        gyro.turn_on_bias = settings.gyro_turn_on_bias;

        accel.vrw = settings.accel_vrw;
        accel.tau = settings.accel_tau;
        accel.bias_stability = settings.accel_bias_stability;
        accel.turn_on_bias = settings.accel_turn_on_bias;

        relative_pose.position = settings.position;
        if (std::isnan(relative_pose.position.x()))
            relative_pose.position.x() = 0;
        if (std::isnan(relative_pose.position.y()))
            relative_pose.position.y() = 0;
        if (std::isnan(relative_pose.position.z()))
	    relative_pose.position.z() = 0;

        float pitch, roll, yaw;
        pitch = !std::isnan(settings.rotation.pitch) ? settings.rotation.pitch : 0;
        roll = !std::isnan(settings.rotation.roll) ? settings.rotation.roll : 0;
        yaw = !std::isnan(settings.rotation.yaw) ? settings.rotation.yaw : 0;
        relative_pose.orientation = VectorMath::toQuaternion(
            Utils::degreesToRadians(pitch),   //pitch - rotation around Y axis
            Utils::degreesToRadians(roll),    //roll  - rotation around X axis
            Utils::degreesToRadians(yaw)      //yaw   - rotation around Z axis
        );
    }
};


}} //namespace
#endif
