#include "CarPawnSimApi.h"
#include "AirBlueprintLib.h"
#include "UnrealSensors/UnrealSensorFactory.h"
#include "CarPawnApi.h"
#include <exception>

using namespace msr::airlib;

CarPawnSimApi::CarPawnSimApi(const Params& params,
                             const msr::airlib::CarApiBase::CarControls& keyboard_controls, UWheeledVehicleMovementComponent* movement)
    : PawnSimApi(params), params_(params), keyboard_controls_(keyboard_controls)
{
}

void CarPawnSimApi::initialize()
{
    PawnSimApi::initialize();

    createVehicleApi(static_cast<ACarPawn*>(params_.pawn), params_.home_geopoint);

    //TODO: should do reset() here?
    joystick_controls_ = msr::airlib::CarApiBase::CarControls();
}

void CarPawnSimApi::createVehicleApi(ACarPawn* pawn, const msr::airlib::GeoPoint& home_geopoint)
{
    //create vehicle params
    std::shared_ptr<UnrealSensorFactory> sensor_factory = std::make_shared<UnrealSensorFactory>(getPawn(), &getNedTransform());

    vehicle_api_ = CarApiFactory::createApi(getVehicleSetting(), sensor_factory, (*getGroundTruthKinematics()), (*getGroundTruthEnvironment()), home_geopoint);
    pawn_api_ = std::unique_ptr<CarPawnApi>(new CarPawnApi(pawn, getGroundTruthKinematics(), vehicle_api_.get()));
}

std::string CarPawnSimApi::getRecordFileLine(bool is_header_line) const
{
    std::string common_line = PawnSimApi::getRecordFileLine(is_header_line);
    if (is_header_line) {
        return common_line +
               "Throttle\tSteering\tBrake\tGear\tHandbrake\tRPM\tSpeed\t";
    }

    const auto& state = pawn_api_->getCarState();

    std::ostringstream ss;
    ss << common_line;
    ss << current_controls_.throttle << "\t" << current_controls_.steering << "\t" << current_controls_.brake << "\t";
    ss << state.gear << "\t" << state.handbrake << "\t" << state.rpm << "\t" << state.speed << "\t";

    return ss.str();
}

//these are called on render ticks
void CarPawnSimApi::updateRenderedState(float dt)
{
    PawnSimApi::updateRenderedState(dt);

    vehicle_api_->getStatusMessages(vehicle_api_messages_);

    //TODO: do we need this for cars?
    if (getRemoteControlID() >= 0)
        vehicle_api_->setRCData(getRCData());
}
void CarPawnSimApi::updateRendering(float dt)
{
    PawnSimApi::updateRendering(dt);

    updateCarControls();

    for (auto i = 0; i < vehicle_api_messages_.size(); ++i) {
        UAirBlueprintLib::LogMessage(FString(vehicle_api_messages_[i].c_str()), TEXT(""), LogDebugLevel::Success, 30);
    }

    try {
        vehicle_api_->sendTelemetry(dt);
    }
    catch (std::exception& e) {
        UAirBlueprintLib::LogMessage(FString(e.what()), TEXT(""), LogDebugLevel::Failure, 30);
    }
}

void CarPawnSimApi::updateCarControls()
{
    auto rc_data = getRCData();

    if (rc_data.is_initialized) {
        if (!rc_data.is_valid) {
            UAirBlueprintLib::LogMessageString("Control Mode: ", "[INVALID] Wheel/Joystick", LogDebugLevel::Informational);
            return;
        }
        UAirBlueprintLib::LogMessageString("Control Mode: ", "Wheel/Joystick", LogDebugLevel::Informational);

        //TODO: move this to SimModeBase?
        //if ((joystick_state_.buttons & 4) | (joystick_state_.buttons & 1024)) { //X button or Start button
        //    reset();
        //    return;
        //}

        // Thrustmaster devices
        if (rc_data.vendor_id == "VID_044F") {
            joystick_controls_.steering = rc_data.yaw;
            joystick_controls_.throttle = (-rc_data.right_z + 1) / 2;
            joystick_controls_.brake = rc_data.throttle;

            auto car_state = vehicle_api_->getCarState();
            float rumble_strength = 0.66 + (car_state.rpm / car_state.maxrpm) / 3;
            float auto_center = (1.0 - 1.0 / (std::abs(car_state.speed / 120) + 1.0)) * (rc_data.yaw / 3);
            setRCForceFeedback(rumble_strength, auto_center);
        }
        // Anything else, typically Logitech G920 wheel
        else {
            joystick_controls_.steering = (rc_data.throttle * 2 - 1) * 1.25;
            joystick_controls_.throttle = (-rc_data.roll + 1) / 2;
            joystick_controls_.brake = -rc_data.right_z + 1;
        }
        //Two steel levers behind wheel
        joystick_controls_.handbrake = (rc_data.getSwitch(5)) | (rc_data.getSwitch(6)) ? 1 : 0;

        if ((rc_data.getSwitch(8)) | (rc_data.getSwitch(1))) { //RSB button or B button
            joystick_controls_.manual_gear = -1;
            joystick_controls_.is_manual_gear = true;
            joystick_controls_.gear_immediate = true;
        }
        else if ((rc_data.getSwitch(9)) | (rc_data.getSwitch(0))) { //LSB button or A button
            joystick_controls_.manual_gear = 0;
            joystick_controls_.is_manual_gear = false;
            joystick_controls_.gear_immediate = true;
        }

        current_controls_ = joystick_controls_;
    }
    else {
        UAirBlueprintLib::LogMessageString("Control Mode: ", "Keyboard", LogDebugLevel::Informational);
        current_controls_ = keyboard_controls_;
    }

    //if API-client control is not active then we route keyboard/joystick control to car
    if (!vehicle_api_->isApiControlEnabled()) {
        //all car controls from anywhere must be routed through API component
        vehicle_api_->setCarControls(current_controls_);
        pawn_api_->updateMovement(current_controls_);
    }
    else {
        UAirBlueprintLib::LogMessageString("Control Mode: ", "API", LogDebugLevel::Informational);
        current_controls_ = vehicle_api_->getCarControls();
        pawn_api_->updateMovement(current_controls_);
    }
    UAirBlueprintLib::LogMessageString("Accel: ", std::to_string(current_controls_.throttle), LogDebugLevel::Informational);
    UAirBlueprintLib::LogMessageString("Break: ", std::to_string(current_controls_.brake), LogDebugLevel::Informational);
    UAirBlueprintLib::LogMessageString("Steering: ", std::to_string(current_controls_.steering), LogDebugLevel::Informational);
    UAirBlueprintLib::LogMessageString("Handbrake: ", std::to_string(current_controls_.handbrake), LogDebugLevel::Informational);
    UAirBlueprintLib::LogMessageString("Target Gear: ", std::to_string(current_controls_.manual_gear), LogDebugLevel::Informational);
}

//*** Start: UpdatableState implementation ***//
void CarPawnSimApi::resetImplementation()
{
    PawnSimApi::resetImplementation();

    pawn_api_->reset();
}

//physics tick
void CarPawnSimApi::update()
{
    pawn_api_->update();

    PawnSimApi::update();
}

void CarPawnSimApi::reportState(StateReporter& reporter)
{
    PawnSimApi::reportState(reporter);

    vehicle_api_->reportState(reporter);
}

//*** End: UpdatableState implementation ***//
