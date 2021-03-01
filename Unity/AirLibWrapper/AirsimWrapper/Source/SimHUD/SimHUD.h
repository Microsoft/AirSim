#pragma once

#include "../SimMode/SimModeBase.h"

class SimHUD
{
public:
	typedef msr::airlib::ImageCaptureBase::ImageType ImageType;
	typedef msr::airlib::AirSimSettings AirSimSettings;

private:
	void createSimMode();
	void initializeSettings();
	const std::vector<AirSimSettings::SubwindowSetting>& getSubWindowSettings() const;
	std::vector<AirSimSettings::SubwindowSetting>& getSubWindowSettings();
	bool getSettingsText(std::string& settingsText);
	bool readSettingsTextFromFile(std::string fileName, std::string& settingsText);
	std::string getSimModeFromUser();

public:
	SimHUD(std::string sime_mode_name, int port_number);
	SimModeBase* GetSimMode();
	ImageType getSubwindowCameraType(int window_index);
	void setSubwindowCameraType(int window_index, ImageType type);
	bool getSubwindowVisible(int window_index);
	void setSubwindowVisible(int window_index, bool is_visible);
	virtual void BeginPlay();
	virtual void EndPlay();
	virtual void Tick(float DeltaSeconds);

private:
	typedef common_utils::Utils Utils;
	SimModeBase* simmode_;
	std::string sim_mode_name_;
	int port_number_;

public:
	bool server_started_Successfully_;
};