// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_ImuBase_hpp
#define msr_airlib_ImuBase_hpp

#include "sensors/SensorBase.hpp"

namespace msr
{
namespace airlib
{

    class ImuBase : public SensorBase
    {
    public:
        ImuBase(const std::string& sensor_name = "")
            : SensorBase(sensor_name)
        {
        }

    public: //types
        struct Output
        { //structure is same as ROS IMU message
            EIGEN_MAKE_ALIGNED_OPERATOR_NEW
            TTimePoint time_stamp;
            Quaternionr orientation;
            Vector3r angular_velocity;
            Vector3r linear_acceleration;
        };

    public:
        virtual void reportState(StateReporter& reporter) override
        {
            //call base
            UpdatableObject::reportState(reporter);

            reporter.writeValue("IMU-Ang", output_.angular_velocity);
            reporter.writeValue("IMU-Lin", output_.linear_acceleration);
        }

        const Output& getOutput() const
        {
            return output_;
        }

    protected:
        void setOutput(const Output& output)
        {
            output_ = output;
        }

    private:
        Output output_;
    };
}
} //namespace
#endif
