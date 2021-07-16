#include "common/common_utils/StrictMode.hpp"
STRICT_MODE_OFF //todo what does this do?
#ifndef RPCLIB_MSGPACK
#define RPCLIB_MSGPACK clmdep_msgpack
#endif // !RPCLIB_MSGPACK
#include "rpc/rpc_error.h"
    STRICT_MODE_ON

#include "common/AirSimSettings.hpp"
#include "common/common_utils/FileSystem.hpp"
#include "sensors/lidar/LidarSimpleParams.hpp"
#include "ros/ros.h"
#include "sensors/imu/ImuBase.hpp"
#include "vehicles/multirotor/api/MultirotorRpcLibClient.hpp"
#include "vehicles/car/api/CarRpcLibClient.hpp"
#include "yaml-cpp/yaml.h"
#include <airsim_ros_pkgs/GimbalAngleEulerCmd.h>
#include <airsim_ros_pkgs/GimbalAngleQuatCmd.h>
#include <airsim_ros_pkgs/GPSYaw.h>
#include <airsim_ros_pkgs/Land.h>
#include <airsim_ros_pkgs/LandGroup.h>
#include <airsim_ros_pkgs/Reset.h>
#include <airsim_ros_pkgs/Takeoff.h>
#include <airsim_ros_pkgs/TakeoffGroup.h>
#include <airsim_ros_pkgs/VelCmd.h>
#include <airsim_ros_pkgs/VelCmdGroup.h>
#include <airsim_ros_pkgs/CarControls.h>
#include <airsim_ros_pkgs/CarState.h>
#include <airsim_ros_pkgs/Environment.h>
#include <chrono>
#include <cv_bridge/cv_bridge.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/TransformStamped.h>
#include <geometry_msgs/Twist.h>
#include <image_transport/image_transport.h>
#include <iostream>
#include <math.h>
#include <math_common.h>
#include <mavros_msgs/State.h>
#include <nav_msgs/Odometry.h>
#include <opencv2/opencv.hpp>
#include <ros/callback_queue.h>
#include <ros/console.h>
#include <sensor_msgs/CameraInfo.h>
#include <sensor_msgs/distortion_models.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/image_encodings.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/NavSatFix.h>
#include <airsim_ros_pkgs/Altimeter.h> //hector_uav_msgs defunct?
#include <sensor_msgs/MagneticField.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/Range.h>
#include <rosgraph_msgs/Clock.h>
#include <std_srvs/Empty.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#include <tf2_ros/static_transform_broadcaster.h>
#include <tf2_ros/transform_broadcaster.h>
#include <tf2_ros/transform_listener.h>
#include <tf2/convert.h>
#include <unordered_map>
#include <memory>
    // #include "nodelet/nodelet.h"

    // todo move airlib typedefs to separate header file?
    typedef msr::airlib::ImageCaptureBase::ImageRequest ImageRequest;
typedef msr::airlib::ImageCaptureBase::ImageResponse ImageResponse;
typedef msr::airlib::ImageCaptureBase::ImageType ImageType;
typedef msr::airlib::AirSimSettings::CaptureSetting CaptureSetting;
typedef msr::airlib::AirSimSettings::VehicleSetting VehicleSetting;
typedef msr::airlib::AirSimSettings::CameraSetting CameraSetting;
typedef msr::airlib::AirSimSettings::LidarSetting LidarSetting;

struct SimpleMatrix
{
    int rows;
    int cols;
    double* data;

    SimpleMatrix(int rows, int cols, double* data)
        : rows(rows), cols(cols), data(data)
    {
    }
};

struct VelCmd
{
    double x;
    double y;
    double z;
    msr::airlib::DrivetrainType drivetrain;
    msr::airlib::YawMode yaw_mode;
    std::string vehicle_name;

    // VelCmd() :
    //     x(0), y(0), z(0),
    //     vehicle_name("") {drivetrain = msr::airlib::DrivetrainType::MaxDegreeOfFreedom;
    //             yaw_mode = msr::airlib::YawMode();};

