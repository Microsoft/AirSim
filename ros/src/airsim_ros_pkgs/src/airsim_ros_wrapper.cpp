#include <airsim_ros_wrapper.h>
#include <boost/make_shared.hpp>
// #include <pluginlib/class_list_macros.h>
// PLUGINLIB_EXPORT_CLASS(AirsimROSWrapper, nodelet::Nodelet)
#include "common/AirSimSettings.hpp"
#include <tf2_sensor_msgs/tf2_sensor_msgs.h>

constexpr char AirsimROSWrapper::CAM_YML_NAME[];
constexpr char AirsimROSWrapper::WIDTH_YML_NAME[];
constexpr char AirsimROSWrapper::HEIGHT_YML_NAME[];
constexpr char AirsimROSWrapper::K_YML_NAME[];
constexpr char AirsimROSWrapper::D_YML_NAME[];
constexpr char AirsimROSWrapper::R_YML_NAME[];
constexpr char AirsimROSWrapper::P_YML_NAME[];
constexpr char AirsimROSWrapper::DMODEL_YML_NAME[];

const std::unordered_map<int, std::string> AirsimROSWrapper::image_type_int_to_string_map_ = {
    { 0, "Scene" },
    { 1, "DepthPlanar" },
    { 2, "DepthPerspective" },
    { 3, "DepthVis" },
    { 4, "DisparityNormalized" },
    { 5, "Segmentation" },
    { 6, "SurfaceNormals" },
    { 7, "Infrared" }
};

AirsimROSWrapper::AirsimROSWrapper(const ros::NodeHandle& nh, const ros::NodeHandle& nh_private, const std::string& host_ip)
    : nh_(nh), nh_private_(nh_private), img_async_spinner_(1, &img_timer_cb_queue_), // a thread for image callbacks to be 'spun' by img_async_spinner_
    lidar_async_spinner_(1, &lidar_timer_cb_queue_)
    , // same as above, but for lidar
    host_ip_(host_ip)
    , airsim_client_images_(host_ip)
    , airsim_client_lidar_(host_ip)
    , airsim_settings_parser_(host_ip)
    , tf_listener_(tf_buffer_)
{
    ros_clock_.clock.fromSec(0);
    is_used_lidar_timer_cb_queue_ = false;
    is_used_img_timer_cb_queue_ = false;

    if (AirSimSettings::singleton().simmode_name != AirSimSettings::kSimModeTypeCar) {
        airsim_mode_ = AIRSIM_MODE::DRONE;
        ROS_INFO("Setting ROS wrapper to DRONE mode");
    }
    else {
        airsim_mode_ = AIRSIM_MODE::CAR;
        ROS_INFO("Setting ROS wrapper to CAR mode");
    }

    initialize_ros();

    std::cout << "AirsimROSWrapper Initialized!\n";
}

void AirsimROSWrapper::initialize_airsim()
{
    // todo do not reset if already in air?
    try {

        if (airsim_mode_ == AIRSIM_MODE::DRONE) {
            airsim_client_ = std::unique_ptr<msr::airlib::RpcLibClientBase>(new msr::airlib::MultirotorRpcLibClient(host_ip_));
        }
        else {
            airsim_client_ = std::unique_ptr<msr::airlib::RpcLibClientBase>(new msr::airlib::CarRpcLibClient(host_ip_));
        }
        airsim_client_->confirmConnection();
        airsim_client_images_.confirmConnection();
        airsim_client_lidar_.confirmConnection();

        for (const auto& vehicle_name_ptr_pair : vehicle_name_ptr_map_) {
            airsim_client_->enableApiControl(true, vehicle_name_ptr_pair.first); // todo expose as rosservice?
            airsim_client_->armDisarm(true, vehicle_name_ptr_pair.first); // todo exposes as rosservice?
        }

        origin_geo_point_ = airsim_client_->getHomeGeoPoint("");
        // todo there's only one global origin geopoint for environment. but airsim API accept a parameter vehicle_name? inside carsimpawnapi.cpp, there's a geopoint being assigned in the constructor. by?
        origin_geo_point_msg_ = get_gps_msg_from_airsim_geo_point(origin_geo_point_);
    }
    catch (rpc::rpc_error& e) {
        std::string msg = e.get_error().as<std::string>();
        std::cout << "Exception raised by the API, something went wrong." << std::endl
                  << msg << std::endl;
    }
}

void AirsimROSWrapper::initialize_ros()
{

    // ros params
    double update_airsim_control_every_n_sec;
    nh_private_.getParam("is_vulkan", is_vulkan_);
    nh_private_.getParam("update_airsim_control_every_n_sec", update_airsim_control_every_n_sec);
    nh_private_.getParam("publish_clock", publish_clock_);
    nh_private_.param("world_frame_id", world_frame_id_, world_frame_id_);
    odom_frame_id_ = world_frame_id_ == AIRSIM_FRAME_ID ? AIRSIM_ODOM_FRAME_ID : ENU_ODOM_FRAME_ID;
    nh_private_.param("odom_frame_id", odom_frame_id_, odom_frame_id_);
    isENU_ = !(odom_frame_id_ == AIRSIM_ODOM_FRAME_ID);
    nh_private_.param("coordinate_system_enu", isENU_, isENU_);
    vel_cmd_duration_ = 0.05; // todo rosparam
    // todo enforce dynamics constraints in this node as well?
    // nh_.getParam("max_vert_vel_", max_vert_vel_);
    // nh_.getParam("max_horz_vel", max_horz_vel_)

    create_ros_pubs_from_settings_json();
    airsim_control_update_timer_ = nh_private_.createTimer(ros::Duration(update_airsim_control_every_n_sec), &AirsimROSWrapper::drone_state_timer_cb, this);
}

