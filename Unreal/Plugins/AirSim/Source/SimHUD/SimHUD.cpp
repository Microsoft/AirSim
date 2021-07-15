#include "SimHUD.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/FileHelper.h"

#include "Vehicles/Multirotor/SimModeWorldMultiRotor.h"
#include "Vehicles/Car/SimModeCar.h"
#include "Vehicles/ComputerVision/SimModeComputerVision.h"

#include "common/AirSimSettings.hpp"
#include <stdexcept>

ASimHUD::ASimHUD()
{
    static ConstructorHelpers::FClassFinder<UUserWidget> hud_widget_class(TEXT("WidgetBlueprint'/AirSim/Blueprints/BP_SimHUDWidget'"));
    widget_class_ = hud_widget_class.Succeeded() ? hud_widget_class.Class : nullptr;
}

void ASimHUD::BeginPlay()
{
    Super::BeginPlay();

    try {
        UAirBlueprintLib::OnBeginPlay();
        loadLevel();

        initializeSettings();
        setUnrealEngineSettings();
        createSimMode();
        createMainWidget();
        setupInputBindings();
        if (simmode_)
            simmode_->startApiServer();
    }
    catch (std::exception& ex) {
        UAirBlueprintLib::LogMessageString("Error at startup: ", ex.what(), LogDebugLevel::Failure);
        //FGenericPlatformMisc::PlatformInit();
        //FGenericPlatformMisc::MessageBoxExt(EAppMsgType::Ok, TEXT("Error at Startup"), ANSI_TO_TCHAR(ex.what()));
        UAirBlueprintLib::ShowMessage(EAppMsgType::Ok, std::string("Error at startup: ") + ex.what(), "Error");
    }
}

void ASimHUD::Tick(float DeltaSeconds)
{
    if (simmode_ && simmode_->EnableReport)
        widget_->updateDebugReport(simmode_->getDebugReport());
}

void ASimHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (simmode_)
        simmode_->stopApiServer();

    if (widget_) {
        widget_->Destruct();
        widget_ = nullptr;
    }
    if (simmode_) {
        simmode_->Destroy();
        simmode_ = nullptr;
    }

    UAirBlueprintLib::OnEndPlay();

    Super::EndPlay(EndPlayReason);
}

void ASimHUD::toggleRecordHandler()
{
    simmode_->toggleRecording();
}

void ASimHUD::inputEventToggleRecording()
{
    toggleRecordHandler();
}

void ASimHUD::inputEventToggleReport()
{
    simmode_->EnableReport = !simmode_->EnableReport;
    widget_->setReportVisible(simmode_->EnableReport);
}

void ASimHUD::inputEventToggleHelp()
{
    widget_->toggleHelpVisibility();
}

void ASimHUD::inputEventToggleTrace()
{
    simmode_->getVehicleSimApi()->toggleTrace();
}

ASimHUD::ImageType ASimHUD::getSubwindowCameraType(int window_index)
{
    //TODO: index check
    return getSubWindowSettings().at(window_index).image_type;
}

void ASimHUD::setSubwindowCameraType(int window_index, ImageType type)
{
    getSubWindowSettings().at(window_index).image_type = type;
    updateWidgetSubwindowVisibility();
}

APIPCamera* ASimHUD::getSubwindowCamera(int window_index)
{
    return subwindow_cameras_[window_index]; //TODO: index check
}

void ASimHUD::setSubwindowCamera(int window_index, APIPCamera* camera)
{
    subwindow_cameras_[window_index] = camera; //TODO: index check
    updateWidgetSubwindowVisibility();
}

bool ASimHUD::getSubwindowVisible(int window_index)
{
    return getSubWindowSettings().at(window_index).visible;
}

void ASimHUD::setSubwindowVisible(int window_index, bool is_visible)
{
    getSubWindowSettings().at(window_index).visible = is_visible;
    updateWidgetSubwindowVisibility();
}

void ASimHUD::updateWidgetSubwindowVisibility()
{
    for (int window_index = 0; window_index < AirSimSettings::kSubwindowCount; ++window_index) {
        APIPCamera* camera = subwindow_cameras_[window_index];
        ImageType camera_type = getSubWindowSettings().at(window_index).image_type;

        bool is_visible = getSubWindowSettings().at(window_index).visible && camera != nullptr;

        if (camera != nullptr)
            camera->setCameraTypeEnabled(camera_type, is_visible);

        widget_->setSubwindowVisibility(window_index,
                                        is_visible,
                                        is_visible ? camera->getRenderTarget(camera_type, false) : nullptr);
    }
}

