using System;
using UnityEngine;
using System.IO;
using System.Text;
using System.Collections.Generic;
using System.Linq.Expressions;
using UnityEditor;

namespace AirSimUnity {
    /*
     * Container class for settings.json file. This class utilizes JSONUtility in Unity for parsing the json file.
     * A new file will be created if settings.json file is not already present.
     *
     * Current file location is Documents\AirSim\settings.json
     *
     * NOTE : All the variable names has to match the attribute names(case sensitive) in the json file
     * struct  names are customizable unlike variable names, and make sure all the structs are serializable.
     */

    [Serializable]
    public class AirSimSettings {
        private const int DEFAULT_DRONE_PORT = 41451;
        private const int DEFAULT_CAR_PORT = 41451;

        private static string AIRSIM_DATA_FOLDER = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments) + "/AirSim/";

        [Serializable]
        public struct RecordingSettings {
            public bool RecordOnMove;
            public float RecordInterval;
            public List<CamerasSettings> Cameras;
        }

        [Serializable]
        public struct CamerasSettings {
            public string CameraName;
            public int ImageType;
            public bool PixelAsFloat;
            public bool Compress;
        }

        [Serializable]
        public struct GimbleSettings {
            public int Stabilization;
            public float Pitch;
            public float Roll;
            public float Yaw;
        }

        [Serializable]
        public struct CameraCaptureSettings {
            public int ImageType;
            public int Width;
            public int Height;
            public int FOV_Degrees;
            public int AutoExposureSpeed;
            public int AutoExposureBias;
            public float AutoExposureMaxBrightness;
            public float AutoExposureMinBrightness;
            public int MotionBlurAmout;
            public int TargetGamma;
            public int ProjectionMode;
            public float OrthoWidth;
            public GimbleSettings Gimble;
        }

        [Serializable]
        public struct GeoPointSettings {
            public float Latitude;
            public float Longitude;
            public float Altitude;
        }

        [Serializable]
        public struct TimeOfDaySettings {
            public bool Enable;
            public long StartDateTime;
            public int CelestialClockSpeed;
            public bool StartDateTimeDst;
            public int UpdateIntervalSecs;
        }

        [Serializable]
        public struct SubWindowsSettings {
            public int WindowID;
            public int CameraID;
            public int ImageType;
            public bool Visible;
        }

        public struct RCSettings {
            public int RemoteControlID;
            public bool AllowAPIWhenDisconnected;
            public bool AllowAPIAlways;
        }

        [Serializable]
        public struct SimpleFlightSettings {
            public string FirmwareName;
            public string DefaultVehicleState;
            public RCSettings RC;
            public int ApiServerPort;
        }

        [Serializable]
        public struct PX4Settings {
            public string FirmwareName;
            public string LogViewerHostIp;
            public int LogViewerPort;
            public int OffboarCompID;
            public int OffboardSysID;
            public string QgcHostIp;
            public int QgcPort;
            public int SerialBaudRate;
            public string SerialPort;
            public int SimCompID;
            public string SitlIp;
            public int SitlPort;
            public string UdpIp;
            public int UdpPort;
            public bool UseSerial;
            public int VehicleCompID;
            public int VehicleSysID;
            public int ApiServerPort;
        }

        public float SettingsVersion = 1.0f;
        public string DefaultVehicleConfig = "SimpleFlight";
        public string SimMode = "";
        public int ClockSpeed = 1;
        public string LocalHostIP = "127.0.0.1";
        public bool RecordUIVisible = true;
        public bool LogMessagesVisible = true;
        public int ViewMode = 0;
        public string UsageScenario = "";
        public bool RpcEnabled = true;
        public bool EngineSound = true;
        public string PhysicsEngine = "";
        public bool EnableCollisionPassthrough = false;

        public RecordingSettings Recording;
        public List<CameraCaptureSettings> CaptureSettings;
        public GeoPointSettings OriginGeopoint;
        public TimeOfDaySettings TimeOfDay;
        public List<SubWindowsSettings> SubWindows;
        public SimpleFlightSettings SimpleFlight;
        public PX4Settings PX4;

        private static AirSimSettings settings = null;

        public static AirSimSettings GetSettings() {
            if (settings == null)
                Initialize();
            return settings;
        }

        public static bool Initialize() {
            settings = new AirSimSettings();
            settings.SetDefaults();

            if (!Directory.Exists(AIRSIM_DATA_FOLDER)) {
                Directory.CreateDirectory(AIRSIM_DATA_FOLDER);
            }

            string jsonString = GetSettingsContent();
            JsonUtility.FromJsonOverwrite(jsonString, settings);
            return true;
        }

        public CameraCaptureSettings GetCaptureSettingsBasedOnImageType(ImageType type) {
            if (CaptureSettings.Count == 1) {
                return CaptureSettings[0];
            }

            foreach (CameraCaptureSettings capSettings in CaptureSettings) {
                if (capSettings.ImageType == (int)type) {
                    return capSettings;
                }
            }
            return CaptureSettings[0];
        }