    // VelCmd(const double& x, const double& y, const double& z,
    //         msr::airlib::DrivetrainType drivetrain,
    //         const msr::airlib::YawMode& yaw_mode,
    //         const std::string& vehicle_name) :
    //     x(x), y(y), z(z),
    //     drivetrain(drivetrain),
    //     yaw_mode(yaw_mode),
    //     vehicle_name(vehicle_name) {};
};

struct GimbalCmd
{
    std::string vehicle_name;
    std::string camera_name;
    msr::airlib::Quaternionr target_quat;

    // GimbalCmd() : vehicle_name(vehicle_name), camera_name(camera_name), target_quat(msr::airlib::Quaternionr(1,0,0,0)) {}

    // GimbalCmd(const std::string& vehicle_name,
    //         const std::string& camera_name,
    //         const msr::airlib::Quaternionr& target_quat) :
    //         vehicle_name(vehicle_name), camera_name(camera_name), target_quat(target_quat) {};
};

class AirsimROSWrapper
{
public:
    enum class AIRSIM_MODE : unsigned
    {
        DRONE,
        CAR
    };

    AirsimROSWrapper(const ros::NodeHandle& nh, const ros::NodeHandle& nh_private, const std::string& host_ip);
    ~AirsimROSWrapper(){};

    void initialize_airsim();
    void initialize_ros();

    // std::vector<ros::CallbackQueue> callback_queues_;
    ros::AsyncSpinner img_async_spinner_;
    ros::AsyncSpinner lidar_async_spinner_;
    bool is_used_lidar_timer_cb_queue_;
    bool is_used_img_timer_cb_queue_;

private:
    struct SensorPublisher
    {
        SensorBase::SensorType sensor_type;
        std::string sensor_name;
        ros::Publisher publisher;
    };

    // utility struct for a SINGLE robot
    class VehicleROS
    {
    public:
        virtual ~VehicleROS() {}
        std::string vehicle_name;

        /// All things ROS
        ros::Publisher odom_local_pub;
        ros::Publisher global_gps_pub;
        ros::Publisher env_pub;
        airsim_ros_pkgs::Environment env_msg;
        std::vector<SensorPublisher> sensor_pubs;
        // handle lidar seperately for max performance as data is collected on its own thread/callback
        std::vector<SensorPublisher> lidar_pubs;

        nav_msgs::Odometry curr_odom;
        sensor_msgs::NavSatFix gps_sensor_msg;

        std::vector<geometry_msgs::TransformStamped> static_tf_msg_vec;

        ros::Time stamp;

        std::string odom_frame_id;
        /// Status
        // bool is_armed_;
        // std::string mode_;
    };

    class CarROS : public VehicleROS
    {
    public:
        msr::airlib::CarApiBase::CarState curr_car_state;

        ros::Subscriber car_cmd_sub;
        ros::Publisher car_state_pub;
        airsim_ros_pkgs::CarState car_state_msg;

        bool has_car_cmd;
        msr::airlib::CarApiBase::CarControls car_cmd;
    };

    class MultiRotorROS : public VehicleROS
    {
    public:
        /// State
        msr::airlib::MultirotorState curr_drone_state;
        // bool in_air_; // todo change to "status" and keep track of this

        ros::Subscriber vel_cmd_body_frame_sub;
        ros::Subscriber vel_cmd_world_frame_sub;

        ros::ServiceServer takeoff_srvr;
        ros::ServiceServer land_srvr;

        bool has_vel_cmd;
        VelCmd vel_cmd;

        /// Status
        // bool in_air_; // todo change to "status" and keep track of this
    };

    /// ROS timer callbacks
    void img_response_timer_cb(const ros::TimerEvent& event); // update images from airsim_client_ every nth sec
    void drone_state_timer_cb(const ros::TimerEvent& event); // update drone state from airsim_client_ every nth sec
    void lidar_timer_cb(const ros::TimerEvent& event);

