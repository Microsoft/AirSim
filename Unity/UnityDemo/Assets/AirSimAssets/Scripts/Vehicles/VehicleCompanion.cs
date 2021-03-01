using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using AirSimUnity.DroneStructs;
using AirSimUnity.CarStructs;
using UnityEngine;

namespace AirSimUnity {
    /*
     * An implementation of IAirSimInterface, facilitating calls from Unity to AirLib.
     * And also a bridge for the calls originating from AirLib into Unity.
     *
     * Unity client components should use an instance of this class to interact with AirLib.
     */

    internal class VehicleCompanion : IAirSimInterface {

        //All the vehicles that are created in this game.
        private static List<VehicleCompanion> Vehicles = new List<VehicleCompanion>();

        private static int basePortId;

        //An interface to interact with Unity vehicle component.
        private IVehicleInterface VehicleInterface;

        private string vehicleName;
        private readonly bool isDrone;

        static VehicleCompanion() {
            InitDelegators();
        }

        private VehicleCompanion(IVehicleInterface vehicleInterface) {
            VehicleInterface = vehicleInterface;
            isDrone = vehicleInterface is Drone ? true : false;
            basePortId = AirSimSettings.GetSettings().GetPortIDForVehicle(isDrone);
        }

        public static VehicleCompanion GetVehicleCompanion(string vehicleName, IVehicleInterface vehicleInterface) {
            var companion = new VehicleCompanion(vehicleInterface);

            companion.vehicleName = vehicleName;

            Vehicles.Add(companion);
            return companion;
        }

        public void InvokeTickInAirSim(float deltaSecond)
        {
            PInvokeWrapper.CallTick(deltaSecond);
        }

        public void InvokeCollisionDetectionInAirSim(string vehicleName, CollisionInfo collisionInfo)
        {
            PInvokeWrapper.InvokeCollisionDetection(vehicleName, collisionInfo);
        }

        public KinemticState GetKinematicState() {
            return PInvokeWrapper.GetKinematicState(vehicleName);
        }

        public static DataRecorder.ImageData GetRecordingData() {
            return Vehicles[0].VehicleInterface.GetRecordingData();
        }

        public static DataCaptureScript GetCameraCaptureForRecording() {
            AirSimSettings.CamerasSettings recordCamSettings = AirSimSettings.GetSettings().Recording.Cameras[0];
            DataCaptureScript recordCam = Vehicles[0].VehicleInterface.GetCameraCapture(recordCamSettings.CameraName);
            return recordCam;
        }

        //Register the delegate functions to AirLib, based on IVehicleInterface
        private static void InitDelegators() {
            PInvokeWrapper.InitVehicleManager(
                Marshal.GetFunctionPointerForDelegate(new Func<AirSimPose, bool, string, bool>(SetPose)),
                Marshal.GetFunctionPointerForDelegate(new Func<string, AirSimPose>(GetPose)),
                Marshal.GetFunctionPointerForDelegate(new Func<string, CollisionInfo>(GetCollisionInfo)),
                Marshal.GetFunctionPointerForDelegate(new Func<string, AirSimRCData>(GetRCData)),
                Marshal.GetFunctionPointerForDelegate(new Func<ImageRequest, string, ImageResponse>(GetSimImages)),
                Marshal.GetFunctionPointerForDelegate(new Func<int, RotorInfo, string, bool>(SetRotorSpeed)),
                Marshal.GetFunctionPointerForDelegate(new Func<bool, string, bool>(SetEnableApi)),
                Marshal.GetFunctionPointerForDelegate(new Func<CarControls, string, bool>(SetCarApiControls)),
                Marshal.GetFunctionPointerForDelegate(new Func<string, CarState>(GetCarState)),
                Marshal.GetFunctionPointerForDelegate(new Func<string, string, CameraInfo>(GetCameraInfo)),
                Marshal.GetFunctionPointerForDelegate(new Func<string, AirSimPose, string, bool>(SetCameraPose)),
                Marshal.GetFunctionPointerForDelegate(new Func<string, float, string, bool>(SetCameraFoV)),
                Marshal.GetFunctionPointerForDelegate(new Func<string, string, float, string, bool>(SetDistortionParam)),
                Marshal.GetFunctionPointerForDelegate(new Func<string, string, bool>(GetDistortionParams)),
                Marshal.GetFunctionPointerForDelegate(new Func<string, int, bool, bool>(SetSegmentationObjectId)),
                Marshal.GetFunctionPointerForDelegate(new Func<string, int>(GetSegmentationObjectId)),
                Marshal.GetFunctionPointerForDelegate(new Func<string, string, string, int, bool>(PrintLogMessage)),
                Marshal.GetFunctionPointerForDelegate(new Func<string, UnityTransform>(GetTransformFromUnity)),
                Marshal.GetFunctionPointerForDelegate(new Func<bool>(Reset)),
                Marshal.GetFunctionPointerForDelegate(new Func<string, AirSimVector>(GetVelocity)),
                Marshal.GetFunctionPointerForDelegate(new Func<AirSimVector, AirSimVector, string, RayCastHitResult>(GetRayCastHit)),
                Marshal.GetFunctionPointerForDelegate(new Func<float, bool>(Pause))
            );
        }

