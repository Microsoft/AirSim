#pragma once

#include "CarPawn.h"
#include "../../SimMode/SimModeBase.h"
#include "../../UnityPawn.h"

class SimModeCar : public SimModeBase
{
private:
	typedef msr::airlib::ClockFactory ClockFactory;
	typedef common_utils::Utils Utils;
	typedef msr::airlib::TTimePoint TTimePoint;
	typedef msr::airlib::TTimeDelta TTimeDelta;
	typedef CarPawn TVehiclePawn;
	typedef msr::airlib::VehicleSimApiBase VehicleSimApiBase;
	typedef msr::airlib::VectorMath VectorMath;
	typedef msr::airlib::Vector3r Vector3r;

private:
	void initializePauseState();

protected:
	virtual void setupClockSpeed() override;
	virtual std::unique_ptr<msr::airlib::ApiServerBase> createApiServer() const override;
	virtual bool isVehicleTypeSupported(const std::string& vehicle_type) const override;
	virtual std::unique_ptr<PawnSimApi> createVehicleSimApi(const PawnSimApi::Params& pawn_sim_api_params) const override;
	virtual msr::airlib::VehicleApiBase* getVehicleApi(const PawnSimApi::Params& pawn_sim_api_params,
		const PawnSimApi* sim_api) const override;

public:
	SimModeCar(int port_number);
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool isPaused() const override;
	virtual void pause(bool is_paused) override;
	virtual void continueForTime(double seconds) override;
	virtual UnityPawn* GetVehiclePawn(const std::string& vehicle_name) override;

private:
	std::atomic<float> current_clockspeed_;
	std::atomic<TTimeDelta> pause_period_;
	std::atomic<TTimePoint> pause_period_start_;
};