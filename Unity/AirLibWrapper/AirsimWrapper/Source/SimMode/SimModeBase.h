#pragma once

#include <string>
#include "common/AirSimSettings.hpp"
#include "common/ClockFactory.hpp"
#include "common/StateReporterWrapper.hpp"
#include "api/ApiProvider.hpp"
#include "api/ApiServerBase.hpp"
#include "../PawnSimApi.h"
#include "../UnityPawn.h"

class SimModeBase
{
private:
	typedef common_utils::Utils Utils;
	typedef msr::airlib::ClockFactory ClockFactory;
	typedef msr::airlib::TTimePoint TTimePoint;
	typedef msr::airlib::TTimeDelta TTimeDelta;

protected:
	typedef msr::airlib::AirSimSettings AirSimSettings;

private:
	const msr::airlib::Vector3r GetVehiclePosition(const std::string& vehicle_name);
	UnityTransform GetVehicleStartTransform();
	void showClockStats();

protected:
	virtual std::unique_ptr<msr::airlib::ApiServerBase> createApiServer() const;
	virtual bool isVehicleTypeSupported(const std::string& vehicle_type) const;
	virtual std::unique_ptr<PawnSimApi> createVehicleSimApi(const PawnSimApi::Params& pawn_sim_api_params) const;
	virtual msr::airlib::VehicleApiBase* getVehicleApi(const PawnSimApi::Params& pawn_sim_api_params, const PawnSimApi* sim_api) const;
	virtual UnityPawn* GetVehiclePawn(const std::string& vehicle_name);
	virtual void setupVehiclesAndCamera();
	virtual void setupClockSpeed(); //called when SimMode should handle clock speed setting
	virtual void updateDebugReport(msr::airlib::StateReporterWrapper& debug_reporter);
	virtual const msr::airlib::AirSimSettings& getSettings() const;

public:
	SimModeBase(int port_number);
	virtual void BeginPlay();
	virtual void EndPlay();
	virtual void Tick(float DeltaSeconds) = 0;
	virtual void reset();
	virtual std::string getDebugReport();
	virtual bool isPaused() const;
	virtual void pause(bool is_paused);
	virtual void continueForTime(double seconds);
	virtual void continueForFrames(uint32_t frames);
    virtual void setWind(const msr::airlib::Vector3r& wind) const;
	void startApiServer();
	void stopApiServer();
	bool isApiServerStarted();
	const NedTransform& getGlobalNedTransform();
    virtual void setTimeOfDay(bool is_enabled, const std::string& start_datetime, bool is_start_datetime_dst,
        float celestial_clock_speed, float update_interval_secs, bool move_sun);

	msr::airlib::ApiProvider* getApiProvider() const
	{
		return api_provider_.get();
	}

	const PawnSimApi* getVehicleSimApi(const std::string& vehicle_name = "") const
	{
		return static_cast<PawnSimApi*>(api_provider_->getVehicleSimApi(vehicle_name));
	}

	PawnSimApi* getVehicleSimApi(const std::string& vehicle_name = "")
	{
		return static_cast<PawnSimApi*>(api_provider_->getVehicleSimApi(vehicle_name));
	}

private:
	TTimePoint tod_sim_clock_start_;
	TTimePoint tod_last_update_;
	std::time_t tod_start_time_;
	std::unique_ptr<NedTransform> global_ned_transform_;
	std::unique_ptr<msr::airlib::WorldSimApiBase> world_sim_api_;
	std::unique_ptr<msr::airlib::ApiProvider> api_provider_;
	std::unique_ptr<msr::airlib::ApiServerBase> api_server_;
	msr::airlib::StateReporterWrapper debug_reporter_;
	std::vector<std::unique_ptr<msr::airlib::VehicleSimApiBase>> vehicle_sim_apis_;

protected:
	int record_tick_count;

public:
	int port_number_;
	bool EnableReport = false;
};