// XmlRpc::XmlRpcValue can't be const in this case
void AirsimROSWrapper::create_ros_pubs_from_settings_json()
{
    // subscribe to control commands on global nodehandle
    gimbal_angle_quat_cmd_sub_ = nh_private_.subscribe("gimbal_angle_quat_cmd", 50, &AirsimROSWrapper::gimbal_angle_quat_cmd_cb, this);
    gimbal_angle_euler_cmd_sub_ = nh_private_.subscribe("gimbal_angle_euler_cmd", 50, &AirsimROSWrapper::gimbal_angle_euler_cmd_cb, this);
    origin_geo_point_pub_ = nh_private_.advertise<airsim_ros_pkgs::GPSYaw>("origin_geo_point", 10);

    airsim_img_request_vehicle_name_pair_vec_.clear();
    image_pub_vec_.clear();
    cam_info_pub_vec_.clear();
    camera_info_msg_vec_.clear();
    vehicle_name_ptr_map_.clear();
    size_t lidar_cnt = 0;

    image_transport::ImageTransport image_transporter(nh_private_);

    // iterate over std::map<std::string, std::unique_ptr<VehicleSetting>> vehicles;
    for (const auto& curr_vehicle_elem : AirSimSettings::singleton().vehicles) {
        auto& vehicle_setting = curr_vehicle_elem.second;
        auto curr_vehicle_name = curr_vehicle_elem.first;
        set_nans_to_zeros_in_pose(*vehicle_setting);

        std::unique_ptr<VehicleROS> vehicle_ros = nullptr;

        if (airsim_mode_ == AIRSIM_MODE::DRONE) {
            vehicle_ros = std::unique_ptr<MultiRotorROS>(new MultiRotorROS());
        }
        else {
            vehicle_ros = std::unique_ptr<CarROS>(new CarROS());
        }

        vehicle_ros->odom_frame_id = curr_vehicle_name + "/" + odom_frame_id_;
        vehicle_ros->vehicle_name = curr_vehicle_name;

        append_static_vehicle_tf(vehicle_ros.get(), *vehicle_setting);

        vehicle_ros->odom_local_pub = nh_private_.advertise<nav_msgs::Odometry>(curr_vehicle_name + "/" + odom_frame_id_, 10);

        vehicle_ros->env_pub = nh_private_.advertise<airsim_ros_pkgs::Environment>(curr_vehicle_name + "/environment", 10);

        vehicle_ros->global_gps_pub = nh_private_.advertise<sensor_msgs::NavSatFix>(curr_vehicle_name + "/global_gps", 10);

        if (airsim_mode_ == AIRSIM_MODE::DRONE) {
            auto drone = static_cast<MultiRotorROS*>(vehicle_ros.get());

            // bind to a single callback. todo optimal subs queue length
            // bind multiple topics to a single callback, but keep track of which vehicle name it was by passing curr_vehicle_name as the 2nd argument
            drone->vel_cmd_body_frame_sub = nh_private_.subscribe<airsim_ros_pkgs::VelCmd>(curr_vehicle_name + "/vel_cmd_body_frame", 1, boost::bind(&AirsimROSWrapper::vel_cmd_body_frame_cb, this, _1, vehicle_ros->vehicle_name)); // todo ros::TransportHints().tcpNoDelay();
            drone->vel_cmd_world_frame_sub = nh_private_.subscribe<airsim_ros_pkgs::VelCmd>(curr_vehicle_name + "/vel_cmd_world_frame", 1, boost::bind(&AirsimROSWrapper::vel_cmd_world_frame_cb, this, _1, vehicle_ros->vehicle_name));

            drone->takeoff_srvr = nh_private_.advertiseService<airsim_ros_pkgs::Takeoff::Request, airsim_ros_pkgs::Takeoff::Response>(curr_vehicle_name + "/takeoff",
                                                                                                                                      boost::bind(&AirsimROSWrapper::takeoff_srv_cb, this, _1, _2, vehicle_ros->vehicle_name));
            drone->land_srvr = nh_private_.advertiseService<airsim_ros_pkgs::Land::Request, airsim_ros_pkgs::Land::Response>(curr_vehicle_name + "/land",
                                                                                                                             boost::bind(&AirsimROSWrapper::land_srv_cb, this, _1, _2, vehicle_ros->vehicle_name));
            // vehicle_ros.reset_srvr = nh_private_.advertiseService(curr_vehicle_name + "/reset",&AirsimROSWrapper::reset_srv_cb, this);
        }
        else {
            auto car = static_cast<CarROS*>(vehicle_ros.get());
            car->car_cmd_sub = nh_private_.subscribe<airsim_ros_pkgs::CarControls>(curr_vehicle_name + "/car_cmd", 1, boost::bind(&AirsimROSWrapper::car_cmd_cb, this, _1, vehicle_ros->vehicle_name));
            car->car_state_pub = nh_private_.advertise<airsim_ros_pkgs::CarState>(curr_vehicle_name + "/car_state", 10);
        }

        // iterate over camera map std::map<std::string, CameraSetting> .cameras;
        for (auto& curr_camera_elem : vehicle_setting->cameras) {
            auto& camera_setting = curr_camera_elem.second;
            auto& curr_camera_name = curr_camera_elem.first;

            set_nans_to_zeros_in_pose(*vehicle_setting, camera_setting);
            append_static_camera_tf(vehicle_ros.get(), curr_camera_name, camera_setting);
            // camera_setting.gimbal
            std::vector<ImageRequest> current_image_request_vec;
            current_image_request_vec.clear();

            // iterate over capture_setting std::map<int, CaptureSetting> capture_settings
            for (const auto& curr_capture_elem : camera_setting.capture_settings) {
                auto& capture_setting = curr_capture_elem.second;

                // todo why does AirSimSettings::loadCaptureSettings calls AirSimSettings::initializeCaptureSettings()
                // which initializes default capture settings for _all_ NINE msr::airlib::ImageCaptureBase::ImageType
                if (!(std::isnan(capture_setting.fov_degrees))) {
                    ImageType curr_image_type = msr::airlib::Utils::toEnum<ImageType>(capture_setting.image_type);
                    // if scene / segmentation / surface normals / infrared, get uncompressed image with pixels_as_floats = false
                    if (capture_setting.image_type == 0 || capture_setting.image_type == 5 || capture_setting.image_type == 6 || capture_setting.image_type == 7) {
                        current_image_request_vec.push_back(ImageRequest(curr_camera_name, curr_image_type, false, false));
                    }
                    // if {DepthPlanar, DepthPerspective,DepthVis, DisparityNormalized}, get float image
                    else {
                        current_image_request_vec.push_back(ImageRequest(curr_camera_name, curr_image_type, true));
                    }

                    image_pub_vec_.push_back(image_transporter.advertise(curr_vehicle_name + "/" + curr_camera_name + "/" + image_type_int_to_string_map_.at(capture_setting.image_type), 1));
                    cam_info_pub_vec_.push_back(nh_private_.advertise<sensor_msgs::CameraInfo>(curr_vehicle_name + "/" + curr_camera_name + "/" + image_type_int_to_string_map_.at(capture_setting.image_type) + "/camera_info", 10));
                    camera_info_msg_vec_.push_back(generate_cam_info(curr_camera_name, camera_setting, capture_setting));
                }
            }
            // push back pair (vector of image captures, current vehicle name)
            airsim_img_request_vehicle_name_pair_vec_.push_back(std::make_pair(current_image_request_vec, curr_vehicle_name));
        }

        // iterate over sensors
        std::vector<SensorPublisher> sensors;
        for (auto& curr_sensor_map : vehicle_setting->sensors) {
            auto& sensor_name = curr_sensor_map.first;
            auto& sensor_setting = curr_sensor_map.second;

            if (sensor_setting->enabled) {
                SensorPublisher sensor_publisher;
                sensor_publisher.sensor_name = sensor_setting->sensor_name;
                sensor_publisher.sensor_type = sensor_setting->sensor_type;
                switch (sensor_setting->sensor_type) {
                case SensorBase::SensorType::Barometer: {
                    std::cout << "Barometer" << std::endl;
                    sensor_publisher.publisher = nh_private_.advertise<airsim_ros_pkgs::Altimeter>(curr_vehicle_name + "/altimeter/" + sensor_name, 10);
                    break;
                }
                case SensorBase::SensorType::Imu: {
                    std::cout << "Imu" << std::endl;
                    sensor_publisher.publisher = nh_private_.advertise<sensor_msgs::Imu>(curr_vehicle_name + "/imu/" + sensor_name, 10);
                    break;
                }
                case SensorBase::SensorType::Gps: {
                    std::cout << "Gps" << std::endl;
                    sensor_publisher.publisher = nh_private_.advertise<sensor_msgs::NavSatFix>(curr_vehicle_name + "/gps/" + sensor_name, 10);
                    break;
                }
                case SensorBase::SensorType::Magnetometer: {
                    std::cout << "Magnetometer" << std::endl;
                    sensor_publisher.publisher = nh_private_.advertise<sensor_msgs::MagneticField>(curr_vehicle_name + "/magnetometer/" + sensor_name, 10);
                    break;
                }
                case SensorBase::SensorType::Distance: {
                    std::cout << "Distance" << std::endl;
                    sensor_publisher.publisher = nh_private_.advertise<sensor_msgs::Range>(curr_vehicle_name + "/distance/" + sensor_name, 10);
                    break;
                }
                case SensorBase::SensorType::Lidar: {
                    std::cout << "Lidar" << std::endl;
                    auto lidar_setting = *static_cast<LidarSetting*>(sensor_setting.get());
                    msr::airlib::LidarSimpleParams params;
                    params.initializeFromSettings(lidar_setting);
                    append_static_lidar_tf(vehicle_ros.get(), sensor_name, params);
                    sensor_publisher.publisher = nh_private_.advertise<sensor_msgs::PointCloud2>(curr_vehicle_name + "/lidar/" + sensor_name, 10);
                    break;
                }
                default: {
                    throw std::invalid_argument("Unexpected sensor type");
                }
                }
                sensors.emplace_back(sensor_publisher);
            }
        }

        // we want fast access to the lidar sensors for callback handling, sort them out now
        auto isLidar = std::function<bool(const SensorPublisher& pub)>([](const SensorPublisher& pub) {
            return pub.sensor_type == SensorBase::SensorType::Lidar;
        });
        size_t cnt = std::count_if(sensors.begin(), sensors.end(), isLidar);
        lidar_cnt += cnt;
        vehicle_ros->lidar_pubs.resize(cnt);
        vehicle_ros->sensor_pubs.resize(sensors.size() - cnt);
        std::partition_copy(sensors.begin(), sensors.end(), vehicle_ros->lidar_pubs.begin(), vehicle_ros->sensor_pubs.begin(), isLidar);

        vehicle_name_ptr_map_.emplace(curr_vehicle_name, std::move(vehicle_ros)); // allows fast lookup in command callbacks in case of a lot of drones
    }

    // add takeoff and land all services if more than 2 drones
    if (vehicle_name_ptr_map_.size() > 1 && airsim_mode_ == AIRSIM_MODE::DRONE) {
        takeoff_all_srvr_ = nh_private_.advertiseService("all_robots/takeoff", &AirsimROSWrapper::takeoff_all_srv_cb, this);
        land_all_srvr_ = nh_private_.advertiseService("all_robots/land", &AirsimROSWrapper::land_all_srv_cb, this);

        // gimbal_angle_quat_cmd_sub_ = nh_.subscribe("gimbal_angle_quat_cmd", 50, &AirsimROSWrapper::gimbal_angle_quat_cmd_cb, this);

        vel_cmd_all_body_frame_sub_ = nh_private_.subscribe("all_robots/vel_cmd_body_frame", 1, &AirsimROSWrapper::vel_cmd_all_body_frame_cb, this);
        vel_cmd_all_world_frame_sub_ = nh_private_.subscribe("all_robots/vel_cmd_world_frame", 1, &AirsimROSWrapper::vel_cmd_all_world_frame_cb, this);

        vel_cmd_group_body_frame_sub_ = nh_private_.subscribe("group_of_robots/vel_cmd_body_frame", 1, &AirsimROSWrapper::vel_cmd_group_body_frame_cb, this);
        vel_cmd_group_world_frame_sub_ = nh_private_.subscribe("group_of_obots/vel_cmd_world_frame", 1, &AirsimROSWrapper::vel_cmd_group_world_frame_cb, this);

        takeoff_group_srvr_ = nh_private_.advertiseService("group_of_robots/takeoff", &AirsimROSWrapper::takeoff_group_srv_cb, this);
        land_group_srvr_ = nh_private_.advertiseService("group_of_robots/land", &AirsimROSWrapper::land_group_srv_cb, this);
    }

    // todo add per vehicle reset in AirLib API
    reset_srvr_ = nh_private_.advertiseService("reset", &AirsimROSWrapper::reset_srv_cb, this);

    if (publish_clock_) {
        clock_pub_ = nh_private_.advertise<rosgraph_msgs::Clock>("clock", 1);
    }

    // if >0 cameras, add one more thread for img_request_timer_cb
    if (!airsim_img_request_vehicle_name_pair_vec_.empty()) {
        double update_airsim_img_response_every_n_sec;
        nh_private_.getParam("update_airsim_img_response_every_n_sec", update_airsim_img_response_every_n_sec);

        ros::TimerOptions timer_options(ros::Duration(update_airsim_img_response_every_n_sec), boost::bind(&AirsimROSWrapper::img_response_timer_cb, this, _1), &img_timer_cb_queue_);
        airsim_img_response_timer_ = nh_private_.createTimer(timer_options);
        is_used_img_timer_cb_queue_ = true;
    }

    // lidars update on their own callback/thread at a given rate
    if (lidar_cnt > 0) {
        double update_lidar_every_n_sec;
        nh_private_.getParam("update_lidar_every_n_sec", update_lidar_every_n_sec);
        // nh_private_.setCallbackQueue(&lidar_timer_cb_queue_);
        ros::TimerOptions timer_options(ros::Duration(update_lidar_every_n_sec), boost::bind(&AirsimROSWrapper::lidar_timer_cb, this, _1), &lidar_timer_cb_queue_);
        airsim_lidar_update_timer_ = nh_private_.createTimer(timer_options);
        is_used_lidar_timer_cb_queue_ = true;
    }

    initialize_airsim();
}

// todo: error check. if state is not landed, return error.
bool AirsimROSWrapper::takeoff_srv_cb(airsim_ros_pkgs::Takeoff::Request& request, airsim_ros_pkgs::Takeoff::Response& response, const std::string& vehicle_name)
{
    std::lock_guard<std::mutex> guard(drone_control_mutex_);

    if (request.waitOnLastTask)
        static_cast<msr::airlib::MultirotorRpcLibClient*>(airsim_client_.get())->takeoffAsync(20, vehicle_name)->waitOnLastTask(); // todo value for timeout_sec?
    // response.success =
    else
        static_cast<msr::airlib::MultirotorRpcLibClient*>(airsim_client_.get())->takeoffAsync(20, vehicle_name);
    // response.success =

    return true;
}

