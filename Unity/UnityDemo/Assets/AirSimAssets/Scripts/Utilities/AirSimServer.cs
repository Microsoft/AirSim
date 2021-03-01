using System.Collections;
using System.Collections.Generic;
using UnityEngine;

using System;
using AirSimUnity.DroneStructs;
using System.Threading;
using UnityEditor;

namespace AirSimUnity
{
    public class AirSimServer : MonoBehaviour
    {
        private const string DRONE_MODE = "Multirotor";

        // Start is called before the first frame update
        void Start()
        {
            string simMode = AirSimSettings.GetSettings().SimMode;
            int basePortId = AirSimSettings.GetSettings().GetPortIDForVehicle(simMode == DRONE_MODE);
            bool isServerStarted = PInvokeWrapper.StartServer(simMode, basePortId);
            if (isServerStarted == false)
            {
#if UNITY_EDITOR
                EditorUtility.DisplayDialog("Problem in starting AirSim server!!!", "Please check logs for more information.", "Exit");
                //EditorApplication.Exit(1);
#else
                Application.Quit();
#endif
            }
        }

        protected void OnApplicationQuit()
        {
            PInvokeWrapper.StopServer();
        }
    }
}