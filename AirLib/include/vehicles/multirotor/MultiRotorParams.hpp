// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_MultiRotorParameters_hpp
#define msr_airlib_MultiRotorParameters_hpp

#include "common/Common.hpp"
#include "RotorParams.hpp"
#include "sensors/SensorCollection.hpp"
#include "sensors/SensorFactory.hpp"
#include "vehicles/multirotor/api/MultirotorApiBase.hpp"

namespace msr
{
namespace airlib
{

    class MultiRotorParams
    {
        //All units are SI
    public: //types
        struct RotorPose
        {
            Vector3r position; //relative to center of gravity of vehicle body
            Vector3r normal;
            RotorTurningDirection direction;

            RotorPose()
            {
            }
            RotorPose(const Vector3r& position_val, const Vector3r& normal_val, RotorTurningDirection direction_val)
                : position(position_val), normal(normal_val), direction(direction_val)
            {
            }
        };

        struct Params
        {
            /*********** required parameters ***********/
            uint rotor_count;
            vector<RotorPose> rotor_poses;
            real_T mass;
            Matrix3x3r inertia;
            Vector3r body_box;

            /*********** optional parameters with defaults ***********/
            real_T linear_drag_coefficient = 1.3f / 4.0f;
            //sample value 1.3 from http://klsin.bpmsg.com/how-fast-can-a-quadcopter-fly/, but divided by 4 to account
            // for nice streamlined frame design and allow higher top speed which is more fun.
            //angular coefficient is usually 10X smaller than linear, however we should replace this with exact number
            //http://physics.stackexchange.com/q/304742/14061
            real_T angular_drag_coefficient = linear_drag_coefficient;
            real_T restitution = 0.55f; // value of 1 would result in perfectly elastic collisions, 0 would be completely inelastic.
            real_T friction = 0.5f;
            RotorParams rotor_params;
        };

    protected: //must override by derived class
        virtual void setupParams() = 0;
        virtual const SensorFactory* getSensorFactory() const = 0;

    public: //interface
        virtual std::unique_ptr<MultirotorApiBase> createMultirotorApi() = 0;

        virtual ~MultiRotorParams() = default;
        virtual void initialize(const AirSimSettings::VehicleSetting* vehicle_setting)
        {
            sensor_storage_.clear();
            sensors_.clear();

            setupParams();

            addSensorsFromSettings(vehicle_setting);
        }

        const Params& getParams() const
        {
            return params_;
        }
        Params& getParams()
        {
            return params_;
        }
        SensorCollection& getSensors()
        {
            return sensors_;
        }
        const SensorCollection& getSensors() const
        {
            return sensors_;
        }

        void addSensorsFromSettings(const AirSimSettings::VehicleSetting* vehicle_setting)
        {
            const auto& sensor_settings = vehicle_setting->sensors;

            getSensorFactory()->createSensorsFromSettings(sensor_settings, sensors_, sensor_storage_);
        }