bool AirsimROSWrapper::takeoff_group_srv_cb(airsim_ros_pkgs::TakeoffGroup::Request& request, airsim_ros_pkgs::TakeoffGroup::Response& response)
{
    std::lock_guard<std::mutex> guard(drone_control_mutex_);

    if (request.waitOnLastTask)
        for (const auto& vehicle_name : request.vehicle_names)
            static_cast<msr::airlib::MultirotorRpcLibClient*>(airsim_client_.get())->takeoffAsync(20, vehicle_name)->waitOnLastTask(); // todo value for timeout_sec?
    // response.success =
    else
        for (const auto& vehicle_name : request.vehicle_names)
            static_cast<msr::airlib::MultirotorRpcLibClient*>(airsim_client_.get())->takeoffAsync(20, vehicle_name);
    // response.success =

    return true;
}

bool AirsimROSWrapper::takeoff_all_srv_cb(airsim_ros_pkgs::Takeoff::Request& request, airsim_ros_pkgs::Takeoff::Response& response)
{
    std::lock_guard<std::mutex> guard(drone_control_mutex_);

    if (request.waitOnLastTask)
        for (const auto& vehicle_name_ptr_pair : vehicle_name_ptr_map_)
            static_cast<msr::airlib::MultirotorRpcLibClient*>(airsim_client_.get())->takeoffAsync(20, vehicle_name_ptr_pair.first)->waitOnLastTask(); // todo value for timeout_sec?
    // response.success =
    else
        for (const auto& vehicle_name_ptr_pair : vehicle_name_ptr_map_)
            static_cast<msr::airlib::MultirotorRpcLibClient*>(airsim_client_.get())->takeoffAsync(20, vehicle_name_ptr_pair.first);
    // response.success =

    return true;
}

bool AirsimROSWrapper::land_srv_cb(airsim_ros_pkgs::Land::Request& request, airsim_ros_pkgs::Land::Response& response, const std::string& vehicle_name)
{
    std::lock_guard<std::mutex> guard(drone_control_mutex_);

    if (request.waitOnLastTask)
        static_cast<msr::airlib::MultirotorRpcLibClient*>(airsim_client_.get())->landAsync(60, vehicle_name)->waitOnLastTask();
    else
        static_cast<msr::airlib::MultirotorRpcLibClient*>(airsim_client_.get())->landAsync(60, vehicle_name);

    return true; //todo
}

bool AirsimROSWrapper::land_group_srv_cb(airsim_ros_pkgs::LandGroup::Request& request, airsim_ros_pkgs::LandGroup::Response& response)
{
    std::lock_guard<std::mutex> guard(drone_control_mutex_);

    if (request.waitOnLastTask)
        for (const auto& vehicle_name : request.vehicle_names)
            static_cast<msr::airlib::MultirotorRpcLibClient*>(airsim_client_.get())->landAsync(60, vehicle_name)->waitOnLastTask();
    else
        for (const auto& vehicle_name : request.vehicle_names)
            static_cast<msr::airlib::MultirotorRpcLibClient*>(airsim_client_.get())->landAsync(60, vehicle_name);

    return true; //todo
}

bool AirsimROSWrapper::land_all_srv_cb(airsim_ros_pkgs::Land::Request& request, airsim_ros_pkgs::Land::Response& response)
{
    std::lock_guard<std::mutex> guard(drone_control_mutex_);

    if (request.waitOnLastTask)
        for (const auto& vehicle_name_ptr_pair : vehicle_name_ptr_map_)
            static_cast<msr::airlib::MultirotorRpcLibClient*>(airsim_client_.get())->landAsync(60, vehicle_name_ptr_pair.first)->waitOnLastTask();
    else
        for (const auto& vehicle_name_ptr_pair : vehicle_name_ptr_map_)
            static_cast<msr::airlib::MultirotorRpcLibClient*>(airsim_client_.get())->landAsync(60, vehicle_name_ptr_pair.first);

    return true; //todo
}

// todo add reset by vehicle_name API to airlib
// todo not async remove waitonlasttask
bool AirsimROSWrapper::reset_srv_cb(airsim_ros_pkgs::Reset::Request& request, airsim_ros_pkgs::Reset::Response& response)
{
    std::lock_guard<std::mutex> guard(drone_control_mutex_);

    airsim_client_.reset();
    return true; //todo
}

tf2::Quaternion AirsimROSWrapper::get_tf2_quat(const msr::airlib::Quaternionr& airlib_quat) const
{
    return tf2::Quaternion(airlib_quat.x(), airlib_quat.y(), airlib_quat.z(), airlib_quat.w());
}

msr::airlib::Quaternionr AirsimROSWrapper::get_airlib_quat(const geometry_msgs::Quaternion& geometry_msgs_quat) const
{
    return msr::airlib::Quaternionr(geometry_msgs_quat.w, geometry_msgs_quat.x, geometry_msgs_quat.y, geometry_msgs_quat.z);
}

msr::airlib::Quaternionr AirsimROSWrapper::get_airlib_quat(const tf2::Quaternion& tf2_quat) const
{
    return msr::airlib::Quaternionr(tf2_quat.w(), tf2_quat.x(), tf2_quat.y(), tf2_quat.z());
}

void AirsimROSWrapper::car_cmd_cb(const airsim_ros_pkgs::CarControls::ConstPtr& msg, const std::string& vehicle_name)
{
    std::lock_guard<std::mutex> guard(drone_control_mutex_);

    auto car = static_cast<CarROS*>(vehicle_name_ptr_map_[vehicle_name].get());
    car->car_cmd.throttle = msg->throttle;
    car->car_cmd.steering = msg->steering;
    car->car_cmd.brake = msg->brake;
    car->car_cmd.handbrake = msg->handbrake;
    car->car_cmd.is_manual_gear = msg->manual;
    car->car_cmd.manual_gear = msg->manual_gear;
    car->car_cmd.gear_immediate = msg->gear_immediate;

    car->has_car_cmd = true;
}

msr::airlib::Pose AirsimROSWrapper::get_airlib_pose(const float& x, const float& y, const float& z, const msr::airlib::Quaternionr& airlib_quat) const
{
    return msr::airlib::Pose(msr::airlib::Vector3r(x, y, z), airlib_quat);
}

// void AirsimROSWrapper::vel_cmd_body_frame_cb(const airsim_ros_pkgs::VelCmd& msg, const std::string& vehicle_name)
void AirsimROSWrapper::vel_cmd_body_frame_cb(const airsim_ros_pkgs::VelCmd::ConstPtr& msg, const std::string& vehicle_name)
{
    std::lock_guard<std::mutex> guard(drone_control_mutex_);

    auto drone = static_cast<MultiRotorROS*>(vehicle_name_ptr_map_[vehicle_name].get());

    double roll, pitch, yaw;
    tf2::Matrix3x3(get_tf2_quat(drone->curr_drone_state.kinematics_estimated.pose.orientation)).getRPY(roll, pitch, yaw); // ros uses xyzw

    // todo do actual body frame?
    drone->vel_cmd.x = (msg->twist.linear.x * cos(yaw)) - (msg->twist.linear.y * sin(yaw)); //body frame assuming zero pitch roll
    drone->vel_cmd.y = (msg->twist.linear.x * sin(yaw)) + (msg->twist.linear.y * cos(yaw)); //body frame
    drone->vel_cmd.z = msg->twist.linear.z;
    drone->vel_cmd.drivetrain = msr::airlib::DrivetrainType::MaxDegreeOfFreedom;
    drone->vel_cmd.yaw_mode.is_rate = true;
    // airsim uses degrees
    drone->vel_cmd.yaw_mode.yaw_or_rate = math_common::rad2deg(msg->twist.angular.z);
    drone->has_vel_cmd = true;
}

void AirsimROSWrapper::vel_cmd_group_body_frame_cb(const airsim_ros_pkgs::VelCmdGroup& msg)
{
    std::lock_guard<std::mutex> guard(drone_control_mutex_);

    for (const auto& vehicle_name : msg.vehicle_names) {
        auto drone = static_cast<MultiRotorROS*>(vehicle_name_ptr_map_[vehicle_name].get());

        double roll, pitch, yaw;
        tf2::Matrix3x3(get_tf2_quat(drone->curr_drone_state.kinematics_estimated.pose.orientation)).getRPY(roll, pitch, yaw); // ros uses xyzw

        // todo do actual body frame?
        drone->vel_cmd.x = (msg.twist.linear.x * cos(yaw)) - (msg.twist.linear.y * sin(yaw)); //body frame assuming zero pitch roll
        drone->vel_cmd.y = (msg.twist.linear.x * sin(yaw)) + (msg.twist.linear.y * cos(yaw)); //body frame
        drone->vel_cmd.z = msg.twist.linear.z;
        drone->vel_cmd.drivetrain = msr::airlib::DrivetrainType::MaxDegreeOfFreedom;
        drone->vel_cmd.yaw_mode.is_rate = true;
        // airsim uses degrees
        drone->vel_cmd.yaw_mode.yaw_or_rate = math_common::rad2deg(msg.twist.angular.z);
        drone->has_vel_cmd = true;
    }
}

// void AirsimROSWrapper::vel_cmd_all_body_frame_cb(const airsim_ros_pkgs::VelCmd::ConstPtr& msg)
void AirsimROSWrapper::vel_cmd_all_body_frame_cb(const airsim_ros_pkgs::VelCmd& msg)
{
    std::lock_guard<std::mutex> guard(drone_control_mutex_);

    // todo expose waitOnLastTask or nah?
    for (auto& vehicle_name_ptr_pair : vehicle_name_ptr_map_) {
        auto drone = static_cast<MultiRotorROS*>(vehicle_name_ptr_pair.second.get());

        double roll, pitch, yaw;
        tf2::Matrix3x3(get_tf2_quat(drone->curr_drone_state.kinematics_estimated.pose.orientation)).getRPY(roll, pitch, yaw); // ros uses xyzw

        // todo do actual body frame?
        drone->vel_cmd.x = (msg.twist.linear.x * cos(yaw)) - (msg.twist.linear.y * sin(yaw)); //body frame assuming zero pitch roll
        drone->vel_cmd.y = (msg.twist.linear.x * sin(yaw)) + (msg.twist.linear.y * cos(yaw)); //body frame
        drone->vel_cmd.z = msg.twist.linear.z;
        drone->vel_cmd.drivetrain = msr::airlib::DrivetrainType::MaxDegreeOfFreedom;
        drone->vel_cmd.yaw_mode.is_rate = true;
        // airsim uses degrees
        drone->vel_cmd.yaw_mode.yaw_or_rate = math_common::rad2deg(msg.twist.angular.z);
        drone->has_vel_cmd = true;
    }
}

