// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "MavLinkNodeImpl.hpp"
#include "Utils.hpp"
#include "MavLinkMessages.hpp"
#include "Semaphore.hpp"
#include "ThreadUtils.hpp"

using namespace mavlink_utils;

using namespace mavlinkcom_impl;

const int heartbeatMilliseconds = 1000;

MavLinkNodeImpl::MavLinkNodeImpl(int localSystemId, int localComponentId)
{
    local_system_id = localSystemId;
    local_component_id = localComponentId;
}

MavLinkNodeImpl::~MavLinkNodeImpl()
{
    close();
}

// start listening to this connection
void MavLinkNodeImpl::connect(std::shared_ptr<MavLinkConnection> connection)
{
    has_cap_ = false;
    connection_ = connection;
    subscription_ = connection_->subscribe([=](std::shared_ptr<MavLinkConnection> con, const MavLinkMessage& msg) {
        handleMessage(con, msg);
    });
}

// Send heartbeat to drone.  You should not do this if some other node is
// already doing it.
void MavLinkNodeImpl::startHeartbeat()
{
    if (!heartbeat_running_) {
        heartbeat_running_ = true;
        Utils::cleanupThread(heartbeat_thread_);
        heartbeat_thread_ = std::thread{ &MavLinkNodeImpl::sendHeartbeat, this };
    }
}

void MavLinkNodeImpl::sendHeartbeat()
{
    CurrentThread::setThreadName("MavLinkThread");
    while (heartbeat_running_) {
        sendOneHeartbeat();
        std::this_thread::sleep_for(std::chrono::milliseconds(heartbeatMilliseconds));
    }
}

// this is called for all messages received on the connection.
void MavLinkNodeImpl::handleMessage(std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& msg)
{
    unused(connection);

    switch (msg.msgid) {
    case static_cast<uint8_t>(MavLinkMessageIds::MAVLINK_MSG_ID_HEARTBEAT):
        // we received a heartbeat, so let's get the capabilities.
        if (!req_cap_) {
            req_cap_ = true;
            MavCmdRequestAutopilotCapabilities cmd{};
            cmd.param1 = 1;
            sendCommand(cmd);
        }
        break;
    case static_cast<uint8_t>(MavLinkMessageIds::MAVLINK_MSG_ID_AUTOPILOT_VERSION):
        cap_.decode(msg);
        has_cap_ = true;
        break;
    }

    // this is for the subclasses to play with.  We put nothing here so we are not dependent on the
    // subclasses remembering to call this base implementation.
}

// stop listening to the connection.
void MavLinkNodeImpl::close()
{
    if (subscription_ != 0 && connection_ != nullptr) {
        connection_->unsubscribe(subscription_);
        subscription_ = 0;
    }
    if (heartbeat_running_) {
        heartbeat_running_ = false;
        if (heartbeat_thread_.joinable()) {
            heartbeat_thread_.join();
        }
    }
    if (connection_ != nullptr) {
        connection_->close();
    }
    connection_ = nullptr;
}

AsyncResult<MavLinkAutopilotVersion> MavLinkNodeImpl::getCapabilities()
{
    if (has_cap_) {
        AsyncResult<MavLinkAutopilotVersion> nowait([=](int state) {
            unused(state);
        });
        nowait.setResult(cap_);
        return nowait;
    }

    auto con = ensureConnection();

    AsyncResult<MavLinkAutopilotVersion> result([=](int state) {
        con->unsubscribe(state);
    });

    int subscription = con->subscribe([=](std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& m) {
        unused(connection);
        unused(m);
        result.setResult(cap_);
    });

    result.setState(subscription);

    // request capabilities, it will respond with AUTOPILOT_VERSION.
    MavCmdRequestAutopilotCapabilities cmd{};
    cmd.param1 = 1;
    sendCommand(cmd);

    return result;
}