        public int GetPortIDForVehicle(bool isDrone) {
            if (!isDrone) {
                return DEFAULT_CAR_PORT;
            }

            if ("PX4".Equals(DefaultVehicleConfig)) {
                return PX4.ApiServerPort;
            } else {
                return SimpleFlight.ApiServerPort;
            }
        }

        /********** Methods internal to AirSimSettngs ********/

        //Get the settings from the "setting.json" file.
        private static string GetSettingsContent() {
            var fileName = InitializeAirSim.GetAirSimSettingsFileName();

            string content = "";
            try {
                string line = "";
                using (var reader = new StreamReader(fileName, Encoding.Default))
                {
                    do
                    {
                        content += " " + line;
                        line = reader.ReadLine();
                    } while (line != null);
                    reader.Close();
                }
            } catch (Exception e) {
                Debug.LogError("Unable to read the settings file : " + e.Message);
                throw e;
            }
            return content;
        }
        
        private void SetDefaults() {
            Recording.RecordInterval = 0.05f;
            Recording.RecordOnMove = false;
            Recording.Cameras = new List<CamerasSettings>();
            Recording.Cameras.Add(GetDefaultCameraSettings());

            CaptureSettings = new List<CameraCaptureSettings>();
            CaptureSettings.Add(GetDefaultCaptureSettings());

            OriginGeopoint.Latitude = 7;
            OriginGeopoint.Longitude = 10;
            OriginGeopoint.Altitude = 0;

            TimeOfDay.Enable = false;
            TimeOfDay.StartDateTime = 0;
            TimeOfDay.CelestialClockSpeed = 1;
            TimeOfDay.StartDateTimeDst = false;
            TimeOfDay.UpdateIntervalSecs = 60;

            SubWindows = new List<SubWindowsSettings>();
            SubWindows.Add(GetSubWindowSettings(0, 0, 3, false));
            SubWindows.Add(GetSubWindowSettings(1, 0, 5, false));
            SubWindows.Add(GetSubWindowSettings(2, 0, 0, false));

            SimpleFlight = GetDefaultSimpleFlightSettings();

            PX4 = GetDefaultPX4Settings();
        }

        private PX4Settings GetDefaultPX4Settings() {
            PX4Settings px4;
            px4.FirmwareName = "PX4";
            px4.LogViewerHostIp = "127.0.0.1";
            px4.LogViewerPort = 14388;
            px4.OffboarCompID = 1;
            px4.OffboardSysID = 134;
            px4.QgcHostIp = "127.0.0.1";
            px4.QgcPort = 14550;
            px4.SerialBaudRate = 115200;
            px4.SerialPort = "*";
            px4.SimCompID = 42;
            px4.SimCompID = 142;
            px4.SitlIp = "127.0.0.1";
            px4.SitlPort = 14556;
            px4.UdpIp = "127.0.0.1";
            px4.UdpPort = 14560;
            px4.UseSerial = true;
            px4.VehicleCompID = 1;
            px4.VehicleSysID = 135;
            px4.ApiServerPort = DEFAULT_DRONE_PORT;
            return px4;
        }

        private SimpleFlightSettings GetDefaultSimpleFlightSettings() {
            SimpleFlightSettings settings;
            settings.FirmwareName = "SimpleFlight";
            settings.DefaultVehicleState = "Armed";
            RCSettings rc;
            rc.RemoteControlID = 0;
            rc.AllowAPIWhenDisconnected = false;
            rc.AllowAPIAlways = true;
            settings.RC = rc;
            settings.ApiServerPort = DEFAULT_DRONE_PORT;
            return settings;
        }

        private SubWindowsSettings GetSubWindowSettings(int windowID, int camID, int imageType, bool visible) {
            SubWindowsSettings subSettings;
            subSettings.WindowID = windowID;
            subSettings.CameraID = camID;
            subSettings.ImageType = imageType;
            subSettings.Visible = visible;
            return subSettings;
        }

        private CamerasSettings GetDefaultCameraSettings() {
            CamerasSettings camSettings;
            camSettings.CameraName = "";
            camSettings.ImageType = 0;
            camSettings.PixelAsFloat = false;
            camSettings.Compress = true;
            return camSettings;
        }

        private CameraCaptureSettings GetDefaultCaptureSettings() {
            CameraCaptureSettings capSettings;
            capSettings.ImageType = 0;
            capSettings.Width = 256;
            capSettings.Height = 144;
            capSettings.FOV_Degrees = 90;
            capSettings.AutoExposureSpeed = 100;
            capSettings.AutoExposureBias = 0;
            capSettings.AutoExposureMaxBrightness = 0.64f;
            capSettings.AutoExposureMinBrightness = 0.03f;
            capSettings.MotionBlurAmout = 0;
            capSettings.TargetGamma = 1;
            capSettings.ProjectionMode = 0;
            capSettings.OrthoWidth = 5.12f;
            capSettings.Gimble.Stabilization = 0;
            capSettings.Gimble.Pitch = 0;
            capSettings.Gimble.Roll = 0;
            capSettings.Gimble.Yaw = 0;
            return capSettings;
        }
    }
}
