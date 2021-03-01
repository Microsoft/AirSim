#pragma once

#include "SimModeBase.h"
#include "physics/FastPhysicsEngine.hpp"
#include "physics/PhysicsWorld.hpp"

class SimModeWorldBase : public SimModeBase
{
private:
	typedef msr::airlib::UpdatableObject UpdatableObject;
	typedef msr::airlib::PhysicsEngineBase PhysicsEngineBase;
	typedef msr::airlib::ClockFactory ClockFactory;

private:
	//create the physics engine as needed from settings
	std::unique_ptr<PhysicsEngineBase> createPhysicsEngine();

protected:
	void startAsyncUpdator();
	void stopAsyncUpdator();
	virtual void updateDebugReport(msr::airlib::StateReporterWrapper& debug_reporter) override;
	long long getPhysicsLoopPeriod() const;
	void setPhysicsLoopPeriod(long long  period);

	//should be called by derived class once all api_provider_ is ready to use
	void initializeForPlay();

public:
	SimModeWorldBase(int port_number);
	virtual void BeginPlay() override;
	virtual void EndPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void reset() override;
	virtual std::string getDebugReport() override;
	virtual bool isPaused() const override;
	virtual void pause(bool is_paused) override;
	virtual void continueForTime(double seconds) override;

    virtual void setWind(const msr::airlib::Vector3r& wind) const override;

private:
	std::unique_ptr<msr::airlib::PhysicsWorld> physics_world_;
	PhysicsEngineBase* physics_engine_;

	/* 300Hz seems to be minimum for non-aggressive flights
	400Hz is needed for moderately aggressive flights (such as
	high yaw rate with simultaneous back move)
	500Hz is recommended for more aggressive flights
	Lenovo P50 high-end config laptop seems to be topping out at 400Hz.
	HP Z840 desktop high-end config seems to be able to go up to 500Hz.
	To increase freq with limited CPU power, switch Barometer to constant ref mode. */
	long long physics_loop_period_ = 3000000LL; //3ms
};