AsyncResult<MavLinkHeartbeat> MavLinkNodeImpl::waitForHeartbeat()
{
    Utils::log("Waiting for heartbeat from PX4...");

    // wait for a heartbeat msg since this will give us the port to send commands to...
    //this->setMessageInterval(static_cast<int>(MavLinkMessageIds::MAVLINK_MSG_ID_HEARTBEAT), 1);
    auto con = ensureConnection();

    AsyncResult<MavLinkHeartbeat> result([=](int state) {
        con->unsubscribe(state);
    });

    int subscription = con->subscribe([=](std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& m) {
        unused(connection);
        if (m.msgid == static_cast<uint8_t>(MavLinkMessageIds::MAVLINK_MSG_ID_HEARTBEAT)) {
            MavLinkHeartbeat heartbeat;
            heartbeat.decode(m);
            result.setResult(heartbeat);
        }
    });
    result.setState(subscription);

    return result;
}

void MavLinkNodeImpl::sendOneHeartbeat()
{
    MavLinkHeartbeat heartbeat;
    // send a heart beat so that the remote node knows we are still alive
    // (otherwise drone will trigger a failsafe operation).
    heartbeat.autopilot = static_cast<uint8_t>(MAV_AUTOPILOT::MAV_AUTOPILOT_GENERIC);
    heartbeat.type = static_cast<uint8_t>(MAV_TYPE::MAV_TYPE_GCS);
    heartbeat.mavlink_version = 3;
    heartbeat.base_mode = 0; // ignored by PX4
    heartbeat.custom_mode = 0; // ignored by PX4
    heartbeat.system_status = 0; // ignored by PX4
    try {
        sendMessage(heartbeat);
    }
    catch (std::exception& e) {
        // ignore any failures here because we are running in our own thread here.
        Utils::log(Utils::stringf("Caught and ignoring exception sending heartbeat: %s", e.what()));
    }
}

void MavLinkNodeImpl::setMessageInterval(int msgId, int frequency)
{
    float intervalMicroseconds = 1000000.0f / frequency;
    MavCmdSetMessageInterval cmd{};
    cmd.MessageId = static_cast<float>(msgId);
    cmd.Interval = intervalMicroseconds;
    sendCommand(cmd);
}

union param_value_u
{
    int8_t b;
    int16_t s;
    int32_t i;
    uint8_t ub;
    uint16_t us;
    uint32_t ui;
    float f;
};

float UnpackParameter(uint8_t type, float param_value)
{
    param_value_u pu;
    pu.f = param_value;

    float value = 0;
    switch (static_cast<MAV_PARAM_TYPE>(type)) {
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_UINT8:
        value = static_cast<float>(pu.ub);
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_INT8:
        value = static_cast<float>(pu.b);
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_UINT16:
        value = static_cast<float>(pu.us);
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_INT16:
        value = static_cast<float>(pu.s);
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_UINT32:
        value = static_cast<float>(pu.ui);
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_INT32:
        value = static_cast<float>(pu.i);
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_UINT64:
        // we only have 4 bytes for the value in mavlink_param_value_t, so how does this one work?
        value = static_cast<float>(pu.ui);
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_INT64:
        // we only have 4 bytes for the value in mavlink_param_value_t, so how does this one work?
        value = static_cast<float>(pu.i);
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_REAL32:
        value = param_value;
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_REAL64:
        // we only have 4 bytes for the value in mavlink_param_value_t, so how does this one work?
        value = param_value;
        break;
    default:
        break;
    }
    return value;
}