bool ASimHUD::isWidgetSubwindowVisible(int window_index)
{
    return widget_->getSubwindowVisibility(window_index) != 0;
}

void ASimHUD::inputEventToggleSubwindow0()
{
    getSubWindowSettings().at(0).visible = !getSubWindowSettings().at(0).visible;
    updateWidgetSubwindowVisibility();
}

void ASimHUD::inputEventToggleSubwindow1()
{
    getSubWindowSettings().at(1).visible = !getSubWindowSettings().at(1).visible;
    updateWidgetSubwindowVisibility();
}

void ASimHUD::inputEventToggleSubwindow2()
{
    getSubWindowSettings().at(2).visible = !getSubWindowSettings().at(2).visible;
    updateWidgetSubwindowVisibility();
}

void ASimHUD::inputEventToggleAll()
{
    getSubWindowSettings().at(0).visible = !getSubWindowSettings().at(0).visible;
    getSubWindowSettings().at(1).visible = getSubWindowSettings().at(2).visible = getSubWindowSettings().at(0).visible;
    updateWidgetSubwindowVisibility();
}

void ASimHUD::createMainWidget()
{
    //create main widget
    if (widget_class_ != nullptr) {
        APlayerController* player_controller = this->GetWorld()->GetFirstPlayerController();
        auto* pawn = player_controller->GetPawn();
        if (pawn) {
            std::string pawn_name = std::string(TCHAR_TO_ANSI(*pawn->GetName()));
            Utils::log(pawn_name);
        }
        else {
            UAirBlueprintLib::ShowMessage(EAppMsgType::Ok, std::string("There were no compatible vehicles created for current SimMode! Check your settings.json."), "Error");
            UAirBlueprintLib::LogMessage(TEXT("There were no compatible vehicles created for current SimMode! Check your settings.json."), TEXT(""), LogDebugLevel::Failure);
        }

        widget_ = CreateWidget<USimHUDWidget>(player_controller, widget_class_);
    }
    else {
        widget_ = nullptr;
        UAirBlueprintLib::LogMessage(TEXT("Cannot instantiate BP_SimHUDWidget blueprint!"), TEXT(""), LogDebugLevel::Failure);
    }

    initializeSubWindows();

    widget_->AddToViewport();

    //synchronize PIP views
    widget_->initializeForPlay();
    if (simmode_)
        widget_->setReportVisible(simmode_->EnableReport);
    widget_->setOnToggleRecordingHandler(std::bind(&ASimHUD::toggleRecordHandler, this));
    widget_->setRecordButtonVisibility(AirSimSettings::singleton().is_record_ui_visible);
    updateWidgetSubwindowVisibility();
}

void ASimHUD::setUnrealEngineSettings()
{
    //TODO: should we only do below on SceneCapture2D components and cameras?
    //avoid motion blur so capture images don't get
    GetWorld()->GetGameViewport()->GetEngineShowFlags()->SetMotionBlur(false);

    //use two different methods to set console var because sometime it doesn't seem to work
    static const auto custom_depth_var = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth"));
    custom_depth_var->Set(3);

    //Equivalent to enabling Custom Stencil in Project > Settings > Rendering > Postprocessing
    UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(), FString("r.CustomDepth 3"));

    //during startup we init stencil IDs to random hash and it takes long time for large environments
    //we get error that GameThread has timed out after 30 sec waiting on render thread
    static const auto render_timeout_var = IConsoleManager::Get().FindConsoleVariable(TEXT("g.TimeoutForBlockOnRenderFence"));
    render_timeout_var->Set(300000);
}

