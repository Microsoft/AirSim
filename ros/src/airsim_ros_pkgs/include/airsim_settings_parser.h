#ifndef AIRSIM_SETTINGS_PARSER_H
#define AIRSIM_SETTINGS_PARSER_H

#include "common/common_utils/StrictMode.hpp"
STRICT_MODE_OFF
#ifndef RPCLIB_MSGPACK
#define RPCLIB_MSGPACK clmdep_msgpack
#endif // !RPCLIB_MSGPACK
#include "rpc/rpc_error.h"
STRICT_MODE_ON

#include "common/AirSimSettings.hpp"
#include "common/common_utils/FileSystem.hpp"
#include "vehicles/multirotor/api/MultirotorRpcLibClient.hpp"

// a minimal airsim settings parser, adapted from Unreal/Plugins/AirSim/SimHUD/SimHUD.h
class AirSimSettingsParser
{
public:
    typedef msr::airlib::AirSimSettings AirSimSettings;

public:
    AirSimSettingsParser(const std::string& host_ip);
    ~AirSimSettingsParser(){};

    bool success();

private:
    std::string getSimMode();
    bool getSettingsText(std::string& settings_text) const;
    bool initializeSettings();

    bool success_;
    std::string settings_text_;
    std::string host_ip_;
};

#endif // AIRSIM_SETTINGS_PARSER_H