float PackParameter(uint8_t type, float param_value)
{
    param_value_u pu;
    pu.f = 0;
    switch (static_cast<MAV_PARAM_TYPE>(type)) {
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_UINT8:
        pu.ub = static_cast<uint8_t>(param_value);
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_INT8:
        pu.b = static_cast<int8_t>(param_value);
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_UINT16:
        pu.us = static_cast<uint16_t>(param_value);
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_INT16:
        pu.s = static_cast<int16_t>(param_value);
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_UINT32:
        pu.ui = static_cast<uint32_t>(param_value);
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_INT32:
        pu.i = static_cast<int32_t>(param_value);
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_UINT64:
        // we only have 4 bytes for the value in mavlink_param_value_t, so how does this one work?
        pu.ui = static_cast<uint32_t>(param_value);
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_INT64:
        // we only have 4 bytes for the value in mavlink_param_value_t, so how does this one work?
        pu.i = static_cast<int32_t>(param_value);
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_REAL32:
        pu.f = param_value;
        break;
    case MAV_PARAM_TYPE::MAV_PARAM_TYPE_REAL64:
        // we only have 4 bytes for the value in mavlink_param_value_t, so how does this one work?
        pu.f = param_value;
        break;
    default:
        break;
    }
    return pu.f;
}

void MavLinkNodeImpl::assertNotPublishingThread()
{
    auto con = ensureConnection();
    if (con->isPublishThread()) {
        throw std::runtime_error("Cannot perform blocking operation on the connection publish thread");
    }
}

std::vector<MavLinkParameter> MavLinkNodeImpl::getParamList()
{
    std::vector<MavLinkParameter> result;
    bool done = false;
    Semaphore paramReceived;
    bool waiting = false;
    size_t paramCount = 0;

    auto con = ensureConnection();
    assertNotPublishingThread();

    int subscription = con->subscribe([&](std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& message) {
        unused(connection);
        if (message.msgid == MavLinkParamValue::kMessageId) {
            MavLinkParamValue param;
            param.decode(message);
            MavLinkParameter p;
            p.index = param.param_index;
            p.type = param.param_type;
            char buf[17];
            std::memset(buf, 0, 17);
            std::memcpy(buf, param.param_id, 16);
            p.name = buf;
            p.value = param.param_value;
            result.push_back(p);
            paramCount = param.param_count;
            if (param.param_index == param.param_count - 1) {
                done = true;
            }
            if (waiting) {
                paramReceived.post();
            }
        }
    });

    //MAVLINK_MSG_ID_PARAM_REQUEST_LIST
    MavLinkParamRequestList cmd;
    cmd.target_system = getTargetSystemId();
    cmd.target_component = getTargetComponentId();
    sendMessage(cmd);

    while (!done) {
        waiting = true;
        if (!paramReceived.timed_wait(3000)) {
            // timeout, so we'll drop through to the code below which will try and fix this...
            done = true;
        }
        waiting = false;
    }
    con->unsubscribe(subscription);

    // note that UDP does not guarantee delivery of messages, so we have to also check if some parameters are missing and get them individually.
    std::vector<size_t> missing;

    for (size_t i = 0; i < paramCount; i++) {
        // nested loop is inefficient, but it is needed because UDP also doesn't guarantee in-order delivery
        bool found = false;
        for (auto iter = result.begin(), end = result.end(); iter != end; iter++) {
            MavLinkParameter p = *iter;
            if (static_cast<size_t>(p.index) == i) {
                found = true;
                break;
            }
        }
        if (!found) {
            missing.push_back(i);
        }
    }

    // ok, now fetch the missing parameters.
    for (auto iter = missing.begin(), end = missing.end(); iter != end; iter++) {
        size_t index = *iter;
        MavLinkParameter r;
        if (getParameterByIndex(static_cast<int16_t>(*iter)).wait(2000, &r)) {
            result.push_back(r);
        }
        else {
            Utils::log(Utils::stringf("Paremter %d does not seem to exist", index), Utils::kLogLevelWarn);
        }
    }

    std::sort(result.begin(), result.end(), [&](const MavLinkParameter& p1, const MavLinkParameter& p2) {
        return p1.name.compare(p2.name) < 0;
    });

    this->parameters_ = result;

    return result;
}