void AirsimROSWrapper::vel_cmd_world_frame_cb(const airsim_ros_pkgs::VelCmd::ConstPtr& msg, const std::string& vehicle_name)
{
    std::lock_guard<std::mutex> guard(drone_control_mutex_);

    auto drone = static_cast<MultiRotorROS*>(vehicle_name_ptr_map_[vehicle_name].get());

    drone->vel_cmd.x = msg->twist.linear.x;
    drone->vel_cmd.y = msg->twist.linear.y;
    drone->vel_cmd.z = msg->twist.linear.z;
    drone->vel_cmd.drivetrain = msr::airlib::DrivetrainType::MaxDegreeOfFreedom;
    drone->vel_cmd.yaw_mode.is_rate = true;
    drone->vel_cmd.yaw_mode.yaw_or_rate = math_common::rad2deg(msg->twist.angular.z);
    drone->has_vel_cmd = true;
}

// this is kinda unnecessary but maybe it makes life easier for the end user.
void AirsimROSWrapper::vel_cmd_group_world_frame_cb(const airsim_ros_pkgs::VelCmdGroup& msg)
{
    std::lock_guard<std::mutex> guard(drone_control_mutex_);

    for (const auto& vehicle_name : msg.vehicle_names) {
        auto drone = static_cast<MultiRotorROS*>(vehicle_name_ptr_map_[vehicle_name].get());

        drone->vel_cmd.x = msg.twist.linear.x;
        drone->vel_cmd.y = msg.twist.linear.y;
        drone->vel_cmd.z = msg.twist.linear.z;
        drone->vel_cmd.drivetrain = msr::airlib::DrivetrainType::MaxDegreeOfFreedom;
        drone->vel_cmd.yaw_mode.is_rate = true;
        drone->vel_cmd.yaw_mode.yaw_or_rate = math_common::rad2deg(msg.twist.angular.z);
        drone->has_vel_cmd = true;
    }
}

void AirsimROSWrapper::vel_cmd_all_world_frame_cb(const airsim_ros_pkgs::VelCmd& msg)
{
    std::lock_guard<std::mutex> guard(drone_control_mutex_);

    // todo expose waitOnLastTask or nah?
    for (auto& vehicle_name_ptr_pair : vehicle_name_ptr_map_) {
        auto drone = static_cast<MultiRotorROS*>(vehicle_name_ptr_pair.second.get());

        drone->vel_cmd.x = msg.twist.linear.x;
        drone->vel_cmd.y = msg.twist.linear.y;
        drone->vel_cmd.z = msg.twist.linear.z;
        drone->vel_cmd.drivetrain = msr::airlib::DrivetrainType::MaxDegreeOfFreedom;
        drone->vel_cmd.yaw_mode.is_rate = true;
        drone->vel_cmd.yaw_mode.yaw_or_rate = math_common::rad2deg(msg.twist.angular.z);
        drone->has_vel_cmd = true;
    }
}

// todo support multiple gimbal commands
void AirsimROSWrapper::gimbal_angle_quat_cmd_cb(const airsim_ros_pkgs::GimbalAngleQuatCmd& gimbal_angle_quat_cmd_msg)
{
    tf2::Quaternion quat_control_cmd;
    try {
        tf2::convert(gimbal_angle_quat_cmd_msg.orientation, quat_control_cmd);
        quat_control_cmd.normalize();
        gimbal_cmd_.target_quat = get_airlib_quat(quat_control_cmd); // airsim uses wxyz
        gimbal_cmd_.camera_name = gimbal_angle_quat_cmd_msg.camera_name;
        gimbal_cmd_.vehicle_name = gimbal_angle_quat_cmd_msg.vehicle_name;
        has_gimbal_cmd_ = true;
    }
    catch (tf2::TransformException& ex) {
        ROS_WARN("%s", ex.what());
    }
}

// todo support multiple gimbal commands
// 1. find quaternion of default gimbal pose
// 2. forward multiply with quaternion equivalent to desired euler commands (in degrees)
// 3. call airsim client's setCameraPose which sets camera pose wrt world (or takeoff?) ned frame. todo
void AirsimROSWrapper::gimbal_angle_euler_cmd_cb(const airsim_ros_pkgs::GimbalAngleEulerCmd& gimbal_angle_euler_cmd_msg)
{
    try {
        tf2::Quaternion quat_control_cmd;
        quat_control_cmd.setRPY(math_common::deg2rad(gimbal_angle_euler_cmd_msg.roll), math_common::deg2rad(gimbal_angle_euler_cmd_msg.pitch), math_common::deg2rad(gimbal_angle_euler_cmd_msg.yaw));
        quat_control_cmd.normalize();
        gimbal_cmd_.target_quat = get_airlib_quat(quat_control_cmd);
        gimbal_cmd_.camera_name = gimbal_angle_euler_cmd_msg.camera_name;
        gimbal_cmd_.vehicle_name = gimbal_angle_euler_cmd_msg.vehicle_name;
        has_gimbal_cmd_ = true;
    }
    catch (tf2::TransformException& ex) {
        ROS_WARN("%s", ex.what());
    }
}

airsim_ros_pkgs::CarState AirsimROSWrapper::get_roscarstate_msg_from_car_state(const msr::airlib::CarApiBase::CarState& car_state) const
{
    airsim_ros_pkgs::CarState state_msg;
    const auto odo = get_odom_msg_from_car_state(car_state);

    state_msg.pose = odo.pose;
    state_msg.twist = odo.twist;
    state_msg.speed = car_state.speed;
    state_msg.gear = car_state.gear;
    state_msg.rpm = car_state.rpm;
    state_msg.maxrpm = car_state.maxrpm;
    state_msg.handbrake = car_state.handbrake;
    state_msg.header.stamp = airsim_timestamp_to_ros(car_state.timestamp);

    return state_msg;
}

nav_msgs::Odometry AirsimROSWrapper::get_odom_msg_from_car_state(const msr::airlib::CarApiBase::CarState& car_state) const
{
    nav_msgs::Odometry odom_msg;

    odom_msg.pose.pose.position.x = car_state.getPosition().x();
    odom_msg.pose.pose.position.y = car_state.getPosition().y();
    odom_msg.pose.pose.position.z = car_state.getPosition().z();
    odom_msg.pose.pose.orientation.x = car_state.getOrientation().x();
    odom_msg.pose.pose.orientation.y = car_state.getOrientation().y();
    odom_msg.pose.pose.orientation.z = car_state.getOrientation().z();
    odom_msg.pose.pose.orientation.w = car_state.getOrientation().w();

    odom_msg.twist.twist.linear.x = car_state.kinematics_estimated.twist.linear.x();
    odom_msg.twist.twist.linear.y = car_state.kinematics_estimated.twist.linear.y();
    odom_msg.twist.twist.linear.z = car_state.kinematics_estimated.twist.linear.z();
    odom_msg.twist.twist.angular.x = car_state.kinematics_estimated.twist.angular.x();
    odom_msg.twist.twist.angular.y = car_state.kinematics_estimated.twist.angular.y();
    odom_msg.twist.twist.angular.z = car_state.kinematics_estimated.twist.angular.z();

    if (isENU_) {
        std::swap(odom_msg.pose.pose.position.x, odom_msg.pose.pose.position.y);
        odom_msg.pose.pose.position.z = -odom_msg.pose.pose.position.z;
        std::swap(odom_msg.pose.pose.orientation.x, odom_msg.pose.pose.orientation.y);
        odom_msg.pose.pose.orientation.z = -odom_msg.pose.pose.orientation.z;
        std::swap(odom_msg.twist.twist.linear.x, odom_msg.twist.twist.linear.y);
        odom_msg.twist.twist.linear.z = -odom_msg.twist.twist.linear.z;
        std::swap(odom_msg.twist.twist.angular.x, odom_msg.twist.twist.angular.y);
        odom_msg.twist.twist.angular.z = -odom_msg.twist.twist.angular.z;
    }

    return odom_msg;
}

nav_msgs::Odometry AirsimROSWrapper::get_odom_msg_from_multirotor_state(const msr::airlib::MultirotorState& drone_state) const
{
    nav_msgs::Odometry odom_msg;

    odom_msg.pose.pose.position.x = drone_state.getPosition().x();
    odom_msg.pose.pose.position.y = drone_state.getPosition().y();
    odom_msg.pose.pose.position.z = drone_state.getPosition().z();
    odom_msg.pose.pose.orientation.x = drone_state.getOrientation().x();
    odom_msg.pose.pose.orientation.y = drone_state.getOrientation().y();
    odom_msg.pose.pose.orientation.z = drone_state.getOrientation().z();
    odom_msg.pose.pose.orientation.w = drone_state.getOrientation().w();

    odom_msg.twist.twist.linear.x = drone_state.kinematics_estimated.twist.linear.x();
    odom_msg.twist.twist.linear.y = drone_state.kinematics_estimated.twist.linear.y();
    odom_msg.twist.twist.linear.z = drone_state.kinematics_estimated.twist.linear.z();
    odom_msg.twist.twist.angular.x = drone_state.kinematics_estimated.twist.angular.x();
    odom_msg.twist.twist.angular.y = drone_state.kinematics_estimated.twist.angular.y();
    odom_msg.twist.twist.angular.z = drone_state.kinematics_estimated.twist.angular.z();

    if (isENU_) {
        std::swap(odom_msg.pose.pose.position.x, odom_msg.pose.pose.position.y);
        odom_msg.pose.pose.position.z = -odom_msg.pose.pose.position.z;
        std::swap(odom_msg.pose.pose.orientation.x, odom_msg.pose.pose.orientation.y);
        odom_msg.pose.pose.orientation.z = -odom_msg.pose.pose.orientation.z;
        std::swap(odom_msg.twist.twist.linear.x, odom_msg.twist.twist.linear.y);
        odom_msg.twist.twist.linear.z = -odom_msg.twist.twist.linear.z;
        std::swap(odom_msg.twist.twist.angular.x, odom_msg.twist.twist.angular.y);
        odom_msg.twist.twist.angular.z = -odom_msg.twist.twist.angular.z;
    }

    return odom_msg;
}