    protected: //static utility functions for derived classes to use
        /// Initializes 4 rotors in the usual QuadX pattern:  http://ardupilot.org/copter/_images/MOTORS_QuadX_QuadPlus.jpg
        /// which shows that given an array of 4 motors, the first is placed top right and flies counter clockwise (CCW) and
        /// the second is placed bottom left, and also flies CCW.  The third in the array is placed top left and flies clockwise (CW)
        /// while the last is placed bottom right and also flies clockwise.  This is how the 4 items in the arm_lengths and
        /// arm_angles arrays will be used.  So arm_lengths is 4 numbers (in meters) where four arm lengths, 0 is top right,
        /// 1 is bottom left, 2 is top left and 3 is bottom right.  arm_angles is 4 numbers (in degrees)  relative to forward vector (0,1),
        /// provided in the same order where 0 is top right, 1 is bottom left, 2 is top left and 3 is bottom right, so for example,
        /// the angles for a regular symmetric X pattern would be 45, 225, 315, 135.  The rotor_z is the offset of each motor upwards
        /// relative to the center of mass (in meters).
        static void initializeRotorQuadX(vector<RotorPose>& rotor_poses /* the result we are building */,
                                         uint rotor_count /* must be 4 */,
                                         real_T arm_lengths[],
                                         real_T rotor_z /* z relative to center of gravity */)
        {
            Vector3r unit_z(0, 0, -1); //NED frame
            if (rotor_count == 4) {
                rotor_poses.clear();

                /* Note: rotor_poses are built in this order:
                 x-axis
            (2)  |   (0)
                 |
            -------------- y-axis
                 |
            (1)  |   (3)
            */
                // vectors below are rotated according to NED left hand rule (so the vectors are rotated counter clockwise).
                Quaternionr quadx_rot(AngleAxisr(M_PIf / 4, unit_z));
                rotor_poses.emplace_back(VectorMath::rotateVector(Vector3r(0, arm_lengths[0], rotor_z), quadx_rot, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCCW);
                rotor_poses.emplace_back(VectorMath::rotateVector(Vector3r(0, -arm_lengths[1], rotor_z), quadx_rot, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCCW);
                rotor_poses.emplace_back(VectorMath::rotateVector(Vector3r(arm_lengths[2], 0, rotor_z), quadx_rot, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCW);
                rotor_poses.emplace_back(VectorMath::rotateVector(Vector3r(-arm_lengths[3], 0, rotor_z), quadx_rot, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCW);
            }
            else
                throw std::invalid_argument("Rotor count other than 4 is not supported by this method!");
        }

        static void initializeRotorHexX(vector<RotorPose>& rotor_poses /* the result we are building */,
                                        uint rotor_count /* must be 6 */,
                                        real_T arm_lengths[],
                                        real_T rotor_z /* z relative to center of gravity */)
        {
            Vector3r unit_z(0, 0, -1); //NED frame
            if (rotor_count == 6) {
                rotor_poses.clear();
                /* Note: rotor_poses are built in this order: rotor 0 is CW
              See HEXA X configuration on http://ardupilot.org/copter/docs/connect-escs-and-motors.html

                     x-axis
                (2)    (4)
                   \  /
                    \/
               (1)-------(0) y-axis
                    /\
                   /  \
                 (5)  (3)

            */

                // vectors below are rotated according to NED left hand rule (so the vectors are rotated counter clockwise).
                Quaternionr hexa_rot30(AngleAxisr(M_PIf / 6, unit_z)); // 30 degrees
                Quaternionr hexa_rot60(AngleAxisr(M_PIf / 3, unit_z)); // 60 degrees
                Quaternionr no_rot(AngleAxisr(0, unit_z));
                rotor_poses.emplace_back(VectorMath::rotateVector(Vector3r(0, arm_lengths[0], rotor_z), no_rot, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCW);
                rotor_poses.emplace_back(VectorMath::rotateVector(Vector3r(0, -arm_lengths[1], rotor_z), no_rot, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCCW);
                rotor_poses.emplace_back(VectorMath::rotateVector(Vector3r(arm_lengths[2], 0, rotor_z), hexa_rot30, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCW);
                rotor_poses.emplace_back(VectorMath::rotateVector(Vector3r(-arm_lengths[3], 0, rotor_z), hexa_rot30, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCCW);
                rotor_poses.emplace_back(VectorMath::rotateVector(Vector3r(0, arm_lengths[4], rotor_z), hexa_rot60, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCCW);
                rotor_poses.emplace_back(VectorMath::rotateVector(Vector3r(0, -arm_lengths[5], rotor_z), hexa_rot60, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCW);
            }
            else
                throw std::invalid_argument("Rotor count other than 6 is not supported by this method!");
        }

        static void initializeRotorOctoX(vector<RotorPose>& rotor_poses /* the result we are building */,
                                         uint rotor_count /* must be 8 */,
                                         real_T arm_lengths[],
                                         real_T rotor_z /* z relative to center of gravity */)
        {
            Vector3r unit_z(0, 0, -1); //NED frame
            if (rotor_count == 8) {
                rotor_poses.clear();
                /* Note: rotor_poses are built in this order: rotor 0 is CW
              See OCTO X configuration on http://ardupilot.org/copter/docs/connect-escs-and-motors.html

                     x-axis
                  
                 (4)  |  (0) 
                      |
            (6)       |       (2)
            __________|__________  y-axis
                      |
            (5)       |       (7)
                      |
                 (1)  |  (3)

            0 CW: 67.5 from +Y
            1 CW: 67.5 from -Y 
            2 CCW: 22.5 from +Y
            3 CCW: 22.5 from -X
            4 CCW: 22.5 from +X
            5 CCW: 22.5 from -Y
            6 CW: 67.5 from +X
            7 CW: 67.5 from -X
            
            */

                // vectors below are rotated according to NED left hand rule (so the vectors are rotated counter clockwise).
                Quaternionr octo_rot22(AngleAxisr(M_PIf / 8, unit_z)); // 22.5 degrees
                Quaternionr octo_rot67(AngleAxisr(3 * M_PIf / 8, unit_z)); // 67.5 degrees

                rotor_poses.emplace_back(VectorMath::rotateVector(
                                             Vector3r(0, arm_lengths[0], rotor_z), octo_rot67, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCW);

                rotor_poses.emplace_back(VectorMath::rotateVector(
                                             Vector3r(0, -arm_lengths[1], rotor_z), octo_rot67, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCW);

                rotor_poses.emplace_back(VectorMath::rotateVector(
                                             Vector3r(0, arm_lengths[2], rotor_z), octo_rot22, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCCW);

                rotor_poses.emplace_back(VectorMath::rotateVector(
                                             Vector3r(-arm_lengths[3], 0, rotor_z), octo_rot22, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCCW);

                rotor_poses.emplace_back(VectorMath::rotateVector(
                                             Vector3r(arm_lengths[4], 0, rotor_z), octo_rot22, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCCW);

                rotor_poses.emplace_back(VectorMath::rotateVector(
                                             Vector3r(0, -arm_lengths[5], rotor_z), octo_rot22, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCCW);

                rotor_poses.emplace_back(VectorMath::rotateVector(
                                             Vector3r(arm_lengths[6], 0, rotor_z), octo_rot67, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCW);

                rotor_poses.emplace_back(VectorMath::rotateVector(
                                             Vector3r(-arm_lengths[7], 0, rotor_z), octo_rot67, true),
                                         unit_z,
                                         RotorTurningDirection::RotorTurningDirectionCW);
            }
            else
                throw std::invalid_argument("Rotor count other than 8 is not supported by this method!");
        }

        /// Initialize the rotor_poses given the rotor_count, the arm lengths and the arm angles (relative to forwards vector).
        /// Also provide the direction you want to spin each rotor and the z-offset of the rotors relative to the center of gravity.
        static void initializeRotors(vector<RotorPose>& rotor_poses, uint rotor_count, real_T arm_lengths[], real_T arm_angles[], RotorTurningDirection rotor_directions[], real_T rotor_z /* z relative to center of gravity */)
        {
            Vector3r unit_z(0, 0, -1); //NED frame
            rotor_poses.clear();
            for (uint i = 0; i < rotor_count; i++) {
                Quaternionr angle(AngleAxisr(arm_angles[i] * M_PIf / 180, unit_z));
                rotor_poses.emplace_back(VectorMath::rotateVector(Vector3r(0, arm_lengths[i], rotor_z), angle, true),
                                         unit_z,
                                         rotor_directions[i]);
            }
        }

        static void computeInertiaMatrix(Matrix3x3r& inertia, const Vector3r& body_box, const vector<RotorPose>& rotor_poses,
                                         real_T box_mass, real_T motor_assembly_weight)
        {
            inertia = Matrix3x3r::Zero();

            //http://farside.ph.utexas.edu/teaching/336k/Newtonhtml/node64.html
            inertia(0, 0) = box_mass / 12.0f * (body_box.y() * body_box.y() + body_box.z() * body_box.z());
            inertia(1, 1) = box_mass / 12.0f * (body_box.x() * body_box.x() + body_box.z() * body_box.z());
            inertia(2, 2) = box_mass / 12.0f * (body_box.x() * body_box.x() + body_box.y() * body_box.y());

            for (size_t i = 0; i < rotor_poses.size(); ++i) {
                const auto& pos = rotor_poses.at(i).position;
                inertia(0, 0) += (pos.y() * pos.y() + pos.z() * pos.z()) * motor_assembly_weight;
                inertia(1, 1) += (pos.x() * pos.x() + pos.z() * pos.z()) * motor_assembly_weight;
                inertia(2, 2) += (pos.x() * pos.x() + pos.y() * pos.y()) * motor_assembly_weight;
            }
        }

        // Some Frame types which can be used by different firmwares
        // Specific frame configurations, modifications can be done in the Firmware Params

        void setupFrameGenericQuad(Params& params)
        {
            //set up arm lengths
            //dimensions are for F450 frame: http://artofcircuits.com/product/quadcopter-frame-hj450-with-power-distribution
            params.rotor_count = 4;
            std::vector<real_T> arm_lengths(params.rotor_count, 0.2275f);

            //set up mass
            //this has to be between max_thrust*rotor_count/10 (1.6kg using default parameters in RotorParams.hpp) and (idle throttle percentage)*max_thrust*rotor_count/10 (0.8kg using default parameters and SimpleFlight)
            //any value above the maximum would result in the motors not being able to lift the body even at max thrust,
            //and any value below the minimum would cause the drone to fly upwards on idling throttle (50% of the max throttle if using SimpleFlight)
            //Note that the default idle throttle percentage is 50% if you are using SimpleFlight
            params.mass = 1.0f;

            real_T motor_assembly_weight = 0.055f; //weight for MT2212 motor for F450 frame
            real_T box_mass = params.mass - params.rotor_count * motor_assembly_weight;

            // using rotor_param default, but if you want to change any of the rotor_params, call calculateMaxThrust() to recompute the max_thrust
            // given new thrust coefficients, motor max_rpm and propeller diameter.
            params.rotor_params.calculateMaxThrust();

            //set up dimensions of core body box or abdomen (not including arms).
            params.body_box.x() = 0.180f;
            params.body_box.y() = 0.11f;
            params.body_box.z() = 0.040f;
            real_T rotor_z = 2.5f / 100;

            //computer rotor poses
            initializeRotorQuadX(params.rotor_poses, params.rotor_count, arm_lengths.data(), rotor_z);

            //compute inertia matrix
            computeInertiaMatrix(params.inertia, params.body_box, params.rotor_poses, box_mass, motor_assembly_weight);
        }

        void setupFrameGenericHex(Params& params)
        {
            //set up arm lengths
            //dimensions are for F450 frame: http://artofcircuits.com/product/quadcopter-frame-hj450-with-power-distribution
            params.rotor_count = 6;
            std::vector<real_T> arm_lengths(params.rotor_count, 0.2275f);

            //set up mass
            //this has to be between max_thrust*rotor_count/10 (2.5kg using default parameters in RotorParams.hpp) and (idle throttle percentage)*max_thrust*rotor_count/10 (1.25kg using default parameters and SimpleFlight)
            //any value above the maximum would result in the motors not being able to lift the body even at max thrust,
            //and any value below the minimum would cause the drone to fly upwards on idling throttle (50% of the max throttle if using SimpleFlight)
            params.mass = 1.0f;

            real_T motor_assembly_weight = 0.055f; //weight for MT2212 motor for F450 frame
            real_T box_mass = params.mass - params.rotor_count * motor_assembly_weight;

            // using rotor_param default, but if you want to change any of the rotor_params, call calculateMaxThrust() to recompute the max_thrust
            // given new thrust coefficients, motor max_rpm and propeller diameter.
            params.rotor_params.calculateMaxThrust();

            //set up dimensions of core body box or abdomen (not including arms).
            params.body_box.x() = 0.180f;
            params.body_box.y() = 0.11f;
            params.body_box.z() = 0.040f;
            real_T rotor_z = 2.5f / 100;

            //computer rotor poses
            initializeRotorHexX(params.rotor_poses, params.rotor_count, arm_lengths.data(), rotor_z);

            //compute inertia matrix
            computeInertiaMatrix(params.inertia, params.body_box, params.rotor_poses, box_mass, motor_assembly_weight);
        }

        void setupFrameGenericOcto(Params& params)
        {
            //set up arm lengths
            //dimensions are for F450 frame: http://artofcircuits.com/product/quadcopter-frame-hj450-with-power-distribution
            params.rotor_count = 8;
            std::vector<real_T> arm_lengths(params.rotor_count, 0.2275f);

            //set up mass
            //this has to be between max_thrust*rotor_count/10 (2.5kg using default parameters in RotorParams.hpp) and (idle throttle percentage)*max_thrust*rotor_count/10 (1.25kg using default parameters and SimpleFlight)
            //any value above the maximum would result in the motors not being able to lift the body even at max thrust,
            //and any value below the minimum would cause the drone to fly upwards on idling throttle (50% of the max throttle if using SimpleFlight)

            params.mass = 1.0f; //can be varied from 0.800 to 1.600
            real_T motor_assembly_weight = 0.055f; //weight for MT2212 motor for F450 frame  0.148
            real_T box_mass = params.mass - params.rotor_count * motor_assembly_weight;

            // using rotor_param default, but if you want to change any of the rotor_params, call calculateMaxThrust() to recompute the max_thrust
            // given new thrust coefficients, motor max_rpm and propeller diameter.
            params.rotor_params.calculateMaxThrust();

            //set up dimensions of core body box or abdomen (not including arms).
            params.body_box.x() = 0.180f;
            params.body_box.y() = 0.11f;
            params.body_box.z() = 0.040f;
            real_T rotor_z = 2.5f / 100;

            //computer rotor poses
            initializeRotorOctoX(params.rotor_poses, params.rotor_count, arm_lengths.data(), rotor_z);

            //compute inertia matrix
            computeInertiaMatrix(params.inertia, params.body_box, params.rotor_poses, box_mass, motor_assembly_weight);
        }

        void setupFrameFlamewheel(Params& params)
        {
            //set up arm lengths
            //dimensions are for F450 frame: http://artofcircuits.com/product/quadcopter-frame-hj450-with-power-distribution
            params.rotor_count = 4;
            std::vector<real_T> arm_lengths(params.rotor_count, 0.225f);

            //set up mass
            params.mass = 1.635f;
            real_T motor_assembly_weight = 0.052f;
            real_T box_mass = params.mass - params.rotor_count * motor_assembly_weight;

            params.rotor_params.C_T = 0.11f;
            params.rotor_params.C_P = 0.047f;
            params.rotor_params.max_rpm = 9500;
            params.rotor_params.calculateMaxThrust();
            params.linear_drag_coefficient *= 4; // make top speed more real.

            //set up dimensions of core body box or abdomen (not including arms).
            params.body_box.x() = 0.16f;
            params.body_box.y() = 0.10f;
            params.body_box.z() = 0.14f;
            real_T rotor_z = 0.15f;

            //computer rotor poses
            initializeRotorQuadX(params.rotor_poses, params.rotor_count, arm_lengths.data(), rotor_z);

            //compute inertia matrix
            computeInertiaMatrix(params.inertia, params.body_box, params.rotor_poses, box_mass, motor_assembly_weight);
        }

        void setupFrameFlamewheelFLA(Params& params)
        {
            //set up arm lengths
            //dimensions are for F450 frame: http://artofcircuits.com/product/quadcopter-frame-hj450-with-power-distribution
            params.rotor_count = 4;
            std::vector<real_T> arm_lengths(params.rotor_count, 0.225f);

            //set up mass
            params.mass = 2.25f;
            real_T motor_assembly_weight = 0.1f;
            real_T box_mass = params.mass - params.rotor_count * motor_assembly_weight;

            params.rotor_params.C_T = 0.2f;
            params.rotor_params.C_P = 0.1f;
            params.rotor_params.max_rpm = 9324;
            params.rotor_params.calculateMaxThrust();
            params.linear_drag_coefficient *= 4; // make top speed more real.

            //set up dimensions of core body box or abdomen (not including arms).
            params.body_box.x() = 0.16f;
            params.body_box.y() = 0.10f;
            params.body_box.z() = 0.14f;
            real_T rotor_z = 0.15f;

            //computer rotor poses
            initializeRotorQuadX(params.rotor_poses, params.rotor_count, arm_lengths.data(), rotor_z);

            //compute inertia matrix
            computeInertiaMatrix(params.inertia, params.body_box, params.rotor_poses, box_mass, motor_assembly_weight);
        }

        void setupFrameBlacksheep(Params& params)
        {
            /*
        Motor placement:
        x
        (2)  |   (0)
        |
        ------------ y
        |
        (1)  |   (3)
        |

        */
            //set up arm lengths
            //dimensions are for Team Blacksheep Discovery (http://team-blacksheep.com/products/product:98)
            params.rotor_count = 4;
            std::vector<real_T> arm_lengths;

            Vector3r unit_z(0, 0, -1); //NED frame

            // relative to Forward vector in the order (0,3,1,2) required by quad X pattern
            // http://ardupilot.org/copter/_images/MOTORS_QuadX_QuadPlus.jpg
            arm_lengths.push_back(0.22f);
            arm_lengths.push_back(0.255f);
            arm_lengths.push_back(0.22f);
            arm_lengths.push_back(0.255f);

            // note: the Forward vector is actually the "x" axis, and the AngleAxisr rotation is pointing down and is left handed, so this means the rotation
            // is counter clockwise, so the vector (arm_lengths[i], 0) is the X-axis, so the CCW rotations to position each arm correctly are listed below:
            // See measurements here: http://diydrones.com/profiles/blogs/arducopter-tbs-discovery-style (angles reversed because we are doing CCW rotation)
            std::vector<real_T> arm_angles;
            arm_angles.push_back(-55.0f);
            arm_angles.push_back(125.0f);
            arm_angles.push_back(55.0f);
            arm_angles.push_back(-125.0f);

            // quad X pattern
            std::vector<RotorTurningDirection> rotor_directions;
            rotor_directions.push_back(RotorTurningDirection::RotorTurningDirectionCCW);
            rotor_directions.push_back(RotorTurningDirection::RotorTurningDirectionCCW);
            rotor_directions.push_back(RotorTurningDirection::RotorTurningDirectionCW);
            rotor_directions.push_back(RotorTurningDirection::RotorTurningDirectionCW);

            // data from
            // http://dronesvision.net/team-blacksheep-750kv-motor-esc-set-for-tbs-discovery-fpv-quadcopter/
            //set up mass
            params.mass = 2.0f; //can be varied from 0.800 to 1.600
            real_T motor_assembly_weight = 0.052f; // weight for TBS motors
            real_T box_mass = params.mass - params.rotor_count * motor_assembly_weight;

            // the props we are using a E-Prop, which I didn't find in UIUC database, but this one is close:
            // http://m-selig.ae.illinois.edu/props/volume-2/plots/ef_130x70_static_ctcp.png
            params.rotor_params.C_T = 0.11f;
            params.rotor_params.C_P = 0.047f;
            params.rotor_params.max_rpm = 9500;
            params.rotor_params.calculateMaxThrust();

            //set up dimensions of core body box or abdomen (not including arms).
            params.body_box.x() = 0.20f;
            params.body_box.y() = 0.12f;
            params.body_box.z() = 0.04f;
            real_T rotor_z = 2.5f / 100;

            //computer rotor poses
            params.rotor_poses.clear();
            for (uint i = 0; i < 4; i++) {
                Quaternionr angle(AngleAxisr(arm_angles[i] * M_PIf / 180, unit_z));
                params.rotor_poses.emplace_back(VectorMath::rotateVector(Vector3r(arm_lengths[i], 0, rotor_z), angle, true), unit_z, rotor_directions[i]);
            };

            //compute inertia matrix
            computeInertiaMatrix(params.inertia, params.body_box, params.rotor_poses, box_mass, motor_assembly_weight);
        }

    private:
        Params params_;
        SensorCollection sensors_; //maintains sensor type indexed collection of sensors
        vector<shared_ptr<SensorBase>> sensor_storage_; //RAII for created sensors
    };
}
} //namespace
#endif