MavLinkParameter MavLinkNodeImpl::getCachedParameter(const std::string& name)
{
    if (this->parameters_.size() == 0) {
        throw std::runtime_error("Error: please call getParamList during initialization so we have cached snapshot of the parameter values");
    }

    for (int i = static_cast<int>(this->parameters_.size()) - 1; i >= 0; i--) {
        MavLinkParameter p = this->parameters_[i];
        if (p.name == name) {
            return p;
        }
    }

    throw std::runtime_error(Utils::stringf("Error: parameter name '%s' not found"));
}

AsyncResult<MavLinkParameter> MavLinkNodeImpl::getParameter(const std::string& name)
{
    int size = static_cast<int>(name.size());
    if (size > 16) {
        throw std::runtime_error(Utils::stringf("Error: parameter name '%s' is too long, must be <= 16 chars", name.c_str()));
    }
    else if (size < 16) {
        size++; // we can include the null terminator.
    }
    auto con = ensureConnection();
    AsyncResult<MavLinkParameter> asyncResult([=](int state) {
        con->unsubscribe(state);
    });

    MavLinkParamRequestRead cmd;
    std::strncpy(cmd.param_id, name.c_str(), size);
    cmd.param_index = -1;
    cmd.target_component = getTargetComponentId();
    cmd.target_system = getTargetSystemId();

    int subscription = con->subscribe([=](std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& message) {
        unused(connection);
        if (message.msgid == MavLinkParamValue::kMessageId) {
            MavLinkParamValue param;
            param.decode(message);
            if (std::strncmp(param.param_id, cmd.param_id, size) == 0) {
                MavLinkParameter result;
                result.name = name;
                result.type = param.param_type;
                result.index = param.param_index;
                result.value = UnpackParameter(param.param_type, param.param_value);
                asyncResult.setResult(result);
            }
        }
    });
    asyncResult.setState(subscription);

    sendMessage(cmd);

    return asyncResult;
}

AsyncResult<MavLinkParameter> MavLinkNodeImpl::getParameterByIndex(int16_t index)
{
    auto con = ensureConnection();
    AsyncResult<MavLinkParameter> asyncResult([=](int state) {
        con->unsubscribe(state);
    });

    MavLinkParamRequestRead cmd;
    cmd.param_id[0] = '\0';
    cmd.param_index = index;
    cmd.target_component = getTargetComponentId();
    cmd.target_system = getTargetSystemId();

    int subscription = con->subscribe([=](std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& message) {
        unused(connection);
        if (message.msgid == MavLinkParamValue::kMessageId) {
            MavLinkParamValue param;
            param.decode(message);
            if (param.param_index == index) {
                MavLinkParameter result;
                char buf[17];
                std::memset(buf, 0, 17);
                std::memcpy(buf, param.param_id, 16);
                result.name = buf;
                result.type = param.param_type;
                result.index = param.param_index;
                result.value = UnpackParameter(param.param_type, param.param_value);
                asyncResult.setResult(result);
            }
        }
    });
    asyncResult.setState(subscription);
    sendMessage(cmd);

    return asyncResult;
}

