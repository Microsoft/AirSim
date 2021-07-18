// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <iostream>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <cmath>
#include <utility>
#include "SimpleShell.hpp"
#include "common/Common.hpp"
#include "common/common_utils/Utils.hpp"
#include "common/common_utils/FileSystem.hpp"
#include "common/common_utils/AsyncTasker.hpp"
#include "vehicles/multirotor/api/MultirotorRpcLibClient.hpp"
#include "common/EarthUtils.hpp"
#include "vehicles/multirotor/api/MultirotorCommon.hpp"
#include "vehicles/multirotor/api/MultirotorApiBase.hpp"
#include "safety/SafetyEval.hpp"
#include "common/common_utils/Timer.hpp"

namespace msr
{
namespace airlib
{

    using namespace std;
    using namespace common_utils;

    struct CommandContext
    {
    public:
        CommandContext(const std::string& server_address = "localhost")
            : client(server_address)
        {
        }

        MultirotorRpcLibClient client;
        AsyncTasker tasker;

        void sleep_for(TTimeDelta wall_clock_dt)
        {
            clock_->sleep_for(wall_clock_dt);
        }

    private:
        ClockBase* clock_ = ClockFactory::get();
    };

    using DroneCommandParameters = SimpleShell<CommandContext>::ShellCommandParameters;
    using DroneCommandSwitch = SimpleShell<CommandContext>::ShellCommandSwitch;

    class DroneCommand : public SimpleShell<CommandContext>::ShellCommand
    {
    public:
        DroneCommand(std::string name, std::string help)
            : ShellCommand(name, help)
        {
        }

        YawMode getYawMode()
        {
            std::string yaw = getSwitch("-yaw").value;
            std::string rate = getSwitch("-yaw_rate").value;
            if (yaw.empty()) {
                if (rate.empty()) {
                    return YawMode(true, 0);
                }
                return YawMode(true, getSwitch("-yaw_rate").toFloat());
            }
            return YawMode(/*bool is_rate_val, float yaw_or_rate_val*/ false, getSwitch("-yaw").toFloat());
        }

        void addYawModeSwitches()
        {
            this->addSwitch({ "-yaw", "", "specify yaw in degrees (default not set)" });
            this->addSwitch({ "-yaw_rate", "", "specify yaw rate in degrees per second (default not set)" });
        }

        void addLookaheadSwitches()
        {
            this->addSwitch({ "-lookahead", "-1", "How far to look ahead on the path (default -1 means auto)" });
            this->addSwitch({ "-adaptive_lookahead", "1", "Whether to apply adaptive lookahead (1=yes, 0=no) (default 1)" });
        }

        void addRelativeSwitch()
        {
            this->addSwitch({ "-r", "0", "specify x,y,z is relative to current position rather than home position, 0 means absolute, 1 means relative" });
        }

        DrivetrainType getDriveTrain()
        {
            int drivetrain = getSwitch("-drivetrain").toInt();
            return static_cast<DrivetrainType>(drivetrain);
        }

        void addDriveTrainSwitch()
        {
            this->addSwitch({ "-drivetrain", "0", "type of drive mode (1=forward only, 0= max degree of freedom) (default is 1)" });
        }
    };

    class ArmCommand : public DroneCommand
    {
    public:
        ArmCommand()
            : DroneCommand("Arm", "Arm the motors so the drone is ready to fly")
        {
        }

        bool execute(const DroneCommandParameters& params)
        {
            params.context->client.armDisarm(true);
            return false;
        }
    };

    class DisarmCommand : public DroneCommand
    {
    public:
        DisarmCommand()
            : DroneCommand("Disarm", "Disarm the motors so we can safely approach the drone")
        {
        }

        bool execute(const DroneCommandParameters& params)
        {
            params.context->client.armDisarm(false);
            return false;
        }
    };

    class RequestControlCommand : public DroneCommand
    {
    public:
        RequestControlCommand()
            : DroneCommand("RequestControl", "Take offboard control of drone")
        {
        }

        bool execute(const DroneCommandParameters& params)
        {
            params.context->client.enableApiControl(true);
            return false;
        }
    };

    class ReleaseControlCommand : public DroneCommand
    {
    public:
        ReleaseControlCommand()
            : DroneCommand("ReleaseControl", "Release offboard control of drone")
        {
        }

        bool execute(const DroneCommandParameters& params)
        {
            params.context->client.enableApiControl(false);
            return false;
        }
    };

    class TakeOffCommand : public DroneCommand
    {
    public:
        TakeOffCommand()
            : DroneCommand("TakeOff", "Drone takeoff to a default altitude")
        {
            this->addSwitch({ "-timeout_sec", "30", "specify time to wait after issuing command (default 30 seconds)" });
        }

        bool execute(const DroneCommandParameters& params)
        {
            params.context->client.takeoffAsync(getSwitch("-timeout_sec").toFloat());
            return false;
        }
    };

    class LandCommand : public DroneCommand
    {
    public:
        LandCommand()
            : DroneCommand("Land", "Land the drone")
        {
            this->addSwitch({ "-timeout_sec", "30", "specify time to wait after issuing command (default 30 seconds)" });
        }

        bool execute(const DroneCommandParameters& params)
        {
            params.context->client.landAsync(getSwitch("-timeout_sec").toFloat());
            return false;
        }
    };

    class GoHomeCommand : public DroneCommand
    {
    public:
        GoHomeCommand()
            : DroneCommand("GoHome", "Go back to takeoff point and land")
        {
            this->addSwitch({ "-timeout_sec", "30000", "specify time to wait after issuing command (default 30000 seconds)" });
        }

        bool execute(const DroneCommandParameters& params)
        {
            params.context->client.goHomeAsync(getSwitch("-timeout_sec").toFloat());
            return false;
        }
    };

    class GetHomeGeoPointCommand : public DroneCommand
    {
    public:
        GetHomeGeoPointCommand()
            : DroneCommand("GetGeoHomePoint", "Display the home geo_point set in the drone")
        {
        }

        bool execute(const DroneCommandParameters& params)
        {
            auto geo_point = params.context->client.getHomeGeoPoint();
            if (std::isnan(geo_point.longitude))
                params.shell_ptr->showMessage("Home point is not set!");
            else
                params.shell_ptr->showMessage(geo_point.to_string().c_str());

            return false;
        }
    };

    //move commands

    class MoveToZCommand : public DroneCommand
    {
    public:
        MoveToZCommand()
            : DroneCommand("MoveToZ", "Move to z in meters (measured from launch point) with given velocity in m/s")
        {
            this->addSwitch({ "-z", "-2.5", "Move to specified z above launch position (default -2.5 meters)" });
            this->addSwitch({ "-velocity", "0.5", "Velocity to move at (default 0.5 meters per second)" });
            this->addSwitch({ "-duration", "10", "maximum time to wait to reach altitude (default 5 seconds)" });
            addYawModeSwitches();
            addLookaheadSwitches();
        }