void ASimHUD::setupInputBindings()
{
    UAirBlueprintLib::EnableInput(this);

    UAirBlueprintLib::BindActionToKey("inputEventToggleRecording", EKeys::R, this, &ASimHUD::inputEventToggleRecording);
    UAirBlueprintLib::BindActionToKey("InputEventToggleReport", EKeys::Semicolon, this, &ASimHUD::inputEventToggleReport);
    UAirBlueprintLib::BindActionToKey("InputEventToggleHelp", EKeys::F1, this, &ASimHUD::inputEventToggleHelp);
    UAirBlueprintLib::BindActionToKey("InputEventToggleTrace", EKeys::T, this, &ASimHUD::inputEventToggleTrace);

    UAirBlueprintLib::BindActionToKey("InputEventToggleSubwindow0", EKeys::One, this, &ASimHUD::inputEventToggleSubwindow0);
    UAirBlueprintLib::BindActionToKey("InputEventToggleSubwindow1", EKeys::Two, this, &ASimHUD::inputEventToggleSubwindow1);
    UAirBlueprintLib::BindActionToKey("InputEventToggleSubwindow2", EKeys::Three, this, &ASimHUD::inputEventToggleSubwindow2);
    UAirBlueprintLib::BindActionToKey("InputEventToggleAll", EKeys::Zero, this, &ASimHUD::inputEventToggleAll);
}

void ASimHUD::initializeSettings()
{
    std::string settingsText;
    if (getSettingsText(settingsText))
        AirSimSettings::initializeSettings(settingsText);
    else
        AirSimSettings::createDefaultSettingsFile();

    AirSimSettings::singleton().load(std::bind(&ASimHUD::getSimModeFromUser, this));
    for (const auto& warning : AirSimSettings::singleton().warning_messages) {
        UAirBlueprintLib::LogMessageString(warning, "", LogDebugLevel::Failure);
    }
    for (const auto& error : AirSimSettings::singleton().error_messages) {
        UAirBlueprintLib::ShowMessage(EAppMsgType::Ok, error, "settings.json");
    }
}

const std::vector<ASimHUD::AirSimSettings::SubwindowSetting>& ASimHUD::getSubWindowSettings() const
{
    return AirSimSettings::singleton().subwindow_settings;
}

std::vector<ASimHUD::AirSimSettings::SubwindowSetting>& ASimHUD::getSubWindowSettings()
{
    return AirSimSettings::singleton().subwindow_settings;
}

std::string ASimHUD::getSimModeFromUser()
{
    if (EAppReturnType::No == UAirBlueprintLib::ShowMessage(EAppMsgType::YesNo,
                                                            "Would you like to use car simulation? Choose no to use quadrotor simulation.",
                                                            "Choose Vehicle")) {
        return AirSimSettings::kSimModeTypeMultirotor;
    }
    else
        return AirSimSettings::kSimModeTypeCar;
}

void ASimHUD::loadLevel()
{
    if (AirSimSettings::singleton().level_name != "")
        UAirBlueprintLib::RunCommandOnGameThread([&]() { UAirBlueprintLib::loadLevel(this->GetWorld(), FString(AirSimSettings::singleton().level_name.c_str())); }, true);
    else
        UAirBlueprintLib::RunCommandOnGameThread([&]() { UAirBlueprintLib::loadLevel(this->GetWorld(), FString("Blocks")); }, true);
}
void ASimHUD::createSimMode()
{
    std::string simmode_name = AirSimSettings::singleton().simmode_name;

    FActorSpawnParameters simmode_spawn_params;
    simmode_spawn_params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    //spawn at origin. We will use this to do global NED transforms, for ex, non-vehicle objects in environment
    if (simmode_name == AirSimSettings::kSimModeTypeMultirotor)
        simmode_ = this->GetWorld()->SpawnActor<ASimModeWorldMultiRotor>(FVector::ZeroVector,
                                                                         FRotator::ZeroRotator,
                                                                         simmode_spawn_params);
    else if (simmode_name == AirSimSettings::kSimModeTypeCar)
        simmode_ = this->GetWorld()->SpawnActor<ASimModeCar>(FVector::ZeroVector,
                                                             FRotator::ZeroRotator,
                                                             simmode_spawn_params);
    else if (simmode_name == AirSimSettings::kSimModeTypeComputerVision)
        simmode_ = this->GetWorld()->SpawnActor<ASimModeComputerVision>(FVector::ZeroVector,
                                                                        FRotator::ZeroRotator,
                                                                        simmode_spawn_params);
    else {
        UAirBlueprintLib::ShowMessage(EAppMsgType::Ok, std::string("SimMode is not valid: ") + simmode_name, "Error");
        UAirBlueprintLib::LogMessageString("SimMode is not valid: ", simmode_name, LogDebugLevel::Failure);
    }
}

