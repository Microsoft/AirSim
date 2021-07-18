// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "MavLinkVideoStream.hpp"
#include "impl/MavLinkVideoStreamImpl.hpp"

using namespace mavlinkcom;
using namespace mavlinkcom_impl;

// ============================== CLIENT ============================================

MavLinkVideoClient::MavLinkVideoClient(int localSystemId, int localComponentId)
    : MavLinkNode(localSystemId, localComponentId)
{
    pImpl.reset(new MavLinkVideoClientImpl(localSystemId, localComponentId));
}

MavLinkVideoClient::~MavLinkVideoClient()
{
    pImpl = nullptr;
}

//image APIs
void MavLinkVideoClient::requestVideo(int camera_id, float every_n_sec, bool save_locally)
{
    auto ptr = dynamic_cast<MavLinkVideoClientImpl*>(pImpl.get());
    ptr->requestVideo(camera_id, every_n_sec, save_locally);
}

// or if you are implementing the client side call this function to get the most recent frame.
// returns false if there is no new frame available.
bool MavLinkVideoClient::readNextFrame(MavLinkVideoFrame& image)
{
    auto ptr = dynamic_cast<MavLinkVideoClientImpl*>(pImpl.get());
    return ptr->readNextFrame(image);
}

// ============================== SERVER ============================================

MavLinkVideoServer::MavLinkVideoServer(int localSystemId, int localComponentId)
    : MavLinkNode(localSystemId, localComponentId)
{
    pImpl.reset(new MavLinkVideoServerImpl(localSystemId, localComponentId));
}

MavLinkVideoServer::~MavLinkVideoServer()
{
    pImpl = nullptr;
}

bool MavLinkVideoServer::hasVideoRequest(MavLinkVideoRequest& req)
{
    auto ptr = dynamic_cast<MavLinkVideoServerImpl*>(pImpl.get());
    return ptr->hasVideoRequest(req);
}

// call this to send the image back over the connection given to start function.
void MavLinkVideoServer::sendFrame(uint8_t data[], uint32_t data_size, uint16_t width, uint16_t height, uint8_t image_type, uint8_t image_quality)
{
    auto ptr = dynamic_cast<MavLinkVideoServerImpl*>(pImpl.get());
    ptr->sendFrame(data, data_size, width, height, image_type, image_quality);
}