        bool execute(const DroneCommandParameters& params)
        {
            float z = getSwitch("-z").toFloat();
            float duration = getSwitch("-duration").toFloat();
            float velocity = getSwitch("-velocity").toFloat();
            float lookahead = getSwitch("-lookahead").toFloat();
            float adaptive_lookahead = getSwitch("-adaptive_lookahead").toFloat();
            CommandContext* context = params.context;

            context->tasker.execute([=]() {
                context->client.moveToZAsync(z, velocity, duration, getYawMode(), lookahead, adaptive_lookahead);
            });

            return false;
        }
    };

    class RotateByYawRateCommand : public DroneCommand
    {
    public:
        RotateByYawRateCommand()
            : DroneCommand("Rotate", "Rotate with angular velocity in degrees/s for given time in seconds")
        {
            this->addSwitch({ "-duration", "5", "maximum time to wait (default 5 seconds)" });
            this->addSwitch({ "-yaw_rate", "20", "degrees per second (default 20 degrees per second))" });
        }

        bool execute(const DroneCommandParameters& params)
        {
            float yaw_rate = getSwitch("-yaw_rate").toFloat();
            float duration = getSwitch("-duration").toFloat();
            CommandContext* context = params.context;

            context->tasker.execute([=]() {
                context->client.rotateByYawRateAsync(yaw_rate, duration);
            });

            return false;
        }
    };

    class RotateToYawCommand : public DroneCommand
    {
    public:
        RotateToYawCommand()
            : DroneCommand("RotateTo", "Rotate to a particular angle")
        {
            this->addSwitch({ "-yaw_margin", "5", "how accurate to be (default within 5 degrees)" });
            this->addSwitch({ "-yaw", "0", "degrees (default 0)" });
            this->addSwitch({ "-duration", "10", "maximum time to wait to reach given yaw (default 5 seconds)" });
        }

        bool execute(const DroneCommandParameters& params)
        {
            float yaw = getSwitch("-yaw").toFloat();
            float duration = getSwitch("-duration").toFloat();
            float margin = getSwitch("-yaw_margin").toFloat();
            CommandContext* context = params.context;

            context->tasker.execute([=]() {
                context->client.rotateToYawAsync(yaw, duration, margin);
            });

            return false;
        }
    };

    class HoverCommand : public DroneCommand
    {
    public:
        HoverCommand()
            : DroneCommand("Hover", "Enter hover mode")
        {
        }

        bool execute(const DroneCommandParameters& params)
        {
            CommandContext* context = params.context;
            context->tasker.execute([=]() {
                context->client.hoverAsync();
            });

            return false;
        }
    };

    class GetPositionCommand : public DroneCommand
    {
    public:
        GetPositionCommand()
            : DroneCommand("Pos", "Get the current position")
        {
        }

        bool execute(const DroneCommandParameters& params)
        {
            CommandContext* context = params.context;
            context->tasker.execute([=]() {
                auto pos = context->client.getMultirotorState().getPosition();
                auto gps = context->client.getMultirotorState().gps_location;

                std::cout << "Local position: x=" << pos.x() << ", y=" << pos.y() << ", z=" << pos.z() << std::endl;
                std::cout << "Global position: lat=" << gps.latitude << ", lon=" << gps.longitude << ", alt=" << gps.altitude << std::endl;
            });

            return false;
        }
    };

    class MoveOnPathCommand : public DroneCommand
    {
    public:
        MoveOnPathCommand()
            : DroneCommand("MoveOnPath", "Move along the given series of x,y,z coordinates with the specified velocity")
        {
            this->addSwitch({ "-path", "", "a series of x,y,z coordinates separated by commas, e.g. 0,0,-10,100,0,-10,100,100,-10,0,100,-10,0,0,-10 will fly a square box pattern" });
            this->addSwitch({ "-velocity", "2.5", "the velocity in meters per second (default 2.5)" });
            this->addSwitch({ "-duration", "0", "maximum time to wait to reach end of path (default no wait)" });
            addYawModeSwitches();
            addDriveTrainSwitch();
            addLookaheadSwitches();
            addRelativeSwitch();
        }

        bool execute(const DroneCommandParameters& params)
        {
            CommandContext* context = params.context;
            std::string points = getSwitch("-path").value;

            vector<Vector3r> path;
            size_t start = 0;
            int count = 0;
            float x = 0, y = 0, z = 0;
            for (size_t i = 0, n = points.size(); i < n; i++) {
                char c = points[i];
                if (c == ',' || i + 1 == n) {
                    if (i > start) {
                        if (i + 1 == n) i++;
                        std::string number = points.substr(start, i - start);
                        real_T v = static_cast<real_T>(atof(number.c_str()));
                        switch (count++) {
                        case 0:
                            x = v;
                            break;
                        case 1:
                            y = v;
                            break;
                        case 2:
                            z = v;
                            count = 0;
                            path.push_back(Vector3r(x, y, z));
                            break;
                        default:
                            break;
                        }
                        start = i + 1;
                    }
                }
            }
            if (path.size() == 0) {
                std::cout << "incomplete path, please provide at least 3 numbers defining a 3d point" << endl;
                return false;
            }

            auto pos = context->client.getMultirotorState().getPosition();
            if (getSwitch("-r").toInt() == 1) {
                for (size_t i = 0, n = path.size(); i < n; i++) {
                    Vector3r v = path[i];
                    path[i] = Vector3r(v.x() + pos.x(), v.y() + pos.y(), v.z() + pos.z());
                }
            }

            float velocity = getSwitch("-velocity").toFloat();
            float duration = getSwitch("-duration").toFloat();
            float lookahead = getSwitch("-lookahead").toFloat();
            float adaptive_lookahead = getSwitch("-adaptive_lookahead").toFloat();
            auto drivetrain = getDriveTrain();
            auto yawMode = getYawMode();

            context->tasker.execute([=]() {
                context->client.moveOnPathAsync(path, velocity, duration, drivetrain, yawMode, lookahead, adaptive_lookahead);
            });

            return false;
        }
    };

    class MoveToPositionCommand : public DroneCommand
    {
    public:
        MoveToPositionCommand()
            : DroneCommand("MoveToPosition", "Move to x,y,z with specified velocity")
        {
            this->addSwitch({ "-x", "0", "x position in meters (default 0)" });
            this->addSwitch({ "-y", "0", "y position in meters (default 0)" });
            this->addSwitch({ "-z", "-2.5", "z position in meters (default -2.5)" });
            this->addSwitch({ "-velocity", "0.5", "the velocity to approach the position in meters per second (default 0.5)" });
            this->addSwitch({ "-duration", "0", "maximum time to wait to reach position (default no wait)" });
            addYawModeSwitches();
            addDriveTrainSwitch();
            addLookaheadSwitches();
            addRelativeSwitch();
        }

        void MoveToPosition(CommandContext* context, float x, float y, float z, float velocity, float duration, DrivetrainType drivetrain, YawMode yawMode, float lookahead, float adaptive_lookahead)
        {
            context->client.moveToPositionAsync(x, y, z, velocity, duration, drivetrain, yawMode, lookahead, adaptive_lookahead);
        }

