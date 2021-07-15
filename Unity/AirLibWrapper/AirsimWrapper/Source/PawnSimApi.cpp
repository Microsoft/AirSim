#include "PawnSimApi.h"
#include "common/ClockFactory.hpp"
#include "common/EarthUtils.hpp"
#include "PInvokeWrapper.h"
#include "AirSimStructs.hpp"
#include "UnityUtilities.hpp"

PawnSimApi::PawnSimApi(const Params& params)
    : params_(params), ned_transform_(*params.global_transform)
{
}

void PawnSimApi::initialize()
{
    image_capture_.reset(new UnityImageCapture(params_.vehicle_name));

    Kinematics::State initial_kinematic_state = Kinematics::State::zero();
    ;
    initial_kinematic_state.pose = getPose();
    kinematics_.reset(new Kinematics(initial_kinematic_state));
    Environment::State initial_environment;
    initial_environment.position = initial_kinematic_state.pose.position;
    initial_environment.geo_point = params_.home_geopoint;
    environment_.reset(new Environment(initial_environment));

    //initialize state
    UnityTransform initialTransform = GetTransformFromUnity(params_.vehicle_name.c_str());
    initial_state_.mesh_origin = initialTransform.Position;
    setStartPosition(initialTransform.Position, initialTransform.Rotation);

    initial_state_.tracing_enabled = getVehicleSetting()->enable_trace;
    initial_state_.collisions_enabled = getVehicleSetting()->enable_collisions;
    initial_state_.passthrough_enabled = getVehicleSetting()->enable_collision_passthrough;

    initial_state_.collision_info = CollisionInfo();

    initial_state_.was_last_move_teleport = false;
    initial_state_.was_last_move_teleport = canTeleportWhileMove();
    state_ = initial_state_;
}

void PawnSimApi::setStartPosition(const AirSimVector& position, const AirSimQuaternion& rotator)
{
    initial_state_.start_location = position;
    initial_state_.start_rotation = rotator;

    //compute our home point
    home_geo_point_ = msr::airlib::EarthUtils::nedToGeodetic(UnityUtilities::Convert_to_Vector3r(initial_state_.start_location),
                                                             AirSimSettings::singleton().origin_geopoint);
}

void PawnSimApi::pawnTick(float dt)
{
    //default behavior is to call update every tick
    //for custom physics engine, this method should be overridden and update should be
    //called from every physics tick
    update();
    updateRenderedState(dt);
    updateRendering(dt);
}

bool PawnSimApi::testLineOfSightToPoint(const msr::airlib::GeoPoint& point) const
{
    unused(point);

    throw std::invalid_argument(common_utils::Utils::stringf(
                                    "testLineOfSightToPoint is not supported on unity")
                                    .c_str());

    return false;
}

bool PawnSimApi::testLineOfSightBetweenPoints(const msr::airlib::GeoPoint& point1, const msr::airlib::GeoPoint& point2) const
{
    unused(point1);
    unused(point2);

    throw std::invalid_argument(common_utils::Utils::stringf(
                                    "testLineOfSightBetweenPoints is not supported on unity")
                                    .c_str());

    return false;
}

void PawnSimApi::getWorldExtents(msr::airlib::GeoPoint& min, msr::airlib::GeoPoint& max) const
{
    unused(min);
    unused(max);

    throw std::invalid_argument(common_utils::Utils::stringf(
                                    "getWorldExtents is not supported on unity")
                                    .c_str());
}

void PawnSimApi::OnCollision(msr::airlib::CollisionInfo collisionInfo)
{
    state_.collision_info.has_collided = collisionInfo.has_collided;
    state_.collision_info.normal = collisionInfo.normal;
    state_.collision_info.impact_point = collisionInfo.impact_point;
    state_.collision_info.position = collisionInfo.position;
    state_.collision_info.penetration_depth = collisionInfo.penetration_depth;
    state_.collision_info.time_stamp = msr::airlib::ClockFactory::get()->nowNanos();
    state_.collision_info.object_name = collisionInfo.object_name;
    state_.collision_info.object_id = collisionInfo.object_id;

    ++state_.collision_info.collision_count;

    PrintLogMessage("Collision", Utils::stringf("#%d with %s - ObjID %d", state_.collision_info.collision_count, state_.collision_info.object_name.c_str(), state_.collision_info.object_id).c_str(), getVehicleName().c_str(), ErrorLogSeverity::Information);
}

