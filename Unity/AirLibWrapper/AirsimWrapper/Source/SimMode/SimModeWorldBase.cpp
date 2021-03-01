#include <exception>
#include "SimModeWorldBase.h"
#include "../PInvokeWrapper.h"

SimModeWorldBase::SimModeWorldBase(int port_number) : 
	SimModeBase(port_number)
{
}

void SimModeWorldBase::BeginPlay()
{
	SimModeBase::BeginPlay();
}

void SimModeWorldBase::initializeForPlay()
{
	std::vector<msr::airlib::UpdatableObject*> vehicles;
	for (auto& api : getApiProvider()->getVehicleSimApis())
		vehicles.push_back(api);

	std::unique_ptr<PhysicsEngineBase> physics_engine = createPhysicsEngine();
	physics_engine_ = physics_engine.get();
	physics_world_.reset(new msr::airlib::PhysicsWorld(std::move(physics_engine),
		vehicles, getPhysicsLoopPeriod()));
}

void SimModeWorldBase::EndPlay()
{
	//remove everything that we created in BeginPlay
	physics_world_.reset();
	SimModeBase::EndPlay();
}

void SimModeWorldBase::startAsyncUpdator()
{
	physics_world_->startAsyncUpdator();
}

void SimModeWorldBase::stopAsyncUpdator()
{
	physics_world_->stopAsyncUpdator();
}

long long SimModeWorldBase::getPhysicsLoopPeriod() const //nanoseconds
{
	return physics_loop_period_;
}

void SimModeWorldBase::setPhysicsLoopPeriod(long long  period)
{
	physics_loop_period_ = period;
}

std::unique_ptr<SimModeWorldBase::PhysicsEngineBase> SimModeWorldBase::createPhysicsEngine()
{
	std::unique_ptr<PhysicsEngineBase> physics_engine;
	std::string physics_engine_name = getSettings().physics_engine_name;
	if (physics_engine_name == "")
	{
		physics_engine.reset(); //no physics engine
	}
	else if (physics_engine_name == "FastPhysicsEngine") 
	{
		msr::airlib::Settings fast_phys_settings;
		if (msr::airlib::Settings::singleton().getChild("FastPhysicsEngine", fast_phys_settings)) 
		{
			physics_engine.reset(new msr::airlib::FastPhysicsEngine(fast_phys_settings.getBool("EnableGroundLock", true)));
		}
		else 
		{
			physics_engine.reset(new msr::airlib::FastPhysicsEngine());
		}
	}
	else 
	{
		physics_engine.reset();
		PrintLogMessage("Unrecognized physics engine name: ", physics_engine_name.c_str(), "", ErrorLogSeverity::Warnning);
	}
	return physics_engine;
}

bool SimModeWorldBase::isPaused() const
{
	return physics_world_->isPaused();
}

void SimModeWorldBase::pause(bool is_paused)
{
	physics_world_->pause(is_paused);
}

void SimModeWorldBase::continueForTime(double seconds)
{
	physics_world_->continueForTime(seconds);
}

void SimModeWorldBase::setWind(const msr::airlib::Vector3r& wind) const
{
    physics_engine_->setWind(wind);
}

void SimModeWorldBase::updateDebugReport(msr::airlib::StateReporterWrapper& debug_reporter)
{
	unused(debug_reporter);
	//we use custom debug reporting for this class
}

void SimModeWorldBase::Tick(float DeltaSeconds)
{
	{ //keep this lock as short as possible
		physics_world_->lock();

		physics_world_->enableStateReport(EnableReport);
		physics_world_->updateStateReport();

		for (auto& api : getApiProvider()->getVehicleSimApis())
			api->updateRenderedState(DeltaSeconds);

		physics_world_->unlock();
	}

	//perform any expensive rendering update outside of lock region
	for (auto& api : getApiProvider()->getVehicleSimApis())
		api->updateRendering(DeltaSeconds);

	SimModeBase::Tick(DeltaSeconds);
}

void SimModeWorldBase::reset()
{
	physics_world_->reset();
	//no need to call base reset because of our custom implementation
}

std::string SimModeWorldBase::getDebugReport()
{
	return physics_world_->getDebugReport();
}