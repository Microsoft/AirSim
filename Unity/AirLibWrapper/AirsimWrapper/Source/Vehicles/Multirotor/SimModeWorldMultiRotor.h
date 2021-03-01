#pragma once

#include "FlyingPawn.h"
#include "../../SimMode/SimModeWorldBase.h"

class SimModeWorldMultiRotor : public SimModeWorldBase
{
protected:
	virtual void setupClockSpeed() override;
	virtual std::unique_ptr<msr::airlib::ApiServerBase> createApiServer() const override;
	virtual bool isVehicleTypeSupported(const std::string& vehicle_type) const override;
	virtual std::unique_ptr<PawnSimApi> createVehicleSimApi(
		const PawnSimApi::Params& pawn_sim_api_params) const override;
	virtual msr::airlib::VehicleApiBase* getVehicleApi(const PawnSimApi::Params& pawn_sim_api_params,
		const PawnSimApi* sim_api) const override;

public:
	SimModeWorldMultiRotor(int port_number);
	virtual void BeginPlay() override;
	virtual void EndPlay() override;
	void Tick(float DeltaSeconds) override;
	UnityPawn* GetVehiclePawn(const std::string& vehicle_name) override;

private:
	typedef FlyingPawn TVehiclePawn;
};
