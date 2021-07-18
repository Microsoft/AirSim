// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef airsim_core_PhysicsBodyVertex_hpp
#define airsim_core_PhysicsBodyVertex_hpp

#include "common/UpdatableObject.hpp"
#include "common/Common.hpp"
#include "common/CommonStructs.hpp"

namespace msr
{
namespace airlib
{

    class PhysicsBodyVertex : public UpdatableObject
    {
    protected:
        virtual void setWrench(Wrench& wrench)
        {
            unused(wrench);
            //derived class should override if this is force/torque
            //generating vertex
        }

    public:
        real_T getDragFactor() const
        {
            return drag_factor_;
        }
        void setDragFactor(real_T val)
        {
            drag_factor_ = val;
        }

        PhysicsBodyVertex()
        {
            //allow default constructor with later call for initialize
        }
        PhysicsBodyVertex(const Vector3r& position, const Vector3r& normal, real_T drag_factor = 0)
        {
            initialize(position, normal, drag_factor);
        }
        void initialize(const Vector3r& position, const Vector3r& normal, real_T drag_factor = 0)
        {
            initial_position_ = position;
            initial_normal_ = normal;
            drag_factor_ = drag_factor;
        }

        //*** Start: UpdatableState implementation ***//
        virtual void resetImplementation() override
        {
            position_ = initial_position_;
            normal_ = initial_normal_;

            current_wrench_ = Wrench::zero();
        }

        virtual void update() override
        {
            UpdatableObject::update();

            setWrench(current_wrench_);
        }
        //*** End: UpdatableState implementation ***//

        //getters, setters
        Vector3r getPosition() const
        {
            return position_;
        }
        void setPosition(const Vector3r& position)
        {
            position_ = position;
        }

        Vector3r getNormal() const
        {
            return normal_;
        }
        void setNormal(const Vector3r& normal)
        {
            normal_ = normal;
        }

        Wrench getWrench() const
        {
            return current_wrench_;
        }

    private:
        Vector3r initial_position_, position_;
        Vector3r initial_normal_, normal_;
        Wrench current_wrench_;
        real_T drag_factor_;
    };
}
} //namespace
#endif
