// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_FrequencyLimiter_hpp
#define msr_airlib_FrequencyLimiter_hpp

#include "common/Common.hpp"
#include "UpdatableObject.hpp"
#include "common/Common.hpp"

namespace msr
{
namespace airlib
{

    class FrequencyLimiter : public UpdatableObject
    {
    public:
        FrequencyLimiter(real_T frequency = Utils::max<float>(), real_T startup_delay = 0)
        {
            initialize(frequency, startup_delay);
        }

        void initialize(real_T frequency = Utils::max<float>(), real_T startup_delay = 0)
        {
            frequency_ = frequency;
            startup_delay_ = startup_delay;
        }

        //*** Start: UpdatableState implementation ***//
        virtual void resetImplementation() override
        {
            last_time_ = clock()->nowNanos();
            first_time_ = last_time_;

            if (Utils::isApproximatelyZero(frequency_))
                interval_size_sec_ = 1E10; //some high number
            else
                interval_size_sec_ = 1.0f / frequency_;

            elapsed_total_sec_ = 0;
            elapsed_interval_sec_ = 0;
            last_elapsed_interval_sec_ = 0;
            update_count_ = 0;
            interval_complete_ = false;
            startup_complete_ = false;
        }

        virtual void failResetUpdateOrdering(std::string err) override
        {
            // Do nothing.
            // Disable checks for reset/update sequence because
            // this object may get created but not used.
        }

        virtual void update() override
        {
            UpdatableObject::update();

            elapsed_total_sec_ = clock()->elapsedSince(first_time_);
            elapsed_interval_sec_ = clock()->elapsedSince(last_time_);
            ++update_count_;

            //if startup_delay_ > 0 then we consider startup_delay_ as the first interval
            //that needs to be complete
            if (!startup_complete_) {
                if (Utils::isDefinitelyGreaterThan(startup_delay_, 0.0f)) {
                    //see if we have spent startup_delay_ time yet
                    interval_complete_ = elapsed_interval_sec_ >= startup_delay_;
                }
                else //no special startup delay is needed
                    startup_complete_ = true;
            }

            //if startup is complete, we will do regular intervals from now one
            if (startup_complete_)
                interval_complete_ = elapsed_interval_sec_ >= interval_size_sec_;

            //when any interval is done, reset the state and repeat
            if (interval_complete_) {
                last_elapsed_interval_sec_ = elapsed_interval_sec_;
                last_time_ = clock()->nowNanos();
                elapsed_interval_sec_ = 0;
                startup_complete_ = true;
            }
        }
        //*** End: UpdatableState implementation ***//

        TTimeDelta getElapsedTotalSec() const
        {
            return elapsed_total_sec_;
        }

        TTimeDelta getElapsedIntervalSec() const
        {
            return elapsed_interval_sec_;
        }

        TTimeDelta getLastElapsedIntervalSec() const
        {
            return last_elapsed_interval_sec_;
        }

        bool isWaitComplete() const
        {
            return interval_complete_;
        }

        bool isStartupComplete() const
        {
            return startup_complete_;
        }

        uint getUpdateCount() const
        {
            return update_count_;
        }

    private:
        real_T interval_size_sec_;
        TTimeDelta elapsed_total_sec_;
        TTimeDelta elapsed_interval_sec_;
        TTimeDelta last_elapsed_interval_sec_;
        uint update_count_;
        real_T frequency_;
        real_T startup_delay_;
        bool interval_complete_;
        bool startup_complete_;
        TTimePoint last_time_, first_time_;
    };
}
} //namespace
#endif
