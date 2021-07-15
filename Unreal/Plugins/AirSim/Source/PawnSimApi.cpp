#include "PawnSimApi.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Components/LineBatchComponent.h"

#include "AirBlueprintLib.h"
#include "common/ClockFactory.hpp"
#include "PIPCamera.h"
#include "NedTransform.h"
#include "common/EarthUtils.hpp"

#include "Materials/MaterialParameterCollectionInstance.h"
#include "DrawDebugHelpers.h"

PawnSimApi::PawnSimApi(const Params& params)
    : params_(params), ned_transform_(params.pawn, *params.global_transform)
{
}

void PawnSimApi::initialize()
{
    Kinematics::State initial_kinematic_state = Kinematics::State::zero();
    ;
    initial_kinematic_state.pose = getPose();
    kinematics_.reset(new Kinematics(initial_kinematic_state));

    Environment::State initial_environment;
    initial_environment.position = initial_kinematic_state.pose.position;
    initial_environment.geo_point = params_.home_geopoint;
    environment_.reset(new Environment(initial_environment));

    //initialize state
    params_.pawn->GetActorBounds(true, initial_state_.mesh_origin, initial_state_.mesh_bounds);
    initial_state_.ground_offset = FVector(0, 0, initial_state_.mesh_bounds.Z);
    initial_state_.transformation_offset = params_.pawn->GetActorLocation() - initial_state_.ground_offset;
    ground_margin_ = FVector(0, 0, 20); //TODO: can we explain params_.pawn experimental setting? 7 seems to be minimum
    ground_trace_end_ = initial_state_.ground_offset + ground_margin_;

    setStartPosition(getUUPosition(), getUUOrientation());

    initial_state_.tracing_enabled = getVehicleSetting()->enable_trace;
    initial_state_.collisions_enabled = getVehicleSetting()->enable_collisions;
    initial_state_.passthrough_enabled = getVehicleSetting()->enable_collision_passthrough;

    initial_state_.collision_info = CollisionInfo();

    initial_state_.was_last_move_teleport = false;
    initial_state_.was_last_move_teleport = canTeleportWhileMove();

    setupCamerasFromSettings(params_.cameras);
    image_capture_.reset(new UnrealImageCapture(&cameras_));

    //add listener for pawn's collision event
    params_.pawn_events->getCollisionSignal().connect_member(this, &PawnSimApi::onCollision);
    params_.pawn_events->getPawnTickSignal().connect_member(this, &PawnSimApi::pawnTick);
}

