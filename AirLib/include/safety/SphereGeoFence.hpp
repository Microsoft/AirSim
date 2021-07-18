// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef air_SphereGeoFence_hpp
#define air_SphereGeoFence_hpp

#include "common/Common.hpp"

namespace msr
{
namespace airlib
{

    class SphereGeoFence : public IGeoFence
    {
    private:
        Vector3r center_;
        float radius_, distance_accuracy_, min_height_, max_height_;

    public:
        SphereGeoFence(const Vector3r& center, float radius, float max_height, float min_height, float distance_accuracy)
            : center_(center), radius_(radius), distance_accuracy_(distance_accuracy), min_height_(min_height), max_height_(max_height)
        {
            Utils::logMessage("SphereGeoFence: %s", toString().c_str());
        }

        void setBoundry(const Vector3r& origin, float xy_length, float max_z, float min_z) override
        {
            center_ = origin;
            radius_ = xy_length;
            max_height_ = min_z;
            min_height_ = max_z;

            Utils::logMessage("SphereGeoFence: %s", toString().c_str());
        }

        void checkFence(const Vector3r& cur_loc, const Vector3r& dest_loc,
                        bool& in_fence, bool& allow) override
        {
            Vector3r center_dest = dest_loc - center_;
            Vector3r center_dest_xy(center_dest[0], center_dest[1], 0);

            in_fence = center_dest_xy.norm() <= radius_ && dest_loc[2] >= min_height_ && dest_loc[2] <= max_height_;

            if (!in_fence) {
                float cur_center_norm = (cur_loc - center_).norm();
                allow = cur_center_norm - center_dest.norm() >= -distance_accuracy;
            }
            else
                allow = true;
        }

        string toString() const override
        {
            return Utils::stringf("CubeGeoFence: radius=%f, min_height=%f, max_height=%f, center=%s", radius_, min_height_, max_height_, Vector3r::toString(center_).c_str());
        }

        virtual ~SphereGeoFence(){};
    };
}
} //namespace
#endif
