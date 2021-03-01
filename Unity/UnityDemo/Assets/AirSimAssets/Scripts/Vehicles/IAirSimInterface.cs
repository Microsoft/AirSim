
namespace AirSimUnity
{
    /**
     * An interface to Unity client into AirLib wrapper.
     */
    public interface IAirSimInterface
    {
        KinemticState GetKinematicState();

        void InvokeTickInAirSim(float deltaSecond);

        void InvokeCollisionDetectionInAirSim(string vehicleName, CollisionInfo collisionInfo);
    }
}
