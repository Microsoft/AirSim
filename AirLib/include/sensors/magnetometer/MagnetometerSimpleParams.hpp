// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_MagnetometerSimpleParams_hpp
#define msr_airlib_MagnetometerSimpleParams_hpp

#include "common/Common.hpp"

namespace msr
{
namespace airlib
{

    struct MagnetometerSimpleParams
    {
        enum ReferenceSource
        {
            ReferenceSource_Constant,
            ReferenceSource_DipoleModel
        };

        Vector3r noise_sigma = Vector3r(0.005f, 0.005f, 0.005f); //5 mgauss as per specs sheet (RMS is same as stddev) https://goo.gl/UOz6FT
        real_T scale_factor = 1.0f;
        Vector3r noise_bias = Vector3r(0.0f, 0.0f, 0.0f); //no offset as per specsheet (zero gauss level) https://goo.gl/UOz6FT
        float ref_update_frequency = 0.2f; //Hz

        //use dipole model if there is enough compute power available
        bool dynamic_reference_source = true;
        ReferenceSource ref_source = ReferenceSource::ReferenceSource_DipoleModel;
        //bool dynamic_reference_source = false;
        //ReferenceSource ref_source = ReferenceSource::ReferenceSource_Constant;

        //see PX4 param reference for EKF: https://dev.px4.io/en/advanced/parameter_reference.html
        real_T update_latency = 0.0f; //sec: from PX4 doc
        real_T update_frequency = 50; //Hz
        real_T startup_delay = 0; //sec

        void initializeFromSettings(const AirSimSettings::MagnetometerSetting& settings)
        {
            const auto& json = settings.settings;
            float noise = json.getFloat("NoiseSigma", noise_sigma.x());
            noise_sigma = Vector3r(noise, noise, noise);
            scale_factor = json.getFloat("ScaleFactor", scale_factor);
            float bias = json.getFloat("NoiseBias", noise_bias.x());
            noise_bias = Vector3r(bias, bias, bias);
            update_latency = json.getFloat("UpdateLatency", update_latency);
            update_frequency = json.getFloat("UpdateFrequency", update_frequency);
            startup_delay = json.getFloat("StartupDelay", startup_delay);
        }
    };
}
} //namespace
#endif