        /*********************** Delegate functions to be registered with AirLib *****************************/

        private static bool SetPose(AirSimPose pose, bool ignoreCollision, string vehicleName) {
            var vehicle = Vehicles.Find(element => element.vehicleName == vehicleName);
            vehicle.VehicleInterface.SetPose(pose, ignoreCollision);
            return true;
        }

        private static AirSimPose GetPose(string vehicleName) {
            var vehicle = Vehicles.Find(element => element.vehicleName == vehicleName);
            return vehicle.VehicleInterface.GetPose();
        }

        private static CollisionInfo GetCollisionInfo(string vehicleName) {
            var vehicle = Vehicles.Find(element => element.vehicleName == vehicleName);
            return vehicle.VehicleInterface.GetCollisionInfo();
        }

        private static AirSimRCData GetRCData(string vehicleName) {
            var vehicle = Vehicles.Find(element => element.vehicleName == vehicleName);
            return vehicle.VehicleInterface.GetRCData();
        }

        private static ImageResponse GetSimImages(ImageRequest request, string vehicleName) {
            var vehicle = Vehicles.Find(element => element.vehicleName == vehicleName);
            return vehicle.VehicleInterface.GetSimulationImages(request);
        }

        private static UnityTransform GetTransformFromUnity(string vehicleName)
        {
            var vehicle = Vehicles.Find(element => element.vehicleName == vehicleName);
            return vehicle.VehicleInterface.GetTransform();
        }

        private static bool Reset()
        {
            foreach (var vehicle in Vehicles)
            {
                vehicle.VehicleInterface.ResetVehicle();
            }
            return true;
        }

        private static AirSimVector GetVelocity(string vehicleName)
        {
            var vehicle = Vehicles.Find(element => element.vehicleName == vehicleName);
            return vehicle.VehicleInterface.GetVelocity();
        }

        private static RayCastHitResult GetRayCastHit(AirSimVector start, AirSimVector end, string vehicleName)
        {
            var vehicle = Vehicles.Find(element => element.vehicleName == vehicleName);
            return vehicle.VehicleInterface.GetRayCastHit(start, end);
        }

        private static bool SetRotorSpeed(int rotorIndex, RotorInfo rotorInfo, string vehicleName) {
            var vehicle = Vehicles.Find(element => element.vehicleName == vehicleName);
            return vehicle.VehicleInterface.SetRotorSpeed(rotorIndex, rotorInfo);
        }

        private static bool SetEnableApi(bool enableApi, string vehicleName) {
            var vehicle = Vehicles.Find(element => element.vehicleName == vehicleName);
            return vehicle.VehicleInterface.SetEnableApi(enableApi);
        }

        private static bool SetCarApiControls(CarControls controls, string vehicleName) {
            var vehicle = Vehicles.Find(element => element.vehicleName == vehicleName);
            return vehicle.VehicleInterface.SetCarControls(controls);
        }

        private static CarState GetCarState(string vehicleName) {
            var vehicle = Vehicles.Find(element => element.vehicleName == vehicleName);
            return vehicle.VehicleInterface.GetCarState();
        }

        private static CameraInfo GetCameraInfo(string cameraName, string vehicleName) {
            var vehicle = Vehicles.Find(element => element.vehicleName == vehicleName);
            return vehicle.VehicleInterface.GetCameraInfo(cameraName);
        }

        private static bool SetCameraPose(string cameraName, AirSimPose pose, string vehicleName) {
            var vehicle = Vehicles.Find(element => element.vehicleName == vehicleName);
            return vehicle.VehicleInterface.SetCameraPose(cameraName, pose);
        }

        private static bool SetCameraFoV(string cameraName, float fov_degrees, string vehicleName) {
            var vehicle = Vehicles.Find(element => element.vehicleName == vehicleName);
            return vehicle.VehicleInterface.SetCameraFoV(cameraName, fov_degrees);
        }

        private static bool SetDistortionParam(string cameraName, string paramName, float value, string vehicleName) {
            var vehicle = Vehicles.Find(element => element.vehicleName == vehicleName);
            return vehicle.VehicleInterface.SetDistortionParam(cameraName, paramName, value);
        }

        private static bool GetDistortionParams(string cameraName, string vehicleName)
        {
            var vehicle = Vehicles.Find(element => element.vehicleName == vehicleName);
            return vehicle.VehicleInterface.GetDistortionParams(cameraName);
        }

        private static bool PrintLogMessage(string message, string messageParams, string vehicleName, int severity) {
            var vehicle = Vehicles[0]; //we just let first vehicle print the message
            return vehicle.VehicleInterface.PrintLogMessage(message, messageParams, vehicleName, severity);
        }

        private static bool SetSegmentationObjectId(string objectName, int objectId, bool isNameRegex) {
            return Vehicle.SetSegmentationObjectId(objectName, objectId, isNameRegex);
        }

        private static int GetSegmentationObjectId(string objectName) {
            return Vehicle.GetSegmentationObjectId(objectName);
        }

        private static bool Pause(float timeScale)
        {
            foreach (var vehicle in Vehicles)
            {
                vehicle.VehicleInterface.Pause(timeScale);
            }
            return true;
        }
    }
}