    /// ROS subscriber callbacks
    void vel_cmd_world_frame_cb(const airsim_ros_pkgs::VelCmd::ConstPtr& msg, const std::string& vehicle_name);
    void vel_cmd_body_frame_cb(const airsim_ros_pkgs::VelCmd::ConstPtr& msg, const std::string& vehicle_name);

    void vel_cmd_group_body_frame_cb(const airsim_ros_pkgs::VelCmdGroup& msg);
    void vel_cmd_group_world_frame_cb(const airsim_ros_pkgs::VelCmdGroup& msg);

    void vel_cmd_all_world_frame_cb(const airsim_ros_pkgs::VelCmd& msg);
    void vel_cmd_all_body_frame_cb(const airsim_ros_pkgs::VelCmd& msg);

    // void vel_cmd_body_frame_cb(const airsim_ros_pkgs::VelCmd& msg, const std::string& vehicle_name);
    void gimbal_angle_quat_cmd_cb(const airsim_ros_pkgs::GimbalAngleQuatCmd& gimbal_angle_quat_cmd_msg);
    void gimbal_angle_euler_cmd_cb(const airsim_ros_pkgs::GimbalAngleEulerCmd& gimbal_angle_euler_cmd_msg);

    // commands
    void car_cmd_cb(const airsim_ros_pkgs::CarControls::ConstPtr& msg, const std::string& vehicle_name);
    void update_commands();

    // state, returns the simulation timestamp best guess based on drone state timestamp, airsim needs to return timestap for environment
    ros::Time update_state();
    void update_and_publish_static_transforms(VehicleROS* vehicle_ros);
    void publish_vehicle_state();

    /// ROS service callbacks
    bool takeoff_srv_cb(airsim_ros_pkgs::Takeoff::Request& request, airsim_ros_pkgs::Takeoff::Response& response, const std::string& vehicle_name);
    bool takeoff_group_srv_cb(airsim_ros_pkgs::TakeoffGroup::Request& request, airsim_ros_pkgs::TakeoffGroup::Response& response);
    bool takeoff_all_srv_cb(airsim_ros_pkgs::Takeoff::Request& request, airsim_ros_pkgs::Takeoff::Response& response);
    bool land_srv_cb(airsim_ros_pkgs::Land::Request& request, airsim_ros_pkgs::Land::Response& response, const std::string& vehicle_name);
    bool land_group_srv_cb(airsim_ros_pkgs::LandGroup::Request& request, airsim_ros_pkgs::LandGroup::Response& response);
    bool land_all_srv_cb(airsim_ros_pkgs::Land::Request& request, airsim_ros_pkgs::Land::Response& response);
    bool reset_srv_cb(airsim_ros_pkgs::Reset::Request& request, airsim_ros_pkgs::Reset::Response& response);

    /// ROS tf broadcasters
    void publish_camera_tf(const ImageResponse& img_response, const ros::Time& ros_time, const std::string& frame_id, const std::string& child_frame_id);
    void publish_odom_tf(const nav_msgs::Odometry& odom_msg);

    /// camera helper methods
    sensor_msgs::CameraInfo generate_cam_info(const std::string& camera_name, const CameraSetting& camera_setting, const CaptureSetting& capture_setting) const;
    cv::Mat manual_decode_depth(const ImageResponse& img_response) const;

    sensor_msgs::ImagePtr get_img_msg_from_response(const ImageResponse& img_response, const ros::Time curr_ros_time, const std::string frame_id);
    sensor_msgs::ImagePtr get_depth_img_msg_from_response(const ImageResponse& img_response, const ros::Time curr_ros_time, const std::string frame_id);

    void process_and_publish_img_response(const std::vector<ImageResponse>& img_response_vec, const int img_response_idx, const std::string& vehicle_name);

