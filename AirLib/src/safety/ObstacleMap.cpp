// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

//in header only mode, control library is not available
#ifndef AIRLIB_HEADER_ONLY

#include <thread>
#include "safety/ObstacleMap.hpp"
#include "common/common_utils/Utils.hpp"

namespace msr
{
namespace airlib
{

    ObstacleMap::ObstacleMap(int ticks, bool odd_blindspots)
        : distances_(ticks, Utils::max<float>() / 2), confidences_(ticks, 1), ticks_(ticks), blindspots_(ticks_, false) //init with all distances at max/2 (setting it to max can cause overflow later)
    {
        if (odd_blindspots)
            for (uint i = 1; i < distances_.size(); i += 2)
                blindspots_.at(i) = true;
    }

    //handles +/- tick and wraps around circle
    //return value of this function is always >= 0 and < ticks_ (i.e. valid indices)
    int ObstacleMap::wrap(int tick) const
    {
        int iw = tick % ticks_;
        if (iw < 0)
            iw = ticks_ + iw;
        return iw;
    }

    void ObstacleMap::update(float distance, int tick, int window, float confidence)
    {
        std::lock_guard<std::mutex> lock(mutex_); //lock the map before update

        //update the specified window on the map
        for (int i = tick - window; i <= tick + window; ++i) {
            int iw = wrap(i);
            distances_[iw] = distance;
            confidences_[iw] = confidence;
        }
    }

    void ObstacleMap::update(float distances[], float confidences[])
    {
        std::lock_guard<std::mutex> lock(mutex_); //lock the map before update

        std::copy(distances, distances + ticks_, std::begin(distances_));
        std::copy(confidences, confidences + ticks_, std::begin(confidences_));
    }

    void ObstacleMap::setBlindspot(int tick, bool blindspot)
    {
        blindspots_.at(tick) = blindspot;
    }

    ObstacleMap::ObstacleInfo ObstacleMap::hasObstacle_(int from_tick, int to_tick) const
    {
        //make sure from <= to
        if (from_tick > to_tick) {
            //normalize the ticks so both are valid indices
            from_tick = wrap(from_tick);
            to_tick = wrap(to_tick);

            //if from is still larger then
            //to ticks is then added one full circle to make it larger than from_tick
            if (from_tick > to_tick)
                to_tick += ticks_;
        }

        //find closest obstacle in given window
        ObstacleMap::ObstacleInfo obs;
        obs.distance = Utils::max<float>();
        obs.confidence = 0;
        for (int i = from_tick; i <= to_tick; ++i) {
            int iw = wrap(i);
            if (obs.distance > distances_[iw]) {
                obs.tick = iw;
                obs.distance = distances_[iw];
                obs.confidence = confidences_[iw];
            }
        }

        return obs;
    }

    ObstacleMap::ObstacleInfo ObstacleMap::hasObstacle(int from_tick, int to_tick)
    {
        std::lock_guard<std::mutex> lock(mutex_); //lock the map before query

        if (blindspots_.at(wrap(from_tick)))
            from_tick--;
        if (blindspots_.at(wrap(to_tick)))
            to_tick++;

        return hasObstacle_(from_tick, to_tick);
    }

    //search whole map to find closest obstacle
    ObstacleMap::ObstacleInfo ObstacleMap::getClosestObstacle()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        return hasObstacle_(0, ticks_ - 1);
    }

    int ObstacleMap::getTicks() const
    {
        return ticks_;
    }

    int ObstacleMap::angleToTick(float angle_rad) const
    {
        return Utils::floorToInt(
            ((angle_rad * ticks_ / M_PIf) + 1) / 2);
    }

    float ObstacleMap::tickToAngleStart(int tick) const
    {
        return M_PIf * (2 * tick - 1) / ticks_;
    }

    float ObstacleMap::tickToAngleEnd(int tick) const
    {
        return M_PIf * (2 * tick + 1) / ticks_;
    }

    float ObstacleMap::tickToAngleMid(int tick) const
    {
        return 2 * M_PIf * tick / ticks_;
    }
}
} //namespace

#endif
