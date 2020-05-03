#include "common/common_utils/StrictMode.hpp"
STRICT_MODE_OFF
#ifndef RPCLIB_MSGPACK
#define RPCLIB_MSGPACK clmdep_msgpack
#endif // !RPCLIB_MSGPACK
#include "rpc/rpc_error.h"
STRICT_MODE_ON

#include "vehicles/multirotor/api/MultirotorRpcLibClient.hpp"
#include "common/common_utils/FileSystem.hpp"
#include <iostream>
#include <chrono>
#include "common/AirSimSettings.hpp"

class SettingsLoader
{
public:
    SettingsLoader()
    {
        initializeSettings();
    }

    ~SettingsLoader() {};

private:
    std::string getSimMode()
    {
        Settings& settings_json = Settings::loadJSonString(settingsText_);
        return settings_json.getString("SimMode", "");
    }

    bool readSettingsTextFromFile(std::string settingsFilepath) 
    {
        // check if path exists
        bool found = std::ifstream(settingsFilepath.c_str()).good(); 
        if (found)
        {
            std::ifstream ifs(settingsFilepath);
            std::stringstream buffer;
            buffer << ifs.rdbuf();
            settingsText_ = buffer.str(); 
        }
        return found;
    }

    bool initializeSettings()
    {
        if (readSettingsTextFromFile(msr::airlib::Settings::Settings::getUserDirectoryFullPath("settings.json")))
        {
            AirSimSettings::initializeSettings(settingsText_);

            Settings& settings_json = Settings::loadJSonString(settingsText_);
            std::string simmode_name = settings_json.getString("SimMode", "");
            AirSimSettings::singleton().load(std::bind(&SettingsLoader::getSimMode, this));
            return true;
        }
        else
        {
            return false;
        }
    }

private:
    std::string settingsText_;
};