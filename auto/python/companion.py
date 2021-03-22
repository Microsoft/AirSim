import rospy
import glob
import json
import math
import os
import px4tools
import sys
import time
import actionlib
import roslaunch
import numpy as np
import tf2_ros
import tf2_geometry_msgs
import cv2
import shutil

from cv_bridge import CvBridge, CvBridgeError

from mavros import mavlink
from mavros_msgs.msg import Mavlink, Waypoint, WaypointReached, GlobalPositionTarget, State, TakeoffAction, TakeoffGoal, LandAction, LandGoal, WaypointsAction, WaypointsGoal, HomePosition
from mavros_msgs.srv import CommandBool, SetMode, CommandTOL, WaypointPush, WaypointClear, CommandHome
from orb_slam2_ros.msg import KeyFrames, Observations
from sensor_msgs.msg import NavSatFix, Image, PointCloud2
from std_msgs.msg import Header
from nav_msgs.msg import Odometry
from geometry_msgs.msg import Pose, PoseWithCovariance, PoseStamped, PoseWithCovarianceStamped
from mavros_test_common import MavrosTestCommon
from pymavlink import mavutil

class KeyFrame(object):
	empty = []

	def __init__(self):
		self.image_id = ""
		self.latitude = 0.0
		self.longitude = 0.0
		self.altitude = 0.0
		self.orientation = [0.0,0.0,0.0,0.0]
		self.image = ""	

