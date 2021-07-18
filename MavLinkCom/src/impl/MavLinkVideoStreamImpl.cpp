// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "MavLinkVideoStreamImpl.hpp"
#include <chrono>
#include "Utils.hpp"
#include "MavLinkMessages.hpp"

#define PACKET_PAYLOAD 253 //hard coded in MavLink code - do not change

using namespace mavlink_utils;

using namespace mavlinkcom_impl;
//================================= CLIENT ==============================================================

MavLinkVideoClientImpl::MavLinkVideoClientImpl(int localSystemId, int localComponentId)
    : MavLinkNodeImpl(localSystemId, localComponentId)
{
}

MavLinkVideoClientImpl::~MavLinkVideoClientImpl()
{
    close();
}

void MavLinkVideoClientImpl::handleMessage(std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& message)
{
    unused(connection);
    switch (message.msgid) {
    case MavLinkDataTransmissionHandshake::kMessageId: //MAVLINK_MSG_ID_DATA_TRANSMISSION_HANDSHAKE:
    {
        MavLinkDataTransmissionHandshake p;
        p.decode(message);

        std::lock_guard<std::mutex> guard(state_mutex);
        incoming_image.size = p.size;
        incoming_image.packets = p.packets;
        incoming_image.payload = p.payload;
        incoming_image.quality = p.jpg_quality;
        incoming_image.type = p.type;
        incoming_image.width = p.width;
        incoming_image.height = p.height;
        incoming_image.start = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        incoming_image.packetsArrived = 0;
        incoming_image.data.resize(incoming_image.size);

        break;
    }
    case MavLinkEncapsulatedData::kMessageId: // MAVLINK_MSG_ID_ENCAPSULATED_DATA:
    {
        MavLinkEncapsulatedData img;
        img.decode(message);

        std::lock_guard<std::mutex> guard(state_mutex);

        int seq = img.seqnr;
        int pos = seq * incoming_image.payload;

        // Check if we have a valid transaction
        if (incoming_image.packets == 0) {
            incoming_image.packetsArrived = 0;
            Utils::log("Aborting image reception because of zero packets", Utils::kLogLevelWarn);
            break;
        }

        for (int i = 0; i < incoming_image.payload; ++i) {
            if (pos < incoming_image.size) {
                incoming_image.data[pos] = img.data[i];
            }
            ++pos;
        }

        ++incoming_image.packetsArrived;

        // emit signal if all packets arrived
        if (incoming_image.packetsArrived >= incoming_image.packets) {
            Utils::log(Utils::stringf("Image is available: %d packets arrived", incoming_image.packetsArrived));

            // Restart statemachine
            incoming_image.packets = 0;
            incoming_image.packetsArrived = 0;
            incoming_image.ready();
        }
        break;
    }
    default:
        break;
    }
}

bool MavLinkVideoClientImpl::readNextFrame(MavLinkVideoClient::MavLinkVideoFrame& image)
{
    return incoming_image.read(image);
}

//image APIs
void MavLinkVideoClientImpl::requestVideo(int camera_id, float every_n_sec, bool save_locally)
{
    MavCmdDoControlVideo cmd{};
    cmd.Id = static_cast<float>(camera_id);
    cmd.Transmission = 1.0f;
    cmd.Interval = every_n_sec;
    cmd.Recording = save_locally ? 1.0f : 0.0f;
    sendCommand(cmd);
}

//================================= SERVER ==============================================================

MavLinkVideoServerImpl::MavLinkVideoServerImpl(int system_id, int component_id)
    : MavLinkNodeImpl(system_id, component_id)
{
}

MavLinkVideoServerImpl::~MavLinkVideoServerImpl()
{
}

void MavLinkVideoServerImpl::handleMessage(std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& message)
{
    unused(connection);
    switch (message.msgid) {
    case MavLinkCommandLong::kMessageId: {
        MavLinkCommandLong cmd;
        cmd.decode(message);

        switch (static_cast<MAV_CMD>(cmd.command)) {
        case MAV_CMD::MAV_CMD_DO_CONTROL_VIDEO: {
            std::lock_guard<std::mutex> guard(state_mutex);
            image_request_.camera_id = static_cast<int>(cmd.param1);
            image_request_.every_n_sec = cmd.param2;
            image_request_.save_locally = cmd.param3 == 0 ? false : true;
            image_request_.valid = true;
            break;
        }
        default:
            break;
        }
    }
    default:
        break;
    }
}

bool MavLinkVideoServerImpl::hasVideoRequest(MavLinkVideoServer::MavLinkVideoRequest& req)
{
    std::lock_guard<std::mutex> guard(state_mutex);
    if (image_request_.valid) {
        req = image_request_;
        image_request_.valid = false;
        return true;
    }
    else
        return false;
}

void MavLinkVideoServerImpl::sendFrame(uint8_t data[], uint32_t data_size, uint16_t width, uint16_t height, uint8_t image_type, uint8_t image_quality)
{
    MavLinkDataTransmissionHandshake ack;
    // Prepare and send acknowledgment packet
    ack.type = image_type;
    ack.size = static_cast<uint32_t>(data_size);
    ack.packets = static_cast<uint16_t>(ack.size / PACKET_PAYLOAD);
    if (ack.size % PACKET_PAYLOAD) // one more packet with the rest of data
        ++ack.packets;
    ack.payload = static_cast<uint8_t>(PACKET_PAYLOAD);
    ack.jpg_quality = image_quality;
    ack.width = width;
    ack.height = height;

    sendMessage(ack);

    uint32_t byteIndex = 0;
    MavLinkEncapsulatedData packet;

    for (int i = 0; i < ack.packets; ++i) {
        // Copy PACKET_PAYLOAD bytes of image data to send buffer
        for (int j = 0; j < PACKET_PAYLOAD; ++j) {
            if (byteIndex < ack.size)
                packet.data[j] = data[byteIndex];
            else // fill packet data with padding bits
                packet.data[j] = 0;
            ++byteIndex;
        }

        // Send ENCAPSULATED_IMAGE packet
        packet.seqnr = i;
        sendMessage(packet);
    }
}
