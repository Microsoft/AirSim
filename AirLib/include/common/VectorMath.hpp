// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef air_VectorMath_hpp
#define air_VectorMath_hpp

#include <cmath>
#include "common/common_utils/Utils.hpp"
#include "common_utils/RandomGenerator.hpp"

STRICT_MODE_OFF
//if not using unaligned types then disable vectorization to avoid alignment issues all over the places
//#define EIGEN_DONT_VECTORIZE
#include "Eigen/Dense"
STRICT_MODE_ON

namespace msr
{
namespace airlib
{

    template <class Vector3T, class QuaternionT, class RealT>
    class VectorMathT
    {
    public:
        //IMPORTANT: make sure fixed size vectorization types have no alignment assumption
        //https://eigen.tuxfamily.org/dox/group__TopicUnalignedArrayAssert.html
        typedef Eigen::Matrix<float, 1, 1> Vector1f;
        typedef Eigen::Matrix<double, 1, 1> Vector1d;
        typedef Eigen::Matrix<float, 2, 1, Eigen::DontAlign> Vector2f;
        typedef Eigen::Matrix<double, 4, 1, Eigen::DontAlign> Vector2d;
        typedef Eigen::Vector3f Vector3f;
        typedef Eigen::Vector3d Vector3d;
        typedef Eigen::Array3f Array3f;
        typedef Eigen::Array3d Array3d;
        typedef Eigen::Quaternion<float, Eigen::DontAlign> Quaternionf;
        typedef Eigen::Quaternion<double, Eigen::DontAlign> Quaterniond;
        typedef Eigen::Matrix<double, 3, 3> Matrix3x3d;
        typedef Eigen::Matrix<float, 3, 3> Matrix3x3f;
        typedef Eigen::AngleAxisd AngleAxisd;
        typedef Eigen::AngleAxisf AngleAxisf;

        typedef common_utils::Utils Utils;
        //use different seeds for each component
        //TODO: below we are using double instead of RealT because of VC++2017 bug in random implementation
        typedef common_utils::RandomGenerator<RealT, std::normal_distribution<double>, 1> RandomGeneratorGausianXT;
        typedef common_utils::RandomGenerator<RealT, std::normal_distribution<double>, 2> RandomGeneratorGausianYT;
        typedef common_utils::RandomGenerator<RealT, std::normal_distribution<double>, 3> RandomGeneratorGausianZT;
        typedef common_utils::RandomGenerator<RealT, std::uniform_real_distribution<RealT>, 1> RandomGeneratorXT;
        typedef common_utils::RandomGenerator<RealT, std::uniform_real_distribution<RealT>, 2> RandomGeneratorYT;
        typedef common_utils::RandomGenerator<RealT, std::uniform_real_distribution<RealT>, 3> RandomGeneratorZT;

        struct Pose
        {
            EIGEN_MAKE_ALIGNED_OPERATOR_NEW
            Vector3T position = Vector3T::Zero();
            QuaternionT orientation = QuaternionT(1, 0, 0, 0);

            Pose()
            {
            }

            Pose(const Vector3T& position_val, const QuaternionT& orientation_val)
            {
                orientation = orientation_val;
                position = position_val;
            }

            friend Pose operator-(const Pose& lhs, const Pose& rhs)
            {
                return VectorMathT::subtract(lhs, rhs);
            }
            friend Pose operator+(const Pose& lhs, const Pose& rhs)
            {
                return VectorMathT::add(lhs, rhs);
            }
            friend bool operator==(const Pose& lhs, const Pose& rhs)
            {
                return lhs.position == rhs.position && lhs.orientation.coeffs() == rhs.orientation.coeffs();
            }
            friend bool operator!=(const Pose& lhs, const Pose& rhs)
            {
                return !(lhs == rhs);
                ;
            }

            static Pose nanPose()
            {
                static const Pose nan_pose(VectorMathT::nanVector(), VectorMathT::nanQuaternion());
                return nan_pose;
            }
            static Pose zero()
            {
                static const Pose zero_pose(Vector3T::Zero(), QuaternionT(1, 0, 0, 0));
                return zero_pose;
            }
        };

        struct Transform
        {
            EIGEN_MAKE_ALIGNED_OPERATOR_NEW
            Vector3T translation;
            QuaternionT rotation;
        };