        bool execute(const DroneCommandParameters& params)
        {
            CommandContext* context = params.context;
            float x = getSwitch("-x").toFloat();
            float y = getSwitch("-y").toFloat();
            float z = getSwitch("-z").toFloat();

            if (getSwitch("-r").toInt() == 1) {
                auto pos = context->client.getMultirotorState().getPosition();
                x += pos.x();
                y += pos.y();
                z += pos.z();
            }

            float velocity = getSwitch("-velocity").toFloat();
            float duration = getSwitch("-duration").toFloat();
            float lookahead = getSwitch("-lookahead").toFloat();
            float adaptive_lookahead = getSwitch("-adaptive_lookahead").toFloat();
            auto drivetrain = getDriveTrain();
            auto yawMode = getYawMode();

            context->tasker.execute([=]() {
                MoveToPosition(context, x, y, z, velocity, duration, drivetrain, yawMode, lookahead, adaptive_lookahead);
            });

            return false;
        }
    };

    class MoveByManualCommand : public DroneCommand
    {
    public:
        MoveByManualCommand()
            : DroneCommand("MoveByManual", "Move using remote control manually")
        {
            this->addSwitch({ "-vx", "0", "velocity in x direction in meters per second (default 0)" });
            this->addSwitch({ "-vy", "0", "velocity in y direction in meters per second (default 0)" });
            this->addSwitch({ "-z", "-2.5", "z position in meters (default -2.5)" });
            this->addSwitch({ "-duration", "0", "the duration of this command in seconds (default no wait)" });

            addDriveTrainSwitch();
            addLookaheadSwitches();
            addYawModeSwitches();
        }

        bool execute(const DroneCommandParameters& params)
        {
            float vx = getSwitch("-vx").toFloat();
            float vy = getSwitch("-vy").toFloat();
            float z = getSwitch("-z").toFloat();
            float duration = getSwitch("-duration").toFloat();
            auto drivetrain = getDriveTrain();
            auto yawMode = getYawMode();
            CommandContext* context = params.context;

            context->tasker.execute([=]() {
                context->client.moveByManualAsync(vx, vy, z, duration, drivetrain, yawMode);
            });

            return false;
        }
    };

    class MoveByAngleZCommand : public DroneCommand
    {
    public:
        MoveByAngleZCommand()
            : DroneCommand("MoveByAngleZ", "Move with specified roll and pitch, leaving z as-is")
        {
            this->addSwitch({ "-pitch", "0", "pitch angle in degrees (default 0)" });
            this->addSwitch({ "-roll", "0", "roll angle in degrees (default 0)" });
            this->addSwitch({ "-z", "-2.5", "z position in meters (default -2.5)" });
            this->addSwitch({ "-duration", "5", "the duration of this command in seconds (default 5)" });
            this->addSwitch({ "-yaw", "0", "target yaw angle in degrees (default is 0)" });
        }

        bool execute(const DroneCommandParameters& params)
        {
            float pitch = getSwitch("-pitch").toFloat();
            float roll = getSwitch("-roll").toFloat();
            float z = getSwitch("-z").toFloat();
            float yaw = getSwitch("-yaw").toFloat();
            float duration = getSwitch("-duration").toFloat();
            CommandContext* context = params.context;

            context->tasker.execute([=]() {
                context->client.moveByRollPitchYawZAsync(roll, pitch, yaw, z, duration);
            });

            return false;
        }
    };

    class MoveByAngleThrottleCommand : public DroneCommand
    {
    public:
        MoveByAngleThrottleCommand()
            : DroneCommand("MoveByAngleThrottle", "Move with specified roll and pitch, leaving z as-is")
        {
            this->addSwitch({ "-pitch", "0", "pitch angle in degrees (default 0)" });
            this->addSwitch({ "-roll", "0", "roll angle in degrees (default 0)" });
            this->addSwitch({ "-yaw_rate", "0", "target yaw rate in degrees/sec (default is 0)" });
            this->addSwitch({ "-throttle", "-2.5", "z position in meters (default -2.5)" });
            this->addSwitch({ "-duration", "5", "the duration of this command in seconds (default 5)" });
        }

        bool execute(const DroneCommandParameters& params)
        {
            float pitch = getSwitch("-pitch").toFloat();
            float roll = getSwitch("-roll").toFloat();
            float yaw_rate = getSwitch("-yaw_rate").toFloat();
            float throttle = getSwitch("-throttle").toFloat();
            float duration = getSwitch("-duration").toFloat();
            CommandContext* context = params.context;

            context->tasker.execute([=]() {
                context->client.moveByRollPitchYawrateThrottleAsync(roll, pitch, yaw_rate, throttle, duration);
            });

            return false;
        }
    };

    class MoveByVelocityCommand : public DroneCommand
    {
    public:
        MoveByVelocityCommand()
            : DroneCommand("MoveByVelocity", "Move by specified velocity components vx, vy, vz, axis wrt body")
        {
            this->addSwitch({ "-vx", "0", "velocity in x direction in meters per second (default 0)" });
            this->addSwitch({ "-vy", "0", "velocity in y direction in meters per second (default 0)" });
            this->addSwitch({ "-vz", "0", "velocity in z direction in meters per second (default 0)" });
            this->addSwitch({ "-duration", "5", "the duration of this command in seconds (default 5)" });
            addDriveTrainSwitch();
            addYawModeSwitches();
        }

        bool execute(const DroneCommandParameters& params)
        {
            float vx = getSwitch("-vx").toFloat();
            float vy = getSwitch("-vy").toFloat();
            float vz = getSwitch("-vz").toFloat();
            float duration = getSwitch("-duration").toFloat();
            auto drivetrain = getDriveTrain();
            auto yawMode = getYawMode();
            CommandContext* context = params.context;

            context->tasker.execute([=]() {
                context->client.moveByVelocityAsync(vx, vy, vz, duration, drivetrain, yawMode);
            });

            return false;
        }
    };

    class MoveByVelocityZCommand : public DroneCommand
    {
    public:
        MoveByVelocityZCommand()
            : DroneCommand("MoveByVelocityZ", "Move by specified velocity components vx, vy, and fixed z")
        {
            this->addSwitch({ "-vx", "0", "velocity in x direction in meters per second (default 0)" });
            this->addSwitch({ "-vy", "0", "velocity in y direction in meters per second (default 0)" });
            this->addSwitch({ "-z", "-2.5", "z position in meters (default -2.5)" });
            this->addSwitch({ "-duration", "5", "the duration of this command in seconds (default 5)" });
            addDriveTrainSwitch();
            addYawModeSwitches();
        }

        bool execute(const DroneCommandParameters& params)
        {
            float vx = getSwitch("-vx").toFloat();
            float vy = getSwitch("-vy").toFloat();
            float z = getSwitch("-z").toFloat();
            float duration = getSwitch("-duration").toFloat();
            auto drivetrain = getDriveTrain();
            auto yawMode = getYawMode();
            CommandContext* context = params.context;

            context->tasker.execute([=]() {
                context->client.moveByVelocityZAsync(vx, vy, z, duration, drivetrain, yawMode);
            });

            return false;
        }
    };

