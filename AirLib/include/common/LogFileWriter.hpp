// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef common_utils_LogFileWriter_hpp
#define common_utils_LogFileWriter_hpp

#include <iostream>
#include <fstream>
#include "common/Common.hpp"

namespace msr
{
namespace airlib
{

    class LogFileWriter
    {
    public:
        LogFileWriter()
        {
        }
        LogFileWriter(const string& file_name, bool enabled = true)
        {
            open(file_name, enabled);
        }
        ~LogFileWriter()
        {
            close();
        }
        void open(const string& file_name, bool enabled = true)
        {
            close();
            file_name_ = file_name;
            enabled_ = enabled;

            if (enabled_)
                log_file_ = std::ofstream(file_name);
        }
        void close()
        {
            if (log_file_.is_open())
                log_file_.close();
        }

        template <typename T>
        void write(const T& val)
        {
            if (enabled_)
                log_file_ << val << "\t";
        }
        void write(const Vector3r& vec)
        {
            if (enabled_)
                log_file_ << vec.x() << "\t" << vec.y() << "\t" << vec.z() << "\t";
        }
        void write(const Quaternionr& q)
        {
            if (enabled_) {
                real_T p, r, y;
                VectorMath::toEulerianAngle(q, p, r, y);
                log_file_ << Utils::radiansToDegrees(r) << "\t" << Utils::radiansToDegrees(p) << "\t" << Utils::radiansToDegrees(y) << "\t";
            }
        }
        void endl()
        {
            if (enabled_) {
                log_file_ << std::endl;
            }
        }

    private:
        std::ofstream log_file_;
        string file_name_;
        bool enabled_ = false;
    };
}
} //namespace
#endif
