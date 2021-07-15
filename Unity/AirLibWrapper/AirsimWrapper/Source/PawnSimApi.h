#pragma once

#include "common/common_utils/UniqueValueMap.hpp"
#include "common/Common.hpp"
#include "common/common_utils/Signal.hpp"
#include "physics/Kinematics.hpp"
#include "common/AirSimSettings.hpp"
#include "common/CommonStructs.hpp"
#include "api/VehicleSimApiBase.hpp"
#include "UnityImageCapture.h"
#include "UnityPawn.h"
#include "NedTransform.h"

class PawnSimApi : public msr::airlib::VehicleSimApiBase
{
private:
    typedef msr::airlib::AirSimSettings AirSimSettings;
    typedef msr::airlib::Kinematics Kinematics;
    typedef msr::airlib::Environment Environment;

public:
    typedef msr::airlib::GeoPoint GeoPoint;
    typedef msr::airlib::Vector3r Vector3r;
    typedef msr::airlib::Pose Pose;
    typedef msr::airlib::Quaternionr Quaternionr;
    typedef msr::airlib::CollisionInfo CollisionInfo;
    typedef msr::airlib::VectorMath VectorMath;
    typedef msr::airlib::real_T real_T;
    typedef msr::airlib::Utils Utils;
    typedef msr::airlib::AirSimSettings::VehicleSetting VehicleSetting;
    typedef msr::airlib::ImageCaptureBase ImageCaptureBase;

public:
    struct Params
    {
        UnityPawn* pawn;
        const NedTransform* global_transform;
        msr::airlib::GeoPoint home_geopoint;
        std::string vehicle_name;

        Params()
        {
        }

        Params(UnityPawn* pawn_val, const NedTransform* global_transform_val, const msr::airlib::GeoPoint home_geopoint_val,
               std::string vehicle_name_val)
        {
            pawn = pawn_val;
            global_transform = global_transform_val;
            home_geopoint = home_geopoint_val;
            vehicle_name = vehicle_name_val;
        }
    };

private:
    bool canTeleportWhileMove() const;
    void allowPassthroughToggleInput();
    void setStartPosition(const AirSimVector& position, const AirSimQuaternion& rotator);
    void updateKinematics(float dt);

protected:
    AirSimPose GetInitialPose();
    msr::airlib::Kinematics* getKinematics();
    msr::airlib::Environment* getEnvironment();

public:
    virtual void initialize() override;
    PawnSimApi(const Params& params);
    virtual void resetImplementation() override;
    virtual void update() override;
    virtual const UnityImageCapture* getImageCapture() const override;

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
    void OnCollision(msr::airlib::CollisionInfo collisionInfo);
    const NedTransform& getNedTransform() const;
    virtual void pawnTick(float dt);

    virtual bool testLineOfSightToPoint(const msr::airlib::GeoPoint& point) const override;
    virtual bool testLineOfSightBetweenPoints(const msr::airlib::GeoPoint& point1, const msr::airlib::GeoPoint& point2) const override;
    virtual void getWorldExtents(msr::airlib::GeoPoint& min, msr::airlib::GeoPoint& max) const override;

private:
    Params params_;
    msr::airlib::GeoPoint home_geo_point_;
    std::map<std::string, int> vehiclesInfo_;
    NedTransform ned_transform_;
    std::unique_ptr<UnityImageCapture> image_capture_;
    std::string log_line_;
    mutable msr::airlib::RCData rc_data_;

    struct State
    {
        AirSimVector start_location;
        AirSimQuaternion start_rotation;
        bool tracing_enabled;
        bool collisions_enabled;
        bool passthrough_enabled;
        bool was_last_move_teleport;
        CollisionInfo collision_info;
        AirSimVector mesh_origin;
    };
    State state_, initial_state_;

    std::unique_ptr<msr::airlib::Kinematics> kinematics_;
    std::unique_ptr<msr::airlib::Environment> environment_;
};
