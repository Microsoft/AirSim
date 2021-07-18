#pragma once

#include "sensors/imu/ImuSimple.hpp"
#include "sensors/Barometer/BarometerSimple.hpp"
#include "sensors/Magnetometer/MagnetometerSimple.hpp"
#include "common/Common.hpp"
#include <thread>
#include <ostream>

namespace msr
{
namespace airlib
{
    class StandALoneSensors
    {
    public:
        static void generateImuStaticData(std::ostream& output_stream, float period, float total_duration)
        {
            auto kinematics = Kinematics::State::zero();
            msr::airlib::Environment::State initial_environment(kinematics.pose.position, GeoPoint());
            msr::airlib::Environment environment(initial_environment);
            environment.reset();

            ImuSimple imu;
            imu.initialize(&kinematics, &environment);
            imu.reset();

            float interations = total_duration / period;

            output_stream << std::fixed;
            output_stream << "time\tx-gyro\ty-gyro\tz-gyro\tx-acc\ty-acc\t-z-acc" << std::endl;

            TTimeDelta last = Utils::getTimeSinceEpochSecs();
            for (auto i = 0; i < interations; ++i) {
                const auto& output = imu.getOutput();
                output_stream << Utils::getTimeSinceEpochSecs() << "\t";
                output_stream << output.angular_velocity.x() << "\t" << output.angular_velocity.y() << "\t" << output.angular_velocity.z() << "\t";
                output_stream << output.linear_acceleration.x() << "\t" << output.linear_acceleration.y() << "\t" << output.linear_acceleration.z() << "\n";

                std::this_thread::sleep_for(std::chrono::duration<double>(period - (Utils::getTimeSinceEpochSecs() - last)));

                last = Utils::getTimeSinceEpochSecs();
                environment.update();
                imu.update();
            }
        }

        static void generateBarometerStaticData(std::ostream& output_stream, float period, float total_duration, GeoPoint loc)
        {
            auto kinematics = Kinematics::State::zero();
            msr::airlib::Environment::State initial_environment(kinematics.pose.position, loc);
            msr::airlib::Environment environment(initial_environment);
            environment.reset();

            BarometerSimple baro;
            baro.initialize(&kinematics, &environment);
            baro.reset();

            float interations = total_duration / period;

            output_stream << std::fixed;
            output_stream << "time\tpressure\taltitude" << std::endl;

            TTimeDelta last = Utils::getTimeSinceEpochSecs();
            for (auto i = 0; i < interations; ++i) {
                const auto& output = baro.getOutput();
                output_stream << Utils::getTimeSinceEpochSecs() << "\t";
                output_stream << output.pressure << "\t" << output.altitude << std::endl;

                std::this_thread::sleep_for(std::chrono::duration<double>(period - (Utils::getTimeSinceEpochSecs() - last)));

                last = Utils::getTimeSinceEpochSecs();
                environment.update();
                baro.update();
            }
        }

        static void generateBarometerDynamicData(std::ostream& output_stream, float period, float total_duration, GeoPoint loc)
        {
            auto kinematics = Kinematics::State::zero();
            msr::airlib::Environment::State initial_environment(kinematics.pose.position, loc);
            msr::airlib::Environment environment(initial_environment);
            environment.reset();

            BarometerSimple baro;
            baro.initialize(&kinematics, &environment);
            baro.reset();

            float interations_20s = 20.0f / period;

            output_stream << std::fixed;
            output_stream << "time\tpressure\taltitude\tgps_alt" << std::endl;

            TTimeDelta last = Utils::getTimeSinceEpochSecs();
            bool which_alt = false;
            for (auto j = 0; j < 10; ++j) {
                for (auto i = 0; i < interations_20s; ++i) {
                    const auto& output = baro.getOutput();
                    output_stream << Utils::getTimeSinceEpochSecs() << "\t";
                    output_stream << output.pressure << "\t" << output.altitude << "\t" << environment.getState().geo_point.altitude << std::endl;

                    std::this_thread::sleep_for(std::chrono::duration<double>(period - (Utils::getTimeSinceEpochSecs() - last)));

                    last = Utils::getTimeSinceEpochSecs();
                    environment.update();
                    baro.update();
                }

                which_alt = !which_alt;
                environment.setPosition(Vector3r(0, 0, which_alt ? -1.78f : 0));
            }
        }