// https://docs.ros.org/jade/api/sensor_msgs/html/point__cloud__conversion_8h_source.html#l00066
// look at UnrealLidarSensor.cpp UnrealLidarSensor::getPointCloud() for math
// read this carefully https://docs.ros.org/kinetic/api/sensor_msgs/html/msg/PointCloud2.html
sensor_msgs::PointCloud2 AirsimROSWrapper::get_lidar_msg_from_airsim(const msr::airlib::LidarData& lidar_data, const std::string& vehicle_name) const
{
    sensor_msgs::PointCloud2 lidar_msg;
    lidar_msg.header.stamp = ros::Time::now();
    lidar_msg.header.frame_id = vehicle_name;

    if (lidar_data.point_cloud.size() > 3) {
        lidar_msg.height = 1;
        lidar_msg.width = lidar_data.point_cloud.size() / 3;

        lidar_msg.fields.resize(3);
        lidar_msg.fields[0].name = "x";
        lidar_msg.fields[1].name = "y";
        lidar_msg.fields[2].name = "z";

        int offset = 0;

        for (size_t d = 0; d < lidar_msg.fields.size(); ++d, offset += 4) {
            lidar_msg.fields[d].offset = offset;
            lidar_msg.fields[d].datatype = sensor_msgs::PointField::FLOAT32;
            lidar_msg.fields[d].count = 1;
        }

        lidar_msg.is_bigendian = false;
        lidar_msg.point_step = offset; // 4 * num fields
        lidar_msg.row_step = lidar_msg.point_step * lidar_msg.width;

        lidar_msg.is_dense = true; // todo
        std::vector<float> data_std = lidar_data.point_cloud;

        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data_std.data());
        vector<unsigned char> lidar_msg_data(bytes, bytes + sizeof(float) * data_std.size());
        lidar_msg.data = std::move(lidar_msg_data);
    }
    else {
        // msg = []
    }

    if (isENU_) {
        try {
            sensor_msgs::PointCloud2 lidar_msg_enu;
            auto transformStampedENU = tf_buffer_.lookupTransform(AIRSIM_FRAME_ID, vehicle_name, ros::Time(0), ros::Duration(1));
            tf2::doTransform(lidar_msg, lidar_msg_enu, transformStampedENU);

            lidar_msg_enu.header.stamp = lidar_msg.header.stamp;
            lidar_msg_enu.header.frame_id = lidar_msg.header.frame_id;

            lidar_msg = std::move(lidar_msg_enu);
        }
        catch (tf2::TransformException& ex) {
            ROS_WARN("%s", ex.what());
            ros::Duration(1.0).sleep();
        }
    }

    return lidar_msg;
}

airsim_ros_pkgs::Environment AirsimROSWrapper::get_environment_msg_from_airsim(const msr::airlib::Environment::State& env_data) const
{
    airsim_ros_pkgs::Environment env_msg;
    env_msg.position.x = env_data.position.x();
    env_msg.position.y = env_data.position.y();
    env_msg.position.z = env_data.position.z();
    env_msg.geo_point.latitude = env_data.geo_point.latitude;
    env_msg.geo_point.longitude = env_data.geo_point.longitude;
    env_msg.geo_point.altitude = env_data.geo_point.altitude;
    env_msg.gravity.x = env_data.gravity.x();
    env_msg.gravity.y = env_data.gravity.y();
    env_msg.gravity.z = env_data.gravity.z();
    env_msg.air_pressure = env_data.air_pressure;
    env_msg.temperature = env_data.temperature;
    env_msg.air_density = env_data.temperature;

    return env_msg;
}

sensor_msgs::MagneticField AirsimROSWrapper::get_mag_msg_from_airsim(const msr::airlib::MagnetometerBase::Output& mag_data) const
{
    sensor_msgs::MagneticField mag_msg;
    mag_msg.magnetic_field.x = mag_data.magnetic_field_body.x();
    mag_msg.magnetic_field.y = mag_data.magnetic_field_body.y();
    mag_msg.magnetic_field.z = mag_data.magnetic_field_body.z();
    std::copy(std::begin(mag_data.magnetic_field_covariance),
              std::end(mag_data.magnetic_field_covariance),
              std::begin(mag_msg.magnetic_field_covariance));
    mag_msg.header.stamp = airsim_timestamp_to_ros(mag_data.time_stamp);

    return mag_msg;
}

// todo covariances
sensor_msgs::NavSatFix AirsimROSWrapper::get_gps_msg_from_airsim(const msr::airlib::GpsBase::Output& gps_data) const
{
    sensor_msgs::NavSatFix gps_msg;
    gps_msg.header.stamp = airsim_timestamp_to_ros(gps_data.time_stamp);
    gps_msg.latitude = gps_data.gnss.geo_point.latitude;
    gps_msg.longitude = gps_data.gnss.geo_point.longitude;
    gps_msg.altitude = gps_data.gnss.geo_point.altitude;
    gps_msg.status.service = sensor_msgs::NavSatStatus::SERVICE_GLONASS;
    gps_msg.status.status = gps_data.gnss.fix_type;
    // gps_msg.position_covariance_type =
    // gps_msg.position_covariance =

    return gps_msg;
}

sensor_msgs::Range AirsimROSWrapper::get_range_from_airsim(const msr::airlib::DistanceSensorData& dist_data) const
{
    sensor_msgs::Range dist_msg;
    dist_msg.header.stamp = airsim_timestamp_to_ros(dist_data.time_stamp);
    dist_msg.range = dist_data.distance;
    dist_msg.min_range = dist_data.min_distance;
    dist_msg.max_range = dist_data.min_distance;

    return dist_msg;
}

airsim_ros_pkgs::Altimeter AirsimROSWrapper::get_altimeter_msg_from_airsim(const msr::airlib::BarometerBase::Output& alt_data) const
{
    airsim_ros_pkgs::Altimeter alt_msg;
    alt_msg.header.stamp = airsim_timestamp_to_ros(alt_data.time_stamp);
    alt_msg.altitude = alt_data.altitude;
    alt_msg.pressure = alt_data.pressure;
    alt_msg.qnh = alt_data.qnh;

    return alt_msg;
}

// todo covariances
sensor_msgs::Imu AirsimROSWrapper::get_imu_msg_from_airsim(const msr::airlib::ImuBase::Output& imu_data) const
{
    sensor_msgs::Imu imu_msg;
    // imu_msg.header.frame_id = "/airsim/odom_local_ned";// todo multiple drones
    imu_msg.header.stamp = airsim_timestamp_to_ros(imu_data.time_stamp);
    imu_msg.orientation.x = imu_data.orientation.x();
    imu_msg.orientation.y = imu_data.orientation.y();
    imu_msg.orientation.z = imu_data.orientation.z();
    imu_msg.orientation.w = imu_data.orientation.w();

    // todo radians per second
    imu_msg.angular_velocity.x = imu_data.angular_velocity.x();
    imu_msg.angular_velocity.y = imu_data.angular_velocity.y();
    imu_msg.angular_velocity.z = imu_data.angular_velocity.z();

    // meters/s2^m
    imu_msg.linear_acceleration.x = imu_data.linear_acceleration.x();
    imu_msg.linear_acceleration.y = imu_data.linear_acceleration.y();
    imu_msg.linear_acceleration.z = imu_data.linear_acceleration.z();

    // imu_msg.orientation_covariance = ;
    // imu_msg.angular_velocity_covariance = ;
    // imu_msg.linear_acceleration_covariance = ;

    return imu_msg;
}

void AirsimROSWrapper::publish_odom_tf(const nav_msgs::Odometry& odom_msg)
{
    geometry_msgs::TransformStamped odom_tf;
    odom_tf.header = odom_msg.header;
    odom_tf.child_frame_id = odom_msg.child_frame_id;
    odom_tf.transform.translation.x = odom_msg.pose.pose.position.x;
    odom_tf.transform.translation.y = odom_msg.pose.pose.position.y;
    odom_tf.transform.translation.z = odom_msg.pose.pose.position.z;
    odom_tf.transform.rotation.x = odom_msg.pose.pose.orientation.x;
    odom_tf.transform.rotation.y = odom_msg.pose.pose.orientation.y;
    odom_tf.transform.rotation.z = odom_msg.pose.pose.orientation.z;
    odom_tf.transform.rotation.w = odom_msg.pose.pose.orientation.w;
    tf_broadcaster_.sendTransform(odom_tf);
}

airsim_ros_pkgs::GPSYaw AirsimROSWrapper::get_gps_msg_from_airsim_geo_point(const msr::airlib::GeoPoint& geo_point) const
{
    airsim_ros_pkgs::GPSYaw gps_msg;
    gps_msg.latitude = geo_point.latitude;
    gps_msg.longitude = geo_point.longitude;
    gps_msg.altitude = geo_point.altitude;
    return gps_msg;
}

sensor_msgs::NavSatFix AirsimROSWrapper::get_gps_sensor_msg_from_airsim_geo_point(const msr::airlib::GeoPoint& geo_point) const
{
    sensor_msgs::NavSatFix gps_msg;
    gps_msg.latitude = geo_point.latitude;
    gps_msg.longitude = geo_point.longitude;
    gps_msg.altitude = geo_point.altitude;
    return gps_msg;
}

ros::Time AirsimROSWrapper::chrono_timestamp_to_ros(const std::chrono::system_clock::time_point& stamp) const
{
    auto dur = std::chrono::duration<double>(stamp.time_since_epoch());
    ros::Time cur_time;
    cur_time.fromSec(dur.count());
    return cur_time;
}

ros::Time AirsimROSWrapper::airsim_timestamp_to_ros(const msr::airlib::TTimePoint& stamp) const
{
    // airsim appears to use chrono::system_clock with nanosecond precision
    std::chrono::nanoseconds dur(stamp);
    std::chrono::time_point<std::chrono::system_clock> tp(dur);
    ros::Time cur_time = chrono_timestamp_to_ros(tp);
    return cur_time;
}

