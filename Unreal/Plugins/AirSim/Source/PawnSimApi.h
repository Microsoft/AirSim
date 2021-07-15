#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Particles/ParticleSystemComponent.h"
#include "UnrealImageCapture.h"

#include <vector>
#include <memory>
#include "common/Common.hpp"
#include "common/common_utils/Signal.hpp"
#include "common/CommonStructs.hpp"
#include "PIPCamera.h"
#include "physics/Kinematics.hpp"
#include "NedTransform.h"
#include "common/AirSimSettings.hpp"
#include "SimJoyStick/SimJoyStick.h"
#include "api/VehicleApiBase.hpp"
#include "api/VehicleSimApiBase.hpp"
#include "common/common_utils/UniqueValueMap.hpp"

#include "PawnEvents.h"

class PawnSimApi : public msr::airlib::VehicleSimApiBase
{
public: //types
    typedef msr::airlib::GeoPoint GeoPoint;
    typedef msr::airlib::Vector2r Vector2r;
    typedef msr::airlib::Vector3r Vector3r;
    typedef msr::airlib::Pose Pose;
    typedef msr::airlib::Quaternionr Quaternionr;
    typedef msr::airlib::CollisionInfo CollisionInfo;
    typedef msr::airlib::VectorMath VectorMath;
    typedef msr::airlib::real_T real_T;
    typedef msr::airlib::Utils Utils;
    typedef msr::airlib::AirSimSettings::VehicleSetting VehicleSetting;
    typedef msr::airlib::ImageCaptureBase ImageCaptureBase;
    typedef msr::airlib::DetectionInfo DetectionInfo;

    struct Params
    {
        APawn* pawn;
        const NedTransform* global_transform;
        PawnEvents* pawn_events;
        common_utils::UniqueValueMap<std::string, APIPCamera*> cameras;
        UClass* pip_camera_class;
        UParticleSystem* collision_display_template;
        msr::airlib::GeoPoint home_geopoint;
        std::string vehicle_name;

        Params()
        {
        }

        Params(APawn* pawn_val, const NedTransform* global_transform_val, PawnEvents* pawn_events_val,
               const common_utils::UniqueValueMap<std::string, APIPCamera*>& cameras_val, UClass* pip_camera_class_val,
               UParticleSystem* collision_display_template_val, const msr::airlib::GeoPoint& home_geopoint_val,
               const std::string& vehicle_name_val)
            : pawn(pawn_val)
            , global_transform(global_transform_val)
            , pawn_events(pawn_events_val)
            , cameras(cameras_val)
            , pip_camera_class(pip_camera_class_val)
            , collision_display_template(collision_display_template_val)
            , home_geopoint(home_geopoint_val)
            , vehicle_name(vehicle_name_val)
        {
        }
    };

public: //implementation of VehicleSimApiBase
    virtual void initialize() override;

    virtual void resetImplementation() override;
    virtual void update() override;

    virtual const UnrealImageCapture* getImageCapture() const override;
    virtual Pose getPose() const override;
    virtual void setPose(const Pose& pose, bool ignore_collision) override;

    virtual CollisionInfo getCollisionInfo() const override;
    virtual int getRemoteControlID() const override;
    virtual msr::airlib::RCData getRCData() const override;
    virtual std::string getVehicleName() const override
    {
        return params_.vehicle_name;
    }
    virtual void toggleTrace() override;
    virtual void setTraceLine(const std::vector<float>& color_rgba, float thickness) override;

    virtual void updateRenderedState(float dt) override;
    virtual void updateRendering(float dt) override;
    virtual const msr::airlib::Kinematics::State* getGroundTruthKinematics() const override;
    virtual const msr::airlib::Environment* getGroundTruthEnvironment() const override;
    virtual std::string getRecordFileLine(bool is_header_line) const override;
    virtual void reportState(msr::airlib::StateReporter& reporter) override;

protected: //additional interface for derived class
    virtual void pawnTick(float dt);
    void setPoseInternal(const Pose& pose, bool ignore_collision);
    virtual msr::airlib::VehicleApiBase* getVehicleApiBase() const;
    msr::airlib::Kinematics* getKinematics();
    msr::airlib::Environment* getEnvironment();

public: //Unreal specific methods
    PawnSimApi(const Params& params);

    //returns one of the cameras attached to the pawn
    const APIPCamera* getCamera(const std::string& camera_name) const;
    APIPCamera* getCamera(const std::string& camera_name);
    int getCameraCount();

    virtual bool testLineOfSightToPoint(const msr::airlib::GeoPoint& point) const;
    virtual bool testLineOfSightBetweenPoints(const msr::airlib::GeoPoint& point1, const msr::airlib::GeoPoint& point2) const;
    virtual void getWorldExtents(msr::airlib::GeoPoint& min, msr::airlib::GeoPoint& max) const;

    //if enabled, this would show some flares
    void displayCollisionEffect(FVector hit_location, const FHitResult& hit);

    //return the attached pawn
    APawn* getPawn();

    //get/set pose
    //parameters in NED frame
    void setDebugPose(const Pose& debug_pose);

    FVector getUUPosition() const;
    FRotator getUUOrientation() const;

    const NedTransform& getNedTransform() const;

    void possess();
    void setRCForceFeedback(float rumble_strength, float auto_center);

private: //methods
    bool canTeleportWhileMove() const;
    void allowPassthroughToggleInput();
    void detectUsbRc();
    void setupCamerasFromSettings(const common_utils::UniqueValueMap<std::string, APIPCamera*>& cameras);
    void createCamerasFromSettings();
    //on collision, pawns should update this
    void onCollision(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp,
                     bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit);

    //these methods are for future usage
    void plot(std::istream& s, FColor color, const Vector3r& offset);
    PawnSimApi::Pose toPose(const FVector& u_position, const FQuat& u_quat) const;
    void updateKinematics(float dt);
    void setStartPosition(const FVector& position, const FRotator& rotator);

private: //vars
    typedef msr::airlib::AirSimSettings AirSimSettings;
    typedef msr::airlib::Kinematics Kinematics;
    typedef msr::airlib::Environment Environment;

    Params params_;
    common_utils::UniqueValueMap<std::string, APIPCamera*> cameras_;
    msr::airlib::GeoPoint home_geo_point_;

    std::string vehicle_name_;
    NedTransform ned_transform_;

    FVector ground_trace_end_;
    FVector ground_margin_;
    std::unique_ptr<UnrealImageCapture> image_capture_;
    std::string log_line_;

    mutable msr::airlib::RCData rc_data_;
    mutable SimJoyStick joystick_;
    mutable SimJoyStick::State joystick_state_;

    struct State
    {
        FVector start_location;
        FRotator start_rotation;
        FVector last_position;
        FVector last_debug_position;
        FVector current_position;
        FVector current_debug_position;
        FVector debug_position_offset;
        bool tracing_enabled;
        bool collisions_enabled;
        bool passthrough_enabled;
        bool was_last_move_teleport;
        CollisionInfo collision_info;

        FVector mesh_origin;
        FVector mesh_bounds;
        FVector ground_offset;
        FVector transformation_offset;
    };

    State state_, initial_state_;

    std::unique_ptr<msr::airlib::Kinematics> kinematics_;
    std::unique_ptr<msr::airlib::Environment> environment_;

    FColor trace_color_ = FColor::Purple;
    float trace_thickness_ = 3.0f;
};
