// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef airsim_core_Environment_hpp
#define airsim_core_Environment_hpp

#include "common/Common.hpp"
#include "common/UpdatableObject.hpp"
#include "common/CommonStructs.hpp"
#include "common/EarthUtils.hpp"
#include "common/GeodeticConverter.hpp"

namespace msr
{
namespace airlib
{

    class Environment : public UpdatableObject
    {
    public:
        struct State
        {
            //these fields must be set at initialization time
            Vector3r position;
            GeoPoint geo_point;

            //these fields are computed
            Vector3r gravity;
            real_T air_pressure;
            real_T temperature;
            real_T air_density;

            State()
            {
            }
            State(const Vector3r& position_val, const GeoPoint& geo_point_val)
                : position(position_val), geo_point(geo_point_val)
            {
            }
        };

    public:
        Environment()
        {
            //allow default constructor with later call for initialize
        }
        Environment(const State& initial)
        {
            initialize(initial);
        }
        void initialize(const State& initial)
        {
            initial_ = initial;

            setHomeGeoPoint(initial_.geo_point);

            updateState(initial_);
        }

        void setHomeGeoPoint(const GeoPoint& home_geo_point)
        {
            home_geo_point_ = HomeGeoPoint(home_geo_point);
            geodetic_converter_.setHome(home_geo_point);
        }

        GeoPoint getHomeGeoPoint() const
        {
            return home_geo_point_.home_geo_point;
        }

        //in local NED coordinates
        void setPosition(const Vector3r& position)
        {
            current_.position = position;
        }

        const State& getInitialState() const
        {
            return initial_;
        }
        const State& getState() const
        {
            return current_;
        }
        State& getState()
        {
            return current_;
        }

        virtual void update() override
        {
            updateState(current_);
        }

    protected:
        virtual void resetImplementation() override
        {
            current_ = initial_;
        }

        virtual void failResetUpdateOrdering(std::string err) override
        {
            unused(err);
            //Do nothing.
            //The environment gets reset() twice without an update() inbetween,
            //via MultirotorPawnSimApi::reset() and CarSimApi::reset(), because
            //those functions directly reset an environment, and also call other reset()s that reset the same environment.
        }

    private:
        void updateState(State& state)
        {
            geodetic_converter_.ned2Geodetic(state.position, state.geo_point);

            real_T geo_pot = EarthUtils::getGeopotential(state.geo_point.altitude / 1000.0f);
            state.temperature = EarthUtils::getStandardTemperature(geo_pot);
            state.air_pressure = EarthUtils::getStandardPressure(geo_pot, state.temperature);
            state.air_density = EarthUtils::getAirDensity(state.air_pressure, state.temperature);

            //TODO: avoid recalculating square roots
            state.gravity = Vector3r(0, 0, EarthUtils::getGravity(state.geo_point.altitude));
        }

    private:
        State initial_, current_;
        HomeGeoPoint home_geo_point_;
        GeodeticConverter geodetic_converter_;
    };
}
} //namespace
#endif
