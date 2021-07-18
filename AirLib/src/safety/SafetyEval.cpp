// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

//in header only mode, control library is not available
#ifndef AIRLIB_HEADER_ONLY

#include "safety/SafetyEval.hpp"
#include "vehicles/multirotor/api/MultirotorCommon.hpp"

#include <cmath>
#include <sstream>

namespace msr
{
namespace airlib
{

//TODO: something defines max macro which interfears with code here
#undef max

    SafetyEval::SafetyEval(MultirotorApiParams vehicle_params, shared_ptr<IGeoFence> fence_ptr, shared_ptr<ObstacleMap> obs_xy_ptr)
        : vehicle_params_(vehicle_params), fence_ptr_(fence_ptr), obs_xy_ptr_(obs_xy_ptr)
    {
        Utils::log(Utils::stringf("enable_reasons: %X, obs_strategy=%X", uint(enable_reasons_), uint(obs_strategy_)));
    }

    void SafetyEval::checkFence(const Vector3r& dest_pos, const Vector3r& cur_pos, SafetyEval::EvalResult& appendToResult)
    {
        if (!(enable_reasons_ & SafetyViolationType_::GeoFence)) {
            return;
        }

        //is cur_pos within fence?
        bool in_fence, allow;
        fence_ptr_->checkFence(cur_pos, dest_pos, in_fence, allow);
        if (!allow) {
            appendToResult.is_safe = false;
            appendToResult.reason |= SafetyViolationType_::GeoFence;
            appendToResult.message.append(
                common_utils::Utils::stringf("Destination %s is outside geo fence and is worse off than current %s in geofence=[%s]",
                                             VectorMath::toString(dest_pos).c_str(),
                                             VectorMath::toString(cur_pos).c_str(),
                                             fence_ptr_->toString().c_str()));
        }
    }

    SafetyEval::EvalResult SafetyEval::isSafeDestination(const Vector3r& dest_pos, const Vector3r& cur_pos, const Quaternionr& quaternion)
    {
        SafetyEval::EvalResult result;

        isSafeDestination(dest_pos, cur_pos, quaternion, result);
        return result;
    }

    SafetyEval::EvalResult SafetyEval::isSafePosition(const Vector3r& cur_pos, const Quaternionr& quaternion)
    {
        SafetyEval::EvalResult result;

        isSafeDestination(cur_pos, cur_pos, quaternion, result);
        return result;
    }

    bool SafetyEval::isThisRiskDistLess(float this_risk_dist, float other_risk_dist) const
    {
        //destination risk is not available then consider it zero
        if (std::isnan(other_risk_dist))
            other_risk_dist = this_risk_dist * 2; //2X-X == X

        //if dest risk is lower than its more safe
        return other_risk_dist - this_risk_dist <= vehicle_params_.distance_accuracy;
    }

    void SafetyEval::isCurrentSafer(SafetyEval::EvalResult& result)
    {
        //are we doing better than closest obstacle?
        result.cur_obs = obs_xy_ptr_->getClosestObstacle();

        //if we stay where we are, what is the risk distance?
        result.cur_risk_dist = adjustClearanceForPrStl(vehicle_params_.obs_clearance, result.cur_obs.confidence) - result.cur_obs.distance;

        if (!isThisRiskDistLess(result.cur_risk_dist, result.dest_risk_dist)) {
            result.is_safe = false;
            result.reason |= SafetyViolationType_::Obstacle;
            result.message.append(
                common_utils::Utils::stringf("Current position is safer"));
        }
        //else we are better of moving to dest
    }