AsyncResult<bool> MavLinkNodeImpl::setParameter(MavLinkParameter p)
{
    int size = static_cast<int>(p.name.size());
    if (size > 16) {
        throw std::runtime_error(Utils::stringf("Error: parameter name '%s' is too long, must be <= 16 chars", p.name.c_str()));
    }
    else if (size < 16) {
        size++; // we can include the null terminator.
    }
    auto con = ensureConnection();
    assertNotPublishingThread();
    AsyncResult<bool> result([=](int state) {
        con->unsubscribe(state);
    });
    bool gotit = false;
    MavLinkParameter q;
    for (size_t i = 0; i < 3; i++) {
        if (getParameter(p.name).wait(2000, &q)) {
            gotit = true;
            break;
        }
    }
    if (!gotit) {
        throw std::runtime_error(Utils::stringf("Error: parameter name '%s' was not found", p.name.c_str()));
    }
    MavLinkParamSet setparam;
    setparam.target_component = getTargetComponentId();
    setparam.target_system = getTargetSystemId();
    std::strncpy(setparam.param_id, p.name.c_str(), size);
    setparam.param_type = q.type;
    setparam.param_value = PackParameter(q.type, p.value);
    sendMessage(setparam);

    // confirmation of the PARAM_SET is to receive the updated PARAM_VALUE.
    int subscription = con->subscribe([=](std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& message) {
        unused(connection);
        if (message.msgid == MavLinkParamValue::kMessageId) {
            MavLinkParamValue param;
            param.decode(message);
            if (std::strncmp(param.param_id, setparam.param_id, size) == 0) {
                bool rc = param.param_value == setparam.param_value;
                result.setResult(rc);
            }
        }
    });

    result.setState(subscription);
    return result;
}

void MavLinkNodeImpl::sendMessage(MavLinkMessageBase& msg)
{
    msg.sysid = local_system_id;
    msg.compid = local_component_id;
    ensureConnection()->sendMessage(msg);
}

void MavLinkNodeImpl::sendMessage(MavLinkMessage& msg)
{
    msg.compid = local_component_id;
    msg.sysid = local_system_id;
    ensureConnection()->sendMessage(msg);
}

void MavLinkNodeImpl::sendCommand(MavLinkCommand& command)
{
    MavLinkCommandLong cmd{};
    command.pack();
    cmd.command = command.command;
    cmd.target_system = getTargetSystemId();
    cmd.target_component = getTargetComponentId();
    cmd.confirmation = 1;
    cmd.param1 = command.param1;
    cmd.param2 = command.param2;
    cmd.param3 = command.param3;
    cmd.param4 = command.param4;
    cmd.param5 = command.param5;
    cmd.param6 = command.param6;
    cmd.param7 = command.param7;
    try {
        sendMessage(cmd);
    }
    catch (const std::exception& e) {
        // silently fail since we are on a background thread here...
        unused(e);
    }
}

AsyncResult<bool> MavLinkNodeImpl::sendCommandAndWaitForAck(MavLinkCommand& command)
{
    auto con = ensureConnection();

    AsyncResult<bool> result([=](int state) {
        con->unsubscribe(state);
    });

    uint16_t cmd = command.command;

    int subscription = con->subscribe([=](std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& message) {
        unused(connection);
        if (message.msgid == MavLinkCommandAck::kMessageId) {
            MavLinkCommandAck ack;
            ack.decode(message);
            if (ack.command == cmd) {
                MAV_RESULT ackResult = static_cast<MAV_RESULT>(ack.result);
                if (ackResult == MAV_RESULT::MAV_RESULT_TEMPORARILY_REJECTED) {
                    Utils::log(Utils::stringf("### command %d result: MAV_RESULT_TEMPORARILY_REJECTED", cmd));
                }
                else if (ackResult == MAV_RESULT::MAV_RESULT_UNSUPPORTED) {
                    Utils::log(Utils::stringf("### command %d result: MAV_RESULT_UNSUPPORTED", cmd));
                }
                else if (ackResult == MAV_RESULT::MAV_RESULT_FAILED) {
                    Utils::log(Utils::stringf("### command %d result: MAV_RESULT_FAILED", cmd));
                }
                else if (ackResult == MAV_RESULT::MAV_RESULT_ACCEPTED) {
                    Utils::log(Utils::stringf("### command %d result: MAV_RESULT_ACCEPTED", cmd));
                }
                else {
                    Utils::log(Utils::stringf("### command %d unexpected result: %d", cmd, ackResult));
                }
                // tell the caller this is complete.
                result.setResult(ackResult == MAV_RESULT::MAV_RESULT_ACCEPTED);
            }
        }
    });
    result.setState(subscription);
    sendCommand(command);
    return result;
}
