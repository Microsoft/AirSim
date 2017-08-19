#include "SimModeWorldBase.h"
#include "common/ScalableClock.hpp"
#include "common/SteppableClock.hpp"
#include <exception>

const char ASimModeWorldBase::kUsageScenarioComputerVision[] = "ComputerVision";

ASimModeWorldBase::ASimModeWorldBase()
{
    static ConstructorHelpers::FClassFinder<APIPCamera> external_camera_class(TEXT("Blueprint'/AirSim/Blueprints/BP_PIPCamera'"));
    external_camera_class_ = external_camera_class.Succeeded() ? external_camera_class.Class : nullptr;
    static ConstructorHelpers::FClassFinder<ACameraDirector> camera_director_class(TEXT("Blueprint'/AirSim/Blueprints/BP_CameraDirector'"));
    camera_director_class_ = camera_director_class.Succeeded() ? camera_director_class.Class : nullptr;
}
void ASimModeWorldBase::BeginPlay()
{
    Super::BeginPlay();

    setupClock();

    manual_pose_controller = NewObject<UManualPoseController>();
    setupInputBindings();

    //call virtual method in derived class
    createVehicles(vehicles_);

    physics_world_.reset(new msr::airlib::PhysicsWorld(
        createPhysicsEngine(), toUpdatableObjects(vehicles_), 
        getPhysicsLoopPeriod()));

    if (usage_scenario == kUsageScenarioComputerVision) {
        manual_pose_controller->initializeForPlay();
        manual_pose_controller->setActor(getFpvVehiclePawn());
    }
}

void ASimModeWorldBase::setupClock()
{
    typedef msr::airlib::ClockFactory ClockFactory;


    if (clock_type == "ScalableClock")
        ClockFactory::get(std::make_shared<msr::airlib::ScalableClock>());
    else if (clock_type == "SteppableClock")
        ClockFactory::get(std::make_shared<msr::airlib::SteppableClock>(
            static_cast<msr::airlib::TTimeDelta>(getPhysicsLoopPeriod() * 1E-9)));
    else
        throw std::invalid_argument(common_utils::Utils::stringf(
            "clock_type %s is not recognized", clock_type.c_str()));
}

void ASimModeWorldBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    //remove everything that we created in BeginPlay
    physics_world_.release();
    physics_engine_.release();
    vehicles_.clear();
    manual_pose_controller = nullptr;

    Super::EndPlay(EndPlayReason);
}

void ASimModeWorldBase::startAsyncUpdator()
{
    physics_world_->startAsyncUpdator();
}
void ASimModeWorldBase::stopAsyncUpdator()
{
    physics_world_->stopAsyncUpdator();
}

long long ASimModeWorldBase::getPhysicsLoopPeriod() //nanoseconds
{
    /*
    300Hz seems to be minimum for non-aggresive flights
    400Hz is needed for moderately aggressive flights (such as
    high yaw rate with simultaneous back move)
    500Hz is recommanded for more aggressive flights
    Lenovo P50 high-end config laptop seems to be topping out at 400Hz.
    HP Z840 desktop high-end config seems to be able to go up to 500Hz.
    To increase freq with limited CPU power, switch Barometer to constant ref mode.
    */

    if (usage_scenario == kUsageScenarioComputerVision)
        return 30000000LL; //30ms
    else
        return 3000000LL; //3ms
}

std::vector<ASimModeWorldBase::UpdatableObject*> ASimModeWorldBase::toUpdatableObjects(
    const std::vector<ASimModeWorldBase::VehiclePtr>& vehicles)
{
    std::vector<UpdatableObject*> bodies;
    for (const VehiclePtr& body : vehicles)
        bodies.push_back(body.get());

    return bodies;
}


ASimModeWorldBase::PhysicsEngineBase* ASimModeWorldBase::createPhysicsEngine()
{
    if (physics_engine_name == "" || usage_scenario == kUsageScenarioComputerVision)
        physics_engine_.release(); //no physics engine
    else if (physics_engine_name == "FastPhysicsEngine") {
        msr::airlib::Settings fast_phys_settings;
        if (msr::airlib::Settings::singleton().getChild("FastPhysicsEngine", fast_phys_settings)) {
            physics_engine_.reset(
                new msr::airlib::FastPhysicsEngine(fast_phys_settings.getBool("EnableGroundLock", true))
            );
        }
        else {
            physics_engine_.reset(
                new msr::airlib::FastPhysicsEngine()
            );
        }
    }
    else {
        physics_engine_.release();
        UAirBlueprintLib::LogMessageString("Unrecognized physics engine name: ",  physics_engine_name, LogDebugLevel::Failure);
    }

    return physics_engine_.get();
}

size_t ASimModeWorldBase::getVehicleCount() const
{
    return vehicles_.size();
}

void ASimModeWorldBase::Tick(float DeltaSeconds)
{
    { //keep this lock as short as possible
        physics_world_->lock();

        physics_world_->enableStateReport(EnableReport);
        physics_world_->updateStateReport();

        for (auto& vehicle : vehicles_)
            vehicle->updateRenderedState();

        physics_world_->unlock();
    }

    //perfom any expensive rendering update outside of lock region
    for (auto& vehicle : vehicles_)
        vehicle->updateRendering(DeltaSeconds);

    Super::Tick(DeltaSeconds);
}

bool ASimModeWorldBase::checkConnection()
{
    return true;
}

void ASimModeWorldBase::reset()
{
    physics_world_->reset();
    
    Super::reset();
}

std::string ASimModeWorldBase::getReport()
{
    return physics_world_->getReport();
}

void ASimModeWorldBase::createVehicles(std::vector<VehiclePtr>& vehicles)
{
    //should be overridden by derived class
    //Unreal doesn't allow pure abstract methods in actors
}

void ASimModeWorldBase::setupInputBindings()
{
    Super::setupInputBindings();

    UAirBlueprintLib::BindActionToKey("InputEventResetAll", EKeys::BackSpace, this, &ASimModeWorldBase::reset);
}