    void SafetyEval::isSafeDestination(const Vector3r& dest_pos, const Vector3r& cur_pos, const Quaternionr& quaternion, SafetyEval::EvalResult& result)
    {
        //this function should work even when dest_pos == cur_pos
        result.dest_pos = dest_pos;
        result.cur_pos = cur_pos;

        //is this dest_pos cur_pos within the fence?
        checkFence(dest_pos, cur_pos, result);

        if (!(enable_reasons_ & SafetyViolationType_::Obstacle))
            return;

        //transform dest_pos vector to body frame
        const Vector3r cur_dest = dest_pos - cur_pos;
        float cur_dest_norm = cur_dest.norm();

        //check for approx zero vectors to avoid random yaw angles
        if (cur_dest_norm < vehicle_params_.distance_accuracy) {
            //we are hovering
            result.dest_risk_dist = Utils::nan<float>();
            isCurrentSafer(result);
        }
        else { //see if we have obstacle in direction
            result.cur_dest_body = VectorMath::transformToBodyFrame(cur_dest, quaternion, true);

            //get yaw in body frame, ie, front is always 0 radians
            float point_angle = std::atan2(result.cur_dest_body[1], result.cur_dest_body[0]);

            //yaw to ticks
            int point_tick = obs_xy_ptr_->angleToTick(point_angle);

            //get obstacles in the window at the tick direction around the window
            result.dest_obs = obs_xy_ptr_->hasObstacle(point_tick - vehicle_params_.obs_window, point_tick + vehicle_params_.obs_window);

            //less risk distance is better
            result.dest_risk_dist = cur_dest_norm + adjustClearanceForPrStl(vehicle_params_.obs_clearance, result.dest_obs.confidence) - result.dest_obs.distance;
            if (result.dest_risk_dist >= 0) { //potential collision
                //check obstacles around current position and see if it has lower risk
                isCurrentSafer(result);
            }
            //else obstacle is too far
        }

        //if we detected unsafe condition due to obstacle, find direction to move away to
        if (!result.is_safe && result.reason & SafetyViolationType_::Obstacle) {
            //look for each surrounding tick to see if we have obstacle free angle
            setSuggestedVelocity(result, quaternion);

            Utils::log(Utils::stringf("isSafeDestination: cur_dest_norm=%f, result=%s, quaternion=%s",
                                      cur_dest_norm,
                                      result.toString().c_str(),
                                      VectorMath::toString(quaternion, true).c_str()));
        }
        //else no suggestions required
    }

    float SafetyEval::adjustClearanceForPrStl(float base_clearance, float obs_confidence)
    {
        //3.2 comes from inverse CDF for epsilon = 0.05 (i.e. 95% confidence), author: akapoor
        float additional_clearance = (1 - obs_confidence) * 3.2f;
        if (additional_clearance != 0)
            Utils::log(Utils::stringf("additional_clearance=%f", additional_clearance));

        return base_clearance + additional_clearance;
    }

    void SafetyEval::setSuggestedVelocity(SafetyEval::EvalResult& result, const Quaternionr& quaternion)
    {
        result.suggested_vec = Vector3r::Zero(); //default suggestion
        if (obs_strategy_ == ObsAvoidanceStrategy::RaiseException)
            return;

        int ref_tick;
        int ticks = obs_xy_ptr_->getTicks();
        if (obs_strategy_ == ObsAvoidanceStrategy::ClosestMove)
            ref_tick = result.cur_obs.tick;
        else if (obs_strategy_ == ObsAvoidanceStrategy::OppositeMove)
            ref_tick = result.cur_obs.tick + ticks / 2; //opposite direction of obstacle
        else
            ref_tick = 0; //default doesn't matter as we will raise exception

        for (int i = 0; i <= ticks / 2; ++i) {
            //evaluate right and left side of circle
            ObstacleMap::ObstacleInfo right_obs = obs_xy_ptr_->hasObstacle(ref_tick + i, ref_tick + i);
            ObstacleMap::ObstacleInfo left_obs = obs_xy_ptr_->hasObstacle(ref_tick - i, ref_tick - i);

            //find right and left risk distances
            float right_risk_dist = adjustClearanceForPrStl(vehicle_params_.obs_clearance, right_obs.confidence) - right_obs.distance;
            float left_risk_dist = adjustClearanceForPrStl(vehicle_params_.obs_clearance, left_obs.confidence) - left_obs.distance;

            //at this point we have already determined hover is better than going to dest
            //we now determine is moving to suggested angle better than hovering?
            if (right_risk_dist <= 0 || left_risk_dist <= 0) {
                int suggested_tick = right_risk_dist < left_risk_dist ? right_obs.tick : left_obs.tick;
                result.suggested_obs = right_risk_dist < left_risk_dist ? right_obs : left_obs;

                float suggested_angle = obs_xy_ptr_->tickToAngleMid(suggested_tick);
                const Vector3r suggested_body = Vector3r(std::cos(suggested_angle), std::sin(suggested_angle), 0).normalized();
                result.suggested_vec = VectorMath::transformToWorldFrame(suggested_body, quaternion, true);

                Utils::log(Utils::stringf("right_risk_dist=%f, left_risk_dist=%f, suggested_tick=%i, suggested_angle=%f", right_risk_dist, left_risk_dist, suggested_tick, suggested_angle, suggested_angle));

                break; //if none found then suggested_vec is left as zero vec, meaning enter hover mode
            }
        }
    }

