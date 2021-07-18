// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_MagnetometerBase_hpp
#define msr_airlib_MagnetometerBase_hpp

#include "sensors/SensorBase.hpp"

namespace msr
{
namespace airlib
{

    class MagnetometerBase : public SensorBase
    {
    public:
        MagnetometerBase(const std::string& sensor_name = "")
            : SensorBase(sensor_name)
        {
        }

    public: //types
        struct Output
        { //same fields as ROS message
            TTimePoint time_stamp;
            Vector3r magnetic_field_body; //in Gauss
            vector<real_T> magnetic_field_covariance; //9 elements 3x3 matrix
        };

    public:
        virtual void reportState(StateReporter& reporter) override
        {
            //call base
            UpdatableObject::reportState(reporter);

            reporter.writeValue("Mag-Vec", output_.magnetic_field_body);
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
