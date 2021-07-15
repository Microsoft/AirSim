#pragma once

#include "CoreMinimal.h"
#include "common/CommonStructs.hpp"
#include "api/WorldSimApiBase.hpp"
#include "SimMode/SimModeBase.h"
#include "Components/StaticMeshComponent.h"
#include "Runtime/Engine/Classes/Engine/StaticMesh.h"
#include "Engine/LevelStreamingDynamic.h"
#include <string>

class WorldSimApi : public msr::airlib::WorldSimApiBase
{
public:
    typedef msr::airlib::Pose Pose;
    typedef msr::airlib::Vector3r Vector3r;
    typedef msr::airlib::MeshPositionVertexBuffersResponse MeshPositionVertexBuffersResponse;
    typedef msr::airlib::ImageCaptureBase ImageCaptureBase;
    typedef msr::airlib::CameraDetails CameraDetails;

    WorldSimApi(ASimModeBase* simmode);
    virtual ~WorldSimApi() = default;

    virtual bool loadLevel(const std::string& level_name) override;

    virtual std::string spawnObject(std::string& object_name, const std::string& load_name, const WorldSimApi::Pose& pose, const WorldSimApi::Vector3r& scale, bool physics_enabled) override;
    virtual bool destroyObject(const std::string& object_name) override;

    virtual bool isPaused() const override;
    virtual void reset() override;
    virtual void pause(bool is_paused) override;
    virtual void continueForTime(double seconds) override;
    virtual void continueForFrames(uint32_t frames) override;

    virtual void setTimeOfDay(bool is_enabled, const std::string& start_datetime, bool is_start_datetime_dst,
                              float celestial_clock_speed, float update_interval_secs, bool move_sun);

    virtual void enableWeather(bool enable);
    virtual void setWeatherParameter(WeatherParameter param, float val);

    virtual bool setSegmentationObjectID(const std::string& mesh_name, int object_id, bool is_name_regex = false) override;
    virtual int getSegmentationObjectID(const std::string& mesh_name) const override;

    virtual bool addVehicle(const std::string& vehicle_name, const std::string& vehicle_type, const Pose& pose, const std::string& pawn_path = "") override;

    virtual void printLogMessage(const std::string& message,
                                 const std::string& message_param = "", unsigned char severity = 0) override;

    virtual std::unique_ptr<std::vector<std::string>> swapTextures(const std::string& tag, int tex_id = 0, int component_id = 0, int material_id = 0) override;
    virtual std::vector<std::string> listSceneObjects(const std::string& name_regex) const override;
    virtual Pose getObjectPose(const std::string& object_name) const override;
    virtual bool setObjectPose(const std::string& object_name, const Pose& pose, bool teleport) override;
    virtual bool runConsoleCommand(const std::string& command) override;
    virtual Vector3r getObjectScale(const std::string& object_name) const override;
    virtual bool setObjectScale(const std::string& object_name, const Vector3r& scale) override;

    //----------- Plotting APIs ----------/
    virtual void simFlushPersistentMarkers() override;
    virtual void simPlotPoints(const std::vector<Vector3r>& points, const std::vector<float>& color_rgba, float size, float duration, bool is_persistent) override;
    virtual void simPlotLineStrip(const std::vector<Vector3r>& points, const std::vector<float>& color_rgba, float thickness, float duration, bool is_persistent) override;
    virtual void simPlotLineList(const std::vector<Vector3r>& points, const std::vector<float>& color_rgba, float thickness, float duration, bool is_persistent) override;
    virtual void simPlotArrows(const std::vector<Vector3r>& points_start, const std::vector<Vector3r>& points_end, const std::vector<float>& color_rgba, float thickness, float arrow_size, float duration, bool is_persistent) override;
    virtual void simPlotStrings(const std::vector<std::string>& strings, const std::vector<Vector3r>& positions, float scale, const std::vector<float>& color_rgba, float duration) override;
    virtual void simPlotTransforms(const std::vector<Pose>& poses, float scale, float thickness, float duration, bool is_persistent) override;
    virtual void simPlotTransformsWithNames(const std::vector<Pose>& poses, const std::vector<std::string>& names, float tf_scale, float tf_thickness, float text_scale, const std::vector<float>& text_color_rgba, float duration) override;
    virtual std::vector<MeshPositionVertexBuffersResponse> getMeshPositionVertexBuffers() const override;

    // Recording APIs
    virtual void startRecording() override;
    virtual void stopRecording() override;
    virtual bool isRecording() const override;

    virtual void setWind(const Vector3r& wind) const override;
    virtual bool createVoxelGrid(const Vector3r& position, const int& x_size, const int& y_size, const int& z_size, const float& res, const std::string& output_file) override;
    virtual std::vector<std::string> listVehicles() const override;

    virtual std::string getSettingsString() const override;

    // Camera APIs
    virtual msr::airlib::CameraInfo getCameraInfo(const CameraDetails& camera_details) const override;
    virtual void setCameraPose(const msr::airlib::Pose& pose, const CameraDetails& camera_details) override;
    virtual void setCameraFoV(float fov_degrees, const CameraDetails& camera_details) override;
    virtual void setDistortionParam(const std::string& param_name, float value, const CameraDetails& camera_details) override;
    virtual std::vector<float> getDistortionParams(const CameraDetails& camera_details) const override;

    virtual std::vector<ImageCaptureBase::ImageResponse> getImages(const std::vector<ImageCaptureBase::ImageRequest>& requests,
                                                                   const std::string& vehicle_name, bool external) const override;
    virtual std::vector<uint8_t> getImage(ImageCaptureBase::ImageType image_type, const CameraDetails& camera_details) const override;

    virtual void addDetectionFilterMeshName(ImageCaptureBase::ImageType image_type, const std::string& mesh_name, const CameraDetails& camera_details) override;
    virtual void setDetectionFilterRadius(ImageCaptureBase::ImageType image_type, float radius_cm, const CameraDetails& camera_details) override;
    virtual void clearDetectionMeshNames(ImageCaptureBase::ImageType image_type, const CameraDetails& camera_details) override;
    virtual std::vector<msr::airlib::DetectionInfo> getDetections(ImageCaptureBase::ImageType image_type, const CameraDetails& camera_details) override;

private:
    AActor* createNewActor(const FActorSpawnParameters& spawn_params, const FTransform& actor_transform, const Vector3r& scale, UStaticMesh* static_mesh);
    void spawnPlayer();

private:
    ASimModeBase* simmode_;
    ULevelStreamingDynamic* current_level_;
    std::vector<bool> voxel_grid_;
};