    class SetSafetyCommand : public DroneCommand
    {
    public:
        SetSafetyCommand()
            : DroneCommand("SetSafety", "Set safety parameters")
        {
            this->addSwitch({ "-safety_flags", "1", "0 = none, -1 = all, 1 = geofence, 2 = obstacle (default is 1)" });
            this->addSwitch({ "-obs_clearance", "", "safe distance from obstacles (no default)" });
            this->addSwitch({ "-obs_avoidance_vel", "0.5", "velocity to move away from obstacles (default 0.5)" });
            this->addSwitch({ "-obs_strategy", "0", "0 = none/exception, 1 = closest move, 2 = opposite move (default 0)" });
            this->addSwitch({ "-xy_length", "", "geofence boundary size (no default)" });
            this->addSwitch({ "-max_z", "", "geofence min altitude (no default)" });
            this->addSwitch({ "-min_z", "", "geofence max altitude (no default)" });
            this->addSwitch({ "-origin", "", "geofence origin as 'x,y,z' local position in meters (no default)" });
        }

        bool execute(const DroneCommandParameters& params)
        {
            int safety_flags_switch = getSwitch("-safety_flags").toInt();
            float obs_clearance = getSwitch("-obs_clearance").toFloat();
            float obs_avoidance_vel = getSwitch("-obs_avoidance_vel").toFloat();
            uint obs_strategy = std::stoi(getSwitch("-obs_strategy").value);
            CommandContext* context = params.context;

            uint safety_flags = static_cast<uint>(safety_flags_switch);
            if (safety_flags_switch == -1)
                safety_flags = Utils::max<unsigned int>();

            Vector3r origin(Utils::nan<float>(), Utils::nan<float>(), Utils::nan<float>());
            float max_z = Utils::nan<float>(), min_z = Utils::nan<float>(), xy_length = Utils::nan<float>();

            std::string s_xy_length = getSwitch("-xy_length").value;
            if (!s_xy_length.empty()) {
                xy_length = getSwitch("-xy_length").toFloat();
            }

            std::string s_min_z = getSwitch("-min_z").value;
            if (!s_min_z.empty()) {
                min_z = getSwitch("-min_z").toFloat();
            }

            std::string s_max_z = getSwitch("-max_z").value;
            if (!s_max_z.empty()) {
                max_z = getSwitch("-max_z").toFloat();
            }

            std::string s_origin = getSwitch("-origin").value;
            if (!s_origin.empty()) {
                std::vector<std::string> parts = Utils::split(s_origin, ",", 1);
                if (parts.size() == 3) {
                    origin[0] = std::stof(parts[0]);
                    origin[1] = std::stof(parts[1]);
                    origin[2] = std::stof(parts[2]);
                }
                else {
                    throw std::invalid_argument("-origin argument is expecting 'x,y,z' (separated by commas and no spaces in between)");
                }
            }

            context->tasker.execute([=]() {
                context->client.setSafety(SafetyEval::SafetyViolationType(safety_flags), obs_clearance, SafetyEval::ObsAvoidanceStrategy(obs_strategy), obs_avoidance_vel, origin, xy_length, min_z, max_z);
            });

            return false;
        }
    };

    class BackForthByAngleCommand : public DroneCommand
    {
    public:
        BackForthByAngleCommand()
            : DroneCommand("BackForthByAngle", "Make drone go in linear motion back and forth using pitch/roll")
        {
            this->addSwitch({ "-pitch", "0", "pitch angle in degrees (default 0)" });
            this->addSwitch({ "-roll", "0", "roll angle in degrees (default 0)" });
            this->addSwitch({ "-z", "-2.5", "z position in meters (default -2.5)" });
            this->addSwitch({ "-duration", "5", "the duration of this command in seconds (default 5)" });
            this->addSwitch({ "-yaw", "0", "target yaw angle in degrees (default is 0)" });
            this->addSwitch({ "-pause_time", "0", "pause time between each run back and forth in seconds (default 0)" });
            this->addSwitch({ "-iterations", "10000", "number of times to repeat the task (default 10000)" });
        }

        bool execute(const DroneCommandParameters& params)
        {
            float pitch = getSwitch("-pitch").toFloat();
            float roll = getSwitch("-roll").toFloat();
            float z = getSwitch("-z").toFloat();
            float duration = getSwitch("-duration").toFloat();
            float yaw = getSwitch("-yaw").toFloat();
            TTimeDelta pause_time = getSwitch("-pause_time").toTimeDelta();
            int iterations = getSwitch("-iterations").toInt();
            CommandContext* context = params.context;

            context->tasker.execute([=]() {
                context->client.moveByRollPitchYawZAsync(roll, pitch, yaw, z, duration);
                if (!context->client.waitOnLastTask()) {
                    throw std::runtime_error("BackForthByAngleCommand canceled");
                }
                context->client.hoverAsync();
                context->sleep_for(pause_time);
                context->client.moveByRollPitchYawZAsync(-roll, -pitch, yaw, z, duration);
                if (!context->client.waitOnLastTask()) {
                    throw std::runtime_error("BackForthByAngleCommand canceled");
                }
            },
                                    iterations);

            return false;
        }
    };

    class BackForthByPositionCommand : public DroneCommand
    {
    public:
        BackForthByPositionCommand()
            : DroneCommand("BackForthByPosition", "Make drone go in linear motion back and forth two x positions")
        {
            this->addSwitch({ "-length", "2.5", "length of each run on the x-axis (default 2.5)" });
            this->addSwitch({ "-velocity", "0.5", "velocity in meters per second (default 0.5)" });
            this->addSwitch({ "-z", "-2.5", "z position in meters (default -2.5)" });
            this->addSwitch({ "-duration", "5", "the duration of this command in seconds (default 5)" });
            this->addSwitch({ "-pause_time", "0", "pause time between each run back and forth in seconds (default 0)" });
            this->addSwitch({ "-iterations", "10000", "number of times to repeat the task (default 10000)" });
            addLookaheadSwitches();
            addDriveTrainSwitch();
            addYawModeSwitches();
        }

        bool execute(const DroneCommandParameters& params)
        {
            float length = getSwitch("-length").toFloat();
            float z = getSwitch("-z").toFloat();
            float velocity = getSwitch("-velocity").toFloat();
            TTimeDelta pause_time = getSwitch("-pause_time").toTimeDelta();
            int iterations = getSwitch("-iterations").toInt();
            auto drivetrain = getDriveTrain();
            float lookahead = getSwitch("-lookahead").toFloat();
            float adaptive_lookahead = getSwitch("-adaptive_lookahead").toFloat();
            auto yawMode = getYawMode();
            CommandContext* context = params.context;

            context->tasker.execute([=]() {
                context->client.moveToPositionAsync(length, 0, z, velocity, 0, drivetrain, yawMode, lookahead, adaptive_lookahead);
                if (!context->client.waitOnLastTask()) {
                    throw std::runtime_error("BackForthByPositionCommand canceled");
                }
                context->client.hoverAsync();
                context->sleep_for(pause_time);
                context->client.moveToPositionAsync(-length, 0, z, velocity, 0, drivetrain, yawMode, lookahead, adaptive_lookahead);
                if (!context->client.waitOnLastTask()) {
                    throw std::runtime_error("BackForthByPositionCommand canceled");
                }
                context->client.hoverAsync();
                context->sleep_for(pause_time);
            },
                                    iterations);

            return false;
        }
    };

