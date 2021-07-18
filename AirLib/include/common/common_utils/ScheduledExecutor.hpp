// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef commn_utils_ScheduledExecutor_hpp
#define commn_utils_ScheduledExecutor_hpp

#include <thread>
#include <chrono>
#include <functional>
#include <atomic>
#include <system_error>
#include <mutex>
#include <cstdint>

namespace common_utils
{

class ScheduledExecutor
{
public:
    ScheduledExecutor()
    {
    }

    ScheduledExecutor(const std::function<bool(uint64_t)>& callback, uint64_t period_nanos)
    {
        initialize(callback, period_nanos);
    }

    ~ScheduledExecutor()
    {
        stop();
    }

    void initialize(const std::function<bool(uint64_t)>& callback, uint64_t period_nanos)
    {
        callback_ = callback;
        period_nanos_ = period_nanos;
        started_ = false;
        frame_countdown_enabled_ = false;
    }

    void start()
    {
        started_ = true;
        is_first_period_ = true;

        initializePauseState();

        sleep_time_avg_ = 0;
        Utils::cleanupThread(th_);
        th_ = std::thread(&ScheduledExecutor::executorLoop, this);
    }

    void pause(bool is_paused)
    {
        paused_ = is_paused;
        pause_period_start_ = 0; // cancel any pause period.
    }

    bool isPaused() const
    {
        return paused_;
    }

    void pauseForTime(double seconds)
    {
        pause_period_start_ = nanos();
        pause_period_ = static_cast<TTimeDelta>(1E9 * seconds);
        paused_ = true;
    }

    void continueForTime(double seconds)
    {
        pause_period_start_ = nanos();
        pause_period_ = static_cast<TTimeDelta>(1E9 * seconds);
        paused_ = false;
    }

    void continueForFrames(uint32_t frames)
    {
        pause_period_start_ = 0; // cancel any pause period.
        frame_countdown_enabled_ = true;
        targetFrameNumber_ = frames + currentFrameNumber_;
        paused_ = false;
    }

    void setFrameNumber(uint32_t frameNumber)
    {
        currentFrameNumber_ = frameNumber;
    }

    void stop()
    {
        if (started_) {
            started_ = false;
            initializePauseState();

            try {
                if (th_.joinable()) {
                    th_.join();
                }
            }
            catch (const std::system_error& /* e */) {
            }
        }
    }

    bool isRunning() const
    {
        return started_ && !paused_;
    }

    double getSleepTimeAvg() const
    {
        //TODO: make this function thread safe by using atomic types
        //right now this is not implemented for performance and that
        //return of this function is purely informational/debugging purposes
        return sleep_time_avg_;
    }

    void lock()
    {
        mutex_.lock();
    }
    void unlock()
    {
        mutex_.unlock();
    }

private:
    void initializePauseState()
    {
        paused_ = false;
        pause_period_start_ = 0;
        pause_period_ = 0;
    }

private:
    typedef std::chrono::high_resolution_clock clock;
    typedef uint64_t TTimePoint;
    typedef uint64_t TTimeDelta;
    template <typename T>
    using duration = std::chrono::duration<T>;

    static TTimePoint nanos()
    {
        return clock::now().time_since_epoch().count();
    }

    static void sleep_for(TTimePoint delay_nanos)
    {
        /*
        This is spin loop implementation which may be suitable for sub-millisecond resolution.
        //TODO: investigate below alternatives
        On Windows we can use multimedia timers however this requires including entire Win32 header.
        On Linux we can use nanosleep however below 2ms delays in real-time scheduler settings this 
        probbaly does spin loop anyway.

        */

        if (delay_nanos >= 5000000LL) { //put thread to sleep
            std::this_thread::sleep_for(std::chrono::duration<double>(delay_nanos / 1.0E9));
        }
        else { //for more precise timing, do spinning
            auto start = nanos();
            while ((nanos() - start) < delay_nanos) {
                std::this_thread::yield();
                //std::this_thread::sleep_for(std::chrono::duration<double>(0));
            }
        }
    }

    void executorLoop()
    {
        TTimePoint call_end = nanos();
        while (started_) {
            TTimePoint period_start = nanos();
            TTimeDelta since_last_call = period_start - call_end;

            if (frame_countdown_enabled_) {
                if (targetFrameNumber_ <= currentFrameNumber_) {
                    if (!isPaused())
                        pause(true);

                    frame_countdown_enabled_ = false;
                }
            }

            if (pause_period_start_ > 0) {
                if (nanos() - pause_period_start_ >= pause_period_) {
                    pause(!isPaused());
                    pause_period_start_ = 0;
                }
            }

            //is this first loop?
            if (!is_first_period_) {
                if (!paused_) {
                    //when we are doing work, don't let other thread to cause contention
                    std::lock_guard<std::mutex> locker(mutex_);

                    bool result = callback_(since_last_call);
                    if (!result) {
                        started_ = result;
                    }
                }
            }
            else
                is_first_period_ = false;

            call_end = nanos();

            TTimeDelta elapsed_period = nanos() - period_start;
            //prevent underflow: https://github.com/Microsoft/AirSim/issues/617
            TTimeDelta delay_nanos = period_nanos_ > elapsed_period ? period_nanos_ - elapsed_period : 0;
            //moving average of how much we are sleeping
            sleep_time_avg_ = 0.25f * sleep_time_avg_ + 0.75f * delay_nanos;
            if (delay_nanos > 0 && started_)
                sleep_for(delay_nanos);
        }
    }

private:
    uint64_t period_nanos_;
    std::thread th_;
    std::function<bool(uint64_t)> callback_;
    bool is_first_period_;
    std::atomic_bool started_;
    std::atomic_bool paused_;
    std::atomic<TTimeDelta> pause_period_;
    std::atomic<TTimePoint> pause_period_start_;
    uint32_t currentFrameNumber_;
    uint32_t targetFrameNumber_;
    std::atomic_bool frame_countdown_enabled_;

    double sleep_time_avg_;

    std::mutex mutex_;
};
}
#endif
