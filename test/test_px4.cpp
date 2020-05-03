// #include "gtest/gtest.h"

#include "vehicles/multirotor/MultiRotorParamsFactory.hpp"
#include "settings_json_parser.h"

using namespace msr::airlib;
void run()
{
    SettingsLoader settings_loader;
    auto pixhawk = MultiRotorParamsFactory::createConfig(AirSimSettings::singleton().getVehicleSetting("PX4Vehicle"), 
        std::make_shared<SensorFactory>());
    auto api = pixhawk->createMultirotorApi();
}

int main()
{
    run ();
}