    class SquareByAngleCommand : public DroneCommand
    {
    public:
        SquareByAngleCommand()
            : DroneCommand("SquareByAngle", "Make drone go in square using pitch/roll")
        {
            this->addSwitch({ "-pitch", "0", "pitch angle in degrees (default 0)" });
            this->addSwitch({ "-roll", "0", "roll angle in degrees (default 0)" });
            this->addSwitch({ "-z", "-2.5", "z position in meters (default -2.5)" });
            this->addSwitch({ "-yaw", "0", "target yaw angle in degrees (default is 0)" });
            this->addSwitch({ "-pause_time", "0", "pause time between each run back and forth in seconds (default 0)" });
            this->addSwitch({ "-iterations", "10000", "number of times to repeat the task (default 10000)" });
        }

        bool execute(const DroneCommandParameters& params)
        {
            float pitch = getSwitch("-pitch").toFloat();
            float roll = getSwitch("-roll").toFloat();
            float z = getSwitch("-z").toFloat();
            float yaw = getSwitch("-yaw").toFloat();
            TTimeDelta pause_time = getSwitch("-pause_time").toTimeDelta();
            int iterations = getSwitch("-iterations").toInt();
            CommandContext* context = params.context;

            context->tasker.execute([=]() {
                context->client.moveByRollPitchYawZAsync(-roll, pitch, yaw, z, 0);
                if (!context->client.waitOnLastTask()) {
                    throw std::runtime_error("SquareByAngleCommand canceled");
                }
                context->client.hoverAsync();
                context->sleep_for(pause_time);

                context->client.moveByRollPitchYawZAsync(-roll, -pitch, yaw, z, 0);
                if (!context->client.waitOnLastTask()) {
                    throw std::runtime_error("SquareByAngleCommand canceled");
                }
                context->client.hoverAsync();
                context->sleep_for(pause_time);

                context->client.moveByRollPitchYawZAsync(roll, -pitch, yaw, z, 0);
                if (!context->client.waitOnLastTask()) {
                    throw std::runtime_error("SquareByAngleCommand canceled");
                }
                context->client.hoverAsync();
                context->sleep_for(pause_time);

                context->client.moveByRollPitchYawZAsync(-roll, -pitch, yaw, z, 0);
                if (!context->client.waitOnLastTask()) {
                    throw std::runtime_error("SquareByAngleCommand canceled");
                }
                context->client.hoverAsync();
                context->sleep_for(pause_time);
            },
                                    iterations);

            return false;
        }
    };

    class SquareByPositionCommand : public DroneCommand
    {
    public:
        SquareByPositionCommand()
            : DroneCommand("SquareByPosition", "Make drone go in square using position commands")
        {
            this->addSwitch({ "-length", "2.5", "length of each side (default 2.5)" });
            this->addSwitch({ "-velocity", "0.5", "velocity in meters per second (default 0.5)" });
            this->addSwitch({ "-z", "-2.5", "z position in meters (default -2.5)" });
            this->addSwitch({ "-pause_time", "0", "pause time between each run back and forth in seconds (default 0)" });
            this->addSwitch({ "-iterations", "10000", "number of times to repeat the task (default 10000)" });
            addRelativeSwitch();
            addLookaheadSwitches();
            addDriveTrainSwitch();
            addYawModeSwitches();
        }

        bool execute(const DroneCommandParameters& params)
        {
            CommandContext* context = params.context;

            float length = getSwitch("-length").toFloat();
            float z = getSwitch("-z").toFloat();
            float velocity = getSwitch("-velocity").toFloat();
            TTimeDelta pause_time = getSwitch("-pause_time").toTimeDelta();
            int iterations = getSwitch("-iterations").toInt();
            auto drivetrain = getDriveTrain();
            float lookahead = getSwitch("-lookahead").toFloat();
            float adaptive_lookahead = getSwitch("-adaptive_lookahead").toFloat();
            auto yawMode = getYawMode();

            float x = 0, y = 0;
            if (getSwitch("-r").toInt() == 1) {
                auto pos = context->client.getMultirotorState().getPosition();
                x += pos.x();
                y += pos.y();
                z += pos.z();
            }

            context->tasker.execute([=]() {
                context->client.moveToPositionAsync(x + length, y - length, z, velocity, 0, drivetrain, yawMode, lookahead, adaptive_lookahead);
                if (!context->client.waitOnLastTask()) {
                    throw std::runtime_error("SquareByPositionCommand canceled");
                }
                context->client.hoverAsync();
                context->sleep_for(pause_time);

                context->client.moveToPositionAsync(x - length, y - length, z, velocity, 0, drivetrain, yawMode, lookahead, adaptive_lookahead);
                if (!context->client.waitOnLastTask()) {
                    throw std::runtime_error("SquareByPositionCommand canceled");
                }
                context->client.hoverAsync();
                context->sleep_for(pause_time);

                context->client.moveToPositionAsync(x - length, y + length, z, velocity, 0, drivetrain, yawMode, lookahead, adaptive_lookahead);
                if (!context->client.waitOnLastTask()) {
                    throw std::runtime_error("SquareByPositionCommand canceled");
                }
                context->client.hoverAsync();
                context->sleep_for(pause_time);

                context->client.moveToPositionAsync(x + length, y + length, z, velocity, 0, drivetrain, yawMode, lookahead, adaptive_lookahead);
                if (!context->client.waitOnLastTask()) {
                    throw std::runtime_error("SquareByPositionCommand canceled");
                }
                context->client.hoverAsync();
                context->sleep_for(pause_time);
            },
                                    iterations);

            return false;
        }
    };

    class SquareByPathCommand : public DroneCommand
    {
    public:
        SquareByPathCommand()
            : DroneCommand("SquareByPath", "Make drone go in square using path commands")
        {
            this->addSwitch({ "-length", "2.5", "length of each side (default 2.5)" });
            this->addSwitch({ "-velocity", "0.5", "velocity in meters per second (default 0.5)" });
            this->addSwitch({ "-z", "-2.5", "z position in meters (default -2.5)" });
            this->addSwitch({ "-duration", "60", "the total duration of this command in seconds (default 60)" });
            this->addSwitch({ "-pause_time", "0", "pause time between each run back and forth in seconds (default 0)" });
            this->addSwitch({ "-iterations", "10000", "number of times to repeat the task (default 10000)" });
            this->addSwitch({ "-path_rep", "1000", "number of times around the square (default 1000)" });
            addRelativeSwitch();
            addLookaheadSwitches();
            addDriveTrainSwitch();
            addYawModeSwitches();
        }

