// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef airsim_core_StateReporter_hpp
#define airsim_core_StateReporter_hpp

#include <sstream>
#include <string>
#include <iomanip>
#include "common/Common.hpp"

namespace msr
{
namespace airlib
{

    /*
    This class is simple key-value reporting provider. It can't inherit from
    UpdatableObject or we will have circular dependency. This class essentially
    just provides formatted write APIs over string buffer. The UpdatableObject
    version is provided by StateReporterWrapper. We expect everyone to use
    StateReporterWrapper instead of StateReporter directly.
*/
    class StateReporter
    {
    public:
        StateReporter(int float_precision = 3, bool is_scientific_notation = false)
        {
            initialize(float_precision, is_scientific_notation);
        }
        void initialize(int float_precision = 3, bool is_scientific_notation = false)
        {
            float_precision_ = float_precision;
            is_scientific_notation_ = is_scientific_notation;

            if (float_precision_ >= 0) {
                ss_ << std::setprecision(float_precision_);
                if (is_scientific_notation_)
                    ss_ << std::scientific;
                else
                    ss_ << std::fixed;
            }
        }

        void clear()
        {
            ss_.str(std::string());
            ss_.clear();
        }

        string getOutput() const
        {
            return ss_.str();
        }

        //write APIs - heading
        //TODO: need better line end handling
        void startHeading(string heading, uint heading_size, uint columns = 20)
        {
            unused(heading_size);
            unused(columns);
            ss_ << "\n";
            ss_ << heading;
        }
        void endHeading(bool end_line, uint heading_size, uint columns = 20)
        {
            if (end_line)
                ss_ << "\n";
            for (int lines = heading_size; lines > 0; --lines)
                ss_ << std::string(columns, '_') << "\n";
        }
        void writeHeading(string heading, uint heading_size = 0, uint columns = 20)
        {
            startHeading(heading, heading_size);
            endHeading(true, heading_size, columns);
        }

        //write APIs - specialized objects
        void writeValue(string name, const Vector3r& vector)
        {
            ss_ << name << ": "
                << "(" << vector.norm() << ") - "
                << "[" << vector.x() << ", " << vector.y() << ", " << vector.z() << "]\n";
        }
        void writeValue(string name, const Quaternionr& quat)
        {
            real_T pitch, roll, yaw;
            VectorMath::toEulerianAngle(quat, pitch, roll, yaw);

            ss_ << name << ":\n"
                << "    euler: (" << roll << ", " << pitch << ", " << yaw << ")\n"
                << "    quat: [" << quat.w() << ", " << quat.x() << ", " << quat.y() << ", " << quat.z() << "]\n";
        }

        //write APIs - generic values
        template <typename T>
        void writeValue(string name, const T& r)
        {
            writeNameOnly(name);
            writeValueOnly(r, true);
        }
        void writeNameOnly(string name)
        {
            ss_ << name << ": ";
        }
        template <typename T>
        void writeValueOnly(const T& r, bool end_line_or_tab = false)
        {
            ss_ << r;

            if (end_line_or_tab)
                ss_ << "\n";
            else
                ss_ << "\t";
        }
        void endl()
        {
            ss_ << std::endl;
        }

    private:
        std::stringstream ss_;

        int float_precision_ = 2;
        bool is_scientific_notation_ = false;
    };
}
} //namespace
#endif
