using System;
using AirSimUnity.DroneStructs;
using System.Collections.Generic;
using System.Threading;
using UnityEditor;
using UnityEngine;

namespace AirSimUnity {
    /*
     * The base class for all the vehicle models in the simulation. Holds the data to communicate between AirLib and Unity.
     * An implementation of IVehicleInterface with all the contracts required by VehicleCompanion.
     */

    [RequireComponent(typeof(Rigidbody))]
    public abstract class Vehicle : MonoBehaviour, IVehicleInterface {
        protected Vector3 position;
        protected Quaternion rotation;
        protected Vector3 scale3D = Vector3.one;
        protected AirSimPose poseFromAirLib;
        protected AirSimPose currentPose;
        protected CollisionInfo collisionInfo;
        protected AirSimRCData rcData;
        protected bool resetVehicle;
        protected float time_scale = 1.0f;
        protected bool timeScale_changed = false;
        protected bool isApiEnabled = false;
        protected bool isServerStarted = false;
        bool print_log_messages_ = true;

        protected List<DataCaptureScript> captureCameras = new List<DataCaptureScript>();

        protected AutoResetEvent captureResetEvent;

        protected IAirSimInterface airsimInterface;

        protected bool isCapturingImages = false;
        protected ImageRequest imageRequest;
        protected ImageResponse imageResponse;

        private bool isDrone;
        private static bool isSegmentationUpdated;

        private bool calculateRayCast = false;
        Vector3 startVec;
        Vector3 endVec;
        RaycastHit hitInfo;
        bool hitResult;

        [SerializeField] string vehicleName;

        //Ensure to call this method as the first statement, from derived class `Start()` method.
        protected void Start() {
            isDrone = this is Drone ? true : false;
            if (isDrone) {
                GetComponent<Rigidbody>().useGravity = false;
            }

            InitializeVehicle();

            airsimInterface = VehicleCompanion.GetVehicleCompanion(vehicleName, this);
            isServerStarted = true;
        }

        //Ensure to call this method as the first statement, from derived class `FixedUpdate()` method.
        protected void FixedUpdate() {
            if (isServerStarted)
            {
                DataManager.SetToAirSim(transform.position, ref currentPose.position);
                DataManager.SetToAirSim(transform.rotation, ref currentPose.orientation);
            }
        }

        //Ensure to call this method as the last statement, from derived class `LateUpdate()` method.
        protected void LateUpdate() {
            if (isServerStarted)
            {
                if (timeScale_changed)
                {
                    Time.timeScale = time_scale;
                    timeScale_changed = false;
                }

                if (isSegmentationUpdated)
                {
                    UpdateSegmentationView();
                    isSegmentationUpdated = false;
                }

                if (isCapturingImages)
                {
                    var captureCamera = captureCameras.Find(element => element.GetCameraName() == imageRequest.camera_name);
                    imageResponse = captureCamera.GetImageBasedOnRequest(imageRequest);
                    captureResetEvent.Set(); //Release the GetSimulationImages thread with the image response.
                    isCapturingImages = false;
                }

                if (calculateRayCast)
                {
                    hitResult = Physics.Linecast(startVec, endVec, out hitInfo);
                    calculateRayCast = false;
                }

                if (Input.GetKeyDown(KeyCode.T))
                {
                    print_log_messages_ = !print_log_messages_;
                }
            }
        }

        protected void OnCollisionEnter(Collision collision) {
            if (isServerStarted)
            {
                collisionInfo.has_collided = true;
                collisionInfo.object_id = -1;
                collisionInfo.collision_count++;
                collisionInfo.object_name = collision.collider.name;
                DataManager.SetToAirSim(collision.contacts[0].normal, ref collisionInfo.normal);
                DataManager.SetToAirSim(collision.contacts[0].point, ref collisionInfo.impact_point);
                collisionInfo.position = currentPose.position;
                collisionInfo.penetration_depth = collision.contacts[0].separation;
                collisionInfo.object_name = collision.collider.name;
                collisionInfo.time_stamp = DataManager.GetCurrentTimeInMilli();

                airsimInterface.InvokeCollisionDetectionInAirSim(vehicleName, collisionInfo);
            }
        }