void PawnSimApi::setStartPosition(const FVector& position, const FRotator& rotator)
{
    initial_state_.start_location = getUUPosition();
    initial_state_.start_rotation = getUUOrientation();

    initial_state_.last_position = initial_state_.start_location;
    initial_state_.last_debug_position = initial_state_.start_location;

    //compute our home point
    Vector3r nedWrtOrigin = ned_transform_.toGlobalNed(initial_state_.start_location);
    home_geo_point_ = msr::airlib::EarthUtils::nedToGeodetic(nedWrtOrigin,
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

void PawnSimApi::detectUsbRc()
{
    if (getRemoteControlID() >= 0) {
        joystick_.getJoyStickState(getRemoteControlID(), joystick_state_);

        rc_data_.is_initialized = joystick_state_.is_initialized;

        if (rc_data_.is_initialized)
            UAirBlueprintLib::LogMessageString("RC Controller on USB: ", joystick_state_.pid_vid == "" ? "(Detected)" : joystick_state_.pid_vid, LogDebugLevel::Informational);
        else
            UAirBlueprintLib::LogMessageString("RC Controller on USB not detected: ",
                                               std::to_string(joystick_state_.connection_error_code),
                                               LogDebugLevel::Informational);
    }
}

void PawnSimApi::setupCamerasFromSettings(const common_utils::UniqueValueMap<std::string, APIPCamera*>& cameras)
{
    //add cameras that already exists in pawn
    cameras_.clear();
    for (const auto& p : cameras.getMap())
        cameras_.insert_or_assign(p.first, p.second);

    //create or replace cameras specified in settings
    createCamerasFromSettings();

    //setup individual cameras
    typedef msr::airlib::AirSimSettings AirSimSettings;
    const auto& camera_defaults = AirSimSettings::singleton().camera_defaults;
    for (auto& pair : cameras_.getMap()) {
        const auto& camera_setting = Utils::findOrDefault(getVehicleSetting()->cameras, pair.first, camera_defaults);
        APIPCamera* camera = pair.second;
        camera->setupCameraFromSettings(camera_setting, getNedTransform());
    }
}

void PawnSimApi::createCamerasFromSettings()
{
    //UStaticMeshComponent* bodyMesh = UAirBlueprintLib::GetActorComponent<UStaticMeshComponent>(this, TEXT("BodyMesh"));
    USceneComponent* bodyMesh = params_.pawn->GetRootComponent();
    FActorSpawnParameters camera_spawn_params;
    camera_spawn_params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    const auto& transform = getNedTransform();

    //for each camera in settings
    for (const auto& camera_setting_pair : getVehicleSetting()->cameras) {
        const auto& setting = camera_setting_pair.second;

        //get pose
        FVector position = transform.fromLocalNed(setting.position) - transform.fromLocalNed(Vector3r::Zero());
        FTransform camera_transform(FRotator(setting.rotation.pitch, setting.rotation.yaw, setting.rotation.roll),
                                    position,
                                    FVector(1., 1., 1.));

        //spawn and attach camera to pawn
        APIPCamera* camera = params_.pawn->GetWorld()->SpawnActor<APIPCamera>(params_.pip_camera_class, camera_transform, camera_spawn_params);
        camera->AttachToComponent(bodyMesh, FAttachmentTransformRules::KeepRelativeTransform);

        //add on to our collection
        cameras_.insert_or_assign(camera_setting_pair.first, camera);
    }
}

void PawnSimApi::onCollision(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp,
                             bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
    // Deflect along the surface when we collide.
    //FRotator CurrentRotation = GetActorRotation(RootComponent);
    //SetActorRotation(FQuat::Slerp(CurrentRotation.Quaternion(), HitNormal.ToOrientationQuat(), 0.025f));

    UPrimitiveComponent* comp = Cast<class UPrimitiveComponent>(Other ? (Other->GetRootComponent() ? Other->GetRootComponent() : nullptr) : nullptr);

    state_.collision_info.has_collided = true;
    state_.collision_info.normal = Vector3r(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, -Hit.ImpactNormal.Z);
    state_.collision_info.impact_point = ned_transform_.toLocalNed(Hit.ImpactPoint);
    state_.collision_info.position = ned_transform_.toLocalNed(getUUPosition());
    state_.collision_info.penetration_depth = ned_transform_.toNed(Hit.PenetrationDepth);
    state_.collision_info.time_stamp = msr::airlib::ClockFactory::get()->nowNanos();
    state_.collision_info.object_name = std::string(Other ? TCHAR_TO_UTF8(*(Other->GetName())) : "(null)");
    state_.collision_info.object_id = comp ? comp->CustomDepthStencilValue : -1;

    ++state_.collision_info.collision_count;

    UAirBlueprintLib::LogMessageString("Collision", Utils::stringf("#%d with %s - ObjID %d", state_.collision_info.collision_count, state_.collision_info.object_name.c_str(), state_.collision_info.object_id), LogDebugLevel::Informational);
}

void PawnSimApi::possess()
{
    APlayerController* controller = params_.pawn->GetWorld()->GetFirstPlayerController();
    controller->UnPossess();
    controller->Possess(params_.pawn);
}

const NedTransform& PawnSimApi::getNedTransform() const
{
    return ned_transform_;
}

APawn* PawnSimApi::getPawn()
{
    return params_.pawn;
}

void PawnSimApi::setRCForceFeedback(float rumble_strength, float auto_center)
{
    if (joystick_state_.is_initialized) {
        joystick_.setWheelRumble(getRemoteControlID(), rumble_strength);
        joystick_.setAutoCenter(getRemoteControlID(), auto_center);
    }
}

msr::airlib::RCData PawnSimApi::getRCData() const
{
    joystick_.getJoyStickState(getRemoteControlID(), joystick_state_);

    rc_data_.is_initialized = joystick_state_.is_initialized;
    rc_data_.is_valid = joystick_state_.is_valid;

    if (rc_data_.is_valid) {
        //-1 to 1 --> 0 to 1
        rc_data_.throttle = (joystick_state_.left_y + 1) / 2;

        //-1 to 1
        rc_data_.yaw = joystick_state_.left_x;
        rc_data_.roll = joystick_state_.right_x;
        rc_data_.pitch = -joystick_state_.right_y;

        //these will be available for devices like steering wheels
        rc_data_.left_z = joystick_state_.left_z;
        rc_data_.right_z = joystick_state_.right_z;

        rc_data_.switches = joystick_state_.buttons;
        rc_data_.vendor_id = joystick_state_.pid_vid.substr(0, joystick_state_.pid_vid.find('&'));

        //switch index 0 to 7 for FrSky Taranis RC is:
        //front-upper-left, front-upper-right, top-right-left, top-right-left, top-left-right, top-right-right, top-left-left, top-right-left

        UAirBlueprintLib::LogMessageString("Joystick (T,R,P,Y,Buttons): ", Utils::stringf("%f, %f, %f %f, %s", rc_data_.throttle, rc_data_.roll, rc_data_.pitch, rc_data_.yaw, Utils::toBinaryString(joystick_state_.buttons).c_str()), LogDebugLevel::Informational);

        //TODO: should below be at controller level info?
        UAirBlueprintLib::LogMessageString("RC Mode: ", rc_data_.getSwitch(0) == 0 ? "Angle" : "Rate", LogDebugLevel::Informational);
    }
    //else don't waste time

    return rc_data_;
}

void PawnSimApi::displayCollisionEffect(FVector hit_location, const FHitResult& hit)
{
    if (params_.collision_display_template != nullptr && Utils::isDefinitelyLessThan(hit.ImpactNormal.Z, 0.0f)) {
        UParticleSystemComponent* particles = UGameplayStatics::SpawnEmitterAtLocation(params_.pawn->GetWorld(),
                                                                                       params_.collision_display_template,
                                                                                       FTransform(hit_location),
                                                                                       true);
        particles->SetWorldScale3D(FVector(0.1f, 0.1f, 0.1f));
    }
}

int PawnSimApi::getRemoteControlID() const
{
    return getVehicleSetting()->rc.remote_control_id;
}

const APIPCamera* PawnSimApi::getCamera(const std::string& camera_name) const
{
    return cameras_.findOrDefault(camera_name, nullptr);
}

APIPCamera* PawnSimApi::getCamera(const std::string& camera_name)
{
    return const_cast<APIPCamera*>(
        static_cast<const PawnSimApi*>(this)->getCamera(camera_name));
}

const UnrealImageCapture* PawnSimApi::getImageCapture() const
{
    return image_capture_.get();
}

int PawnSimApi::getCameraCount()
{
    return cameras_.valsSize();
}

bool PawnSimApi::testLineOfSightToPoint(const msr::airlib::GeoPoint& lla) const
{
    bool hit;

    // We need to run this code on the main game thread, since it iterates over actors
    FGraphEventRef task = FFunctionGraphTask::CreateAndDispatchWhenReady([&]() {
        // This default NedTransform is part of how we anchor the AirSim primary LLA origin at 0, 0, 0 in Unreal
        NedTransform zero_based_ned_transform(FTransform::Identity, UAirBlueprintLib::GetWorldToMetersScale(params_.pawn));
        FCollisionQueryParams collision_params(SCENE_QUERY_STAT(LineOfSight), true, params_.pawn);

        // Transform from LLA to NED
        const auto& settings = AirSimSettings::singleton();
        Vector3r ned = msr::airlib::EarthUtils::GeodeticToNedFast(lla, settings.origin_geopoint.home_geo_point);
        FVector target_location = zero_based_ned_transform.fromGlobalNed(ned);

        hit = params_.pawn->GetWorld()->LineTraceTestByChannel(params_.pawn->GetActorLocation(), target_location, ECC_Visibility, collision_params);

        // KM911 remove logging
        //		common_utils::Utils::log("NED from LLA: " + std::to_string(target_location.X) + ", " + std::to_string(target_location.Y) + ", " + std::to_string(target_location.Z), common_utils::Utils::kLogLevelInfo);

        if (AirSimSettings::singleton().show_los_debug_lines_) {
            if (hit) {
                // No LOS, so draw red line
                FLinearColor color{ 1.0f, 0, 0, 0.4f };
                params_.pawn->GetWorld()->LineBatcher->DrawLine(params_.pawn->GetActorLocation(), target_location, color, SDPG_World, 10, -1);
            }
            else {
                // Yes LOS, so draw green line
                FLinearColor color{ 0, 1.0f, 0, 0.4f };
                params_.pawn->GetWorld()->LineBatcher->DrawLine(params_.pawn->GetActorLocation(), target_location, color, SDPG_World, 10, -1);
            }
        }
    },
                                                                         TStatId(),
                                                                         NULL,
                                                                         ENamedThreads::GameThread);

    // Wait for the result
    FTaskGraphInterface::Get().WaitUntilTaskCompletes(task);

    return !hit;
}

bool PawnSimApi::testLineOfSightBetweenPoints(const msr::airlib::GeoPoint& lla1, const msr::airlib::GeoPoint& lla2) const
{
    bool hit;

    // We need to run this code on the main game thread, since it iterates over actors
    FGraphEventRef task = FFunctionGraphTask::CreateAndDispatchWhenReady([&]() {
        // This default NedTransform is part of how we anchor the AirSim primary LLA origin at 0, 0, 0 in Unreal
        NedTransform zero_based_ned_transform(FTransform::Identity, UAirBlueprintLib::GetWorldToMetersScale(params_.pawn));
        FCollisionQueryParams collision_params(SCENE_QUERY_STAT(LineOfSight), true);

        const auto& settings = AirSimSettings::singleton();
        msr::airlib::Vector3r ned = msr::airlib::EarthUtils::GeodeticToNedFast(lla1, settings.origin_geopoint.home_geo_point);
        FVector point1 = zero_based_ned_transform.fromGlobalNed(ned);
        ned = msr::airlib::EarthUtils::GeodeticToNedFast(lla2, settings.origin_geopoint.home_geo_point);
        FVector point2 = zero_based_ned_transform.fromGlobalNed(ned);

        hit = params_.pawn->GetWorld()->LineTraceTestByChannel(point1, point2, ECC_Visibility, collision_params);

        if (AirSimSettings::singleton().show_los_debug_lines_) {
            FLinearColor color;
            if (hit) {
                // No LOS, so draw red line
                color = FLinearColor{ 1.0f, 0, 0, 0.4f };
            }
            else {
                // Yes LOS, so draw green line
                color = FLinearColor{ 0, 1.0f, 0, 0.4f };
            }

            params_.pawn->GetWorld()->PersistentLineBatcher->DrawLine(point1, point2, color, SDPG_World, 4, 999999);
        }
    },
                                                                         TStatId(),
                                                                         NULL,
                                                                         ENamedThreads::GameThread);

    // Wait for the result
    FTaskGraphInterface::Get().WaitUntilTaskCompletes(task);

    return !hit;
}

void PawnSimApi::getWorldExtents(msr::airlib::GeoPoint& lla_min_out, msr::airlib::GeoPoint& lla_max_out) const
{
    // We need to run this code on the main game thread, since it iterates over actors
    FGraphEventRef task = FFunctionGraphTask::CreateAndDispatchWhenReady([&]() {
        // This default NedTransform is part of how we anchor the AirSim primary LLA origin at 0, 0, 0 in Unreal
        NedTransform zero_based_ned_transform(FTransform::Identity, UAirBlueprintLib::GetWorldToMetersScale(params_.pawn));

        // Testing actor enum for world bounds...
        FVector world_min{ FLT_MAX, FLT_MAX, FLT_MAX };
        FVector world_max{ FLT_MIN, FLT_MIN, FLT_MIN };
        for (TActorIterator<AActor> actor_itr(params_.pawn->GetWorld()); actor_itr; ++actor_itr) {
            // Same as with the Object Iterator, access the subclass instance with the * or -> operators.
            AActor* actor = *actor_itr;
            FVector origin;
            FVector extent;
            actor->GetActorBounds(false, origin, extent);
            FString name = actor->GetFullName();
            std::string stdName = std::string(TCHAR_TO_UTF8(*name));

            if (extent[0] > 20000.0f) {
                common_utils::Utils::log("In world bounds calculation, skipping gigantic object: " + stdName, common_utils::Utils::kLogLevelWarn);
                continue;
            }

            for (int coord = 0; coord < 3; coord++) {
                float min = origin[coord] - extent[coord];
                float max = origin[coord] + extent[coord];
                if (min < world_min[coord]) {
                    world_min[coord] = min;
                }
                if (max > world_max[coord]) {
                    world_max[coord] = max;
                }
            }
        }

        // TODO think more about how best to determine/indicate ground level, if anyone cares

        // Convert Uvectors to LLAs
        const auto& settings = AirSimSettings::singleton();
        msr::airlib::Vector3r ned = zero_based_ned_transform.toGlobalNed(world_min);
        lla_min_out = msr::airlib::EarthUtils::nedToGeodeticFast(ned, settings.origin_geopoint.home_geo_point);

        ned = zero_based_ned_transform.toGlobalNed(world_max);
        lla_max_out = msr::airlib::EarthUtils::nedToGeodeticFast(ned, settings.origin_geopoint.home_geo_point);
    },
                                                                         TStatId(),
                                                                         NULL,
                                                                         ENamedThreads::GameThread);

    // Wait for the result
    FTaskGraphInterface::Get().WaitUntilTaskCompletes(task);

    common_utils::Utils::log("Extent min: " + lla_min_out.to_string() + ".  Max: " + lla_max_out.to_string(), common_utils::Utils::kLogLevelInfo);
}

void PawnSimApi::resetImplementation()
{
    state_ = initial_state_;
    rc_data_ = msr::airlib::RCData();
    params_.pawn->SetActorLocationAndRotation(state_.start_location, state_.start_rotation, false, nullptr, ETeleportType::TeleportPhysics);
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
    FVector unrealPosition = getUUPosition();
    reporter.writeValue("unreal pos", Vector3r(unrealPosition.X, unrealPosition.Y, unrealPosition.Z));
}

//void playBack()
//{
//if (params_.pawn->GetRootPrimitiveComponent()->IsAnySimulatingPhysics()) {
//    params_.pawn->GetRootPrimitiveComponent()->SetSimulatePhysics(false);
//    params_.pawn->GetRootPrimitiveComponent()->SetSimulatePhysics(true);
//}
//TODO: refactor below code used for playback
//std::ifstream sim_log("C:\\temp\\mavlogs\\circle\\sim_cmd_006_orbit 5 1.txt.pos.txt");
//plot(sim_log, FColor::Purple, Vector3r(0, 0, -3));
//std::ifstream real_log("C:\\temp\\mavlogs\\circle\\real_cmd_006_orbit 5 1.txt.pos.txt");
//plot(real_log, FColor::Yellow, Vector3r(0, 0, -3));

//std::ifstream sim_log("C:\\temp\\mavlogs\\square\\sim_cmd_005_square 5 1.txt.pos.txt");
//plot(sim_log, FColor::Purple, Vector3r(0, 0, -3));
//std::ifstream real_log("C:\\temp\\mavlogs\\square\\real_cmd_012_square 5 1.txt.pos.txt");
//plot(real_log, FColor::Yellow, Vector3r(0, 0, -3));
//}

PawnSimApi::CollisionInfo PawnSimApi::getCollisionInfo() const
{
    return state_.collision_info;
}

FVector PawnSimApi::getUUPosition() const
{
    return params_.pawn->GetActorLocation(); // - state_.mesh_origin
}

FRotator PawnSimApi::getUUOrientation() const
{
    return params_.pawn->GetActorRotation();
}

void PawnSimApi::toggleTrace()
{
    state_.tracing_enabled = !state_.tracing_enabled;

    if (!state_.tracing_enabled)
        UKismetSystemLibrary::FlushPersistentDebugLines(params_.pawn->GetWorld());
    else {
        state_.debug_position_offset = state_.current_debug_position - state_.current_position;
        state_.last_debug_position = state_.last_position;
    }
}

void PawnSimApi::setTraceLine(const std::vector<float>& color_rgba, float thickness)
{
    FLinearColor color{ color_rgba[0], color_rgba[1], color_rgba[2], color_rgba[3] };
    trace_color_ = color.ToFColor(true);
    trace_thickness_ = thickness;
}

void PawnSimApi::allowPassthroughToggleInput()
{
    state_.passthrough_enabled = !state_.passthrough_enabled;
    UAirBlueprintLib::LogMessage("enable_passthrough_on_collisions: ", FString::FromInt(state_.passthrough_enabled), LogDebugLevel::Informational);
}

void PawnSimApi::plot(std::istream& s, FColor color, const Vector3r& offset)
{
    using namespace msr::airlib;

    Vector3r last_point = VectorMath::nanVector();
    uint64_t timestamp;
    float heading, x, y, z;
    while (s >> timestamp >> heading >> x >> y >> z) {
        std::string discarded_line;
        std::getline(s, discarded_line);

        Vector3r current_point(x, y, z);
        current_point += offset;
        if (!VectorMath::hasNan(last_point)) {
            DrawDebugLine(params_.pawn->GetWorld(), ned_transform_.fromLocalNed(last_point), ned_transform_.fromLocalNed(current_point), color, true, -1.0F, 0, 3.0F);
        }
        last_point = current_point;
    }
}

//parameters in NED frame
PawnSimApi::Pose PawnSimApi::getPose() const
{
    return toPose(getUUPosition(), getUUOrientation().Quaternion());
}

PawnSimApi::Pose PawnSimApi::toPose(const FVector& u_position, const FQuat& u_quat) const
{
    const Vector3r& position = ned_transform_.toLocalNed(u_position);
    const Quaternionr& orientation = ned_transform_.toNed(u_quat);
    return Pose(position, orientation);
}

void PawnSimApi::setPose(const Pose& pose, bool ignore_collision)
{
    UAirBlueprintLib::RunCommandOnGameThread([this, pose, ignore_collision]() {
        setPoseInternal(pose, ignore_collision);
    },
                                             true);
}

void PawnSimApi::setPoseInternal(const Pose& pose, bool ignore_collision)
{
    //translate to new PawnSimApi position & orientation from NED to NEU
    FVector position = ned_transform_.fromLocalNed(pose.position);
    state_.current_position = position;

    //quaternion formula comes from http://stackoverflow.com/a/40334755/207661
    FQuat orientation = ned_transform_.fromNed(pose.orientation);

    bool enable_teleport = ignore_collision || canTeleportWhileMove();

    //must reset collision before we set pose. Setting pose will immediately call NotifyHit if there was collision
    //if there was no collision than has_collided would remain false, else it will be set so its value can be
    //checked at the start of next tick
    state_.collision_info.has_collided = false;
    state_.was_last_move_teleport = enable_teleport;

    if (enable_teleport)
        params_.pawn->SetActorLocationAndRotation(position, orientation, false, nullptr, ETeleportType::TeleportPhysics);
    else
        params_.pawn->SetActorLocationAndRotation(position, orientation, true);

    if (state_.tracing_enabled && (state_.last_position - position).SizeSquared() > 0.25) {
        DrawDebugLine(params_.pawn->GetWorld(), state_.last_position, position, trace_color_, true, -1.0F, 0, trace_thickness_);
        state_.last_position = position;
    }
    else if (!state_.tracing_enabled) {
        state_.last_position = position;
    }
}

void PawnSimApi::setDebugPose(const Pose& debug_pose)
{
    state_.current_debug_position = ned_transform_.fromLocalNed(debug_pose.position);
    if (state_.tracing_enabled && !VectorMath::hasNan(debug_pose.position)) {
        FVector debug_position = state_.current_debug_position - state_.debug_position_offset;
        if ((state_.last_debug_position - debug_position).SizeSquared() > 0.25) {
            DrawDebugLine(params_.pawn->GetWorld(), state_.last_debug_position, debug_position, FColor(0xaa, 0x33, 0x11), true, -1.0F, 0, 10.0F);
            UAirBlueprintLib::LogMessage(FString("Debug Pose: "), debug_position.ToCompactString(), LogDebugLevel::Informational);
            state_.last_debug_position = debug_position;
        }
    }
    else if (!state_.tracing_enabled) {
        state_.last_debug_position = state_.current_debug_position - state_.debug_position_offset;
    }
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

void PawnSimApi::updateKinematics(float dt)
{
    //update kinematics from pawn's movement instead of physics engine

    auto next_kinematics = kinematics_->getState();

    next_kinematics.pose = getPose();
    next_kinematics.twist.linear = getNedTransform().toLocalNedVelocity(getPawn()->GetVelocity());
    next_kinematics.twist.angular = msr::airlib::VectorMath::toAngularVelocity(
        kinematics_->getPose().orientation, next_kinematics.pose.orientation, dt);

    next_kinematics.accelerations.linear = dt > 0 ? (next_kinematics.twist.linear - kinematics_->getTwist().linear) / dt : next_kinematics.accelerations.linear;
    next_kinematics.accelerations.angular = dt > 0 ? (next_kinematics.twist.angular - kinematics_->getTwist().angular) / dt : next_kinematics.accelerations.angular;

    kinematics_->setState(next_kinematics);
    kinematics_->update();
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
        return "VehicleName\tTimeStamp\tPOS_X\tPOS_Y\tPOS_Z\tQ_W\tQ_X\tQ_Y\tQ_Z\t";
    }

    const auto* kinematics = getGroundTruthKinematics();
    const uint64_t timestamp_millis = static_cast<uint64_t>(clock()->nowNanos() / 1.0E6);

    std::ostringstream ss;
    ss << getVehicleName() << "\t";
    ss << timestamp_millis << "\t";
    ss << kinematics->pose.position.x() << "\t" << kinematics->pose.position.y() << "\t" << kinematics->pose.position.z() << "\t";
    ss << kinematics->pose.orientation.w() << "\t" << kinematics->pose.orientation.x() << "\t"
       << kinematics->pose.orientation.y() << "\t" << kinematics->pose.orientation.z() << "\t";

    return ss.str();
}

msr::airlib::VehicleApiBase* PawnSimApi::getVehicleApiBase() const
{
    return nullptr;
}