        class RandomVectorT
        {
        public:
            RandomVectorT()
            {
            }
            RandomVectorT(RealT min_val, RealT max_val)
                : rx_(min_val, max_val), ry_(min_val, max_val), rz_(min_val, max_val)
            {
            }
            RandomVectorT(const Vector3T& min_val, const Vector3T& max_val)
                : rx_(min_val.x(), max_val.x()), ry_(min_val.y(), max_val.y()), rz_(min_val.z(), max_val.z())
            {
            }

            void reset()
            {
                rx_.reset();
                ry_.reset();
                rz_.reset();
            }

            Vector3T next()
            {
                return Vector3T(rx_.next(), ry_.next(), rz_.next());
            }

        private:
            RandomGeneratorXT rx_;
            RandomGeneratorYT ry_;
            RandomGeneratorZT rz_;
        };

        class RandomVectorGaussianT
        {
        public:
            RandomVectorGaussianT()
            {
            }
            RandomVectorGaussianT(RealT mean, RealT stddev)
                : rx_(mean, stddev), ry_(mean, stddev), rz_(mean, stddev)
            {
            }
            RandomVectorGaussianT(const Vector3T& mean, const Vector3T& stddev)
                : rx_(mean.x(), stddev.x()), ry_(mean.y(), stddev.y()), rz_(mean.z(), stddev.z())
            {
            }

            void reset()
            {
                rx_.reset();
                ry_.reset();
                rz_.reset();
            }

            Vector3T next()
            {
                return Vector3T(rx_.next(), ry_.next(), rz_.next());
            }

        private:
            RandomGeneratorGausianXT rx_;
            RandomGeneratorGausianYT ry_;
            RandomGeneratorGausianZT rz_;
        };

    public:
        static float magnitude(const Vector2f& v)
        {
            return v.norm();
        }

        static RealT magnitude(const Vector3T& v)
        {
            return v.norm();
        }

        static Vector3T rotateVector(const Vector3T& v, const QuaternionT& q, bool assume_unit_quat)
        {
            unused(assume_unit_quat); // stop warning: unused parameter.
                //More performant method is at http://gamedev.stackexchange.com/a/50545/20758
                //QuaternionT vq(0, v.x(), v.y(), v.z());
                //QuaternionT qi = assume_unit_quat ? q.conjugate() : q.inverse();
                //return (q * vq * qi).vec();

            return q._transformVector(v);
        }

        static Vector3T rotateVectorReverse(const Vector3T& v, const QuaternionT& q, bool assume_unit_quat)
        {
            //QuaternionT vq(0, v.x(), v.y(), v.z());
            //QuaternionT qi = assume_unit_quat ? q.conjugate() : q.inverse();
            //return (qi * vq * q).vec();

            if (!assume_unit_quat)
                return q.inverse()._transformVector(v);
            else
                return q.conjugate()._transformVector(v);
        }

        static QuaternionT rotateQuaternion(const QuaternionT& q, const QuaternionT& ref, bool assume_unit_quat)
        {
            if (assume_unit_quat) {
                // conjugate and inverse are equivalent for unit-length quaternions,
                // but the conjugate is less expensive to compute
                QuaternionT ref_n = ref;
                QuaternionT ref_n_i = ref.conjugate();
                return ref_n * q * ref_n_i;
            }
            else {
                QuaternionT ref_n = ref.normalized();
                QuaternionT ref_n_i = ref.inverse();
                return ref_n * q * ref_n_i;
            }
        }

        static QuaternionT rotateQuaternionReverse(const QuaternionT& q, const QuaternionT& ref, bool assume_unit_quat)
        {
            if (assume_unit_quat) {
                QuaternionT ref_n = ref;
                QuaternionT ref_n_i = ref.conjugate();
                return ref_n_i * q * ref_n;
            }
            else {
                QuaternionT ref_n = ref.normalized();
                QuaternionT ref_n_i = ref.inverse();
                return ref_n_i * q * ref_n;
            }
        }

        static Vector3T transformToBodyFrame(const Vector3T& v_world, const QuaternionT& q_world, bool assume_unit_quat = true)
        {
            return rotateVectorReverse(v_world, q_world, assume_unit_quat);
        }

        static Vector3T transformToBodyFrame(const Vector3T& v_world, const Pose& body_world, bool assume_unit_quat = true)
        {
            //translate
            Vector3T translated = v_world - body_world.position;
            //rotate
            return transformToBodyFrame(translated, body_world.orientation, assume_unit_quat);
        }

