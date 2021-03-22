// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_BarometerSimpleParams_hpp
#define msr_airlib_BarometerSimpleParams_hpp

#include "common/Common.hpp"


namespace msr { namespace airlib {


struct BarometerSimpleParams {
    //user specified sea level pressure is specified in hPa units
    real_T qnh = EarthUtils::SeaLevelPressure / 100.0f; // hPa

    //sea level min,avh,max = 950,1013,1050 ie approx 3.65% variation
    //regular pressure changes in quiet conditions are taken as 1/20th of this
    //Mariner's Pressure Atlas, David Burch, 2014
    //GM process may generate ~70% of sigma in tau interval
    //This means below config may produce ~10m variance per hour

    //https://www.starpath.com/ebooksamples/9780914025382_sample.pdf
    real_T pressure_factor_sigma = 0.0365f / 20;
    real_T pressure_factor_tau = 3600;

    /*
    Ref: A Stochastic Approach to Noise Modeling for Barometric Altimeters
         Angelo Maria Sabatini* and Vincenzo Genovese
         Sample values are from Table 1
         https://www.ncbi.nlm.nih.gov/pmc/articles/PMC3871085/
         This is however not used because numbers mentioned in paper doesn't match experiments.

         real_T correlated_noise_sigma = 0.27f;
         real_T correlated_noise_tau = 0.87f;
         real_T unnorrelated_noise_sigma = 0.24f;
    */

    //Experiments for MEAS MS56112 sensor shows 0.021mbar, datasheet has resoultion of 0.027mbar @ 1024
    //http://www.te.com/commerce/DocumentDelivery/DDEController?Action=srchrtrv&DocNm=MS5611-01BA03&DocType=Data+Sheet&DocLang=English
    real_T unnorrelated_noise_sigma = 0.027f * 100;
    //jMavSim uses below
    //real_T unnorrelated_noise_sigma = 0.1f;

    //see PX4 param reference for EKF: https://dev.px4.io/en/advanced/parameter_reference.html
    real_T update_latency = 0.0f;    //sec
    real_T update_frequency = 50;    //Hz
    real_T startup_delay = 0;        //sec

    Pose relative_pose;

    void initializeFromSettings(const AirSimSettings::BarometerSetting& settings)
    {
        qnh = settings.qnh;
        pressure_factor_sigma = settings.pressure_factor_sigma;
        pressure_factor_tau = settings.pressure_factor_tau;
        unnorrelated_noise_sigma = settings.unnorrelated_noise_sigma;
        update_latency = settings.update_latency;
        update_frequency = settings.update_frequency;
        startup_delay = settings.startup_delay;

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
            Utils::degreesToRadians(yaw)     //yaw   - rotation around Z axis
        );
    }
};


}} //namespace
#endif
