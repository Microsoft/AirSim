#ifndef CommonUtils_Timer_hpp
#define CommonUtils_Timer_hpp
#include <chrono>
#include "Utils.hpp"

namespace common_utils
{
class Timer
{
public:
    Timer()
    {
        started_ = false;
    }
    void start()
    {
        started_ = true;
        start_ = now();
    }
    void stop()
    {
        started_ = false;
        end_ = now();
    }
    double seconds()
    {
        auto diff = static_cast<double>(end() - start_);
        return diff / 1000000.0;
    }
    double milliseconds()
    {
        return static_cast<double>(end() - start_) / 1000.0;
    }
    double microseconds()
    {
        return static_cast<double>(end() - start_);
    }
    bool started()
    {
        return started_;
    }

private:
    int64_t now()
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
    int64_t end()
    {
        if (started_) {
            // not stopped yet, so return "elapsed time so far".
            end_ = now();
        }
        return end_;
    }
    int64_t start_;
    int64_t end_;
    bool started_;
};
}
#endif