void ASimHUD::initializeSubWindows()
{
    if (!simmode_)
        return;

    auto default_vehicle_sim_api = simmode_->getVehicleSimApi();

    if (default_vehicle_sim_api) {
        auto camera_count = default_vehicle_sim_api->getCameraCount();

        //setup defaults
        if (camera_count > 0) {
            subwindow_cameras_[0] = default_vehicle_sim_api->getCamera("");
            subwindow_cameras_[1] = default_vehicle_sim_api->getCamera(""); //camera_count > 3 ? 3 : 0
            subwindow_cameras_[2] = default_vehicle_sim_api->getCamera(""); //camera_count > 4 ? 4 : 0
        }
        else
            subwindow_cameras_[0] = subwindow_cameras_[1] = subwindow_cameras_[2] = nullptr;
    }

    for (const auto& setting : getSubWindowSettings()) {
        APIPCamera* camera = simmode_->getCamera(msr::airlib::CameraDetails(setting.camera_name, setting.vehicle_name, setting.external));
        if (camera)
            subwindow_cameras_[setting.window_index] = camera;
        else
            UAirBlueprintLib::LogMessageString("Invalid Camera settings in <SubWindows> element",
                                               std::to_string(setting.window_index),
                                               LogDebugLevel::Failure);
    }
}

FString ASimHUD::getLaunchPath(const std::string& filename)
{
    FString launch_rel_path = FPaths::LaunchDir();
    FString abs_path = FPaths::ConvertRelativePathToFull(launch_rel_path);
    return FPaths::Combine(abs_path, FString(filename.c_str()));
}

// Attempts to parse the settings text from one of multiple locations.
// First, check the command line for settings provided via "-s" or "--settings" arguments
// Next, check the executable's working directory for the settings file.
// Finally, check the user's documents folder.
// If the settings file cannot be read, throw an exception

bool ASimHUD::getSettingsText(std::string& settingsText)
{
    return (getSettingsTextFromCommandLine(settingsText) ||
            readSettingsTextFromFile(FString(msr::airlib::Settings::getExecutableFullPath("settings.json").c_str()), settingsText) ||
            readSettingsTextFromFile(getLaunchPath("settings.json"), settingsText) ||
            readSettingsTextFromFile(FString(msr::airlib::Settings::Settings::getUserDirectoryFullPath("settings.json").c_str()), settingsText));
}

// Attempts to parse the settings file path or the settings text from the command line
// Looks for the flag "--settings". If it exists, settingsText will be set to the value.
// Example (Path): AirSim.exe --settings "C:\path\to\settings.json"
// Example (Text): AirSim.exe -s '{"foo" : "bar"}' -> settingsText will be set to {"foo": "bar"}
// Returns true if the argument is present, false otherwise.
bool ASimHUD::getSettingsTextFromCommandLine(std::string& settingsText)
{

    bool found = false;
    FString settingsTextFString;
    const TCHAR* commandLineArgs = FCommandLine::Get();

    if (FParse::Param(commandLineArgs, TEXT("-settings"))) {
        FString commandLineArgsFString = FString(commandLineArgs);
        int idx = commandLineArgsFString.Find(TEXT("-settings"));
        FString settingsJsonFString = commandLineArgsFString.RightChop(idx + 10);

        if (readSettingsTextFromFile(settingsJsonFString.TrimQuotes(), settingsText)) {
            return true;
        }

        if (FParse::QuotedString(*settingsJsonFString, settingsTextFString)) {
            settingsText = std::string(TCHAR_TO_UTF8(*settingsTextFString));
            found = true;
        }
    }

    return found;
}

bool ASimHUD::readSettingsTextFromFile(const FString& settingsFilepath, std::string& settingsText)
{
    bool found = FPaths::FileExists(settingsFilepath);
    if (found) {
        FString settingsTextFStr;
        bool readSuccessful = FFileHelper::LoadFileToString(settingsTextFStr, *settingsFilepath);
        if (readSuccessful) {
            UAirBlueprintLib::LogMessageString("Loaded settings from ", TCHAR_TO_UTF8(*settingsFilepath), LogDebugLevel::Informational);
            settingsText = TCHAR_TO_UTF8(*settingsTextFStr);
        }
        else {
            UAirBlueprintLib::LogMessageString("Cannot read file ", TCHAR_TO_UTF8(*settingsFilepath), LogDebugLevel::Failure);
            throw std::runtime_error("Cannot read settings file.");
        }
    }

    return found;
}
