using System.Collections;
using System.Collections.Generic;
using UnityEngine;

namespace AirSimUnity
{
    public class AirSimTick : MonoBehaviour
    {
        void FixedUpdate()
        {
            PInvokeWrapper.CallTick(Time.fixedDeltaTime);
        }
    }
}