void AirsimROSWrapper::drone_state_timer_cb(const ros::TimerEvent& event)
{
    try {
        // todo this is global origin
        origin_geo_point_pub_.publish(origin_geo_point_msg_);

        // get the basic vehicle pose and environmental state
        const auto now = update_state();

        // on init, will publish 0 to /clock as expected for use_sim_time compatibility
        if (!airsim_client_->simIsPaused()) {
            // airsim_client needs to provide the simulation time in a future version of the API
            ros_clock_.clock = now;
        }
        // publish the simulation clock
        if (publish_clock_) {
            clock_pub_.publish(ros_clock_);
        }

        // publish vehicle state, odom, and all basic sensor types
        publish_vehicle_state();

        // send any commands out to the vehicles
        update_commands();
    }
    catch (rpc::rpc_error& e) {
        std::cout << "error" << std::endl;
        std::string msg = e.get_error().as<std::string>();
        std::cout << "Exception raised by the API:" << std::endl
                  << msg << std::endl;
    }
}

void AirsimROSWrapper::update_and_publish_static_transforms(VehicleROS* vehicle_ros)
{
    if (vehicle_ros && !vehicle_ros->static_tf_msg_vec.empty()) {
        for (auto& static_tf_msg : vehicle_ros->static_tf_msg_vec) {
            static_tf_msg.header.stamp = vehicle_ros->stamp;
            static_tf_pub_.sendTransform(static_tf_msg);
        }
    }
}

ros::Time AirsimROSWrapper::update_state()
{
    bool got_sim_time = false;
    ros::Time curr_ros_time = ros::Time::now();

    //should be easier way to get the sim time through API, something like:
    //msr::airlib::Environment::State env = airsim_client_->simGetGroundTruthEnvironment("");
    //curr_ros_time = airsim_timestamp_to_ros(env.clock().nowNanos());

    // iterate over drones
    for (auto& vehicle_name_ptr_pair : vehicle_name_ptr_map_) {
        ros::Time vehicle_time;
        // get drone state from airsim
        auto& vehicle_ros = vehicle_name_ptr_pair.second;

        // vehicle environment, we can get ambient temperature here and other truths
        auto env_data = airsim_client_->simGetGroundTruthEnvironment(vehicle_ros->vehicle_name);

        if (airsim_mode_ == AIRSIM_MODE::DRONE) {
            auto drone = static_cast<MultiRotorROS*>(vehicle_ros.get());
            auto rpc = static_cast<msr::airlib::MultirotorRpcLibClient*>(airsim_client_.get());
            drone->curr_drone_state = rpc->getMultirotorState(vehicle_ros->vehicle_name);

            vehicle_time = airsim_timestamp_to_ros(drone->curr_drone_state.timestamp);
            if (!got_sim_time) {
                curr_ros_time = vehicle_time;
                got_sim_time = true;
            }

            vehicle_ros->gps_sensor_msg = get_gps_sensor_msg_from_airsim_geo_point(drone->curr_drone_state.gps_location);
            vehicle_ros->gps_sensor_msg.header.stamp = vehicle_time;

            vehicle_ros->curr_odom = get_odom_msg_from_multirotor_state(drone->curr_drone_state);
        }
        else {
            auto car = static_cast<CarROS*>(vehicle_ros.get());
            auto rpc = static_cast<msr::airlib::CarRpcLibClient*>(airsim_client_.get());
            car->curr_car_state = rpc->getCarState(vehicle_ros->vehicle_name);

            vehicle_time = airsim_timestamp_to_ros(car->curr_car_state.timestamp);
            if (!got_sim_time) {
                curr_ros_time = vehicle_time;
                got_sim_time = true;
            }

            vehicle_ros->gps_sensor_msg = get_gps_sensor_msg_from_airsim_geo_point(env_data.geo_point);
            vehicle_ros->gps_sensor_msg.header.stamp = vehicle_time;

            vehicle_ros->curr_odom = get_odom_msg_from_car_state(car->curr_car_state);

            airsim_ros_pkgs::CarState state_msg = get_roscarstate_msg_from_car_state(car->curr_car_state);
            state_msg.header.frame_id = vehicle_ros->vehicle_name;
            car->car_state_msg = state_msg;
        }

        vehicle_ros->stamp = vehicle_time;

        airsim_ros_pkgs::Environment env_msg = get_environment_msg_from_airsim(env_data);
        env_msg.header.frame_id = vehicle_ros->vehicle_name;
        env_msg.header.stamp = vehicle_time;
        vehicle_ros->env_msg = env_msg;

        // convert airsim drone state to ROS msgs
        vehicle_ros->curr_odom.header.frame_id = vehicle_ros->vehicle_name;
        vehicle_ros->curr_odom.child_frame_id = vehicle_ros->odom_frame_id;
        vehicle_ros->curr_odom.header.stamp = vehicle_time;
    }

    return curr_ros_time;
}

void AirsimROSWrapper::publish_vehicle_state()
{
    for (auto& vehicle_name_ptr_pair : vehicle_name_ptr_map_) {
        auto& vehicle_ros = vehicle_name_ptr_pair.second;

        // simulation environment truth
        vehicle_ros->env_pub.publish(vehicle_ros->env_msg);

        if (airsim_mode_ == AIRSIM_MODE::CAR) {
            // dashboard reading from car, RPM, gear, etc
            auto car = static_cast<CarROS*>(vehicle_ros.get());
            car->car_state_pub.publish(car->car_state_msg);
        }

        // odom and transforms
        vehicle_ros->odom_local_pub.publish(vehicle_ros->curr_odom);
        publish_odom_tf(vehicle_ros->curr_odom);

        // ground truth GPS position from sim/HITL
        vehicle_ros->global_gps_pub.publish(vehicle_ros->gps_sensor_msg);

        for (auto& sensor_publisher : vehicle_ros->sensor_pubs) {
            switch (sensor_publisher.sensor_type) {
            case SensorBase::SensorType::Barometer: {
                auto baro_data = airsim_client_->getBarometerData(sensor_publisher.sensor_name, vehicle_ros->vehicle_name);
                airsim_ros_pkgs::Altimeter alt_msg = get_altimeter_msg_from_airsim(baro_data);
                alt_msg.header.frame_id = vehicle_ros->vehicle_name;
                sensor_publisher.publisher.publish(alt_msg);
                break;
            }
            case SensorBase::SensorType::Imu: {
                auto imu_data = airsim_client_->getImuData(sensor_publisher.sensor_name, vehicle_ros->vehicle_name);
                sensor_msgs::Imu imu_msg = get_imu_msg_from_airsim(imu_data);
                imu_msg.header.frame_id = vehicle_ros->vehicle_name;
                sensor_publisher.publisher.publish(imu_msg);
                break;
            }
            case SensorBase::SensorType::Distance: {
                auto distance_data = airsim_client_->getDistanceSensorData(sensor_publisher.sensor_name, vehicle_ros->vehicle_name);
                sensor_msgs::Range dist_msg = get_range_from_airsim(distance_data);
                dist_msg.header.frame_id = vehicle_ros->vehicle_name;
                sensor_publisher.publisher.publish(dist_msg);
                break;
            }
            case SensorBase::SensorType::Gps: {
                auto gps_data = airsim_client_->getGpsData(sensor_publisher.sensor_name, vehicle_ros->vehicle_name);
                sensor_msgs::NavSatFix gps_msg = get_gps_msg_from_airsim(gps_data);
                gps_msg.header.frame_id = vehicle_ros->vehicle_name;
                sensor_publisher.publisher.publish(gps_msg);
                break;
            }
            case SensorBase::SensorType::Lidar: {
                // handled via callback
                break;
            }
            case SensorBase::SensorType::Magnetometer: {
                auto mag_data = airsim_client_->getMagnetometerData(sensor_publisher.sensor_name, vehicle_ros->vehicle_name);
                sensor_msgs::MagneticField mag_msg = get_mag_msg_from_airsim(mag_data);
                mag_msg.header.frame_id = vehicle_ros->vehicle_name;
                sensor_publisher.publisher.publish(mag_msg);
                break;
            }
            }
        }

        update_and_publish_static_transforms(vehicle_ros.get());
    }
}

void AirsimROSWrapper::update_commands()
{
    for (auto& vehicle_name_ptr_pair : vehicle_name_ptr_map_) {
        auto& vehicle_ros = vehicle_name_ptr_pair.second;

        if (airsim_mode_ == AIRSIM_MODE::DRONE) {
            auto drone = static_cast<MultiRotorROS*>(vehicle_ros.get());

            // send control commands from the last callback to airsim
            if (drone->has_vel_cmd) {
                std::lock_guard<std::mutex> guard(drone_control_mutex_);
                static_cast<msr::airlib::MultirotorRpcLibClient*>(airsim_client_.get())->moveByVelocityAsync(drone->vel_cmd.x, drone->vel_cmd.y, drone->vel_cmd.z, vel_cmd_duration_, msr::airlib::DrivetrainType::MaxDegreeOfFreedom, drone->vel_cmd.yaw_mode, drone->vehicle_name);
            }
            drone->has_vel_cmd = false;
        }
        else {
            // send control commands from the last callback to airsim
            auto car = static_cast<CarROS*>(vehicle_ros.get());
            if (car->has_car_cmd) {
                std::lock_guard<std::mutex> guard(drone_control_mutex_);
                static_cast<msr::airlib::CarRpcLibClient*>(airsim_client_.get())->setCarControls(car->car_cmd, vehicle_ros->vehicle_name);
            }
            car->has_car_cmd = false;
        }
    }

    // Only camera rotation, no translation movement of camera
    if (has_gimbal_cmd_) {
        std::lock_guard<std::mutex> guard(drone_control_mutex_);
        airsim_client_->simSetCameraPose(gimbal_cmd_.camera_name, get_airlib_pose(0, 0, 0, gimbal_cmd_.target_quat), gimbal_cmd_.vehicle_name);
    }

    has_gimbal_cmd_ = false;
}

