// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef air_IGeoFence_hpp
#define air_IGeoFence_hpp

#include "common/Common.hpp"

namespace msr
{
namespace airlib
{

    class IGeoFence
    {
    public:
        virtual void setBoundry(const Vector3r& origin, float xy_length, float max_z, float min_z) = 0;
        virtual void checkFence(const Vector3r& cur_loc, const Vector3r& dest_loc,
                                bool& in_fence, bool& allow) = 0;
        virtual string toString() const = 0;

        virtual ~IGeoFence(){};
    };
}
} //namespace
#endif
