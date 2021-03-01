
#include "WorldSimApi.h"
#include "PInvokeWrapper.h"
#include "UnityUtilities.hpp"

WorldSimApi::WorldSimApi(SimModeBase* simmode)
	: simmode_(simmode)
{}

WorldSimApi::~WorldSimApi()
{}

bool WorldSimApi::isPaused() const
{
	return simmode_->isPaused();
}

void WorldSimApi::reset()
{
	simmode_->reset();
}

void WorldSimApi::pause(bool is_paused)
{
	simmode_->pause(is_paused);
}

void WorldSimApi::continueForTime(double seconds)
{
	simmode_->continueForTime(seconds);
}

void WorldSimApi::continueForFrames(uint32_t frames)
{
	simmode_->continueForFrames(frames);
}

void WorldSimApi::setTimeOfDay(bool is_enabled, const std::string& start_datetime, bool is_start_datetime_dst,
    float celestial_clock_speed, float update_interval_secs, bool move_sun)
{
    simmode_->setTimeOfDay(is_enabled, start_datetime, is_start_datetime_dst,
        celestial_clock_speed, update_interval_secs, move_sun);
}

bool WorldSimApi::setSegmentationObjectID(const std::string& mesh_name, int object_id, bool is_name_regex)
{
	return SetSegmentationObjectId(mesh_name.c_str(), object_id, is_name_regex);
}

int WorldSimApi::getSegmentationObjectID(const std::string& mesh_name) const
{
	return GetSegmentationObjectId(mesh_name.c_str());
}

void WorldSimApi::printLogMessage(const std::string& message, const std::string& message_param, unsigned char severity)
{
	PrintLogMessage(message.c_str(), message_param.c_str(), "", severity);
}

std::unique_ptr<std::vector<std::string>> WorldSimApi::swapTextures(const std::string& tag, int tex_id, int component_id, int material_id)
{
    std::unique_ptr<std::vector<std::string>> result;
    throw std::invalid_argument(common_utils::Utils::stringf(
        "swapTextures is not supported on unity").c_str());
    return result;
}

std::vector<std::string> WorldSimApi::listSceneObjects(const std::string& name_regex) const
{
	std::vector<std::string> result;
	throw std::invalid_argument(common_utils::Utils::stringf(
		"simListSceneObject is not supported on unity").c_str());
	return result;
}

WorldSimApi::Pose WorldSimApi::getObjectPose(const std::string& object_name) const
{
	AirSimUnity::AirSimPose airSimPose = GetPose(object_name.c_str());
	return UnityUtilities::Convert_to_Pose(airSimPose);
}

msr::airlib::Vector3r WorldSimApi::getObjectScale(const std::string& object_name) const { return Vector3r(); }
msr::airlib::Vector3r WorldSimApi::getObjectScaleInternal(const std::string& object_name) const { return Vector3r(); }
bool WorldSimApi::setObjectScale(const std::string& object_name, const Vector3r& scale) { return false; }

bool WorldSimApi::setObjectPose(const std::string& object_name, const WorldSimApi::Pose& pose, bool teleport)
{
	AirSimUnity::AirSimPose airSimPose = UnityUtilities::Convert_to_AirSimPose(pose);
	return SetPose(airSimPose, false, object_name.c_str());
}

bool WorldSimApi::runConsoleCommand(const std::string& command)
{
    throw std::invalid_argument(common_utils::Utils::stringf(
        "simrunConsoleCommand is not supported on unity").c_str());
    return false;
}

void WorldSimApi::enableWeather(bool enable)
{
    unused(enable);
    //TODO: implement weather for Unity
}
void WorldSimApi::setWeatherParameter(WeatherParameter param, float val)
{
    unused(param);
    unused(val);
    //TODO: implement weather for Unity
}

bool WorldSimApi::createVoxelGrid(const Vector3r& position, const int& x_size, const int& y_size, const int& z_size, const float& res, const std::string& output_file)
{
    return false;
}

//----------------Plotting APIs-----------/
void WorldSimApi::simFlushPersistentMarkers()
{
    throw std::invalid_argument(common_utils::Utils::stringf(
        "simFlushPersistentMarkers is not supported on unity").c_str());
}

void WorldSimApi::simPlotPoints(const std::vector<Vector3r>& points, const std::vector<float>& color_rgba, float size, float duration, bool is_persistent)
{
    throw std::invalid_argument(common_utils::Utils::stringf(
        "simPlotPoints is not supported on unity").c_str());
}

void WorldSimApi::simPlotLineStrip(const std::vector<Vector3r>& points, const std::vector<float>& color_rgba, float thickness, float duration, bool is_persistent)
{
    throw std::invalid_argument(common_utils::Utils::stringf(
        "simPlotLineStrip is not supported on unity").c_str());
}

void WorldSimApi::simPlotLineList(const std::vector<Vector3r>& points, const std::vector<float>& color_rgba, float thickness, float duration, bool is_persistent)
{
    throw std::invalid_argument(common_utils::Utils::stringf(
        "simPlotLineList is not supported on unity").c_str());
}

void WorldSimApi::simPlotArrows(const std::vector<Vector3r>& points_start, const std::vector<Vector3r>& points_end, const std::vector<float>& color_rgba, float thickness, float arrow_size, float duration, bool is_persistent)
{
    throw std::invalid_argument(common_utils::Utils::stringf(
        "simPlotArrows is not supported on unity").c_str());
}

void WorldSimApi::simPlotStrings(const std::vector<std::string>& strings, const std::vector<Vector3r>& positions, float scale, const std::vector<float>& color_rgba, float duration)
{
    throw std::invalid_argument(common_utils::Utils::stringf(
        "simPlotStrings is not supported on unity").c_str());
}

void WorldSimApi::simPlotTransforms(const std::vector<Pose>& poses, float scale, float thickness, float duration, bool is_persistent)
{
    throw std::invalid_argument(common_utils::Utils::stringf(
        "simPlotTransforms is not supported on unity").c_str());
}

void WorldSimApi::simPlotTransformsWithNames(const std::vector<Pose>& poses, const std::vector<std::string>& names, float tf_scale, float tf_thickness, float text_scale, const std::vector<float>& text_color_rgba, float duration)
{
    throw std::invalid_argument(common_utils::Utils::stringf(
        "simPlotTransformsWithNames is not supported on unity").c_str());
}

std::vector<WorldSimApi::MeshPositionVertexBuffersResponse> WorldSimApi::getMeshPositionVertexBuffers() const
{
    std::vector<MeshPositionVertexBuffersResponse> result;
    throw std::invalid_argument(common_utils::Utils::stringf(
        "getMeshPositionVertexBuffers is not supported on unity").c_str());
    return result;
}

// Recording APIs
void WorldSimApi::startRecording()
{
    throw std::invalid_argument(common_utils::Utils::stringf(
        "startRecording is not supported on unity").c_str());
}

void WorldSimApi::stopRecording()
{
    throw std::invalid_argument(common_utils::Utils::stringf(
        "stopRecording is not supported on unity").c_str());
}

bool WorldSimApi::isRecording() const
{
    throw std::invalid_argument(common_utils::Utils::stringf(
        "isRecording is not supported on unity").c_str());
    return false;
}

void WorldSimApi::setWind(const Vector3r& wind) const
{
    simmode_->setWind(wind);
};

#pragma endregion
