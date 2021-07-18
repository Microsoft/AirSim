// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef air_ObstacleMap_hpp
#define air_ObstacleMap_hpp

#include <mutex>
#include "common/Common.hpp"

namespace msr
{
namespace airlib
{

    /*
    ObstacleMap implements 2D map of obstacles in circular disk around the vehicle. The main
    design criteria is to make insert/delete/queries very fast. This is typically
    not possible in grid based approach because these operations may have large 
    constant. Current code is designed to be O(1) with small constant. This will
    enable sensors like lasers which needs much faster processing.

    We take circle and divide it in to number of ticks. If ticks=4 then circle looks
    like below with ticks marked as 0, 1, 2, 3, 4:

       0XXX1
      XX   XX
      XX   XX
       3XXX2

    The essential part is that tick 0 is always the first tick on left of 12 o'clock.
    Each segment between two ticks forms a cone in the circle where we will put the obstacle
    information. The segment between ticks 0-1 is 0, 1-2 is 1 and so on. So this scheme allows
    us to get the front cone always at segment 0. If we have only 4 sensors like in DJI Matrice
    Guidance system then segment 1 has information about obstacles on right, seg 2 for back and 
    so on.

    Another design criteria is that this class is thread safe for concurrent updates and queries.
    We fully expect one thread to continuously update the obstacles while another to query the map.
*/

    class ObstacleMap
    {
    private:
        //stores distances for each tick segment
        vector<float> distances_;
        //what is the confidence in these values? This should typically be the standard deviation
        vector<float> confidences_;
        //number of ticks, this decides reolution
        int ticks_;
        //blind spots don't get updated so we get its value from neighbours
        vector<bool> blindspots_;

    public:
        //this will be return result of the queries
        struct ObstacleInfo
        {
            int tick; //at what tick we found obstacle
            float distance; //what is the distance from obstacle
            float confidence;

            string toString() const
            {
                return Utils::stringf("Obs: tick=%i, distance=%f, confidence=%f", tick, distance, confidence);
            }
        };

        //private version of hasObstacle doesn't do lock or check inputs
        ObstacleInfo hasObstacle_(int from_tick, int to_tick) const;

    private:
        int wrap(int tick) const;

        //currently we employ simple thread safe model: just serialize queries and updates
        std::mutex mutex_;

    public:
        //if odd_blindspots = true then set all odd ticks as blind spots
        ObstacleMap(int ticks, bool odd_blindspots = false);

        //update the map for tick direction within +/-window ticks
        void update(float distance, int tick, int window, float confidence);
        void update(float distances[], float confidences[]);

        void setBlindspot(int tick, bool blindspot);

        //query if we have obstacle in segment that starts at from to segment that starts at to
        ObstacleInfo hasObstacle(int from_tick, int to_tick);

        //search entire map to find obstacle at minimum distance
        ObstacleInfo getClosestObstacle();

        //number of ticks the map was initialized with
        int getTicks() const;
        //convert angle (in body frame) in radians to tick number
        int angleToTick(float angle_rad) const;
        //convert tick to start of angle (in body frame) in radians
        float tickToAngleStart(int tick) const;
        //convert tick to end of angle (in body frame) in radians
        float tickToAngleEnd(int tick) const;
        //convert tick to mid of the cone in radians
        float tickToAngleMid(int tick) const;
    };
}
} //namespace
#endif