const NedTransform& PawnSimApi::getNedTransform() const
{
    return ned_transform_;
}

msr::airlib::RCData PawnSimApi::getRCData() const
{
    AirSimRCData rcDataFromUnity = GetRCData(getVehicleName().c_str());
    rc_data_.is_valid = rcDataFromUnity.is_valid;

    if (rc_data_.is_valid) {
        rc_data_.throttle = rcDataFromUnity.throttle;
        rc_data_.yaw = rcDataFromUnity.yaw;
        rc_data_.roll = rcDataFromUnity.roll;
        rc_data_.pitch = rcDataFromUnity.pitch;

        //these will be available for devices like steering wheels
        rc_data_.left_z = rcDataFromUnity.left_z;
        rc_data_.right_z = rcDataFromUnity.right_z;

        rc_data_.switches = 0;
        rc_data_.switches |= ((rcDataFromUnity.switch1 & 0x80) == 0 ? 0 : 1) << 0;
        rc_data_.switches |= ((rcDataFromUnity.switch2 & 0x80) == 0 ? 0 : 1) << 1;
        rc_data_.switches |= ((rcDataFromUnity.switch3 & 0x80) == 0 ? 0 : 1) << 2;
        rc_data_.switches |= ((rcDataFromUnity.switch4 & 0x80) == 0 ? 0 : 1) << 3;
        rc_data_.switches |= ((rcDataFromUnity.switch5 & 0x80) == 0 ? 0 : 1) << 4;
        rc_data_.switches |= ((rcDataFromUnity.switch6 & 0x80) == 0 ? 0 : 1) << 5;
        rc_data_.switches |= ((rcDataFromUnity.switch7 & 0x80) == 0 ? 0 : 1) << 6;
        rc_data_.switches |= ((rcDataFromUnity.switch8 & 0x80) == 0 ? 0 : 1) << 7;
        PrintLogMessage("RC Mode: ", rc_data_.getSwitch(0) == 0 ? "Angle" : "Rate", getVehicleName().c_str(), ErrorLogSeverity::Information);
    }

    return rc_data_;
}

int PawnSimApi::getRemoteControlID() const
{
    return getVehicleSetting()->rc.remote_control_id;
}

const UnityImageCapture* PawnSimApi::getImageCapture() const
{
    return image_capture_.get();
}

AirSimPose PawnSimApi::GetInitialPose()
{
    return AirSimPose(state_.start_location, state_.start_rotation);
}

void PawnSimApi::resetImplementation()
{
    state_ = initial_state_;
    rc_data_ = msr::airlib::RCData();
    kinematics_->reset();
    environment_->reset();
}

void PawnSimApi::update()
{
    //sync environment from kinematics
    environment_->setPosition(kinematics_->getPose().position);
    environment_->update();

    VehicleSimApiBase::update();
}

void PawnSimApi::reportState(msr::airlib::StateReporter& reporter)
{
    msr::airlib::VehicleSimApiBase::reportState(reporter);
    kinematics_->reportState(reporter);
    environment_->reportState(reporter);
    // report actual location in unreal coordinates so we can plug that into the UE editor to move the drone.
    //FVector unrealPosition = getUUPosition();
    //reporter.writeValue("unreal pos", Vector3r(unrealPosition.X, unrealPosition.Y, unrealPosition.Z));
}

PawnSimApi::CollisionInfo PawnSimApi::getCollisionInfo() const
{
    return state_.collision_info;
}

void PawnSimApi::toggleTrace()
{
    state_.tracing_enabled = !state_.tracing_enabled;
}

void PawnSimApi::setTraceLine(const std::vector<float>& color_rgba, float thickness)
{
    throw std::invalid_argument(common_utils::Utils::stringf(
                                    "setTraceLine is not supported on unity")
                                    .c_str());
}

