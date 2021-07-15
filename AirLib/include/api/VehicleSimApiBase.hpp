// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef air_VehicleSimApiBase_hpp
#define air_VehicleSimApiBase_hpp

#include "common/CommonStructs.hpp"
#include "common/UpdatableObject.hpp"
#include "common/ImageCaptureBase.hpp"
#include "physics/Kinematics.hpp"
#include "physics/Environment.hpp"
#include "common/AirSimSettings.hpp"

namespace msr
{
namespace airlib
{

    class VehicleSimApiBase : public msr::airlib::UpdatableObject
    {
    public:
        virtual ~VehicleSimApiBase() = default;

        virtual void update() override
        {
            UpdatableObject::update();
        }

        //this method is called at every render tick when we want to transfer state from
        //physics engine to render engine. As physics engine is halted while
        //this happens, this method should do minimal processing
        virtual void updateRenderedState(float dt)
        {
            unused(dt);
            //derived class should override if needed
        }
        //called when render changes are required at every render tick
        virtual void updateRendering(float dt)
        {
            unused(dt);
            //derived class should override if needed
        }

        virtual const ImageCaptureBase* getImageCapture() const = 0;
        virtual ImageCaptureBase* getImageCapture()
        {
            return const_cast<ImageCaptureBase*>(static_cast<const VehicleSimApiBase*>(this)->getImageCapture());
        }

        virtual void initialize() = 0;

        virtual bool testLineOfSightToPoint(const GeoPoint& point) const = 0;
        virtual bool testLineOfSightBetweenPoints(const GeoPoint& point1, const GeoPoint& point2) const = 0;
        virtual void getWorldExtents(GeoPoint& min, GeoPoint& max) const = 0;

        virtual Pose getPose() const = 0;
        virtual void setPose(const Pose& pose, bool ignore_collision) = 0;
        virtual const Kinematics::State* getGroundTruthKinematics() const = 0;
        virtual const msr::airlib::Environment* getGroundTruthEnvironment() const = 0;

        virtual CollisionInfo getCollisionInfo() const = 0;
        virtual int getRemoteControlID() const = 0; //which RC to use, 0 is first one, -1 means disable RC (use keyborad)
        virtual RCData getRCData() const = 0; //get reading from RC from simulator's host OS
        virtual std::string getVehicleName() const = 0;
        virtual std::string getRecordFileLine(bool is_header_line) const = 0;
        virtual void toggleTrace() = 0;
        virtual void setTraceLine(const std::vector<float>& color_rgba, float thickness) = 0;

        //use pointer here because of derived classes for VehicleSetting
        const AirSimSettings::VehicleSetting* getVehicleSetting() const
        {
            return AirSimSettings::singleton().getVehicleSetting(getVehicleName());
        }
    };
}
} //namespace
#endif
