// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef air_CubeGeoFence_hpp
#define air_CubeGeoFence_hpp

#include "common/Common.hpp"

namespace msr
{
namespace airlib
{

    class CubeGeoFence : public IGeoFence
    {
    private:
        Vector3r point_min_, point_max_, point_center_;
        float distance_accuracy_;

    public:
        CubeGeoFence(const Vector3r& point_min, const Vector3r& point_max, float distance_accuracy)
            : point_min_(point_min), point_max_(point_max), distance_accuracy_(distance_accuracy)
        {
            calculateCenter();

            Utils::logMessage("CubeGeoFence: %s", toString().c_str());
        }

        void setBoundry(const Vector3r& origin, float xy_length, float max_z, float min_z) override
        {
            point_min_ = Vector3r(-xy_length, -xy_length, 0) + origin;
            point_min_[2] = max_z;

            point_max_ = Vector3r(xy_length, xy_length, 0) + origin;
            point_max_[2] = min_z;

            calculateCenter();

            Utils::logMessage("CubeGeoFence: %s", toString().c_str());
        }

        void checkFence(const Vector3r& cur_loc, const Vector3r& dest_loc,
                        bool& in_fence, bool& allow) override
        {
            in_fence = dest_loc[0] >= point_min_[0] && dest_loc[1] >= point_min_[1] && dest_loc[2] >= point_min_[2] &&
                       dest_loc[0] <= point_max_[0] && dest_loc[1] <= point_max_[1] && dest_loc[2] <= point_max_[2];

            if (!in_fence) {
                //are we better off with dest than cur location?
                float dest_dist = (dest_loc - point_center_).norm();
                float cur_dist = (cur_loc - point_center_).norm();
                allow = cur_dist - dest_dist >= -distance_accuracy_;
            }
            else
                allow = true;
        }

        string toString() const override
        {
            return Utils::stringf("min=%s, max=%s", VectorMath::toString(point_min_).c_str(), VectorMath::toString(point_max_).c_str());
        }

        virtual ~CubeGeoFence(){};

    private:
        void calculateCenter()
        {
            point_center_ = (point_max_ + point_min_) / 2;
        }
    };
}
} //namespace
#endif
