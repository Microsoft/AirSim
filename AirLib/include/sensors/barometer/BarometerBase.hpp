// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_BarometerBase_hpp
#define msr_airlib_BarometerBase_hpp

#include "sensors/SensorBase.hpp"

namespace msr
{
namespace airlib
{

    class BarometerBase : public SensorBase
    {
    public:
        BarometerBase(const std::string& sensor_name = "")
            : SensorBase(sensor_name)
        {
        }

    public: //types
        struct Output
        { //same fields as ROS message
            TTimePoint time_stamp;
            real_T altitude; //meters
            real_T pressure; //Pascal
            real_T qnh;
        };

    public:
        virtual void reportState(StateReporter& reporter) override
        {
            //call base
            UpdatableObject::reportState(reporter);

            reporter.writeValue("Baro-Alt", output_.altitude);
            reporter.writeValue("Baro-Prs", output_.pressure);
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