        static Pose transformToBodyFrame(const Pose& pose_world, const Pose& body_world, bool assume_unit_quat = true)
        {
            //translate
            Vector3T translated = pose_world.position - body_world.position;
            //rotate vector
            Vector3T v_body = transformToBodyFrame(translated, body_world.orientation, assume_unit_quat);
            //rotate orientation
            QuaternionT q_body = rotateQuaternionReverse(pose_world.orientation, body_world.orientation, assume_unit_quat);

            return Pose(v_body, q_body);
        }

        static Vector3T transformToWorldFrame(const Vector3T& v_body, const QuaternionT& q_world, bool assume_unit_quat = true)
        {
            return rotateVector(v_body, q_world, assume_unit_quat);
        }

        static Vector3T transformToWorldFrame(const Vector3T& v_body, const Pose& body_world, bool assume_unit_quat = true)
        {
            //rotate
            Vector3T v_world = transformToWorldFrame(v_body, body_world.orientation, assume_unit_quat);
            //translate
            return v_world + body_world.position;
        }

        //transform pose specified in body frame to world frame. The body frame in world coordinate is at body_world
        static Pose transformToWorldFrame(const Pose& pose_body, const Pose& body_world, bool assume_unit_quat = true)
        {
            //rotate position
            Vector3T v_world = transformToWorldFrame(pose_body.position, body_world.orientation, assume_unit_quat);
            //rotate orientation
            QuaternionT q_world = rotateQuaternion(pose_body.orientation, body_world.orientation, assume_unit_quat);
            //translate
            return Pose(v_world + body_world.position, q_world);
        }

        static QuaternionT negate(const QuaternionT& q)
        {
            //from Gazebo implementation
            return QuaternionT(-q.w(), -q.x(), -q.y(), -q.z());
        }

        static Vector3T getRandomVectorFromGaussian(RealT stddev = 1, RealT mean = 0)
        {
            return Vector3T(
                Utils::getRandomFromGaussian(stddev, mean),
                Utils::getRandomFromGaussian(stddev, mean),
                Utils::getRandomFromGaussian(stddev, mean));
        }

        static QuaternionT flipZAxis(const QuaternionT& q)
        {
            //quaternion formula comes from http://stackoverflow.com/a/40334755/207661
            return QuaternionT(q.w(), -q.x(), -q.y(), q.z());
        }

        static void toEulerianAngle(const QuaternionT& q, RealT& pitch, RealT& roll, RealT& yaw)
        {
            //z-y-x rotation convention (Tait-Bryan angles)
            //Apply yaw, pitch and roll in order to front vector (+X)
            //http://www.sedris.org/wg8home/Documents/WG80485.pdf
            //http://www.ctralie.com/Teaching/COMPSCI290/Materials/EulerAnglesViz/

            RealT ysqr = q.y() * q.y();

            // roll (x-axis rotation)
            RealT t0 = +2.0f * (q.w() * q.x() + q.y() * q.z());
            RealT t1 = +1.0f - 2.0f * (q.x() * q.x() + ysqr);
            roll = std::atan2(t0, t1);

            // pitch (y-axis rotation)
            RealT t2 = +2.0f * (q.w() * q.y() - q.z() * q.x());
            t2 = ((t2 > 1.0f) ? 1.0f : t2);
            t2 = ((t2 < -1.0f) ? -1.0f : t2);
            pitch = std::asin(t2);

            // yaw (z-axis rotation)
            RealT t3 = +2.0f * (q.w() * q.z() + q.x() * q.y());
            RealT t4 = +1.0f - 2.0f * (ysqr + q.z() * q.z());
            yaw = std::atan2(t3, t4);
        }

        static RealT angleBetween(const Vector3T& v1, const Vector3T& v2, bool assume_normalized = false)
        {
            Vector3T v1n = v1;
            Vector3T v2n = v2;
            if (!assume_normalized) {
                v1n.normalize();
                v2n.normalize();
            }

            return std::acos(v1n.dot(v2n));
        }

        static Vector3T toAngularVelocity(const QuaternionT& start, const QuaternionT& end, RealT dt)
        {
            if (dt == 0)
                return Vector3T(0, 0, 0);

            RealT p_s, r_s, y_s;
            toEulerianAngle(start, p_s, r_s, y_s);

            RealT p_e, r_e, y_e;
            toEulerianAngle(end, p_e, r_e, y_e);

            RealT p_rate = normalizeAngle(p_e - p_s, (RealT)(2 * M_PI)) / dt;
            RealT r_rate = normalizeAngle(r_e - r_s, (RealT)(2 * M_PI)) / dt;
            RealT y_rate = normalizeAngle(y_e - y_s, (RealT)(2 * M_PI)) / dt;

            //TODO: optimize below
            //Sec 1.3, https://ocw.mit.edu/courses/mechanical-engineering/2-154-maneuvering-and-control-of-surface-and-underwater-vehicles-13-49-fall-2004/lecture-notes/lec1.pdf
            RealT wx = r_rate + 0 - y_rate * sinf(p_e);
            RealT wy = 0 + p_rate * cosf(r_e) + y_rate * sinf(r_e) * cosf(p_e);
            RealT wz = 0 - p_rate * sinf(r_e) + y_rate * cosf(r_e) * cosf(p_e);

            return Vector3T(wx, wy, wz);
        }

