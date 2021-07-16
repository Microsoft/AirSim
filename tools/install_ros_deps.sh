#!/usr/bin/env bash

# This script is meant to be run for CI, otherwise it can break exisitng setups

set -x

DISTRO="$(lsb_release -sc)"
if [[ "$DISTRO" == "focal" ]]; then
    ROS_DISTRO="noetic"
elif [[ "$DISTRO" == "bionic" ]]; then
    ROS_DISTRO="melodic"
elif [[ "$DISTRO" == "xenial" ]]; then
    ROS_DISTRO="kinetic"
fi

sudo sh -c 'echo "deb http://packages.ros.org/ros/ubuntu $(lsb_release -sc) main" > /etc/apt/sources.list.d/ros-latest.list'
sudo apt-key adv --keyserver 'hkp://keyserver.ubuntu.com:80' --recv-key C1CF6E31E6BADE8868B172B4F42ED6FBAB17C654

sudo apt-get update
sudo apt-get install -y -qq ros-$ROS_DISTRO-ros-base

echo "source /opt/ros/$ROS_DISTRO/setup.bash" >> ~/.bashrc

sudo apt-get install -y python3-pip python3-yaml python3-setuptools
pip3 install rosdep rosinstall rospkg catkin-pkg
sudo rosdep init
rosdep update

# AirSim ROS Wrapper dependencies

# Only needed for CI due to base install
sudo apt-get install -y ros-$ROS_DISTRO-vision-opencv ros-$ROS_DISTRO-image-transport libyaml-cpp-dev

if [[ "$DISTRO" == "xenial" ]]; then
    sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
    sudo apt-get update
fi

sudo apt-get install -y gcc-8 g++-8
sudo apt-get install -y ros-$ROS_DISTRO-mavros* ros-$ROS_DISTRO-tf2-sensor-msgs ros-$ROS_DISTRO-tf2-geometry-msgs

# TODO: Remove this if-block when new 0.7.0 release of catkin_tools is available
if [[ "$DISTRO" == "focal" ]]; then
    pip3 install "git+https://github.com/catkin/catkin_tools.git#egg=catkin_tools"
else
    pip3 install catkin-tools
fi
