// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_GpsSimpleParams_hpp
#define msr_airlib_GpsSimpleParams_hpp

#include "common/Common.hpp"

namespace msr
{
namespace airlib
{

    struct GpsSimpleParams
    {
        real_T eph_time_constant = 0.9f, epv_time_constant = 0.9f;
        real_T eph_initial = 100.0f, epv_initial = 100.0f; //initially fully diluted positions
        real_T eph_final = 0.1f, epv_final = 0.1f; // PX4 won't converge GPS sensor fusion until we get to 10% accuracty.
        real_T eph_min_3d = 3.0f, eph_min_2d = 4.0f;

        real_T update_latency = 0.2f; //sec
        real_T update_frequency = 50; //Hz
        real_T startup_delay = 1; //sec

        void initializeFromSettings(const AirSimSettings::GpsSetting& settings)
        {
            const auto& json = settings.settings;
            eph_time_constant = json.getFloat("EPH_TimeConstant", eph_time_constant);
            epv_time_constant = json.getFloat("EPV_TimeConstant", epv_time_constant);
            eph_initial = json.getFloat("EphInitial", eph_initial);
            epv_initial = json.getFloat("EpvInitial", epv_initial);
            eph_final = json.getFloat("EphFinal", eph_final);
            epv_final = json.getFloat("EpvFinal", epv_final);
            eph_min_3d = json.getFloat("EphMin3d", eph_min_3d);
            eph_min_2d = json.getFloat("EphMin2d", eph_min_2d);
            update_latency = json.getFloat("UpdateLatency", update_latency);
            update_frequency = json.getFloat("UpdateFrequency", update_frequency);
            startup_delay = json.getFloat("StartupDelay", startup_delay);
        }
    };
}
} //namespace
#endif