    // methods which parse setting json ang generate ros pubsubsrv
    void create_ros_pubs_from_settings_json();
    void append_static_camera_tf(VehicleROS* vehicle_ros, const std::string& camera_name, const CameraSetting& camera_setting);
    void append_static_lidar_tf(VehicleROS* vehicle_ros, const std::string& lidar_name, const msr::airlib::LidarSimpleParams& lidar_setting);
    void append_static_vehicle_tf(VehicleROS* vehicle_ros, const VehicleSetting& vehicle_setting);
    void set_nans_to_zeros_in_pose(VehicleSetting& vehicle_setting) const;
    void set_nans_to_zeros_in_pose(const VehicleSetting& vehicle_setting, CameraSetting& camera_setting) const;
    void set_nans_to_zeros_in_pose(const VehicleSetting& vehicle_setting, LidarSetting& lidar_setting) const;

    /// utils. todo parse into an Airlib<->ROS conversion class
    tf2::Quaternion get_tf2_quat(const msr::airlib::Quaternionr& airlib_quat) const;
    msr::airlib::Quaternionr get_airlib_quat(const geometry_msgs::Quaternion& geometry_msgs_quat) const;
    msr::airlib::Quaternionr get_airlib_quat(const tf2::Quaternion& tf2_quat) const;
    nav_msgs::Odometry get_odom_msg_from_multirotor_state(const msr::airlib::MultirotorState& drone_state) const;
    nav_msgs::Odometry get_odom_msg_from_car_state(const msr::airlib::CarApiBase::CarState& car_state) const;
    airsim_ros_pkgs::CarState get_roscarstate_msg_from_car_state(const msr::airlib::CarApiBase::CarState& car_state) const;
    msr::airlib::Pose get_airlib_pose(const float& x, const float& y, const float& z, const msr::airlib::Quaternionr& airlib_quat) const;
    airsim_ros_pkgs::GPSYaw get_gps_msg_from_airsim_geo_point(const msr::airlib::GeoPoint& geo_point) const;
    sensor_msgs::NavSatFix get_gps_sensor_msg_from_airsim_geo_point(const msr::airlib::GeoPoint& geo_point) const;
    sensor_msgs::Imu get_imu_msg_from_airsim(const msr::airlib::ImuBase::Output& imu_data) const;
    airsim_ros_pkgs::Altimeter get_altimeter_msg_from_airsim(const msr::airlib::BarometerBase::Output& alt_data) const;
    sensor_msgs::Range get_range_from_airsim(const msr::airlib::DistanceSensorData& dist_data) const;
    sensor_msgs::PointCloud2 get_lidar_msg_from_airsim(const msr::airlib::LidarData& lidar_data, const std::string& vehicle_name) const;
    sensor_msgs::NavSatFix get_gps_msg_from_airsim(const msr::airlib::GpsBase::Output& gps_data) const;
    sensor_msgs::MagneticField get_mag_msg_from_airsim(const msr::airlib::MagnetometerBase::Output& mag_data) const;
    airsim_ros_pkgs::Environment get_environment_msg_from_airsim(const msr::airlib::Environment::State& env_data) const;

    // not used anymore, but can be useful in future with an unreal camera calibration environment
    void read_params_from_yaml_and_fill_cam_info_msg(const std::string& file_name, sensor_msgs::CameraInfo& cam_info) const;
    void convert_yaml_to_simple_mat(const YAML::Node& node, SimpleMatrix& m) const; // todo ugly

    // simulation time utility
    ros::Time airsim_timestamp_to_ros(const msr::airlib::TTimePoint& stamp) const;
    ros::Time chrono_timestamp_to_ros(const std::chrono::system_clock::time_point& stamp) const;

private:
    // subscriber / services for ALL robots
    ros::Subscriber vel_cmd_all_body_frame_sub_;
    ros::Subscriber vel_cmd_all_world_frame_sub_;
    ros::ServiceServer takeoff_all_srvr_;
    ros::ServiceServer land_all_srvr_;

    // todo - subscriber / services for a GROUP of robots, which is defined by a list of `vehicle_name`s passed in the ros msg / srv request
    ros::Subscriber vel_cmd_group_body_frame_sub_;
    ros::Subscriber vel_cmd_group_world_frame_sub_;
    ros::ServiceServer takeoff_group_srvr_;
    ros::ServiceServer land_group_srvr_;

