#pragma once

#include <vector>
#include "interfaces/CommonStructs.hpp"
#include "interfaces/IBoard.hpp"
#include "interfaces/ICommLink.hpp"
#include "interfaces/IStateEstimator.hpp"
#include "interfaces/IFirmware.hpp"
#include "Params.hpp"
#include "RemoteControl.hpp"
#include "OffboardApi.hpp"
#include "Mixer.hpp"
#include "CascadeController.hpp"
#include "AdaptiveController.hpp"

namespace simple_flight
{

class Firmware : public IFirmware
{
public:
    Firmware(Params* params, IBoard* board, ICommLink* comm_link, IStateEstimator* state_estimator)
        : params_(params), board_(board), comm_link_(comm_link), state_estimator_(state_estimator), offboard_api_(params, board, board, state_estimator, comm_link), mixer_(params)
    {
        switch (params->controller_type) {
        case Params::ControllerType::Cascade:
            controller_ = std::unique_ptr<CascadeController>(new CascadeController(params, board, comm_link));
            break;
        case Params::ControllerType::Adaptive:
            controller_ = std::unique_ptr<AdaptiveController>(new AdaptiveController());
            break;
        default:
            throw std::invalid_argument("Cannot recognize controller specified by params->controller_type");
        }

        controller_->initialize(&offboard_api_, state_estimator_);
    }

    virtual void reset() override
    {
        IFirmware::reset();

        board_->reset();
        comm_link_->reset();
        controller_->reset();
        offboard_api_.reset();

        motor_outputs_.assign(params_->motor.motor_count, 0);
    }

    virtual void update() override
    {
        IFirmware::update();

        board_->update();
        offboard_api_.update();
        controller_->update();

        const Axis4r& output_controls = controller_->getOutput();

        // if last goal mode is passthrough for all axes (which means moveByMotorPWMs was called),
        // we directly set the motor outputs to controller outputs
        // note that the order of motors is as explained MultiRotorParams::initializeRotorQuadX()
        if (controller_->isLastGoalModeAllPassthrough()) {
            for (uint16_t motor_index = 0; motor_index < params_->motor.motor_count; ++motor_index)
                motor_outputs_[motor_index] = output_controls[motor_index];
        }
        else {
            // apply motor mixing matrix to convert from controller output to motor outputs
            mixer_.getMotorOutput(output_controls, motor_outputs_);
        }

        //finally write the motor outputs
        for (uint16_t motor_index = 0; motor_index < params_->motor.motor_count; ++motor_index)
            board_->writeOutput(motor_index, motor_outputs_.at(motor_index));

        comm_link_->update();
    }

    virtual IOffboardApi& offboardApi() override
    {
        return offboard_api_;
    }

private:
    //objects we use
    Params* params_;
    IBoard* board_;
    ICommLink* comm_link_;
    IStateEstimator* state_estimator_;

    OffboardApi offboard_api_;
    Mixer mixer_;
    std::unique_ptr<IController> controller_;

    std::vector<float> motor_outputs_;
};

} //namespace
