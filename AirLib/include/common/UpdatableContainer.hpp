// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef airsim_core_UpdatableContainer_hpp
#define airsim_core_UpdatableContainer_hpp

#include "UpdatableObject.hpp"
#include "common/Common.hpp"

namespace msr
{
namespace airlib
{

    template <typename TUpdatableObjectPtr>
    class UpdatableContainer : public UpdatableObject
    {
    public: //limited container interface
        typedef vector<TUpdatableObjectPtr> MembersContainer;
        typedef typename MembersContainer::iterator iterator;
        typedef typename MembersContainer::const_iterator const_iterator;
        typedef typename MembersContainer::value_type value_type;
        iterator begin() { return members_.begin(); }
        iterator end() { return members_.end(); }
        const_iterator begin() const { return members_.begin(); }
        const_iterator end() const { return members_.end(); }
        uint size() const { return static_cast<uint>(members_.size()); }
        const TUpdatableObjectPtr& at(uint index) const { members_.at(index); }
        TUpdatableObjectPtr& at(uint index) { return members_.at(index); }
        //allow to override membership modifications
        virtual void clear()
        {
            for (auto m : members_) {
                m->setParent(nullptr);
            }
            members_.clear();
        }
        virtual void insert(TUpdatableObjectPtr member)
        {
            member->setParent(this);
            members_.push_back(member);
        }
        virtual void erase_remove(TUpdatableObjectPtr member)
        {
            member->setParent(nullptr);
            members_.erase(std::remove(members_.begin(), members_.end(), member), members_.end());
        }

    public:
        //*** Start: UpdatableState implementation ***//
        virtual void resetImplementation() override
        {
            for (TUpdatableObjectPtr& member : members_)
                member->reset();
        }

        virtual void update() override
        {
            UpdatableObject::update();

            for (TUpdatableObjectPtr& member : members_)
                member->update();
        }

        virtual void reportState(StateReporter& reporter) override
        {
            for (TUpdatableObjectPtr& member : members_)
                member->reportState(reporter);
        }

        //*** End: UpdatableState implementation ***//

        virtual ~UpdatableContainer() = default;

    private:
        MembersContainer members_;
    };
}
} //namespace
#endif