// airsim uses nans for zeros in settings.json. we set them to zeros here for handling tfs in ROS
void AirsimROSWrapper::set_nans_to_zeros_in_pose(VehicleSetting& vehicle_setting) const
{
    if (std::isnan(vehicle_setting.position.x()))
        vehicle_setting.position.x() = 0.0;

    if (std::isnan(vehicle_setting.position.y()))
        vehicle_setting.position.y() = 0.0;

    if (std::isnan(vehicle_setting.position.z()))
        vehicle_setting.position.z() = 0.0;

    if (std::isnan(vehicle_setting.rotation.yaw))
        vehicle_setting.rotation.yaw = 0.0;

    if (std::isnan(vehicle_setting.rotation.pitch))
        vehicle_setting.rotation.pitch = 0.0;

    if (std::isnan(vehicle_setting.rotation.roll))
        vehicle_setting.rotation.roll = 0.0;
}

// if any nan's in camera pose, set them to match vehicle pose (which has already converted any potential nans to zeros)
void AirsimROSWrapper::set_nans_to_zeros_in_pose(const VehicleSetting& vehicle_setting, CameraSetting& camera_setting) const
{
    if (std::isnan(camera_setting.position.x()))
        camera_setting.position.x() = vehicle_setting.position.x();

    if (std::isnan(camera_setting.position.y()))
        camera_setting.position.y() = vehicle_setting.position.y();

    if (std::isnan(camera_setting.position.z()))
        camera_setting.position.z() = vehicle_setting.position.z();

    if (std::isnan(camera_setting.rotation.yaw))
        camera_setting.rotation.yaw = vehicle_setting.rotation.yaw;

    if (std::isnan(camera_setting.rotation.pitch))
        camera_setting.rotation.pitch = vehicle_setting.rotation.pitch;

    if (std::isnan(camera_setting.rotation.roll))
        camera_setting.rotation.roll = vehicle_setting.rotation.roll;
}

void AirsimROSWrapper::append_static_vehicle_tf(VehicleROS* vehicle_ros, const VehicleSetting& vehicle_setting)
{
    geometry_msgs::TransformStamped vehicle_tf_msg;
    vehicle_tf_msg.header.frame_id = world_frame_id_;
    vehicle_tf_msg.header.stamp = ros::Time::now();
    vehicle_tf_msg.child_frame_id = vehicle_ros->vehicle_name;
    vehicle_tf_msg.transform.translation.x = vehicle_setting.position.x();
    vehicle_tf_msg.transform.translation.y = vehicle_setting.position.y();
    vehicle_tf_msg.transform.translation.z = vehicle_setting.position.z();
    tf2::Quaternion quat;
    quat.setRPY(vehicle_setting.rotation.roll, vehicle_setting.rotation.pitch, vehicle_setting.rotation.yaw);
    vehicle_tf_msg.transform.rotation.x = quat.x();
    vehicle_tf_msg.transform.rotation.y = quat.y();
    vehicle_tf_msg.transform.rotation.z = quat.z();
    vehicle_tf_msg.transform.rotation.w = quat.w();

    if (isENU_) {
        std::swap(vehicle_tf_msg.transform.translation.x, vehicle_tf_msg.transform.translation.y);
        std::swap(vehicle_tf_msg.transform.rotation.x, vehicle_tf_msg.transform.rotation.y);
        vehicle_tf_msg.transform.translation.z = -vehicle_tf_msg.transform.translation.z;
        vehicle_tf_msg.transform.rotation.z = -vehicle_tf_msg.transform.rotation.z;
    }

    vehicle_ros->static_tf_msg_vec.emplace_back(vehicle_tf_msg);
}

void AirsimROSWrapper::append_static_lidar_tf(VehicleROS* vehicle_ros, const std::string& lidar_name, const msr::airlib::LidarSimpleParams& lidar_setting)
{
    geometry_msgs::TransformStamped lidar_tf_msg;
    lidar_tf_msg.header.frame_id = vehicle_ros->vehicle_name + "/" + odom_frame_id_;
    lidar_tf_msg.child_frame_id = vehicle_ros->vehicle_name + "/" + lidar_name;
    lidar_tf_msg.transform.translation.x = lidar_setting.relative_pose.position.x();
    lidar_tf_msg.transform.translation.y = lidar_setting.relative_pose.position.y();
    lidar_tf_msg.transform.translation.z = lidar_setting.relative_pose.position.z();
    lidar_tf_msg.transform.rotation.x = lidar_setting.relative_pose.orientation.x();
    lidar_tf_msg.transform.rotation.y = lidar_setting.relative_pose.orientation.y();
    lidar_tf_msg.transform.rotation.z = lidar_setting.relative_pose.orientation.z();
    lidar_tf_msg.transform.rotation.w = lidar_setting.relative_pose.orientation.w();

    if (isENU_) {
        std::swap(lidar_tf_msg.transform.translation.x, lidar_tf_msg.transform.translation.y);
        std::swap(lidar_tf_msg.transform.rotation.x, lidar_tf_msg.transform.rotation.y);
        lidar_tf_msg.transform.translation.z = -lidar_tf_msg.transform.translation.z;
        lidar_tf_msg.transform.rotation.z = -lidar_tf_msg.transform.rotation.z;
    }

    vehicle_ros->static_tf_msg_vec.emplace_back(lidar_tf_msg);
}

void AirsimROSWrapper::append_static_camera_tf(VehicleROS* vehicle_ros, const std::string& camera_name, const CameraSetting& camera_setting)
{
    geometry_msgs::TransformStamped static_cam_tf_body_msg;
    static_cam_tf_body_msg.header.frame_id = vehicle_ros->vehicle_name + "/" + odom_frame_id_;
    static_cam_tf_body_msg.child_frame_id = camera_name + "_body/static";
    static_cam_tf_body_msg.transform.translation.x = camera_setting.position.x();
    static_cam_tf_body_msg.transform.translation.y = camera_setting.position.y();
    static_cam_tf_body_msg.transform.translation.z = camera_setting.position.z();
    tf2::Quaternion quat;
    quat.setRPY(camera_setting.rotation.roll, camera_setting.rotation.pitch, camera_setting.rotation.yaw);
    static_cam_tf_body_msg.transform.rotation.x = quat.x();
    static_cam_tf_body_msg.transform.rotation.y = quat.y();
    static_cam_tf_body_msg.transform.rotation.z = quat.z();
    static_cam_tf_body_msg.transform.rotation.w = quat.w();

    if (isENU_) {
        std::swap(static_cam_tf_body_msg.transform.translation.x, static_cam_tf_body_msg.transform.translation.y);
        std::swap(static_cam_tf_body_msg.transform.rotation.x, static_cam_tf_body_msg.transform.rotation.y);
        static_cam_tf_body_msg.transform.translation.z = -static_cam_tf_body_msg.transform.translation.z;
        static_cam_tf_body_msg.transform.rotation.z = -static_cam_tf_body_msg.transform.rotation.z;
    }

    geometry_msgs::TransformStamped static_cam_tf_optical_msg = static_cam_tf_body_msg;
    static_cam_tf_optical_msg.child_frame_id = camera_name + "_optical/static";

    tf2::Quaternion quat_cam_body;
    tf2::Quaternion quat_cam_optical;
    tf2::convert(static_cam_tf_body_msg.transform.rotation, quat_cam_body);
    tf2::Matrix3x3 mat_cam_body(quat_cam_body);
    tf2::Matrix3x3 mat_cam_optical;
    mat_cam_optical.setValue(mat_cam_body.getColumn(1).getX(), mat_cam_body.getColumn(2).getX(), mat_cam_body.getColumn(0).getX(), mat_cam_body.getColumn(1).getY(), mat_cam_body.getColumn(2).getY(), mat_cam_body.getColumn(0).getY(), mat_cam_body.getColumn(1).getZ(), mat_cam_body.getColumn(2).getZ(), mat_cam_body.getColumn(0).getZ());
    mat_cam_optical.getRotation(quat_cam_optical);
    quat_cam_optical.normalize();
    tf2::convert(quat_cam_optical, static_cam_tf_optical_msg.transform.rotation);

    vehicle_ros->static_tf_msg_vec.emplace_back(static_cam_tf_body_msg);
    vehicle_ros->static_tf_msg_vec.emplace_back(static_cam_tf_optical_msg);
}

void AirsimROSWrapper::img_response_timer_cb(const ros::TimerEvent& event)
{
    try {
        int image_response_idx = 0;
        for (const auto& airsim_img_request_vehicle_name_pair : airsim_img_request_vehicle_name_pair_vec_) {
            const std::vector<ImageResponse>& img_response = airsim_client_images_.simGetImages(airsim_img_request_vehicle_name_pair.first, airsim_img_request_vehicle_name_pair.second);

            if (img_response.size() == airsim_img_request_vehicle_name_pair.first.size()) {
                process_and_publish_img_response(img_response, image_response_idx, airsim_img_request_vehicle_name_pair.second);
                image_response_idx += img_response.size();
            }
        }
    }

    catch (rpc::rpc_error& e) {
        std::string msg = e.get_error().as<std::string>();
        std::cout << "Exception raised by the API, didn't get image response." << std::endl
                  << msg << std::endl;
    }
}

void AirsimROSWrapper::lidar_timer_cb(const ros::TimerEvent& event)
{
    try {
        for (auto& vehicle_name_ptr_pair : vehicle_name_ptr_map_) {
            if (!vehicle_name_ptr_pair.second->lidar_pubs.empty()) {
                for (auto& lidar_publisher : vehicle_name_ptr_pair.second->lidar_pubs) {
                    auto lidar_data = airsim_client_lidar_.getLidarData(lidar_publisher.sensor_name, vehicle_name_ptr_pair.first);
                    sensor_msgs::PointCloud2 lidar_msg = get_lidar_msg_from_airsim(lidar_data, vehicle_name_ptr_pair.first);
                    lidar_publisher.publisher.publish(lidar_msg);
                }
            }
        }
    }
    catch (rpc::rpc_error& e) {
        std::string msg = e.get_error().as<std::string>();
        std::cout << "Exception raised by the API, didn't get image response." << std::endl
                  << msg << std::endl;
    }
}

cv::Mat AirsimROSWrapper::manual_decode_depth(const ImageResponse& img_response) const
{
    cv::Mat mat(img_response.height, img_response.width, CV_32FC1, cv::Scalar(0));
    int img_width = img_response.width;

    for (int row = 0; row < img_response.height; row++)
        for (int col = 0; col < img_width; col++)
            mat.at<float>(row, col) = img_response.image_data_float[row * img_width + col];
    return mat;
}