        static void generateMagnetometer2D(std::ostream& output_stream, float period, float total_duration, GeoPoint loc, float yawStart, bool ccw = false)
        {
            output_stream << std::fixed;

            output_stream << "time\tx-mag\ty-mag\tz-mag" << std::endl;

            float interations = total_duration / period;
            TTimeDelta last = Utils::getTimeSinceEpochSecs();
            for (float direction = 0; direction < 5; direction++) {

                float yaw = yawStart;
                float yawDelta = (direction * M_PIf / 2.0f);
                if (ccw) {
                    yaw -= yawDelta;
                }
                else {
                    yaw += yawDelta;
                }

                auto kinematics = Kinematics::State::zero();
                kinematics.pose.orientation = VectorMath::toQuaternion(0, 0, yaw);
                msr::airlib::Environment::State initial_environment(kinematics.pose.position, loc);
                msr::airlib::Environment environment(initial_environment);
                environment.reset();

                MagnetometerSimple mag;
                mag.initialize(&kinematics, &environment);
                mag.reset();

                for (auto i = 0; i < interations; ++i) {
                    const auto& output = mag.getOutput();

                    output_stream << Utils::getTimeSinceEpochSecs() << "\t";
                    output_stream << output.magnetic_field_body.x() << "\t" << output.magnetic_field_body.y() << "\t" << output.magnetic_field_body.z();
                    output_stream << std::endl;

                    std::this_thread::sleep_for(std::chrono::duration<double>(period - (Utils::getTimeSinceEpochSecs() - last)));
                    last = Utils::getTimeSinceEpochSecs();
                    environment.update();
                    mag.update();
                }
            }
        }

        static void generateMagnetometer3D(std::ostream& output_stream, float period, float total_duration, GeoPoint loc, float yawStart = 0, bool ccw = false)
        {
            output_stream << std::fixed;

            output_stream << "time\tx-mag\ty-mag\tz-mag\tlat\tlon\talt\tw\tx\ty\tz" << std::endl;

            float interations = total_duration / period;
            TTimeDelta last = Utils::getTimeSinceEpochSecs();
            for (float pitch = 0; pitch < 2.1 * M_PIf; pitch += M_PIf / 2) {
                for (float roll = 0; roll < 2.1 * M_PIf; roll += M_PIf / 2) {
                    for (float direction = 0; direction < 5; direction++) {

                        float yaw = yawStart;
                        float yawDelta = (direction * M_PIf / 2.0f);
                        if (ccw) {
                            yaw -= yawDelta;
                        }
                        else {
                            yaw += yawDelta;
                        }

                        auto kinematics = Kinematics::State::zero();
                        kinematics.pose.orientation = VectorMath::toQuaternion(pitch, roll, yaw);
                        msr::airlib::Environment::State initial_environment(kinematics.pose.position, loc);
                        msr::airlib::Environment environment(initial_environment);
                        environment.reset();

                        MagnetometerSimple mag;
                        mag.initialize(&kinematics, &environment);
                        mag.reset();

                        for (auto i = 0; i < interations; ++i) {
                            const auto& output = mag.getOutput();
                            const auto& geo = environment.getState().geo_point;

                            output_stream << Utils::getTimeSinceEpochSecs() << "\t";
                            output_stream << output.magnetic_field_body.x() << "\t" << output.magnetic_field_body.y() << "\t" << output.magnetic_field_body.z();
                            output_stream << "\t" << geo.latitude << "\t" << geo.longitude << "\t" << geo.altitude;
                            output_stream << "\t" << kinematics.pose.orientation.w() << "\t" << kinematics.pose.orientation.x() << "\t" << kinematics.pose.orientation.y() << "\t" << kinematics.pose.orientation.z();
                            output_stream << std::endl;

                            std::this_thread::sleep_for(std::chrono::duration<double>(period - (Utils::getTimeSinceEpochSecs() - last)));
                            last = Utils::getTimeSinceEpochSecs();
                            environment.update();
                            mag.update();
                        }
                    }
                }
            }
        }

        static void generateMagnetometerMap(std::ostream& output_stream)
        {
            output_stream << std::fixed;

            output_stream << "lat\tlon\tx-mag\ty-mag\tz-mag" << std::endl;

            auto kinematics = Kinematics::State::zero();
            kinematics.pose.orientation = VectorMath::toQuaternion(0, 0, 0);
            msr::airlib::Environment::State initial_environment(kinematics.pose.position, GeoPoint());
            msr::airlib::Environment environment(initial_environment);
            environment.reset();

            MagnetometerSimple mag;
            mag.initialize(&kinematics, &environment);
            mag.reset();

            for (float lat = -90; lat < 90; lat++) {
                for (float lon = -180; lon < 180; lon++) {
                    environment.getState().geo_point = GeoPoint(lat, lon, 0);
                    mag.update();
                    const auto& output = mag.getOutput();

                    output_stream << lat << "\t" << lon << "\t";
                    output_stream << output.magnetic_field_body.x() << "\t" << output.magnetic_field_body.y() << "\t" << output.magnetic_field_body.z();
                    output_stream << std::endl;
                }
            }
        }
    };
}
}