        protected void OnCollisionStay(Collision collision) {
            if (isServerStarted)
            {
                collisionInfo.has_collided = true;
                collisionInfo.object_id = -1;
                collisionInfo.collision_count++;
                collisionInfo.object_name = collision.collider.name;
                DataManager.SetToAirSim(collision.contacts[0].normal, ref collisionInfo.normal);
                DataManager.SetToAirSim(collision.contacts[0].point, ref collisionInfo.impact_point);
                collisionInfo.position = currentPose.position;
                collisionInfo.penetration_depth = collision.contacts[0].separation;
                collisionInfo.object_name = collision.collider.name;
                collisionInfo.time_stamp = DataManager.GetCurrentTimeInMilli();
            }
        }

        protected void OnCollisionExit() {
            if (isServerStarted)
            {
                collisionInfo.has_collided = false;
            }
        }

        //Define the recording data that needs to be saved in the airsim_rec file for Toggle Recording button
        public abstract DataRecorder.ImageData GetRecordingData();

        public void ResetVehicle()
        {
            resetVehicle = true;
        }

        public AirSimVector GetVelocity()
        {
            var rigidBody = GetComponent<Rigidbody>();
            return new AirSimVector(rigidBody.velocity.x, rigidBody.velocity.y, rigidBody.velocity.z);
        }

        public bool SetPose(AirSimPose pose, bool ignore_collision) {
            poseFromAirLib = pose;
            return true;
        }

        public AirSimPose GetPose() {
            return currentPose;
        }

        public UnityTransform GetTransform()
        {
            return new UnityTransform(new AirSimQuaternion(currentPose.orientation.x, currentPose.orientation.y, currentPose.orientation.z, currentPose.orientation.w),
                new AirSimVector(currentPose.position.x, currentPose.position.y, currentPose.position.z),
                new AirSimVector(scale3D.x, scale3D.y, scale3D.z));
        }

        public RayCastHitResult GetRayCastHit(AirSimVector start, AirSimVector end)
        {
            DataManager.SetToUnity(start, ref startVec);
            DataManager.SetToUnity(end, ref endVec);

            calculateRayCast = true;
            int count = 500; // wait for max 0.5 second
            while(calculateRayCast && count > 0)
            {
                count--;
            }

            return new RayCastHitResult(hitResult, hitInfo.distance);
        }

        public bool Pause(float timeScale)
        {
            time_scale = timeScale;
            timeScale_changed = true;
            return true;
        }

        public CollisionInfo GetCollisionInfo() {
            return collisionInfo;
        }

        public AirSimRCData GetRCData() {
            return rcData;
        }

        public DataCaptureScript GetCameraCapture(string cameraName) {
            DataCaptureScript recordCam = captureCameras[0];
            foreach (DataCaptureScript camCapture in captureCameras) {
                if (camCapture.GetCameraName() == cameraName) {
                    recordCam = camCapture;
                    break;
                }
            }
            return recordCam;
        }

        public ImageResponse GetSimulationImages(ImageRequest request) {
            imageResponse.reset();

            if (!captureCameras.Find(element => element.GetCameraName() == request.camera_name))
            {
                return imageResponse;
            }

            imageRequest = request;
            isCapturingImages = true;
            captureResetEvent.WaitOne();
            return imageResponse;
        }

        public bool SetEnableApi(bool enableApi) {
            isApiEnabled = enableApi;
            return true;
        }

        public CameraInfo GetCameraInfo(string cameraName) {
            CameraInfo info = new CameraInfo();
            foreach (DataCaptureScript capture in captureCameras) {
                if (capture.GetCameraName() == cameraName) {
                    info.fov = capture.GetFOV();
                    info.pose = capture.GetPose();
                    return info;
                }
            }
            return info;
        }