void PawnSimApi::allowPassthroughToggleInput()
{
    state_.passthrough_enabled = !state_.passthrough_enabled;
    PrintLogMessage("enable_passthrough_on_collisions: ", state_.passthrough_enabled ? "true" : "false", params_.vehicle_name.c_str(), ErrorLogSeverity::Information);
}

//parameters in NED frame
PawnSimApi::Pose PawnSimApi::getPose() const
{
    AirSimUnity::AirSimPose airSimPose = GetPose(params_.vehicle_name.c_str());
    return UnityUtilities::Convert_to_Pose(airSimPose);
}

void PawnSimApi::setPose(const Pose& pose, bool ignore_collision)
{
    SetPose(UnityUtilities::Convert_to_AirSimPose(pose), ignore_collision, getVehicleName().c_str());
}

bool PawnSimApi::canTeleportWhileMove() const
{
    //allow teleportation
    //  if collisions are not enabled
    //  or we have collided but passthrough is enabled
    //     we will flip-flop was_last_move_teleport flag so on one tick we have passthrough and other tick we don't
    //     without flip flopping, collisions can't be detected
    return !state_.collisions_enabled || (state_.collision_info.has_collided && !state_.was_last_move_teleport && state_.passthrough_enabled);
}

void PawnSimApi::updateRenderedState(float dt)
{
    //by default we update kinematics from UE pawn
    //if SimMod uses its own physics engine then this should be overriden
    updateKinematics(dt);
}

void PawnSimApi::updateRendering(float dt)
{
    unused(dt);
    //no default action in this base class
}

const msr::airlib::Kinematics::State* PawnSimApi::getGroundTruthKinematics() const
{
    return &kinematics_->getState();
}

const msr::airlib::Environment* PawnSimApi::getGroundTruthEnvironment() const
{
    return environment_.get();
}

msr::airlib::Kinematics* PawnSimApi::getKinematics()
{
    return kinematics_.get();
}
msr::airlib::Environment* PawnSimApi::getEnvironment()
{
    return environment_.get();
}

std::string PawnSimApi::getRecordFileLine(bool is_header_line) const
{
    if (is_header_line) {
        return "TimeStamp\tPOS_X\tPOS_Y\tPOS_Z\tQ_W\tQ_X\tQ_Y\tQ_Z\t";
    }

    const msr::airlib::Kinematics::State* kinematics = getGroundTruthKinematics();
    uint64_t timestamp_millis = static_cast<uint64_t>(msr::airlib::ClockFactory::get()->nowNanos() / 1.0E6);

    //TODO: because this bug we are using alternative code with stringstream
    //https://answers.unrealengine.com/questions/664905/unreal-crashes-on-two-lines-of-extremely-simple-st.html

    std::string line;
    line.append(std::to_string(timestamp_millis)).append("\t").append(std::to_string(kinematics->pose.position.x())).append("\t").append(std::to_string(kinematics->pose.position.y())).append("\t").append(std::to_string(kinematics->pose.position.z())).append("\t").append(std::to_string(kinematics->pose.orientation.w())).append("\t").append(std::to_string(kinematics->pose.orientation.x())).append("\t").append(std::to_string(kinematics->pose.orientation.y())).append("\t").append(std::to_string(kinematics->pose.orientation.z())).append("\t");

    return line;
}

void PawnSimApi::updateKinematics(float dt)
{
    //update kinematics from pawn's movement instead of physics engine
    auto next_kinematics = kinematics_->getState();
    next_kinematics.pose = getPose();
    next_kinematics.twist.linear = UnityUtilities::Convert_to_Vector3r(GetVelocity(getVehicleName().c_str()));
    next_kinematics.twist.angular = msr::airlib::VectorMath::toAngularVelocity(
        kinematics_->getPose().orientation, next_kinematics.pose.orientation, dt);
    //TODO: update other fields?
    next_kinematics.accelerations.linear = (next_kinematics.twist.linear - kinematics_->getTwist().linear) / dt;
    next_kinematics.accelerations.angular = (next_kinematics.twist.angular - kinematics_->getTwist().angular) / dt;
    kinematics_->setState(next_kinematics);
    kinematics_->update();
}