        bool execute(const DroneCommandParameters& params)
        {
            float length = getSwitch("-length").toFloat();
            float z = getSwitch("-z").toFloat();
            float velocity = getSwitch("-velocity").toFloat();
            float duration = getSwitch("-duration").toFloat();
            int iterations = getSwitch("-iterations").toInt();
            auto drivetrain = getDriveTrain();
            float lookahead = getSwitch("-lookahead").toFloat();
            float adaptive_lookahead = getSwitch("-adaptive_lookahead").toFloat();
            int path_rep = std::stoi(getSwitch("-path_rep").value);
            auto yawMode = getYawMode();
            float x = 0, y = 0;
            CommandContext* context = params.context;
            if (getSwitch("-r").toInt() == 1) {
                auto pos = context->client.getMultirotorState().getPosition();
                x += pos.x();
                y += pos.y();
                z += pos.z();
            }

            std::vector<Vector3r> path;
            for (int i = 0; i < path_rep; ++i) {
                path.push_back(Vector3r(x + length, y - length, z));
                path.push_back(Vector3r(x - length, y - length, z));
                path.push_back(Vector3r(x - length, y + length, z));
                path.push_back(Vector3r(x + length, y + length, z));
            }

            timer.start();

            context->tasker.execute([=]() {
                if (timer.seconds() > duration) {
                    context->client.hoverAsync();
                    throw std::runtime_error("SquareByPathCommand -duration reached");
                }
                context->client.moveOnPathAsync(path, velocity, duration, drivetrain, yawMode, lookahead, adaptive_lookahead);
                if (!context->client.waitOnLastTask()) {
                    throw std::runtime_error("SquareByPathCommand canceled");
                }
            },
                                    iterations);

            return false;
        }

        Timer timer;
    };

    class CircleByPositionCommand : public DroneCommand
    {
    public:
        CircleByPositionCommand()
            : DroneCommand("CircleByPosition", "Make drone go in square using position commands")
        {
            this->addSwitch({ "-radius", "2.5", "radius of circle (default 2.5)" });
            this->addSwitch({ "-velocity", "0.5", "velocity in meters per second (default 0.5)" });
            this->addSwitch({ "-z", "-2.5", "z position in meters (default -2.5)" });
            this->addSwitch({ "-seg_length", "0.2", "circle is approximated using strait segments (default length .2 meters)" });
            this->addSwitch({ "-duration", "0", "the duration of this command in seconds (default no wait)" });
            this->addSwitch({ "-pause_time", "0", "pause time between each run back and forth in seconds (default 0)" });
            this->addSwitch({ "-iterations", "10000", "number of times to repeat the task (default 10000)" });

            addRelativeSwitch();
            addLookaheadSwitches();
            addDriveTrainSwitch();
            addYawModeSwitches();
        }

        bool execute(const DroneCommandParameters& params)
        {
            CommandContext* context = params.context;
            float radius = getSwitch("-radius").toFloat();
            float z = getSwitch("-z").toFloat();
            float seg_length = getSwitch("-seg_length").toFloat();
            TTimeDelta pause_time = getSwitch("-pause_time").toTimeDelta();
            float velocity = getSwitch("-velocity").toFloat();
            float duration = getSwitch("-duration").toFloat();
            auto drivetrain = getDriveTrain();
            float lookahead = getSwitch("-lookahead").toFloat();
            float adaptive_lookahead = getSwitch("-adaptive_lookahead").toFloat();
            int iterations = getSwitch("-iterations").toInt();
            auto yawMode = getYawMode();
            float cx = 0, cy = 0;
            if (getSwitch("-r").toInt() == 1) {
                auto pos = context->client.getMultirotorState().getPosition();
                cx += pos.x();
                cy += pos.y();
                z += pos.z();
            }

            float angle = std::acos(1 - (seg_length * seg_length / (2 * radius * radius)));
            if (std::isnan(angle))
                throw std::invalid_argument(common_utils::Utils::stringf("radius=%f and seg_length=%f doesn't form valid circle", radius, seg_length));
            int seg_count = common_utils::Utils::floorToInt(2 * M_PIf / angle);
            float seg_angle = 2 * M_PIf / seg_count;

            context->tasker.execute([=]() {
                for (float seg = 0; seg < seg_count; ++seg) {
                    float x = cx + std::cos(seg_angle * seg) * radius;
                    float y = cy + std::sin(seg_angle * seg) * radius;
                    context->client.moveToPositionAsync(x, y, z, velocity, duration, drivetrain, yawMode, lookahead, adaptive_lookahead);
                    if (!context->client.waitOnLastTask()) {
                        throw std::runtime_error("CircleByPositionCommand canceled");
                    }
                    context->client.hoverAsync();
                    context->sleep_for(pause_time);
                }
            },
                                    iterations);

            return false;
        }
    };

    class CircleByPathCommand : public DroneCommand
    {
    public:
        CircleByPathCommand()
            : DroneCommand("CircleByPath", "Make drone go in circle using path commands")
        {
            this->addSwitch({ "-radius", "2.5", "radius of circle (default 2.5)" });
            this->addSwitch({ "-velocity", "0.5", "velocity in meters per second (default 0.5)" });
            this->addSwitch({ "-z", "-2.5", "z position in meters (default -2.5)" });
            this->addSwitch({ "-duration", "60", "the total duration of this command in seconds (default 60)" });
            this->addSwitch({ "-pause_time", "0", "pause time between each run back and forth in seconds (default 0)" });
            this->addSwitch({ "-iterations", "10000", "number of times to repeat the task (default 10000)" });
            this->addSwitch({ "-path_rep", "1000", "number of times around the square (default 1000)" });
            this->addSwitch({ "-plane", "xy", "which plane to fly (default xy)" });
            this->addSwitch({ "-seg_length", "0.2", "circle is approximated using strait segments (default length .2 meters)" });
            addLookaheadSwitches();
            addDriveTrainSwitch();
            addYawModeSwitches();
            addRelativeSwitch();
        }

        bool execute(const DroneCommandParameters& params)
        {
            using std::swap;

            float radius = getSwitch("-radius").toFloat();
            float z_path = getSwitch("-z").toFloat();
            float seg_length = getSwitch("-seg_length").toFloat();
            float velocity = getSwitch("-velocity").toFloat();
            float duration = getSwitch("-duration").toFloat();
            float lookahead = getSwitch("-lookahead").toFloat();
            float adaptive_lookahead = getSwitch("-adaptive_lookahead").toFloat();
            auto drivetrain = getDriveTrain();
            int iterations = getSwitch("-iterations").toInt();
            int path_rep = std::stoi(getSwitch("-path_rep").value);
            string plane = getSwitch("-plane").value;
            auto yawMode = getYawMode();
            CommandContext* context = params.context;
            float cx = 0, cy = 0;
            if (getSwitch("-r").toInt() == 1) {
                auto pos = context->client.getMultirotorState().getPosition();
                cx += pos.x();
                cy += pos.y();
                z_path += pos.z();
            }

            float angle = std::acos(1 - (seg_length * seg_length / (2 * radius * radius)));
            if (std::isnan(angle))
                throw std::invalid_argument(common_utils::Utils::stringf("radius=%f and seg_length=%f doesn't form valid circle", radius, seg_length));
            int seg_count = common_utils::Utils::floorToInt(2 * M_PIf / angle);
            float seg_angle = 2 * M_PIf / seg_count;

            const Vector3r origin = plane != "xy" && plane != "yx" ? Vector3r(cx, cy, z_path + radius) : Vector3r(cx, cy, z_path);

            std::vector<Vector3r> path;
            path.reserve(path_rep * seg_count);
            for (int i = 0; i < path_rep; ++i) {
                for (float seg = 0; seg < seg_count; ++seg) {
                    float x = std::cos(seg_angle * seg) * radius;
                    float y = std::sin(seg_angle * seg) * radius;
                    float z = 0;

                    if (plane == "yx")
                        swap(x, y);
                    else if (plane == "xz")
                        swap(y, z);
                    else if (plane == "zx") {
                        swap(y, z);
                        swap(z, x);
                    }
                    else if (plane == "yz")
                        swap(x, z);
                    else if (plane == "zy") {
                        swap(x, z);
                        swap(z, y);
                    }
                    //else leave as it is

                    path.push_back(Vector3r(x, y, z) + origin);
                }
            }
            timer.start();
            context->tasker.execute([=]() {
                if (timer.seconds() > duration) {
                    context->client.hoverAsync();
                    throw std::runtime_error("CircleByPath -duration reached");
                }
                context->client.moveOnPathAsync(path, velocity, duration, drivetrain, yawMode, lookahead, adaptive_lookahead);
                if (!context->client.waitOnLastTask()) {
                    throw std::runtime_error("CircleByPath canceled");
                }
            },
                                    iterations);

            return false;
        }
        Timer timer;
    };