sensor_msgs::ImagePtr AirsimROSWrapper::get_img_msg_from_response(const ImageResponse& img_response,
                                                                  const ros::Time curr_ros_time,
                                                                  const std::string frame_id)
{
    sensor_msgs::ImagePtr img_msg_ptr = boost::make_shared<sensor_msgs::Image>();
    img_msg_ptr->data = img_response.image_data_uint8;
    img_msg_ptr->step = img_response.width * 3; // todo un-hardcode. image_width*num_bytes
    img_msg_ptr->header.stamp = airsim_timestamp_to_ros(img_response.time_stamp);
    img_msg_ptr->header.frame_id = frame_id;
    img_msg_ptr->height = img_response.height;
    img_msg_ptr->width = img_response.width;
    img_msg_ptr->encoding = "bgr8";
    if (is_vulkan_)
        img_msg_ptr->encoding = "rgb8";
    img_msg_ptr->is_bigendian = 0;
    return img_msg_ptr;
}

sensor_msgs::ImagePtr AirsimROSWrapper::get_depth_img_msg_from_response(const ImageResponse& img_response,
                                                                        const ros::Time curr_ros_time,
                                                                        const std::string frame_id)
{
    // todo using img_response.image_data_float direclty as done get_img_msg_from_response() throws an error,
    // hence the dependency on opencv and cv_bridge. however, this is an extremely fast op, so no big deal.
    cv::Mat depth_img = manual_decode_depth(img_response);
    sensor_msgs::ImagePtr depth_img_msg = cv_bridge::CvImage(std_msgs::Header(), "32FC1", depth_img).toImageMsg();
    depth_img_msg->header.stamp = airsim_timestamp_to_ros(img_response.time_stamp);
    depth_img_msg->header.frame_id = frame_id;
    return depth_img_msg;
}

// todo have a special stereo pair mode and get projection matrix by calculating offset wrt drone body frame?
sensor_msgs::CameraInfo AirsimROSWrapper::generate_cam_info(const std::string& camera_name,
                                                            const CameraSetting& camera_setting,
                                                            const CaptureSetting& capture_setting) const
{
    sensor_msgs::CameraInfo cam_info_msg;
    cam_info_msg.header.frame_id = camera_name + "_optical";
    cam_info_msg.height = capture_setting.height;
    cam_info_msg.width = capture_setting.width;
    float f_x = (capture_setting.width / 2.0) / tan(math_common::deg2rad(capture_setting.fov_degrees / 2.0));
    // todo focal length in Y direction should be same as X it seems. this can change in future a scene capture component which exactly correponds to a cine camera
    // float f_y = (capture_setting.height / 2.0) / tan(math_common::deg2rad(fov_degrees / 2.0));
    cam_info_msg.K = { f_x, 0.0, capture_setting.width / 2.0, 0.0, f_x, capture_setting.height / 2.0, 0.0, 0.0, 1.0 };
    cam_info_msg.P = { f_x, 0.0, capture_setting.width / 2.0, 0.0, 0.0, f_x, capture_setting.height / 2.0, 0.0, 0.0, 0.0, 1.0, 0.0 };
    return cam_info_msg;
}

void AirsimROSWrapper::process_and_publish_img_response(const std::vector<ImageResponse>& img_response_vec, const int img_response_idx, const std::string& vehicle_name)
{
    // todo add option to use airsim time (image_response.TTimePoint) like Gazebo /use_sim_time param
    ros::Time curr_ros_time = ros::Time::now();
    int img_response_idx_internal = img_response_idx;

    for (const auto& curr_img_response : img_response_vec) {
        // todo publishing a tf for each capture type seems stupid. but it foolproofs us against render thread's async stuff, I hope.
        // Ideally, we should loop over cameras and then captures, and publish only one tf.
        publish_camera_tf(curr_img_response, curr_ros_time, vehicle_name, curr_img_response.camera_name);

        // todo simGetCameraInfo is wrong + also it's only for image type -1.
        // msr::airlib::CameraInfo camera_info = airsim_client_.simGetCameraInfo(curr_img_response.camera_name);

        // update timestamp of saved cam info msgs
        camera_info_msg_vec_[img_response_idx_internal].header.stamp = curr_ros_time;
        cam_info_pub_vec_[img_response_idx_internal].publish(camera_info_msg_vec_[img_response_idx_internal]);

        // DepthPlanar / DepthPerspective / DepthVis / DisparityNormalized
        if (curr_img_response.pixels_as_float) {
            image_pub_vec_[img_response_idx_internal].publish(get_depth_img_msg_from_response(curr_img_response,
                                                                                              curr_ros_time,
                                                                                              curr_img_response.camera_name + "_optical"));
        }
        // Scene / Segmentation / SurfaceNormals / Infrared
        else {
            image_pub_vec_[img_response_idx_internal].publish(get_img_msg_from_response(curr_img_response,
                                                                                        curr_ros_time,
                                                                                        curr_img_response.camera_name + "_optical"));
        }
        img_response_idx_internal++;
    }
}

// publish camera transforms
// camera poses are obtained from airsim's client API which are in (local) NED frame.
// We first do a change of basis to camera optical frame (Z forward, X right, Y down)
void AirsimROSWrapper::publish_camera_tf(const ImageResponse& img_response, const ros::Time& ros_time, const std::string& frame_id, const std::string& child_frame_id)
{
    geometry_msgs::TransformStamped cam_tf_body_msg;
    cam_tf_body_msg.header.stamp = ros_time;
    cam_tf_body_msg.header.frame_id = frame_id;
    cam_tf_body_msg.child_frame_id = child_frame_id + "_body";
    cam_tf_body_msg.transform.translation.x = img_response.camera_position.x();
    cam_tf_body_msg.transform.translation.y = img_response.camera_position.y();
    cam_tf_body_msg.transform.translation.z = img_response.camera_position.z();
    cam_tf_body_msg.transform.rotation.x = img_response.camera_orientation.x();
    cam_tf_body_msg.transform.rotation.y = img_response.camera_orientation.y();
    cam_tf_body_msg.transform.rotation.z = img_response.camera_orientation.z();
    cam_tf_body_msg.transform.rotation.w = img_response.camera_orientation.w();

    if (isENU_) {
        std::swap(cam_tf_body_msg.transform.translation.x, cam_tf_body_msg.transform.translation.y);
        std::swap(cam_tf_body_msg.transform.rotation.x, cam_tf_body_msg.transform.rotation.y);
        cam_tf_body_msg.transform.translation.z = -cam_tf_body_msg.transform.translation.z;
        cam_tf_body_msg.transform.rotation.z = -cam_tf_body_msg.transform.rotation.z;
    }

    geometry_msgs::TransformStamped cam_tf_optical_msg;
    cam_tf_optical_msg.header.stamp = ros_time;
    cam_tf_optical_msg.header.frame_id = frame_id;
    cam_tf_optical_msg.child_frame_id = child_frame_id + "_optical";
    cam_tf_optical_msg.transform.translation.x = cam_tf_body_msg.transform.translation.x;
    cam_tf_optical_msg.transform.translation.y = cam_tf_body_msg.transform.translation.y;
    cam_tf_optical_msg.transform.translation.z = cam_tf_body_msg.transform.translation.z;

    tf2::Quaternion quat_cam_body;
    tf2::Quaternion quat_cam_optical;
    tf2::convert(cam_tf_body_msg.transform.rotation, quat_cam_body);
    tf2::Matrix3x3 mat_cam_body(quat_cam_body);
    // tf2::Matrix3x3 mat_cam_optical = matrix_cam_body_to_optical_ * mat_cam_body * matrix_cam_body_to_optical_inverse_;
    // tf2::Matrix3x3 mat_cam_optical = matrix_cam_body_to_optical_ * mat_cam_body;
    tf2::Matrix3x3 mat_cam_optical;
    mat_cam_optical.setValue(mat_cam_body.getColumn(1).getX(), mat_cam_body.getColumn(2).getX(), mat_cam_body.getColumn(0).getX(), mat_cam_body.getColumn(1).getY(), mat_cam_body.getColumn(2).getY(), mat_cam_body.getColumn(0).getY(), mat_cam_body.getColumn(1).getZ(), mat_cam_body.getColumn(2).getZ(), mat_cam_body.getColumn(0).getZ());
    mat_cam_optical.getRotation(quat_cam_optical);
    quat_cam_optical.normalize();
    tf2::convert(quat_cam_optical, cam_tf_optical_msg.transform.rotation);

    tf_broadcaster_.sendTransform(cam_tf_body_msg);
    tf_broadcaster_.sendTransform(cam_tf_optical_msg);
}

void AirsimROSWrapper::convert_yaml_to_simple_mat(const YAML::Node& node, SimpleMatrix& m) const
{
    int rows, cols;
    rows = node["rows"].as<int>();
    cols = node["cols"].as<int>();
    const YAML::Node& data = node["data"];
    for (int i = 0; i < rows * cols; ++i) {
        m.data[i] = data[i].as<double>();
    }
}

void AirsimROSWrapper::read_params_from_yaml_and_fill_cam_info_msg(const std::string& file_name, sensor_msgs::CameraInfo& cam_info) const
{
    std::ifstream fin(file_name.c_str());
    YAML::Node doc = YAML::Load(fin);

    cam_info.width = doc[WIDTH_YML_NAME].as<int>();
    cam_info.height = doc[HEIGHT_YML_NAME].as<int>();

    SimpleMatrix K_(3, 3, &cam_info.K[0]);
    convert_yaml_to_simple_mat(doc[K_YML_NAME], K_);
    SimpleMatrix R_(3, 3, &cam_info.R[0]);
    convert_yaml_to_simple_mat(doc[R_YML_NAME], R_);
    SimpleMatrix P_(3, 4, &cam_info.P[0]);
    convert_yaml_to_simple_mat(doc[P_YML_NAME], P_);

    cam_info.distortion_model = doc[DMODEL_YML_NAME].as<std::string>();

    const YAML::Node& D_node = doc[D_YML_NAME];
    int D_rows, D_cols;
    D_rows = D_node["rows"].as<int>();
    D_cols = D_node["cols"].as<int>();
    const YAML::Node& D_data = D_node["data"];
    cam_info.D.resize(D_rows * D_cols);
    for (int i = 0; i < D_rows * D_cols; ++i) {
        cam_info.D[i] = D_data[i].as<float>();
    }
}