        static Vector3T nanVector()
        {
            static const Vector3T val(std::numeric_limits<RealT>::quiet_NaN(), std::numeric_limits<RealT>::quiet_NaN(), std::numeric_limits<RealT>::quiet_NaN());
            return val;
        }

        static QuaternionT nanQuaternion()
        {
            return QuaternionT(std::numeric_limits<RealT>::quiet_NaN(), std::numeric_limits<RealT>::quiet_NaN(), std::numeric_limits<RealT>::quiet_NaN(), std::numeric_limits<RealT>::quiet_NaN());
        }

        static bool hasNan(const Vector3T& v)
        {
            return std::isnan(v.x()) || std::isnan(v.y()) || std::isnan(v.z());
        }
        static bool hasNan(const QuaternionT& q)
        {
            return std::isnan(q.x()) || std::isnan(q.y()) || std::isnan(q.z()) || std::isnan(q.w());
        }
        static bool hasNan(const Pose& p)
        {
            return hasNan(p.position) || hasNan(p.orientation);
        }

        static QuaternionT addAngularVelocity(const QuaternionT& orientation, const Vector3T& angular_vel, RealT dt)
        {
            QuaternionT dq_unit = QuaternionT(0, angular_vel.x() * 0.5f, angular_vel.y() * 0.5f, angular_vel.z() * 0.5f) * orientation;
            QuaternionT net_q(dq_unit.coeffs() * dt + orientation.coeffs());
            return net_q.normalized();
        }
        //all angles in radians
        static QuaternionT toQuaternion(RealT pitch, RealT roll, RealT yaw)
        {
            //z-y-x rotation convention (Tait-Bryan angles)
            //http://www.sedris.org/wg8home/Documents/WG80485.pdf

            QuaternionT q;
            RealT t0 = std::cos(yaw * 0.5f);
            RealT t1 = std::sin(yaw * 0.5f);
            RealT t2 = std::cos(roll * 0.5f);
            RealT t3 = std::sin(roll * 0.5f);
            RealT t4 = std::cos(pitch * 0.5f);
            RealT t5 = std::sin(pitch * 0.5f);

            q.w() = t0 * t2 * t4 + t1 * t3 * t5;
            q.x() = t0 * t3 * t4 - t1 * t2 * t5;
            q.y() = t0 * t2 * t5 + t1 * t3 * t4;
            q.z() = t1 * t2 * t4 - t0 * t3 * t5;
            return q;
        }

        //from https://github.com/arpg/Gazebo/blob/master/gazebo/math/Pose.cc
        static Vector3T coordPositionSubtract(const Pose& lhs, const Pose& rhs)
        {
            QuaternionT tmp(0,
                            lhs.position.x() - rhs.position.x(),
                            lhs.position.y() - rhs.position.y(),
                            lhs.position.z() - rhs.position.z());

            tmp = rhs.orientation.inverse() * (tmp * rhs.orientation);

            return tmp.vec();
        }
        static QuaternionT coordOrientationSubtract(const QuaternionT& lhs, const QuaternionT& rhs)
        {
            QuaternionT result(rhs.inverse() * lhs);
            result.normalize();
            return result;
        }
        static Vector3T coordPositionAdd(const Pose& lhs, const Pose& rhs)
        {
            QuaternionT tmp(0, lhs.position.x(), lhs.position.y(), lhs.position.z());

            tmp = rhs.orientation * (tmp * rhs.orientation.inverse());

            return tmp.vec() + rhs.position;
        }
        static QuaternionT coordOrientationAdd(const QuaternionT& lhs, const QuaternionT& rhs)
        {
            QuaternionT result(rhs * lhs);
            result.normalize();
            return result;
        }
        static Pose subtract(const Pose& lhs, const Pose& rhs)
        {
            return Pose(coordPositionSubtract(lhs, rhs), coordOrientationSubtract(lhs.orientation, rhs.orientation));
        }
        static Pose add(const Pose& lhs, const Pose& rhs)
        {
            return Pose(coordPositionAdd(lhs, rhs), coordOrientationAdd(lhs.orientation, rhs.orientation));
        }

