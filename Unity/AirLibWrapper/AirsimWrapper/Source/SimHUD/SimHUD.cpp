#include "SimHUD.h"
#include "../PInvokeWrapper.h"
#include "../Vehicles/Car/SimModeCar.h"
#include "../Vehicles/Multirotor/SimModeWorldMultiRotor.h"

SimHUD::SimHUD(std::string sim_mode_name, int port_number) :
	sim_mode_name_(sim_mode_name), port_number_(port_number)
{
	server_started_Successfully_ = false;
}

void SimHUD::BeginPlay()
{
	try {
		initializeSettings();
		createSimMode();
		if (simmode_)
			simmode_->startApiServer();

		server_started_Successfully_ = true;
	}
	catch (std::exception& ex)
	{
		PrintLogMessage("Error at startup: ", ex.what(), "", ErrorLogSeverity::Error);
	}
}

void SimHUD::Tick(float DeltaSeconds)
{
	if (simmode_)
	{
		simmode_->Tick(DeltaSeconds);
	}
}

void SimHUD::EndPlay()
{
	if (simmode_)
	{
		simmode_->EndPlay();
		simmode_->stopApiServer();
	}
	if (simmode_) {
		delete simmode_;
		simmode_ = nullptr;
	}
}

SimHUD::ImageType SimHUD::getSubwindowCameraType(int window_index)
{
	//TODO: index check
	return getSubWindowSettings().at(window_index).image_type;
}

void SimHUD::setSubwindowCameraType(int window_index, ImageType type)
{
	getSubWindowSettings().at(window_index).image_type = type;
}

bool SimHUD::getSubwindowVisible(int window_index)
{
	return getSubWindowSettings().at(window_index).visible;
}

void SimHUD::setSubwindowVisible(int window_index, bool is_visible)
{
	getSubWindowSettings().at(window_index).visible = is_visible;
}

void SimHUD::initializeSettings()
{
	std::string settingsText;
	if (getSettingsText(settingsText))
		AirSimSettings::initializeSettings(settingsText);
	else
		AirSimSettings::createDefaultSettingsFile();

	AirSimSettings::singleton().load(std::bind(&SimHUD::getSimModeFromUser, this));

	for (const auto& warning : AirSimSettings::singleton().warning_messages) {
		PrintLogMessage(warning.c_str(), "LogDebugLevel::Failure", "", ErrorLogSeverity::Error);
	}
	for (const auto& error : AirSimSettings::singleton().error_messages) {
		PrintLogMessage(error.c_str(), "settings.json", "", ErrorLogSeverity::Error);
	}
}

const std::vector<SimHUD::AirSimSettings::SubwindowSetting>& SimHUD::getSubWindowSettings() const
{
	return AirSimSettings::singleton().subwindow_settings;
}

std::vector<SimHUD::AirSimSettings::SubwindowSetting>& SimHUD::getSubWindowSettings()
{
	return AirSimSettings::singleton().subwindow_settings;
}

SimModeBase* SimHUD::GetSimMode()
{
	return simmode_;
}

std::string SimHUD::getSimModeFromUser()
{
	return sim_mode_name_;
}

void SimHUD::createSimMode()
{
	std::string simmode_name = AirSimSettings::singleton().simmode_name;

	if (simmode_name == "Multirotor")
		simmode_ = new SimModeWorldMultiRotor(port_number_);
	else if (simmode_name == "Car")
		simmode_ = new SimModeCar(port_number_);

	simmode_->BeginPlay();
}

// Attempts to parse the settings text from one of multiple locations.
// First, check the command line for settings provided via "-s" or "--settings" arguments
// Next, check the executable's working directory for the settings file.
// Finally, check the user's documents folder. 
// If the settings file cannot be read, throw an exception

bool SimHUD::getSettingsText(std::string& settingsText)
{
	return (
		readSettingsTextFromFile(msr::airlib::Settings::getExecutableFullPath("settings.json"), settingsText)
		||
		readSettingsTextFromFile(msr::airlib::Settings::Settings::getUserDirectoryFullPath("settings.json"), settingsText));
}

bool SimHUD::readSettingsTextFromFile(std::string settingsFilepath, std::string& settingsText)
{
	std::ifstream file(settingsFilepath);
	if (!file.fail())
	{
		std::stringstream buffer;
		buffer << file.rdbuf();
		settingsText = buffer.str();
		PrintLogMessage("Loaded settings from ", settingsFilepath.c_str(), "", ErrorLogSeverity::Information);
		return true;
	}
	else
	{
		PrintLogMessage("Cannot read settings file ", settingsFilepath.c_str(), "", ErrorLogSeverity::Error);
	}
	return false;
}