    SafetyEval::EvalResult SafetyEval::isSafeVelocityZ(const Vector3r& cur_pos, float vx, float vy, float z, const Quaternionr& quaternion)
    {
        SafetyEval::EvalResult result;

        Vector3r dest_pos = getDestination(cur_pos, Vector3r(vx, vy, 0));
        dest_pos.z() = z;

        //check if dest_pos is safe
        isSafeDestination(dest_pos, cur_pos, quaternion, result);

        return result;
    }

    Vector3r SafetyEval::getDestination(const Vector3r& cur_pos, const Vector3r& velocity) const
    {
        //breaking distance at this velocity
        float velocity_mag = velocity.norm();
        float dest_pos_dist = std::max(velocity_mag * vehicle_params_.vel_to_breaking_dist,
                                       vehicle_params_.min_breaking_dist);

        //calculate dest_pos cur_pos we will be if we had to break suddenly
        return velocity_mag >= vehicle_params_.distance_accuracy ? (cur_pos + (velocity / velocity_mag) * dest_pos_dist) : cur_pos;
    }

    SafetyEval::EvalResult SafetyEval::isSafeVelocity(const Vector3r& cur_pos, const Vector3r& velocity, const Quaternionr& quaternion)
    {
        SafetyEval::EvalResult result;

        const Vector3r dest_pos = getDestination(cur_pos, velocity);

        //check if dest_pos is safe
        isSafeDestination(dest_pos, cur_pos, quaternion, result);

        return result;
    }

    //float/vec parameters can have NaN which makes them optional
    void SafetyEval::setSafety(SafetyViolationType enable_reasons, float obs_clearance, SafetyEval::ObsAvoidanceStrategy obs_strategy,
                               const Vector3r& origin, float xy_length, float max_z, float min_z)
    {
        if (!origin.hasNaN() && !std::isnan(xy_length) && !std::isnan(max_z) && !std::isnan(min_z))
            fence_ptr_->setBoundry(origin, xy_length, max_z, min_z);

        if (!std::isnan(obs_clearance))
            vehicle_params_.obs_clearance = obs_clearance;

        enable_reasons_ = enable_reasons;
        setObsAvoidanceStrategy(obs_strategy);

        Utils::log(Utils::stringf("enable_reasons: %X", uint(enable_reasons)));
    }
    void SafetyEval::setObsAvoidanceStrategy(SafetyEval::ObsAvoidanceStrategy obs_strategy)
    {
        obs_strategy_ = obs_strategy;
        Utils::log(Utils::stringf("obs_strategy=%X", uint(obs_strategy)));
    }
    SafetyEval::ObsAvoidanceStrategy SafetyEval::getObsAvoidanceStrategy()
    {
        return obs_strategy_;
    }
}
} //namespace

#endif