        static std::string toString(const Vector3T& vect, const char* prefix = nullptr)
        {
            if (prefix)
                return Utils::stringf("%s[%f, %f, %f]", prefix, vect[0], vect[1], vect[2]);
            else
                return Utils::stringf("[%f, %f, %f]", vect[0], vect[1], vect[2]);
        }
        static std::string toString(const QuaternionT& quaternion, bool add_eularian = false)
        {
            if (!add_eularian)
                return Utils::stringf("[%f, %f, %f, %f]", quaternion.w(), quaternion.x(), quaternion.y(), quaternion.z());
            else {
                RealT pitch, roll, yaw;
                toEulerianAngle(quaternion, pitch, roll, yaw);
                return Utils::stringf("[%f, %f, %f, %f]-[%f, %f, %f]",
                                      quaternion.w(),
                                      quaternion.x(),
                                      quaternion.y(),
                                      quaternion.z(),
                                      pitch,
                                      roll,
                                      yaw);
            }
        }
        static std::string toString(const Vector2f& vect)
        {
            return Utils::stringf("[%f, %f]", vect[0], vect[1]);
        }

        static RealT getYaw(const QuaternionT& q)
        {
            return std::atan2(2.0f * (q.z() * q.w() + q.x() * q.y()), -1.0f + 2.0f * (q.w() * q.w() + q.x() * q.x()));
        }

        static RealT getPitch(const QuaternionT& q)
        {
            return std::asin(2.0f * (q.y() * q.w() - q.z() * q.x()));
        }

        static RealT getRoll(const QuaternionT& q)
        {
            return std::atan2(2.0f * (q.z() * q.y() + q.w() * q.x()), 1.0f - 2.0f * (q.x() * q.x() + q.y() * q.y()));
        }

        static RealT normalizeAngle(RealT angle, RealT max_angle = static_cast<RealT>(360))
        {
            angle = static_cast<RealT>(std::fmod(angle, max_angle));
            if (angle > max_angle / 2)
                return angle - max_angle;
            else if (angle < -max_angle / 2)
                return angle + max_angle;
            else
                return angle;
        }

        // assumes that angles are in 0-360 range
        static bool isAngleBetweenAngles(RealT angle, RealT start_angle, RealT end_angle)
        {
            if (start_angle < end_angle) {
                return (start_angle <= angle && angle <= end_angle);
            }
            else
                return (start_angle <= angle || angle <= end_angle);
        }

        /**
	* \brief Extracts the yaw part from a quaternion, using RPY / euler (z-y'-z'') angles.
	* RPY rotates about the fixed axes in the order x-y-z,
	* which is the same as euler angles in the order z-y'-x''.
	*/
        static RealT yawFromQuaternion(const QuaternionT& q)
        {
            return atan2(2.0 * (q.w() * q.z() + q.x() * q.y()),
                         1.0 - 2.0 * (q.y() * q.y() + q.z() * q.z()));
        }

        static QuaternionT quaternionFromYaw(RealT yaw)
        {
            return QuaternionT(Eigen::AngleAxis<RealT>(yaw, Vector3T::UnitZ()));
        }

        static QuaternionT toQuaternion(const Vector3T& axis, RealT angle)
        {
            //Alternative:
            //auto s = std::sin(angle / 2);
            //auto u = axis.normalized();
            //return Quaternionr(std::cos(angle / 2), u.x() * s, u.y() * s, u.z() * s);

            return QuaternionT(Eigen::AngleAxis<RealT>(angle, axis));
        }

        //linear interpolate
        static QuaternionT lerp(const QuaternionT& from, const QuaternionT& to, RealT alpha)
        {
            QuaternionT r;
            RealT n_alpha = 1 - alpha;
            r.x() = n_alpha * from.x() + alpha * to.x();
            r.y() = n_alpha * from.y() + alpha * to.y();
            r.z() = n_alpha * from.z() + alpha * to.z();
            r.w() = n_alpha * from.w() + alpha * to.w();
            return r.normalized();
        }

