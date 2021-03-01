
#include "SimModeCar.h"
#include "common/AirSimSettings.hpp"
#include "CarPawnSimApi.h"
#include "vehicles/car/api/CarRpcLibServer.hpp"
#include "../../PInvokeWrapper.h"

SimModeCar::SimModeCar(int port_number) : SimModeBase(port_number)
{
}

void SimModeCar::BeginPlay()
{
	SimModeBase::BeginPlay();
	initializePauseState();
}

void SimModeCar::initializePauseState()
{
	pause_period_ = 0;
	pause_period_start_ = 0;
	pause(false);
}

bool SimModeCar::isPaused() const
{
	return current_clockspeed_ == 0;
}

void SimModeCar::pause(bool is_paused)
{
	if (is_paused)
		current_clockspeed_ = 0;
	else
		current_clockspeed_ = getSettings().clock_speed;

	Pause(current_clockspeed_);
}

void SimModeCar::continueForTime(double seconds)
{
	pause_period_start_ = ClockFactory::get()->nowNanos();
	pause_period_ = seconds;
	pause(false);
}

void SimModeCar::setupClockSpeed()
{
	current_clockspeed_ = getSettings().clock_speed;
}

void SimModeCar::Tick(float DeltaSeconds)
{
	SimModeBase::Tick(DeltaSeconds);

	if (pause_period_start_ > 0) {
		if (ClockFactory::get()->elapsedSince(pause_period_start_) >= pause_period_) {
			if (!isPaused())
				pause(true);

			pause_period_start_ = 0;
		}
	}
}

std::unique_ptr<msr::airlib::ApiServerBase> SimModeCar::createApiServer() const
{
#ifdef AIRLIB_NO_RPC
	return ASimModeBase::createApiServer();
#else
	auto ptr = std::unique_ptr<msr::airlib::ApiServerBase>(new msr::airlib::CarRpcLibServer(
		getApiProvider(), getSettings().api_server_address, port_number_));

	return ptr;
#endif
}

bool SimModeCar::isVehicleTypeSupported(const std::string& vehicle_type) const
{
	return vehicle_type == AirSimSettings::kVehicleTypePhysXCar;
}

UnityPawn* SimModeCar::GetVehiclePawn(const std::string& vehicle_name)
{
	return new CarPawn(vehicle_name);
}

std::unique_ptr<PawnSimApi> SimModeCar::createVehicleSimApi(const PawnSimApi::Params& pawn_sim_api_params) const
{
	auto vehicle_pawn = static_cast<TVehiclePawn*>(pawn_sim_api_params.pawn);

	auto vehicle_sim_api = std::unique_ptr<PawnSimApi>(new CarPawnSimApi(pawn_sim_api_params,
		vehicle_pawn->getKeyBoardControls(), pawn_sim_api_params.vehicle_name));
    vehicle_sim_api->initialize();
	vehicle_sim_api->reset();
	return vehicle_sim_api;
}

msr::airlib::VehicleApiBase* SimModeCar::getVehicleApi(const PawnSimApi::Params& pawn_sim_api_params, const PawnSimApi* sim_api) const
{
	const auto car_sim_api = static_cast<const CarPawnSimApi*>(sim_api);
	return car_sim_api->getVehicleApi();
}