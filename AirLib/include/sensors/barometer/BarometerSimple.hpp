// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_Barometer_hpp
#define msr_airlib_Barometer_hpp

#include <random>
#include "common/Common.hpp"
#include "common/EarthUtils.hpp"
#include "BarometerSimpleParams.hpp"
#include "BarometerBase.hpp"
#include "common/GaussianMarkov.hpp"
#include "common/DelayLine.hpp"
#include "common/FrequencyLimiter.hpp"

namespace msr
{
namespace airlib
{

    class BarometerSimple : public BarometerBase
    {
    public:
        BarometerSimple(const AirSimSettings::BarometerSetting& setting = AirSimSettings::BarometerSetting())
            : BarometerBase(setting.sensor_name)
        {
            // initialize params
            params_.initializeFromSettings(setting);

            //GM process that would do random walk for pressure factor
            pressure_factor_.initialize(params_.pressure_factor_tau, params_.pressure_factor_sigma, 0);

            uncorrelated_noise_ = RandomGeneratorGausianR(0.0f, params_.uncorrelated_noise_sigma);
            //correlated_noise_.initialize(params_.correlated_noise_tau, params_.correlated_noise_sigma, 0.0f);

            //initialize frequency limiter
            freq_limiter_.initialize(params_.update_frequency, params_.startup_delay);
            delay_line_.initialize(params_.update_latency);
        }

        //*** Start: UpdatableState implementation ***//
        virtual void resetImplementation() override
        {
            pressure_factor_.reset();
            //correlated_noise_.reset();
            uncorrelated_noise_.reset();

            freq_limiter_.reset();
            delay_line_.reset();

            delay_line_.push_back(getOutputInternal());
        }

        virtual void update() override
        {
            BarometerBase::update();

            freq_limiter_.update();

            if (freq_limiter_.isWaitComplete()) {
                delay_line_.push_back(getOutputInternal());
            }

            delay_line_.update();

            if (freq_limiter_.isWaitComplete())
                setOutput(delay_line_.getOutput());
        }
        //*** End: UpdatableState implementation ***//

        virtual ~BarometerSimple() = default;

    private: //methods
        Output getOutputInternal()
        {
            Output output;
            const GroundTruth& ground_truth = getGroundTruth();

            auto altitude = ground_truth.environment->getState().geo_point.altitude;
            auto pressure = EarthUtils::getStandardPressure(altitude);

            //add drift in pressure, about 10m change per hour using default settings.
            pressure_factor_.update();
            pressure += pressure * pressure_factor_.getOutput();

            //add noise in pressure (about 0.2m sigma)
            pressure += uncorrelated_noise_.next();

            output.pressure = pressure - EarthUtils::SeaLevelPressure + params_.qnh * 100.0f;

            //apply altimeter formula
            //https://en.wikipedia.org/wiki/Pressure_altitude
            //TODO: use same formula as in driver code?
            output.altitude = (1 - pow(pressure / EarthUtils::SeaLevelPressure, 0.190284f)) * 145366.45f * 0.3048f;
            output.qnh = params_.qnh;

            output.time_stamp = clock()->nowNanos();

            return output;
        }

    private:
        BarometerSimpleParams params_;

        GaussianMarkov pressure_factor_;
        //GaussianMarkov correlated_noise_;
        RandomGeneratorGausianR uncorrelated_noise_;

        FrequencyLimiter freq_limiter_;
        DelayLine<Output> delay_line_;
    };
}
} //namespace
#endif