        //spherical lerp
        static QuaternionT slerp(const QuaternionT& from, const QuaternionT& to, RealT alpha)
        {
            /*
        //below is manual way to do this
		RealT n_alpha = 1 - alpha;
		RealT theta = acos(from.x()*to.x() + from.y()*to.y() + from.z()*to.z() + from.w()*to.w());
		//Check for theta > 0 to avoid division by 0.
		if (theta > std::numeric_limits<RealT>::epsilon())
		{
			RealT sn = sin(theta);
			RealT Wa = sin(n_alpha*theta) / sn;
			RealT Wb = sin(alpha*theta) / sn;
			QuaternionT r;
			r.x() = Wa * from.x() + Wb * to.x();
			r.y() = Wa * from.y() + Wb * to.y();
			r.z() = Wa * from.z() + Wb * to.z();
			r.w() = Wa * from.w() + Wb * to.w();
			return r.normalized();
		}
		//Theta is almost 0. Return "to" quaternion.
		//Alternatively, could also do lerp.
		else {
			return to.normalized();
		}
        */

            return from.slerp(alpha, to);
        }

        static Vector3T lerp(const Vector3T& from, const Vector3T& to, RealT alpha)
        {
            return (from + alpha * (to - from));
        }

        static Vector3T slerp(const Vector3T& from, const Vector3T& to, RealT alpha, bool assume_normalized)
        {
            Vector3T from_ortho, to_ortho;
            RealT dot;
            getPlaneOrthoVectors(from, to, assume_normalized, from_ortho, to_ortho, dot);

            RealT theta = std::acos(dot) * alpha;

            return from_ortho * std::cos(theta) + to_ortho * std::sin(theta);
        }

        static void getPlaneOrthoVectors(const Vector3T& from, const Vector3T& to, bool assume_normalized,
                                         Vector3T& from_ortho, Vector3T& to_ortho, RealT& dot)
        {
            unused(from);

            Vector3T to_n = to;

            if (!assume_normalized) {
                from_ortho.normalize();
                to_n.normalize();
            }

            dot = from_ortho.dot(to_n);
            dot = Utils::clip<RealT>(dot, -1, 1);
            to_ortho = (to_n - from_ortho * dot).normalized();
        }

        static Vector3T slerpByAngle(const Vector3T& from, const Vector3T& to, RealT angle, bool assume_normalized = false)
        {
            Vector3T from_ortho, to_ortho;
            RealT dot;
            getPlaneOrthoVectors(from, to, assume_normalized, from_ortho, to_ortho, dot);

            return from_ortho * std::cos(angle) + to_ortho * std::sin(angle);
        }

        static Vector3T nlerp(const Vector3T& from, const Vector3T& to, float alpha)
        {
            return lerp(from, to, alpha).normalized();
        }

        //assuming you are looking at front() vector, what rotation you need to look at destPoint?
        static QuaternionT lookAt(const Vector3T& sourcePoint, const Vector3T& destPoint)
        {
            /*
        //below is manual way to do this without Eigen
		Vector3T toVector = (destPoint - sourcePoint);
		toVector.normalize(); //this is important!

		RealT dot = VectorMathT::front().dot(toVector);
		RealT ang = std::acos(dot);

		Vector3T axis = VectorMathT::front().cross(toVector);
		if (axis == Vector3T::Zero())
			axis = VectorMathT::up();
		else
			axis = axis.normalized();

		return VectorMathT::toQuaternion(axis, ang);
        */

            return QuaternionT::FromTwoVectors(VectorMathT::front(), destPoint - sourcePoint);
        }

        //what rotation we need to rotate "" vector to "to" vector (rotation is around intersection of two vectors)
        static QuaternionT toQuaternion(const Vector3T& from, const Vector3T& to)
        {
            return QuaternionT::FromTwoVectors(from, to);
        }

        static const Vector3T front()
        {
            static Vector3T v(1, 0, 0);
            return v;
        }
        static const Vector3T back()
        {
            static Vector3T v(-1, 0, 0);
            return v;
        }
        static const Vector3T down()
        {
            static Vector3T v(0, 0, 1);
            return v;
        }
        static const Vector3T up()
        {
            static Vector3T v(0, 0, -1);
            return v;
        }
        static const Vector3T right()
        {
            static Vector3T v(0, 1, 0);
            return v;
        }
        static const Vector3T left()
        {
            static Vector3T v(0, -1, 0);
            return v;
        }
    };
    typedef VectorMathT<Eigen::Vector3d, Eigen::Quaternion<double, Eigen::DontAlign>, double> VectorMathd;
    typedef VectorMathT<Eigen::Vector3f, Eigen::Quaternion<float, Eigen::DontAlign>, float> VectorMathf;
}
} //namespace
#endif
