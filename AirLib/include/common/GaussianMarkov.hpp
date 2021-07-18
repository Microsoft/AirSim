// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef common_utils_GaussianMarkov_hpp
#define common_utils_GaussianMarkov_hpp

#include "common/Common.hpp"
#include "UpdatableObject.hpp"
#include <list>
#include "common_utils/RandomGenerator.hpp"

namespace msr
{
namespace airlib
{

    class GaussianMarkov : public UpdatableObject
    {
    public:
        GaussianMarkov()
        {
        }
        GaussianMarkov(real_T tau, real_T sigma, real_T initial_output = 0) //in seconds
        {
            initialize(tau, sigma, initial_output);
        }
        void initialize(real_T tau, real_T sigma, real_T initial_output = 0) //in seconds
        {
            tau_ = tau;
            sigma_ = sigma;
            rand_ = RandomGeneratorGausianR(0.0f, 1.0f);

            if (std::isnan(initial_output))
                initial_output_ = getNextRandom() * sigma_;
            else
                initial_output_ = initial_output;
        }

        //*** Start: UpdatableState implementation ***//
        virtual void resetImplementation() override
        {
            last_time_ = clock()->nowNanos();
            output_ = initial_output_;
            rand_.reset();
        }

        virtual void update() override
        {
            /*
        Ref:
            A Comparison between Different Error Modeling of MEMS Applied to GPS/INS Integrated Systems
            Quinchia, sec 3.2, https://www.ncbi.nlm.nih.gov/pmc/articles/PMC3812568/
        
            A Study of the Effects of Stochastic Inertial Sensor Errors in Dead-Reckoning Navigation
            John H Wall, 2007, eq 2.5, pg 13, http://etd.auburn.edu/handle/10415/945
        */

            UpdatableObject::update();

            TTimeDelta dt = clock()->updateSince(last_time_);

            double alpha = exp(-dt / tau_);
            output_ = static_cast<real_T>(alpha * output_ + (1 - alpha) * getNextRandom() * sigma_);
        }
        //*** End: UpdatableState implementation ***//

        real_T getNextRandom()
        {
            return rand_.next();
        }

        real_T getOutput() const
        {
            return output_;
        }

    private:
        RandomGeneratorGausianR rand_;
        real_T tau_, sigma_;
        real_T output_, initial_output_;
        TTimePoint last_time_;
    };
}
} //namespace
#endif