    class RecordPoseCommand : public DroneCommand
    {
    public:
        RecordPoseCommand()
            : DroneCommand("RecordPose", "Append a single pose snapshot to a log file named 'rec_pos.log' in your $HOME folder\n\
Each record is tab separated floating point numbers containing GPS lat,lon,alt,z,health, position x,y,z, and quaternion w,x,y,z")
        {
        }

        bool execute(const DroneCommandParameters& params)
        {
            //TODO: get these in one call
            Vector3r position = params.context->client.getMultirotorState().getPosition();
            Quaternionr quaternion = params.context->client.getMultirotorState().getOrientation();
            GeoPoint gps_point = params.context->client.getMultirotorState().gps_location;

            params.shell_ptr->showMessage(gps_point.to_string());
            params.shell_ptr->showMessage(VectorMath::toString(position));
            params.shell_ptr->showMessage(VectorMath::toString(quaternion));

            string line = Utils::stringf("%f\t%f\t%f\t%f\t%d\t%f\t%f\t%f\t%f\t%f\t%f\t%f\n",
                                         gps_point.latitude,
                                         gps_point.longitude,
                                         gps_point.altitude, //TODO: gps_point.height, gps_point.health,
                                         position[0],
                                         position[1],
                                         position[2],
                                         quaternion.w(),
                                         quaternion.x(),
                                         quaternion.y(),
                                         quaternion.z());

            std::string folder_path_name = FileSystem::getLogFolderPath(true);
            std::string file_path_name = FileSystem::getLogFileNamePath(folder_path_name, "rec_pos", "", ".log", false);
            FileSystem::appendLineToFile(file_path_name, line);

            return false;
        }
    };

    //shell.addCommand("PlayPose", &playPoseCommand, "Play position, quaternion and GPS coordinates of drone from log file");

    class PlayPoseCommand : public DroneCommand
    {
    public:
        PlayPoseCommand()
            : DroneCommand("PlayPose", "Fly the drone through each recorded pose found in log file named 'rec_pos.log' in your $HOME folder\n\
See RecordPose for information about log file format")
        {
            this->addSwitch({ "-velocity", "0.5", "Velocity to move at (default 0.5 meters per second)" });
            addLookaheadSwitches();
        }

        bool execute(const DroneCommandParameters& params)
        {
            float velocity = getSwitch("-velocity").toFloat();
            float lookahead = getSwitch("-lookahead").toFloat();
            float adaptive_lookahead = getSwitch("-adaptive_lookahead").toFloat();
            CommandContext* context = params.context;

            context->tasker.execute([=]() {
                std::ifstream file;
                //TODO: shouldn't we pass folder path?
                std::string file_path_name = FileSystem::getLogFileNamePath(FileSystem::getAppDataFolder(),
                                                                            "rec_pos",
                                                                            "",
                                                                            ".log",
                                                                            false);
                file.exceptions(file.exceptions() | std::ios::failbit);
                FileSystem::openTextFile(file_path_name, file);

                Vector3r position;
                Quaternionr quaternion;
                GeoPoint gps_point;
                float pitch, roll, yaw;

                while (!file.eof()) {
                    string line = FileSystem::readLineFromFile(file);
                    if (line.length() > 0) {
                        //parse
                        std::stringstream line_ss(line);
                        line_ss >> gps_point.latitude >> gps_point.longitude >> gps_point.altitude; // >> gps_point.height >> gps_point.health;
                        line_ss >> position[0] >> position[1] >> position[2];
                        line_ss >> quaternion.w() >> quaternion.x() >> quaternion.y() >> quaternion.z();

                        params.shell_ptr->showMessage(gps_point.to_string());
                        params.shell_ptr->showMessage(VectorMath::toString(position));
                        params.shell_ptr->showMessage(VectorMath::toString(quaternion));

                        GeoPoint home_geo_point = context->client.getHomeGeoPoint();
                        Vector3r local_point = EarthUtils::GeodeticToNedFast(gps_point, home_geo_point);
                        VectorMath::toEulerianAngle(quaternion, pitch, roll, yaw);

                        context->client.moveToPositionAsync(local_point.x(), local_point.y(), local_point.z(), velocity, 0, DrivetrainType::MaxDegreeOfFreedom, YawMode(false, yaw), lookahead, adaptive_lookahead);
                        context->client.waitOnLastTask();
                    }
                }
            });

            return false;
        }
    };

    class GetImageCommand : public DroneCommand
    {
        int image_index_ = 0;

    public:
        GetImageCommand()
            : DroneCommand("GetImage", "Get an image from the simulator")
        {
            this->addSwitch({ "-type", "depth", "scene, depth, or segmentation" });
            this->addSwitch({ "-name", "image", "name of the file" });
            this->addSwitch({ "-iterations", "1", "number of images to capture" });
            this->addSwitch({ "-pause_time", "100", "pause time between each image in milliseconds (default 100)" });
        }

        void getImages(CommandContext* context, ImageCaptureBase::ImageType imageType, std::string baseName, int iterations, TTimeDelta pause_time)
        {
            // group the images by the current date.
            std::string folderName = Utils::to_string(Utils::now(), "%Y-%m-%d");
            std::string path = FileSystem::ensureFolder(FileSystem::combine(FileSystem::getAppDataFolder(), folderName));

            for (int i = 0; i < iterations; i++) {

                auto image = context->client.simGetImage("0", imageType);

                if (image.size() == 0) {
                    std::cout << "error getting image, check sim for error messages" << endl;
                    return;
                }

                const char* typeName = "";
                switch (imageType) {
                case msr::airlib::ImageCaptureBase::ImageType::Scene:
                    typeName = "scene";
                    break;
                case msr::airlib::ImageCaptureBase::ImageType::DepthVis:
                    typeName = "depth";
                    break;
                case msr::airlib::ImageCaptureBase::ImageType::Segmentation:
                    typeName = "seg";
                    break;
                case msr::airlib::ImageCaptureBase::ImageType::SurfaceNormals:
                    typeName = "normals";
                    break;
                case msr::airlib::ImageCaptureBase::ImageType::DisparityNormalized:
                    typeName = "disparity";
                    break;
                default:
                    break;
                }

                std::string imageName = Utils::stringf("%s_%s%d.png", baseName.c_str(), typeName, image_index_++);
                std::string file_path_name = FileSystem::combine(path, imageName);

                ofstream file;
                FileSystem::createBinaryFile(file_path_name, file);
                file.write(reinterpret_cast<const char*>(image.data()), image.size());
                file.close();

                std::cout << "Image saved to: " << file_path_name << " (" << image.size() << " bytes)" << endl;

                context->sleep_for(pause_time / 1000);
            }
        }