class companion():

	def __init__(self):

		#Initialize ROSLaunch
		launch = roslaunch.scriptapi.ROSLaunch()
		launch.start()	
		uuid = roslaunch.rlutil.get_or_generate_uuid(None, False)
		roslaunch.configure_logging(uuid)

		#Launch MAVROS
		cli_args = ('mavros', 'px4.launch')
		roslaunch_file = [(roslaunch.rlutil.resolve_launch_arguments(cli_args)[0])]
		mavros = roslaunch.parent.ROSLaunchParent(uuid, roslaunch_file)
		mavros.start()

		#Launch ORBSLAM
		cli_args = ('orb_slam2_ros', 'orb_slam2_mono.launch')
		roslaunch_file = [(roslaunch.rlutil.resolve_launch_arguments(cli_args)[0])]
		orbslam = roslaunch.parent.ROSLaunchParent(uuid, roslaunch_file)
		orbslam.start()

   		rospy.init_node('companion_node', anonymous=True)
		
		#clear old images
		shutil.rmtree('img')
		os.mkdir('img')		

		self.local_pose = Pose()
		self.slam_pose = PoseStamped()
		self.raw_slam_pose = PoseStamped()
		self.calib_init_slam = Pose()
		self.calib_init_local = Pose()
		self.slam_calibrated = False
		self.slam_offset_x = 0
		self.slam_offset_y = 0
		self.slam_offset_z = 0
		self.slam_scale = 1
		self.state = "SYSTEM_NOT_READY"
		self.last_state = "SYSTEM_NOT_READY"

			#States:
			#"SYSTEM_NOT_READY"
			#"NO_IMAGES_YET"
			#"NOT_INITIALIZED"
			#"OK"
			#"LOST"

		self.keyframes = []
		self.image_buffer = []
		self.bridge = CvBridge()

		rospy.Subscriber('/mavros/local_position/pose', PoseStamped, self.local_pose_callback)
		rospy.Subscriber('/orb_slam2_mono/pose', PoseStamped, self.slamCallback)
		rospy.Subscriber('/orb_slam2_mono/state', Header, self.stateCallback)
		rospy.Subscriber('/orb_slam2_mono/map_points', PointCloud2, self.pointcloudCallback)
		rospy.Subscriber('/orb_slam2_mono/observations', Observations, self.observationsCallback)
		rospy.Subscriber('/orb_slam2_mono/keyframes', KeyFrames, self.keyframesCallback)
		rospy.Subscriber('/airsim/base_link/camera/image_raw', Image, self.imageBuffer)
		#rospy.Subscriber('/orb_slam2_mono/keyframes', KeyFrames, self.keyframesCallback)
		self.slam_pub = rospy.Publisher('/mavros/vision_pose/pose', PoseStamped, queue_size=50)

		rospy.spin()

		#Code to scale and publish orbslam pose to px4

	def local_pose_callback(self,data):
		#TODO Ensure this data is up to date!!
		self.local_pose = data.pose
		
	def stateCallback(self,data):
		#Check for state change
		self.state = data.frame_id
		if (self.last_state != self.state):
			self.slam_calibrated = False
			if (self.state == "OK"):
				self.calib_init_slam = self.raw_slam_pose.pose
				self.calib_init_local = self.local_pose
		self.last_state = self.state

	def slamCallback(self,data):

		self.raw_slam_pose = self.convertFRD(data)
		self.runCalibration()
		if (self.slam_calibrated):
			#transform SLAM pose
			self.slam_pose.pose.position.x = self.raw_slam_pose.pose.position.x * self.slam_scale + self.slam_offset_x
			self.slam_pose.pose.position.y = self.raw_slam_pose.pose.position.y * self.slam_scale + self.slam_offset_y
			self.slam_pose.pose.position.z = self.raw_slam_pose.pose.position.z * self.slam_scale + self.slam_offset_z
			self.slam_pose.pose.orientation = self.raw_slam_pose.pose.orientation
			self.slam_pose.header = self.raw_slam_pose.header

			self.slam_pub.publish(self.slam_pose)
		#else:
			#if (self.state == "OK"):

			

	def runCalibration(self):
			#check for sufficient calibration movement
			x = self.local_pose.position.x - self.calib_init_local.position.x
			y = self.local_pose.position.y - self.calib_init_local.position.y
			x_slam = self.raw_slam_pose.pose.position.x - self.calib_init_slam.position.x
			y_slam = self.raw_slam_pose.pose.position.y - self.calib_init_slam.position.y
			if (math.sqrt(x*x+y*y) > 5.0 and x_slam != 0):
				self.slam_scale = math.sqrt(x*x+y*y)/math.sqrt(x_slam*x_slam+y_slam*y_slam)			
				self.slam_offset_x = (self.calib_init_local.position.x-self.slam_scale*self.calib_init_slam.position.x)
				self.slam_offset_y = (self.calib_init_local.position.y-self.slam_scale*self.calib_init_slam.position.y)
				self.slam_offset_z = (self.calib_init_local.position.z-self.slam_scale*self.calib_init_slam.position.z)
				#print('offset', self.slam_offset_x, self.slam_offset_y, self.slam_offset_z)
				self.slam_calibrated = True

	def convertFRD(self, data):

		#convert to px4 coord system  (this is working Dec 15th)
		converted = PoseStamped()
		converted.header.frame_id = 'map'
		converted.pose.position.x = -data.pose.position.y
		converted.pose.position.y = data.pose.position.z
		converted.pose.position.z = -data.pose.position.x
		
		converted.pose.orientation.x = -data.pose.orientation.y
		converted.pose.orientation.y = data.pose.orientation.z
		converted.pose.orientation.z = -data.pose.orientation.x
		converted.pose.orientation.w = data.pose.orientation.w

		return converted

	def imageBuffer(self, data):
		buffer_size = 50
		if (len(self.image_buffer) > buffer_size):
			self.image_buffer.pop()
		self.image_buffer.insert(0,data)

	def keyframesCallback(self,data):
		#TODO not currently capturing all keyframes
		#print(len(data.keyframes),len(self.keyframes))
		#check for new keyframes
		data = list(data.keyframes)
		if (len(self.keyframes) == 0):
			for kf in data:
				self.addKeyFrame(kf)
		else:
			#check for removed keyframes
			for old_kf in self.keyframes:
				found = False
				for new_kf in data:
					if (old_kf.image_id == new_kf.header.stamp.to_sec()):
						found = True
						break
				#print(found)				
				if (not found):
					self.removeKeyFrame(old_kf)
			#Add new kfs
			latest = self.keyframes[-1].image_id
			for new_kf in data:
				if (new_kf.header.stamp.to_sec() > latest):
					self.addKeyFrame(new_kf)		

	def removeKeyFrame(self, data):
		#print('remove frame '+data.image)
		self.keyframes.remove(data)
		if os.path.exists(data.image):
 			os.remove(data.image)

	def addKeyFrame(self, data):
		kf = KeyFrame()
		found = False
		for img in self.image_buffer:
			#print(img.header.stamp, data.header.stamp)
			if (img.header.stamp.to_sec() == data.header.stamp.to_sec()):
				#print('add frame')
				kf.image_id = data.header.stamp.to_sec()
				cv_img = self.bridge.imgmsg_to_cv2(img, "passthrough")
				kf.image = 'img/'+str(kf.image_id)+'.png'
				cv2.imwrite(kf.image, cv_img)
				self.keyframes.append(kf)
				found = True
				return
		print('Image not found, consider increasing image buffer size')
			
	def pointcloudCallback(self, data):
		self.pointcloud = data

	def observationsCallback(self, data):
		data = list(data.observations)
		print(self.pointcloud.width, len(data))
		
		
if __name__ == '__main__':
	companion()

