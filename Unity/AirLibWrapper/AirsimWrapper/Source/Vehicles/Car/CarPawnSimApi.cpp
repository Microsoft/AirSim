#include <exception>
#include "CarPawnSimApi.h"
#include "vehicles/car/api/CarApiBase.hpp"
#include "CarPawnApi.h"
#include "../../PInvokeWrapper.h"
#include "../../UnityUtilities.hpp"
#include "../../UnitySensors/UnitySensorFactory.h"


CarPawnSimApi::CarPawnSimApi(const Params& params,
	const msr::airlib::CarApiBase::CarControls& keyboard_controls, std::string car_name)
	: PawnSimApi(params), params_(params),
	keyboard_controls_(keyboard_controls), car_name_(car_name)
{
	createVehicleApi(static_cast<CarPawn*>(params.pawn), params.home_geopoint);
	joystick_controls_ = msr::airlib::CarApiBase::CarControls();
}

void CarPawnSimApi::initialize()
{
    PawnSimApi::initialize();

    createVehicleApi(static_cast<CarPawn*>(params_.pawn), params_.home_geopoint);

    //TODO: should do reset() here?
    joystick_controls_ = msr::airlib::CarApiBase::CarControls();
}

void CarPawnSimApi::createVehicleApi(CarPawn* pawn, const msr::airlib::GeoPoint& home_geopoint)
{
    std::shared_ptr<UnitySensorFactory> sensor_factory = std::make_shared<UnitySensorFactory>(car_name_, &getNedTransform());

    vehicle_api_ = CarApiFactory::createApi(getVehicleSetting(), sensor_factory, (*getGroundTruthKinematics()),
                                            (*getGroundTruthEnvironment()), home_geopoint);
    pawn_api_ = std::unique_ptr<CarPawnApi>(new CarPawnApi(pawn, getGroundTruthKinematics(), car_name_, vehicle_api_.get()));
}

std::string CarPawnSimApi::getRecordFileLine(bool is_header_line) const
{
	std::string common_line = PawnSimApi::getRecordFileLine(is_header_line);
	if (is_header_line)
	{
		return common_line +
			"Throttle\tSteering\tBrake\tGear\tHandbrake\tRPM\tSpeed\t";
	}

	const msr::airlib::Kinematics::State* kinematics = getGroundTruthKinematics();
	const auto state = pawn_api_->getCarState();

	common_line
		.append(std::to_string(current_controls_.throttle)).append("\t")
		.append(std::to_string(current_controls_.steering)).append("\t")
		.append(std::to_string(current_controls_.brake)).append("\t")
		.append(std::to_string(state.gear)).append("\t")
		.append(std::to_string(state.handbrake)).append("\t")
		.append(std::to_string(state.rpm)).append("\t")
		.append(std::to_string(state.speed)).append("\t")
		;

	return common_line;
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

	for (auto i = 0; i < vehicle_api_messages_.size(); ++i)
	{
		PrintLogMessage(vehicle_api_messages_[i].c_str(), "LogDebugLevel::Success", car_name_.c_str(), ErrorLogSeverity::Information);
	}

	try
	{
		vehicle_api_->sendTelemetry(dt);
	}
	catch (std::exception &e)
	{
		PrintLogMessage(e.what(), "LogDebugLevel::Failure", car_name_.c_str(), ErrorLogSeverity::Error);
	}
}

void CarPawnSimApi::updateCarControls()
{
	auto rc_data = getRCData();

	if (rc_data.is_initialized)
	{
		if (!rc_data.is_valid)
		{
			PrintLogMessage("Control Mode: ", "[INVALID] Wheel/Joystick  LogDebugLevel::Informational", car_name_.c_str(), ErrorLogSeverity::Information);
			return;
		}
		PrintLogMessage("Control Mode: ", "Wheel/Joystick - LogDebugLevel::Informational", car_name_.c_str(), ErrorLogSeverity::Information);

		// Thrustmaster devices
		if (rc_data.vendor_id == "VID_044F") {
			joystick_controls_.steering = rc_data.yaw;
			joystick_controls_.throttle = (-rc_data.right_z + 1) / 2;
			joystick_controls_.brake = rc_data.throttle;

			auto car_state = vehicle_api_->getCarState();
			float rumble_strength = 0.66f + (car_state.rpm
				/ car_state.maxrpm) / 3.0f;
			float auto_center = (1.0f - 1.0f / (std::abs(car_state.speed / 120) + 1.0f))
				* (rc_data.yaw / 3.0f);
		}
		// Anything else, typically Logitech G920 wheel
		else
		{
			joystick_controls_.steering = (rc_data.throttle * 2 - 1) * 1.25f;
			joystick_controls_.throttle = (-rc_data.roll + 1) / 2;
			joystick_controls_.brake = -rc_data.right_z + 1;
		}

		//Two steel levers behind wheel
		joystick_controls_.handbrake = (rc_data.getSwitch(5)) | (rc_data.getSwitch(6)) ? 1 : 0;

		if ((rc_data.getSwitch(8)) | (rc_data.getSwitch(1)))
		{ //RSB button or B button
			joystick_controls_.manual_gear = -1;
			joystick_controls_.is_manual_gear = true;
			joystick_controls_.gear_immediate = true;
		}
		else if ((rc_data.getSwitch(9)) | (rc_data.getSwitch(0)))
		{ //LSB button or A button
			joystick_controls_.manual_gear = 0;
			joystick_controls_.is_manual_gear = false;
			joystick_controls_.gear_immediate = true;
		}

		current_controls_ = joystick_controls_;
	}
	else
	{
		PrintLogMessage("Control Mode: ", "Keyboard", getVehicleName().c_str(), ErrorLogSeverity::Information);
		current_controls_ = keyboard_controls_;
	}

	//if API-client control is not active then we route keyboard/joystick control to car
	if (!vehicle_api_->isApiControlEnabled())
	{
		//all car controls from anywhere must be routed through API component
		vehicle_api_->setCarControls(current_controls_);
		pawn_api_->updateMovement(current_controls_);
	}
	else
	{
		PrintLogMessage("Control Mode: ", "API", getVehicleName().c_str(), ErrorLogSeverity::Information);
		current_controls_ = vehicle_api_->getCarControls();
		pawn_api_->updateMovement(current_controls_);
	}
}

//*** Start: UpdatableState implementation ***//
void CarPawnSimApi::resetImplementation()
{
	setPose(UnityUtilities::Convert_to_Pose(GetInitialPose()), false);
	Reset();

	PawnSimApi::resetImplementation();
	pawn_api_->reset();
}

//physics tick
void CarPawnSimApi::update()
{
	pawn_api_->update();
	PawnSimApi::update();
}

//void CarPawnSimApi::reportState(StateReporter& reporter)
//{
//	// report actual location in unreal coordinates so we can plug that into the UE editor to move the drone.
//	AirSimPose pose = GetPose(getVehicleName().c_str());
//	reporter.writeValue("unreal pos", Vector3r(pose.position.x, pose.position.y, pose.position.z));
//}
//*** End: UpdatableState implementation ***//
