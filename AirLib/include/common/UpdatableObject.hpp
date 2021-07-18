// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef airsim_core_UpdatableObject_hpp
#define airsim_core_UpdatableObject_hpp

#include "common/Common.hpp"
#include "StateReporter.hpp"
#include "ClockFactory.hpp"

namespace msr
{
namespace airlib
{

    /*
UpdatableObject provides generalized framework for things that needs to be "ticked". For example,
physics objects that needs to update its position every 10ms.
Typically this objects will take their current state, do some processing and produce new state
on every tick.
The reset() provides important ability to rollback all changes to the state back to original
since when it was first initialized. This allows to reset simulation and put all updatable objects
back to their original state.

After object is created and initialized, reset() must be called first before calling update().
Do not call reset() from constructor or initialization because that will produce sequence of
init->reset calls for base-derived class that would be incorrect.
*/

    class UpdatableObject
    {
    public:
        void reset()
        {
            if (reset_in_progress)
                return;

            reset_in_progress = true;
            //TODO: Do we need this check anymore? Maybe reset() should be idempotent.

            if (reset_called && !update_called)
                failResetUpdateOrdering("Multiple reset() calls detected without call to update()");

            reset_called = true;

            resetImplementation();
            reset_in_progress = false;
        }

        virtual void update()
        {
            if (!reset_called)
                failResetUpdateOrdering("reset() must be called first before update()");
            update_called = true;
        }

        virtual ~UpdatableObject() = default;

        virtual void reportState(StateReporter& reporter)
        {
            unused(reporter);
            //default implementation doesn't do anything
        }

        virtual UpdatableObject* getPhysicsBody()
        {
            return nullptr;
        }

        virtual ClockBase* clock()
        {
            return ClockFactory::get();
        }
        virtual const ClockBase* clock() const
        {
            return ClockFactory::get();
        }

        UpdatableObject* getParent()
        {
            return parent_;
        }

        void setParent(UpdatableObject* container)
        {
            parent_ = container;
        }

        std::string getName()
        {
            return name_;
        }

        void setName(const std::string& name)
        {
            this->name_ = name;
        }

    protected:
        virtual void resetImplementation() = 0;
        virtual void failResetUpdateOrdering(std::string err)
        {
            throw std::runtime_error(err);
        }

    private:
        bool reset_called = false;
        bool update_called = false;
        bool reset_in_progress = false;
        UpdatableObject* parent_ = nullptr;
        std::string name_;
    };
}
} //namespace
#endif