        bool execute(const DroneCommandParameters& params)
        {
            std::string type = getSwitch("-type").value;
            std::string name = getSwitch("-name").value;
            int iterations = std::stoi(getSwitch("-iterations").value);
            TTimeDelta pause_time = getSwitch("-pause_time").toTimeDelta();
            CommandContext* context = params.context;

            ImageCaptureBase::ImageType imageType;

            if (type == "depth") {
                imageType = ImageCaptureBase::ImageType::DepthVis;
            }
            else if (type == "scene") {
                imageType = ImageCaptureBase::ImageType::Scene;
            }
            else if (type == "segmentation") {
                imageType = ImageCaptureBase::ImageType::Segmentation;
            }
            else if (type == "normals") {
                imageType = ImageCaptureBase::ImageType::SurfaceNormals;
            }
            else if (type == "disparity") {
                imageType = ImageCaptureBase::ImageType::DisparityNormalized;
            }
            else {
                cout << "Error: Invalid image type '" << type << "', expecting either 'depth', 'scene' or 'segmentation'" << endl;
                return true;
            }

            context->tasker.execute([=]() {
                getImages(context, imageType, name, iterations, pause_time);
            });

            return false;
        }
    };

    // std::string beforeScriptStartCallback(const DroneCommandParameters& param, std::string scriptFilePath)
    // {
    //     return "";
    // }
    // bool afterScriptEndCallback(const DroneCommandParameters& params, std::string scriptFilePath)
    // {
    //     return false;
    // }
    // std::string beforeScriptCommandStartCallback(const DroneCommandParameters& params) {
    //     params.context->client.newTask();
    // }
    // bool afterScriptCommandEndCallback(const DroneCommandParameters& params, bool commandReturnValue) {
    //     params.context->client.WaitForCompletion(0);
    // }
    //void beforeCommandStartCallback(const DroneCommandParameters& params, std::string command_line)
    //{
    //}
}
} //namespace

std::string server_address("127.0.0.1");

bool parseCommandLine(int argc, const char* argv[])
{
    // parse command line
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (arg[0] == '-' || arg[0] == '/') {
            std::string name = arg + 1;
            if (name == "server" && i + 1 < argc) {
                server_address = argv[++i];
            }
            else {
                return false;
            }
        }
        else {
            return false;
        }
    }
    return true;
}

void printUsage()
{
    std::cout << "Usage: DroneShell [-server 127.0.0.1]" << std::endl;
    std::cout << "The default server address is 127.0.0.1, but use the -server option to specify a different address for the server" << std::endl;
}

int main(int argc, const char* argv[])
{

    using namespace msr::airlib;

    if (!parseCommandLine(argc, argv)) {
        printUsage();
        return -1;
    }

    CommandContext command_context(server_address);

    command_context.tasker.setErrorHandler([](std::exception& e) {
        try {
            rpc::rpc_error& rpc_ex = dynamic_cast<rpc::rpc_error&>(e);
            std::cerr << "Async RPC Error: " << rpc_ex.get_error().as<std::string>() << std::endl;
        }
        catch (...) {
            std::cerr << "Error occurred: " << e.what() << std::endl;
        }
    });

    SimpleShell<CommandContext> shell("==||=> ");

    shell.showMessage(R"(
        Welcome to DroneShell 1.0.
        Type ? for help.
        Microsoft Research (c) 2017.
    )");

    command_context.client.confirmConnection();

    //Shell callbacks
    // shell.beforeScriptStartCallback(std::bind(&beforeScriptStartCallback, std::placeholders::_1, std::placeholders::_2));
    // shell.afterScriptEndCallback(std::bind(&afterScriptEndCallback, std::placeholders::_1, std::placeholders::_2));
    // shell.afterScriptCommandEndCallback(std::bind(&afterScriptCommandEndCallback, std::placeholders::_1, std::placeholders::_2));
    //shell.beforeCommandStartCallback(std::bind(&beforeCommandStartCallback, std::placeholders::_1, std::placeholders::_2));

    //Add shell commands
    ArmCommand arm;
    DisarmCommand disarm;
    RequestControlCommand requestControl;
    ReleaseControlCommand releaseControl;
    TakeOffCommand takeOff;
    LandCommand land;
    GoHomeCommand goHome;
    //TODO: add WaitForCompletion command
    GetHomeGeoPointCommand getHomeGeoPoint;
    MoveToZCommand moveToZ;
    RotateByYawRateCommand rotateByYawRate;
    RotateToYawCommand rotateToYaw;
    HoverCommand hover;
    MoveToPositionCommand moveToPosition;
    GetPositionCommand getPosition;
    MoveByManualCommand moveByManual;
    MoveByAngleZCommand moveByAngleZ;
    MoveByAngleThrottleCommand moveByAngleThrottle;
    MoveByVelocityCommand moveByVelocity;
    MoveByVelocityZCommand moveByVelocityZ;
    MoveOnPathCommand moveOnPath;
    SetSafetyCommand setSafety;
    BackForthByAngleCommand backForthByAngle;
    BackForthByPositionCommand backForthByPosition;
    SquareByAngleCommand squareByAngle;
    SquareByPositionCommand squareByPosition;
    SquareByPathCommand squareByPath;
    CircleByPositionCommand circleByPosition;
    CircleByPathCommand circleByPath;
    RecordPoseCommand recordPose;
    PlayPoseCommand playPose;
    GetImageCommand imageCommand;

    //TODO: add command line args help, arg count validation
    shell.addCommand(arm);
    shell.addCommand(disarm);
    shell.addCommand(requestControl);
    shell.addCommand(releaseControl);
    shell.addCommand(takeOff);
    shell.addCommand(land);
    shell.addCommand(getPosition);
    shell.addCommand(goHome);
    shell.addCommand(getHomeGeoPoint);
    shell.addCommand(moveToZ);
    shell.addCommand(rotateByYawRate);
    shell.addCommand(rotateToYaw);
    shell.addCommand(hover);
    shell.addCommand(moveToPosition);
    shell.addCommand(moveByManual);
    shell.addCommand(moveByAngleZ);
    shell.addCommand(moveByAngleThrottle);
    shell.addCommand(moveByVelocity);
    shell.addCommand(moveByVelocityZ);
    shell.addCommand(moveOnPath);
    shell.addCommand(setSafety);
    shell.addCommand(backForthByAngle);
    shell.addCommand(backForthByPosition);
    shell.addCommand(squareByAngle);
    shell.addCommand(squareByPosition);
    shell.addCommand(squareByPath);
    shell.addCommand(circleByPosition);
    shell.addCommand(circleByPath);
    shell.addCommand(recordPose);
    shell.addCommand(playPose);
    shell.addCommand(imageCommand);

    while (!shell.readLineAndExecute(&command_context)) {
    }

    return 0;
}