    AIRSIM_MODE airsim_mode_ = AIRSIM_MODE::DRONE;

    ros::ServiceServer reset_srvr_;
    ros::Publisher origin_geo_point_pub_; // home geo coord of drones
    msr::airlib::GeoPoint origin_geo_point_; // gps coord of unreal origin
    airsim_ros_pkgs::GPSYaw origin_geo_point_msg_; // todo duplicate

    std::unordered_map<std::string, std::unique_ptr<VehicleROS>> vehicle_name_ptr_map_;
    static const std::unordered_map<int, std::string> image_type_int_to_string_map_;

    bool is_vulkan_; // rosparam obtained from launch file. If vulkan is being used, we BGR encoding instead of RGB

    std::string host_ip_;
    std::unique_ptr<msr::airlib::RpcLibClientBase> airsim_client_ = nullptr;
    // seperate busy connections to airsim, update in their own thread
    msr::airlib::RpcLibClientBase airsim_client_images_;
    msr::airlib::RpcLibClientBase airsim_client_lidar_;

    ros::NodeHandle nh_;
    ros::NodeHandle nh_private_;

    // todo not sure if async spinners shuold be inside this class, or should be instantiated in airsim_node.cpp, and cb queues should be public
    // todo for multiple drones with multiple sensors, this won't scale. make it a part of VehicleROS?
    ros::CallbackQueue img_timer_cb_queue_;
    ros::CallbackQueue lidar_timer_cb_queue_;

    std::mutex drone_control_mutex_;

    // gimbal control
    bool has_gimbal_cmd_;
    GimbalCmd gimbal_cmd_;

    /// ROS tf
    const std::string AIRSIM_FRAME_ID = "world_ned";
    std::string world_frame_id_ = AIRSIM_FRAME_ID;
    const std::string AIRSIM_ODOM_FRAME_ID = "odom_local_ned";
    const std::string ENU_ODOM_FRAME_ID = "odom_local_enu";
    std::string odom_frame_id_ = AIRSIM_ODOM_FRAME_ID;
    tf2_ros::TransformBroadcaster tf_broadcaster_;
    tf2_ros::StaticTransformBroadcaster static_tf_pub_;

    bool isENU_ = false;
    tf2_ros::Buffer tf_buffer_;
    tf2_ros::TransformListener tf_listener_;

    /// ROS params
    double vel_cmd_duration_;

    /// ROS Timers.
    ros::Timer airsim_img_response_timer_;
    ros::Timer airsim_control_update_timer_;
    ros::Timer airsim_lidar_update_timer_;

    typedef std::pair<std::vector<ImageRequest>, std::string> airsim_img_request_vehicle_name_pair;
    std::vector<airsim_img_request_vehicle_name_pair> airsim_img_request_vehicle_name_pair_vec_;
    std::vector<image_transport::Publisher> image_pub_vec_;
    std::vector<ros::Publisher> cam_info_pub_vec_;

    std::vector<sensor_msgs::CameraInfo> camera_info_msg_vec_;

    /// ROS other publishers
    ros::Publisher clock_pub_;
    rosgraph_msgs::Clock ros_clock_;
    bool publish_clock_ = false;

    ros::Subscriber gimbal_angle_quat_cmd_sub_;
    ros::Subscriber gimbal_angle_euler_cmd_sub_;

    static constexpr char CAM_YML_NAME[] = "camera_name";
    static constexpr char WIDTH_YML_NAME[] = "image_width";
    static constexpr char HEIGHT_YML_NAME[] = "image_height";
    static constexpr char K_YML_NAME[] = "camera_matrix";
    static constexpr char D_YML_NAME[] = "distortion_coefficients";
    static constexpr char R_YML_NAME[] = "rectification_matrix";
    static constexpr char P_YML_NAME[] = "projection_matrix";
    static constexpr char DMODEL_YML_NAME[] = "distortion_model";
};
