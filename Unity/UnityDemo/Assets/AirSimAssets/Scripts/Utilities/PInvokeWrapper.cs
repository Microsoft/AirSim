using System;
using System.Runtime.InteropServices;

namespace AirSimUnity {
    /*
     * class for all the PInvoke methods.
     */

    public static class PInvokeWrapper {
        private const string DLL_NAME = "AirsimWrapper";

        // Delegates initializer. All the delegate methods are registered through this PInvoke call
        [DllImport(DLL_NAME)]
        public static extern void InitVehicleManager(IntPtr SetPose, IntPtr GetPose, IntPtr GetCollisionInfo, IntPtr GetRCData,
            IntPtr GetSimImages, IntPtr SetRotorSpeed, IntPtr SetEnableApi, IntPtr SetCarApiControls, IntPtr GetCarState,
            IntPtr GetCameraInfo, IntPtr SetCameraPose, IntPtr SetCameraFoV, IntPtr SetDistortionParam, IntPtr GetDistortionParams,
            IntPtr SetSegmentationObjectId, IntPtr GetSegmentationObjectId, IntPtr PrintLogMessage, IntPtr GetTransformFromUnity, 
            IntPtr Reset, IntPtr GetVelocity, IntPtr GetRayCastHit, IntPtr Pause);

        [DllImport(DLL_NAME)]
        public static extern KinemticState GetKinematicState(string vehicleName);

        [DllImport(DLL_NAME)]
        public static extern void StartDroneServer(string ip, int port, string vehicleId);

        [DllImport(DLL_NAME)]
        public static extern void StopDroneServer(string vehicleName);

        [DllImport(DLL_NAME)]
        public static extern bool StartServer(string simModeName, int portNumber);

        [DllImport(DLL_NAME)]
        public static extern void StopServer();

        [DllImport(DLL_NAME)]
        public static extern void CallTick(float deltaSeconds);

        [DllImport(DLL_NAME)]
        public static extern void InvokeCollisionDetection(string vehicleName, CollisionInfo collisionInfo);
    }
}
