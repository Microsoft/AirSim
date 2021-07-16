#include "airsim_settings_parser.h"
#include <iostream>

AirSimSettingsParser::AirSimSettingsParser(const std::string& host_ip)
    : host_ip_(host_ip)
{
    success_ = initializeSettings();
}

bool AirSimSettingsParser::success()
{
    return success_;
}

bool AirSimSettingsParser::getSettingsText(std::string& settings_text) const
{
    try {
        std::cout << "Fetching settings.json from AirSim" << std::endl;
        msr::airlib::RpcLibClientBase airsim_client(host_ip_);
        airsim_client.confirmConnection();

        settings_text = airsim_client.getSettingsString();
    }
    catch (rpc::rpc_error& e) {
        std::string msg = e.get_error().as<std::string>();
        std::cout << "Exception raised by the API, something went wrong: " << msg << std::endl;

        // TODO: Fallback can be removed after a new release of AirSim
        std::cout << "Falling back to reading from ~/Documents/AirSim/settings.json" << std::endl;

        std::string settings_file_path = msr::airlib::Settings::Settings::getUserDirectoryFullPath("settings.json");

        std::ifstream ifs(settings_file_path);
        // check if path exists
        if (ifs.good()) {
            std::stringstream buffer;
            buffer << ifs.rdbuf();
            // todo airsim's simhud.cpp does error checking here
            settings_text = buffer.str(); // todo convert to utf8 as done in simhud.cpp?
        }
    }

    return !settings_text.empty();
}

std::string AirSimSettingsParser::getSimMode()
{
    Settings& settings_json = Settings::loadJSonString(settings_text_);
    return settings_json.getString("SimMode", "");
}

// mimics void ASimHUD::initializeSettings()
bool AirSimSettingsParser::initializeSettings()
{
    if (getSettingsText(settings_text_)) {
        AirSimSettings::initializeSettings(settings_text_);

        AirSimSettings::singleton().load(std::bind(&AirSimSettingsParser::getSimMode, this));
        std::cout << "SimMode: " << AirSimSettings::singleton().simmode_name << std::endl;

        return true;
    }

    return false;
}