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

        virtual std::vector<ImageCaptureBase::ImageResponse> getImages(const std::vector<ImageCaptureBase::ImageRequest>& request) const = 0;
        virtual std::vector<uint8_t> getImage(const std::string& camera_name, ImageCaptureBase::ImageType image_type) const = 0;

        //CinemAirSim
        virtual std::vector<std::string> getPresetLensSettings() = 0;
        virtual std::string getLensSettings() = 0;
        virtual void setPresetLensSettings(std::string) = 0;
        virtual std::vector<std::string> getPresetFilmbackSettings() = 0;
        virtual void setPresetFilmbackSettings(std::string) = 0;
        virtual std::string getFilmbackSettings() = 0;
        virtual float setFilmbackSettings(float width, float height) = 0;
        virtual float getFocalLength() = 0;
        virtual void setFocalLength(float focal_length) = 0;
        virtual void enableManualFocus(bool enable) = 0;
        virtual float getFocusDistance() = 0;
        virtual void setFocusDistance(float focus_distance) = 0;
        virtual float getFocusAperture() = 0;
        virtual void setFocusAperture(float focus_aperture) = 0;
        virtual void enableFocusPlane(bool enable) = 0;
        virtual std::string getCurrentFieldOfView() = 0;
        //end CinemAirSim

        virtual bool testLineOfSightToPoint(const GeoPoint& point) const = 0;

        virtual Pose getPose() const = 0;
        virtual void setPose(const Pose& pose, bool ignore_collision) = 0;
        virtual const Kinematics::State* getGroundTruthKinematics() const = 0;
        virtual const msr::airlib::Environment* getGroundTruthEnvironment() const = 0;

        virtual CameraInfo getCameraInfo(const std::string& camera_name) const = 0;
        virtual void setCameraPose(const std::string& camera_name, const Pose& pose) = 0;
        virtual void setCameraFoV(const std::string& camera_name, float fov_degrees) = 0;
        virtual void setDistortionParam(const std::string& camera_name, const std::string& param_name, float value) = 0;
        virtual std::vector<float> getDistortionParams(const std::string& camera_name) = 0;

        virtual CollisionInfo getCollisionInfo() const = 0;
        virtual int getRemoteControlID() const = 0; //which RC to use, 0 is first one, -1 means disable RC (use keyborad)
        virtual RCData getRCData() const = 0; //get reading from RC from simulator's host OS
        virtual std::string getVehicleName() const = 0;
        virtual std::string getRecordFileLine(bool is_header_line) const = 0;
        virtual void toggleTrace() = 0;
        virtual void setTraceLine(const std::vector<float>& color_rgba, float thickness) = 0;

        virtual void addDetectionFilterMeshName(const std::string& camera_name, ImageCaptureBase::ImageType image_type, const std::string& mesh_name) = 0;
        virtual void setDetectionFilterRadius(const std::string& camera_name, ImageCaptureBase::ImageType image_type, const float radius_cm) = 0;
        virtual void clearDetectionMeshNames(const std::string& camera_name, ImageCaptureBase::ImageType image_type) = 0;
        virtual std::vector<DetectionInfo> getDetections(const std::string& camera_name, ImageCaptureBase::ImageType image_type) const = 0;

        //use pointer here because of derived classes for VehicleSetting
        const AirSimSettings::VehicleSetting* getVehicleSetting() const
        {
            return AirSimSettings::singleton().getVehicleSetting(getVehicleName());
        }
    };
}
} //namespace
#endif
