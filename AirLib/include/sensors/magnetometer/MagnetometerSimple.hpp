// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_MagnetometerSimple_hpp
#define msr_airlib_MagnetometerSimple_hpp

#include <random>
#include "common/Common.hpp"
#include "common/EarthUtils.hpp"
#include "MagnetometerSimpleParams.hpp"
#include "MagnetometerBase.hpp"
#include "common/FrequencyLimiter.hpp"
#include "common/DelayLine.hpp"

namespace msr
{
namespace airlib
{

    class MagnetometerSimple : public MagnetometerBase
    {
    public:
        MagnetometerSimple(const AirSimSettings::MagnetometerSetting& setting = AirSimSettings::MagnetometerSetting())
            : MagnetometerBase(setting.sensor_name)
        {
            // initialize params
            params_.initializeFromSettings(setting);

            noise_vec_ = RandomVectorGaussianR(Vector3r::Zero(), params_.noise_sigma);
            bias_vec_ = RandomVectorR(-params_.noise_bias, params_.noise_bias).next();

            //initialize frequency limiter
            freq_limiter_.initialize(params_.update_frequency, params_.startup_delay);
            delay_line_.initialize(params_.update_latency);
        }

        //*** Start: UpdatableObject implementation ***//
        virtual void resetImplementation() override
        {
            //Ground truth is reset before sensors are reset
            updateReference(getGroundTruth());
            noise_vec_.reset();

            freq_limiter_.reset();
            delay_line_.reset();

            delay_line_.push_back(getOutputInternal());
        }

        virtual void update() override
        {
            MagnetometerBase::update();

            freq_limiter_.update();

            if (freq_limiter_.isWaitComplete()) {
                delay_line_.push_back(getOutputInternal());
            }

            delay_line_.update();

            if (freq_limiter_.isWaitComplete())
                setOutput(delay_line_.getOutput());
        }
        //*** End: UpdatableObject implementation ***//

        virtual ~MagnetometerSimple() = default;

    private: //methods
        void updateReference(const GroundTruth& ground_truth)
        {
            switch (params_.ref_source) {
            case MagnetometerSimpleParams::ReferenceSource::ReferenceSource_Constant:
                // Constant magnetic field for Seattle
                magnetic_field_true_ = Vector3r(0.34252f, 0.09805f, 0.93438f);
                break;
            case MagnetometerSimpleParams::ReferenceSource::ReferenceSource_DipoleModel:
                magnetic_field_true_ = EarthUtils::getMagField(ground_truth.environment->getState().geo_point) * 1E4f; //Tesla to Gauss
                break;
            default:
                throw std::invalid_argument("magnetic reference source type is not recognized");
            }
        }
        Output getOutputInternal()
        {
            Output output;
            const GroundTruth& ground_truth = getGroundTruth();

            if (params_.dynamic_reference_source)
                updateReference(ground_truth);

            // Calculate the magnetic field noise.
            output.magnetic_field_body = VectorMath::transformToBodyFrame(magnetic_field_true_,
                                                                          ground_truth.kinematics->pose.orientation,
                                                                          true) *
                                             params_.scale_factor +
                                         noise_vec_.next() + bias_vec_;

            // todo output.magnetic_field_covariance ?
            output.time_stamp = clock()->nowNanos();

            return output;
        }

    private:
        RandomVectorGaussianR noise_vec_;
        Vector3r bias_vec_;

        Vector3r magnetic_field_true_;
        MagnetometerSimpleParams params_;

        FrequencyLimiter freq_limiter_;
        DelayLine<Output> delay_line_;
    };
}
} //namespace
#endif
