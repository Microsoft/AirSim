#include "SimModeWorldMultiRotor.h"
#include "vehicles/multirotor/api/MultirotorApiBase.hpp"
#include "MultirotorPawnSimApi.h"
#include "vehicles/multirotor/api/MultirotorRpcLibServer.hpp"

SimModeWorldMultiRotor::SimModeWorldMultiRotor(int port_number) :
	SimModeWorldBase(port_number)
{
}

void SimModeWorldMultiRotor::BeginPlay()
{
	SimModeWorldBase::BeginPlay();

	//let base class setup physics world
	initializeForPlay();
}

void SimModeWorldMultiRotor::Tick(float DeltaSeconds)
{
	SimModeWorldBase::Tick(DeltaSeconds);
}

void SimModeWorldMultiRotor::EndPlay()
{
	//stop physics thread before we dismantle
	stopAsyncUpdator();
	SimModeWorldBase::EndPlay();
}

UnityPawn* SimModeWorldMultiRotor::GetVehiclePawn(const std::string& vehicle_name)
{
	return new FlyingPawn(vehicle_name);
}

void SimModeWorldMultiRotor::setupClockSpeed()
{
	typedef msr::airlib::ClockFactory ClockFactory;
	float clock_speed = getSettings().clock_speed;

	//setup clock in ClockFactory
	std::string clock_type = getSettings().clock_type;

	if (clock_type == "ScalableClock")
	{
		//scalable clock returns interval same as wall clock but multiplied by a scale factor
		ClockFactory::get(std::make_shared<msr::airlib::ScalableClock>(clock_speed == 1 ? 1 : 1 / clock_speed));
	}
	else if (clock_type == "SteppableClock")
	{
		//steppable clock returns interval that is a constant number irrespective of wall clock
		//we can either multiply this fixed interval by scale factor to speed up/down the clock
		//but that would cause vehicles like quadrotors to become unstable
		//so alternative we use here is instead to scale control loop frequency. The downside is that
		//depending on compute power available, we will max out control loop frequency and therefore can no longer
		//get increase in clock speed

		//Approach 1: scale clock period, no longer used now due to quadrotor instability
		//ClockFactory::get(std::make_shared<msr::airlib::SteppableClock>(
		//static_cast<msr::airlib::TTimeDelta>(getPhysicsLoopPeriod() * 1E-9 * clock_speed)));

		//Approach 2: scale control loop frequency if clock is speeded up
		if (clock_speed >= 1)
		{
			ClockFactory::get(std::make_shared<msr::airlib::SteppableClock>(
				static_cast<msr::airlib::TTimeDelta>(getPhysicsLoopPeriod() * 1E-9))); //no clock_speed multiplier

			setPhysicsLoopPeriod(getPhysicsLoopPeriod() / static_cast<long long>(clock_speed));
		}
		else
		{
			//for slowing down, this don't generate instability
			ClockFactory::get(std::make_shared<msr::airlib::SteppableClock>(
				static_cast<msr::airlib::TTimeDelta>(getPhysicsLoopPeriod() * 1E-9 * clock_speed)));
		}
	}
	else
		throw std::invalid_argument(common_utils::Utils::stringf(
			"clock_type %s is not recognized", clock_type.c_str()));
}

//-------------------------------- overrides -----------------------------------------------//

std::unique_ptr<msr::airlib::ApiServerBase> SimModeWorldMultiRotor::createApiServer() const
{
#ifdef AIRLIB_NO_RPC
	return ASimModeBase::createApiServer();
#else
	auto ptr = std::unique_ptr<msr::airlib::ApiServerBase>(new msr::airlib::MultirotorRpcLibServer(
		getApiProvider(), getSettings().api_server_address, port_number_));

	return ptr;
#endif
}

bool SimModeWorldMultiRotor::isVehicleTypeSupported(const std::string& vehicle_type) const
{
	return ((vehicle_type == AirSimSettings::kVehicleTypeSimpleFlight) ||
		(vehicle_type == AirSimSettings::kVehicleTypePX4));
}

std::unique_ptr<PawnSimApi> SimModeWorldMultiRotor::createVehicleSimApi(
	const PawnSimApi::Params& pawn_sim_api_params) const
{
	auto vehicle_sim_api = std::unique_ptr<PawnSimApi>(new MultirotorPawnSimApi(pawn_sim_api_params));
    vehicle_sim_api->initialize();
    //For multirotors the vehicle_sim_api are in PhysicsWOrld container and then get reseted when world gets reseted
	return vehicle_sim_api;
}

msr::airlib::VehicleApiBase* SimModeWorldMultiRotor::getVehicleApi(const PawnSimApi::Params& pawn_sim_api_params,
	const PawnSimApi* sim_api) const
{
	const auto multirotor_sim_api = static_cast<const MultirotorPawnSimApi*>(sim_api);
	return multirotor_sim_api->getVehicleApi();
}