        public bool SetCameraPose(string cameraName, AirSimPose pose) {
            foreach (DataCaptureScript capture in captureCameras) {
                if (capture.GetCameraName() == cameraName) {
                    capture.SetPose(pose);
                    return true;
                }
            }
            return false;
        }

        public bool SetCameraFoV(string cameraName, float fov_degrees) {
            foreach(DataCaptureScript capture in captureCameras) {
                if (capture.GetCameraName() == cameraName) {
                    capture.SetFoV(fov_degrees);
                    return true;;
                }
            }
            return false;
        }

        public bool SetDistortionParam(string cameraName, string paramName, float value)
        {
            // not implemented
            return false;
        }

        public bool GetDistortionParams(string cameraName)
        {
            // not implemented
            return false;
        }

        public bool PrintLogMessage(string message, string messageParams, string vehicleName, int severity) {

            if (!print_log_messages_)
                return true;

            if (severity == 2) {
                Debug.LogError(message + " " + messageParams + " Vehicle=" + vehicleName);
            } else if (severity == 1) {
                Debug.LogWarning(message + " " + messageParams + " Vehicle=" + vehicleName);
            } else {
                Debug.Log(message + " " + messageParams + " Vehicle=" + vehicleName);
            }
            return true;
        }

        public static bool SetSegmentationObjectId(string objectName, int objectId, bool isNameRegex) {
            int finalId = ((objectId + 2) << 16) | ((objectId + 1) << 8) | objectId;
            bool result = CameraFiltersScript.SetSegmentationId(objectName, finalId, isNameRegex);
            isSegmentationUpdated = result;
            return result;
        }

        public static int GetSegmentationObjectId(string objectName) {
            return CameraFiltersScript.GetSegmentationId(objectName);
        }

        public virtual bool SetRotorSpeed(int rotorIndex, RotorInfo rotorInfo) {
            throw new NotImplementedException("This is supposed to be implemented in Drone sub class");
        }

        public virtual bool SetCarControls(CarStructs.CarControls controls) {
            throw new NotImplementedException("This is supposed to be implemented in Car sub class");
        }

        public virtual CarStructs.CarState GetCarState() {
            throw new NotImplementedException("This is supposed to be implemented in Car sub class");
        }

        /****************** Methods for vehicle management **************************/

        private void InitializeVehicle() {
            //Setting the initial get pose(HomePoint in AirLib) values to that of vehicle's location in the scene for initial setup in AirLib
            DataManager.SetToAirSim(transform.position, ref currentPose.position);
            DataManager.SetToAirSim(transform.rotation, ref currentPose.orientation);

            collisionInfo.SetDefaultValues();
            SetUpCameras();
            captureResetEvent = new AutoResetEvent(false);
        }

        //Register all the capture cameras in the scene for recording and data capture.
        //Make sure every camera is a child of a gameobject with tag "CaptureCameras"
        private void SetUpCameras() {
            GameObject camerasParent = GameObject.FindGameObjectWithTag("CaptureCameras");
            if (!camerasParent) {
                Debug.LogWarning("No Cameras found in the scene to capture data");
                return;
            }

            for (int i = 0; i < camerasParent.transform.childCount; i++) {
                DataCaptureScript camCapture = camerasParent.transform.GetChild(i).GetComponent<DataCaptureScript>();
                captureCameras.Add(camCapture);
                camCapture.SetUpCamera(camCapture.GetCameraName(), isDrone);
            }
        }

        private void UpdateSegmentationView() {
            GameObject viewCameras = GameObject.FindGameObjectWithTag("ViewCameras");
            if (viewCameras) {
                foreach (CameraFiltersScript viewCam in viewCameras.GetComponentsInChildren<CameraFiltersScript>()) {
                    if (viewCam.effect != ImageType.Segmentation) {
                        continue;
                    }
                    viewCam.SetShaderEffect(ImageType.Segmentation);
                }
            }
        }
    }
}
