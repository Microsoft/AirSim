#include "AirSimGameMode.h"
#include "Misc/FileHelper.h"
#include "SimHUD/SimHUD.h"
#include "SimHUD/SimHUDMultiRotor.h"
#include "common/Common.hpp"
#include "AirBlueprintLib.h"
#include "controllers/Settings.hpp"
 
////for OutputDebugString
//#ifdef _MSC_VER
//#define WIN32_LEAN_AND_MEAN
//#include <windows.h>
//#endif

class AUnrealLog : public msr::airlib::Utils::Logger
{
public:
    virtual void log(int level, const std::string& message) override 
    {
        size_t tab_pos;
        static const std::string delim = ":\t";
        if ((tab_pos = message.find(delim)) != std::string::npos) {
            UAirBlueprintLib::LogMessageString(message.substr(0, tab_pos), 
                message.substr(tab_pos + delim.size(), std::string::npos), LogDebugLevel::Informational);
            
            return; //display only
        }

        if (level == msr::airlib::Utils::kLogLevelError) {
            UE_LOG(LogAirSim, Error, TEXT("%s"), *FString(message.c_str()));
        }
        else if (level == msr::airlib::Utils::kLogLevelWarn) {
            UE_LOG(LogAirSim, Warning, TEXT("%s"), *FString(message.c_str()));
        }
        else {
            UE_LOG(LogAirSim, Log, TEXT("%s"), *FString(message.c_str()));
        }
  
//#ifdef _MSC_VER
//        //print to VS output window
//        OutputDebugString(std::wstring(message.begin(), message.end()).c_str());
//#endif

        //also do default logging
        msr::airlib::Utils::Logger::log(level, message);
    }
};

static AUnrealLog GlobalASimLog;

AAirSimGameMode::AAirSimGameMode(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    DefaultPawnClass = nullptr;
    common_utils::Utils::getSetLogger(&GlobalASimLog);
    SetHUD();
}

void AAirSimGameMode::StartPlay() 
{
    Super::StartPlay();
}

void AAirSimGameMode::SetHUD()
{
    msr::airlib::Settings& settings = msr::airlib::Settings::singleton();
    std::string default_vehicle = settings.getString("DefaultVehicle", "MultiRotor");
    if (default_vehicle == "MultiRotor")
    {
        HUDClass = ASimHUDMultiRotor::StaticClass();
    }
    else
    {
        // No HUD gets created
    }
}