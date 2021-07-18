# airsim_ros_pkgs

A ROS wrapper over the AirSim C++ client library. 

##  Setup 
- Install gcc >= 8.0.0: `sudo apt-get install gcc-8 g++-8`  
Verify installation by `gcc-8 --version`

- Ubuntu 16.04
    * Install [ROS kinetic](https://wiki.ros.org/kinetic/Installation/Ubuntu)
    * Install tf2 sensor and mavros packages: `sudo apt-get install ros-kinetic-tf2-sensor-msgs ros-kinetic-tf2-geometry-msgs ros-kinetic-mavros*`

- Ubuntu 18.04
    * Install [ROS melodic](https://wiki.ros.org/melodic/Installation/Ubuntu)
    * Install tf2 sensor and mavros packages: `sudo apt-get install ros-melodic-tf2-sensor-msgs ros-melodic-tf2-geometry-msgs ros-melodic-mavros*`
- Ubuntu 20.04
    * Install [ROS noetic](https://wiki.ros.org/noetic/Installation/Ubuntu)
    * Install tf2 sensor and mavros packages: `sudo apt-get install ros-melodic-tf2-sensor-msgs ros-melodic-tf2-geometry-msgs ros-melodic-mavros*`

- Install [catkin_tools](https://catkin-tools.readthedocs.io/en/latest/installing.html)
    `sudo apt-get install python-catkin-tools` or
    `pip install catkin_tools`. If using Ubuntu 20.04 use `pip install "git+https://github.com/catkin/catkin_tools.git#egg=catkin_tools"`

##  Build
- Build AirSim 
```
git clone https://github.com/Microsoft/AirSim.git;
cd AirSim;
./setup.sh;
./build.sh;
```
- Build ROS package

```
cd ros;
catkin build; # or catkin_make
```

If your default GCC isn't 8 or greater (check using `gcc --version`), then compilation will fail. In that case, use `gcc-8` explicitly as follows-

```
catkin build -DCMAKE_C_COMPILER=gcc-8 -DCMAKE_CXX_COMPILER=g++-8
```

## Running
```
source devel/setup.bash;
roslaunch airsim_ros_pkgs airsim_node.launch;
roslaunch airsim_ros_pkgs rviz.launch;
```
   **Note**: If you get an error running `roslaunch airsim_ros_pkgs airsim_node.launch`, run `catkin clean` and try again

# Using AirSim ROS wrapper
The ROS wrapper is composed of two ROS nodes - the first is a wrapper over AirSim's multirotor C++ client library, and the second is a simple PD position controller.    
Let's look at the ROS API for both nodes: 

### AirSim ROS Wrapper Node
#### Publishers:
- `/airsim_node/origin_geo_point` [airsim_ros_pkgs/GPSYaw](https://github.com/microsoft/AirSim/tree/master/ros/src/airsim_ros_pkgs/msg/GPSYaw.msg)   
GPS coordinates corresponding to global NED frame. This is set in the airsim's [settings.json](https://microsoft.github.io/AirSim/settings/) file under the `OriginGeopoint` key. 
  
- `/airsim_node/VEHICLE_NAME/global_gps` [sensor_msgs/NavSatFix](https://docs.ros.org/api/sensor_msgs/html/msg/NavSatFix.html)   
This the current GPS coordinates of the drone in airsim. 

- `/airsim_node/VEHICLE_NAME/odom_local_ned` [nav_msgs/Odometry](https://docs.ros.org/api/nav_msgs/html/msg/Odometry.html)   
Odometry in NED frame (default name: odom_local_ned, launch name and frame type are configurable) wrt take-off point.  
 
- `/airsim_node/VEHICLE_NAME/CAMERA_NAME/IMAGE_TYPE/camera_info` [sensor_msgs/CameraInfo](https://docs.ros.org/api/sensor_msgs/html/msg/CameraInfo.html)

- `/airsim_node/VEHICLE_NAME/CAMERA_NAME/IMAGE_TYPE` [sensor_msgs/Image](https://docs.ros.org/api/sensor_msgs/html/msg/Image.html)   
  RGB or float image depending on image type requested in settings.json.

- `/tf` [tf2_msgs/TFMessage](https://docs.ros.org/api/tf2_msgs/html/msg/TFMessage.html)

- `/airsim_node/VEHICLE_NAME/altimeter/SENSOR_NAME` [airsim_ros_pkgs/Altimeter](https://github.com/microsoft/AirSim/blob/master/ros/src/airsim_ros_pkgs/msg/Altimeter.msg)  
This the current altimeter reading for altitude, pressure, and [QNH](https://en.wikipedia.org/wiki/QNH)
  
- `/airsim_node/VEHICLE_NAME/imu/SENSOR_NAME` [sensor_msgs::Imu](http://docs.ros.org/api/sensor_msgs/html/msg/Imu.html)  
IMU sensor data

- `/airsim_node/VEHICLE_NAME/magnetometer/SENSOR_NAME` [sensor_msgs::MagneticField](http://docs.ros.org/api/sensor_msgs/html/msg/MagneticField.html)  
  Meausrement of magnetic field vector/compass

- `/airsim_node/VEHICLE_NAME/distance/SENSOR_NAME` [sensor_msgs::Range](http://docs.ros.org/api/sensor_msgs/html/msg/Range.html)  
  Meausrement of distance from an active ranger, such as infrared or IR

- `/airsim_node/VEHICLE_NAME/lidar/SENSOR_NAME` [sensor_msgs::PointCloud2](http://docs.ros.org/api/sensor_msgs/html/msg/PointCloud2.html)  
  LIDAR pointcloud

#### Subscribers: 
- `/airsim_node/vel_cmd_body_frame` [airsim_ros_pkgs/VelCmd](https://github.com/microsoft/AirSim/tree/master/ros/src/airsim_ros_pkgs/msg/VelCmd.msg)    
  Ignore `vehicle_name` field, leave it to blank. We will use `vehicle_name` in future for multiple drones.

- `/airsim_node/vel_cmd_world_frame` [airsim_ros_pkgs/VelCmd](https://github.com/microsoft/AirSim/tree/master/ros/src/airsim_ros_pkgs/msg/VelCmd.msg)    
  Ignore `vehicle_name` field, leave it to blank. We will use `vehicle_name` in future for multiple drones.

- `/gimbal_angle_euler_cmd` [airsim_ros_pkgs/GimbalAngleEulerCmd](https://github.com/microsoft/AirSim/tree/master/ros/src/airsim_ros_pkgs/msg/GimbalAngleEulerCmd.msg)   
  Gimbal set point in euler angles.    

- `/gimbal_angle_quat_cmd` [airsim_ros_pkgs/GimbalAngleQuatCmd](https://github.com/microsoft/AirSim/tree/master/ros/src/airsim_ros_pkgs/msg/GimbalAngleQuatCmd.msg)   
  Gimbal set point in quaternion.    

- `/airsim_node/VEHICLE_NAME/car_cmd` [airsim_ros_pkgs/CarControls](https://github.com/microsoft/AirSim/blob/master/ros/src/airsim_ros_pkgs/msg/CarControls.msg)  
Throttle, brake, steering and gear selections for control. Both automatic and manual transmission control possible, see the [`car_joy.py`](https://github.com/microsoft/AirSim/blob/master/ros/src/airsim_ros_pkgs/scripts/car_joy) script for use.

#### Services:
- `/airsim_node/VEHICLE_NAME/land` [airsim_ros_pkgs/Takeoff](https://docs.ros.org/api/std_srvs/html/srv/Empty.html)

- `/airsim_node/takeoff` [airsim_ros_pkgs/Takeoff](https://docs.ros.org/api/std_srvs/html/srv/Empty.html)

- `/airsim_node/reset` [airsim_ros_pkgs/Reset](https://docs.ros.org/api/std_srvs/html/srv/Empty.html)
 Resets *all* drones

#### Parameters:
- `/airsim_node/world_frame_id` [string]   
  Set in: `$(airsim_ros_pkgs)/launch/airsim_node.launch`   
  Default: world_ned   
  Set to "world_enu" to switch to ENU frames automatically

- `/airsim_node/odom_frame_id` [string]   
  Set in: `$(airsim_ros_pkgs)/launch/airsim_node.launch`   
  Default: odom_local_ned   
  If you set world_frame_id to "world_enu", the default odom name will instead default to "odom_local_enu"

- `/airsim_node/coordinate_system_enu` [boolean]   
  Set in: `$(airsim_ros_pkgs)/launch/airsim_node.launch`   
  Default: false   
  If you set world_frame_id to "world_enu", this setting will instead default to true

- `/airsim_node/update_airsim_control_every_n_sec` [double]   
  Set in: `$(airsim_ros_pkgs)/launch/airsim_node.launch`   
  Default: 0.01 seconds.   
  Timer callback frequency for updating drone odom and state from airsim, and sending in control commands.    
  The current RPClib interface to unreal engine maxes out at 50 Hz.   
  Timer callbacks in ROS run at maximum rate possible, so it's best to not touch this parameter.

- `/airsim_node/update_airsim_img_response_every_n_sec` [double]   
  Set in: `$(airsim_ros_pkgs)/launch/airsim_node.launch`   
  Default: 0.01 seconds.   
  Timer callback frequency for receiving images from all cameras in airsim.    
  The speed will depend on number of images requested and their resolution.   
  Timer callbacks in ROS run at maximum rate possible, so it's best to not touch this parameter. 

- `/airsim_node/publish_clock` [double]   
  Set in: `$(airsim_ros_pkgs)/launch/airsim_node.launch`   
  Default: false   
  Will publish the ros /clock topic if set to true.    

### Simple PID Position Controller Node 

#### Build with your configuration

AirSim's ROS nodes are still being developed; e.g., in [`pd_position_controller_simple.cpp`](https://github.com/microsoft/AirSim/blob/master/ros/src/airsim_ros_pkgs/src/pd_position_controller_simple.cpp).  
To make this ROS node work, you need to replace this line:
```
airsim_odom_sub_ = nh_.subscribe("/airsim_node/odom_local_ned", 50, &PIDPositionController::airsim_odom_cb, this);
```
With line
```
airsim_odom_sub_ = nh_.subscribe("/airsim_node/NAME/odom_local_ned", 50, &PIDPositionController::airsim_odom_cb, this);
```
Where `NAME` is the name of the drone in the settings.json file.  
Otherwise you would get `[ERROR] [1234567890.1234567890]: [PIDPositionController] Waiting for odometry!`.

#### Parameters:
- PD controller parameters:
  * `/pd_position_node/kd_x` [double],   
    `/pd_position_node/kp_y` [double],   
    `/pd_position_node/kp_z` [double],   
    `/pd_position_node/kp_yaw` [double]   
    Proportional gains

  * `/pd_position_node/kd_x` [double],   
    `/pd_position_node/kd_y` [double],   
    `/pd_position_node/kd_z` [double],   
    `/pd_position_node/kd_yaw` [double]   
    Derivative gains

  * `/pd_position_node/reached_thresh_xyz` [double]   
    Threshold euler distance (meters) from current position to setpoint position 

  * `/pd_position_node/reached_yaw_degrees` [double]   
    Threshold yaw distance (degrees) from current position to setpoint position 

- `/pd_position_node/update_control_every_n_sec` [double]   
  Default: 0.01 seconds

#### Services:
- `/airsim_node/VEHICLE_NAME/gps_goal` [Request: [srv/SetGPSPosition](https://github.com/microsoft/AirSim/blob/master/ros/src/airsim_ros_pkgs/srv/SetGPSPosition.srv)]   
  Target gps position + yaw.   
  In **absolute** altitude. 

- `/airsim_node/VEHICLE_NAME/local_position_goal` [Request: [srv/SetLocalPosition](https://github.com/microsoft/AirSim/blob/master/ros/src/airsim_ros_pkgs/srv/SetLocalPosition.srv)]   
  Target local position + yaw in global NED frame.   

#### Subscribers:
- `/airsim_node/origin_geo_point` [airsim_ros_pkgs/GPSYaw](https://github.com/microsoft/AirSim/tree/master/ros/src/airsim_ros_pkgs/msg/GPSYaw.msg)   
  Listens to home geo coordinates published by `airsim_node`.  

- `/airsim_node/VEHICLE_NAME/odom_local_ned` [nav_msgs/Odometry](https://docs.ros.org/api/nav_msgs/html/msg/Odometry.html)   
  Listens to odometry published by `airsim_node`

#### Publishers:
- `/vel_cmd_world_frame` [airsim_ros_pkgs/VelCmd](https://github.com/microsoft/AirSim/tree/master/ros/src/airsim_ros_pkgs/msg/VelCmd.msg)   
  Sends velocity command to `airsim_node`

### Global params
- Dynamic constraints. These can be changed in `dynamic_constraints.launch`:  
    * `/max_vel_horz_abs` [double]   
  Maximum horizontal velocity of the drone (meters/second)

    * `/max_vel_vert_abs` [double]   
  Maximum vertical velocity of the drone (meters/second)
    
    * `/max_yaw_rate_degree` [double]   
  Maximum yaw rate (degrees/second)

### Misc


#### Setting up the Build Environment on Windows10 using WSL1 or WSL2


These setup instructions describe how to setup "Bash on Ubuntu on Windows" (aka "Windows Subsystem for Linux").

It involves enabling the built-in Windows Linux environment (WSL) in Windows10, installing a compatible Linux OS image, and finally installing the build environment as if it were a normal Linux system.

Upon completion, you will be able to build and run the ros wrapper as in a native linux machine.

##### WSL1 vs WSL2

WSL2 is the latest version of the Windows10 Subsystem for Linux. It is many times faster than WSL1 (if you use the native file system in `/home/...` rather
than Windows mounted folders under `/mnt/...`) and is therefore much preferred for building the code in terms of speed. 

Once installed, you can switch between WSL1 or WSL2 versions as you prefer.

##### WSL Setup steps

1. Follow the instructions [here](https://docs.microsoft.com/en-us/windows/wsl/install-win10). Check that the ROS version you want to use is supported by the Ubuntu version you want to install.

2. Congratulations, you now have a working Ubuntu subsystem under Windows, you can now go to [Ubuntu 16 / 18 instructions](#setup) and then [How to run Airsim on Windows and ROS wrapper on WSL](#how-to-run-airsim-on-windows-and-ros-wrapper-on-wsl)!

!!! note

    You can run XWindows applications (including SITL) by installing [VcXsrv](https://sourceforge.net/projects/vcxsrv/)  on Windows. 
    To use it find and run `XLaunch` from the Windows start menu.    
    Select `Multiple Windows` in first popup, `Start no client` in second popup, **only** `Clipboard` in third popup. Do **not** select `Native Opengl` (and if you are not able to connect select `Disable access control`).  
    You will need to set the DISPLAY variable to point to your display: in WSL it is `127.0.0.1:0`, in WSL2 it will be the ip address of the PC's network port and can be set by using the code below. Also in WSL2 you may have to disable the firewall for public networks, or create an exception in order for VcXsrv to communicate with WSL2:

    `export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0`

!!! tip

    - If you add this line to your ~/.bashrc file you won't need to run this command again  
    - For code editing you can install VSCode inside WSL.  
    - Windows 10 includes "Windows Defender" virus scanner. It will slow down WSL quite a bit. Disabling it greatly improves disk performance but increases your risk to viruses so disable at your own risk. Here is one of many resources/videos that show you how to disable it: [How to Disable or Enable Windows Defender on Windows 10](https://youtu.be/FmjblGay3AM)

##### File System Access between WSL and Windows10


From within WSL, the Windows drives are referenced in the /mnt directory. For example, in order to list documents within your (<username>) documents folder:


    `ls /mnt/c/'Documents and Settings'/<username>/Documents`
    or
    `ls /mnt/c/Users/<username>/Documents`


From within Windows, the WSL distribution's files are located at (type in windows Explorer address bar):

   `\\wsl$\<distribution name>`
   e.g.
   `\\wsl$\Ubuntu-18.04`
   
#### How to run Airsim on Windows and ROS wrapper on WSL
For WSL 1 execute:  
`export WSL_HOST_IP=127.0.0.1`  
and for WSL 2:  
`export WSL_HOST_IP=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}')`  
Now, as in the [running section for linux](#running), execute the following:  
```
source devel/setup.bash
roslaunch airsim_ros_pkgs airsim_node.launch output:=screen host:=$WSL_HOST_IP
roslaunch airsim_ros_pkgs rviz.launch
```



