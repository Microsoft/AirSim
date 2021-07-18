// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef MavLinkCom_MavLinkVideoStreamImpl_hpp
#define MavLinkCom_MavLinkVideoStreamImpl_hpp

#include "MavLinkVideoStream.hpp"
#include "MavLinkNodeImpl.hpp"

using namespace mavlinkcom;

namespace mavlinkcom_impl
{
class MavLinkVideoClientImpl : public MavLinkNodeImpl
{
public:
    MavLinkVideoClientImpl(int localSystemId, int localComponentId);
    ~MavLinkVideoClientImpl();

    //image APIs
    void requestVideo(int camera_id, float every_n_sec, bool save_locally);

    // or if you are implementing the client side call this function to get the most recent frame.
    // returns false if there is no new frame available.
    bool readNextFrame(MavLinkVideoClient::MavLinkVideoFrame& image);

protected:
    virtual void handleMessage(std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& message);

private:
    std::mutex state_mutex;

    struct IncomingFrame
    {
        int size; ///< Image size being transmitted (bytes)
        int packets; ///< Number of data packets being sent for this image
        int packetsArrived; ///< Number of data packets received
        int payload; ///< Payload size per transmitted packet (bytes). Standard is 254, and decreases when image resolution increases.
        int quality; ///< Quality of the transmitted image (percentage)
        int type; ///< Type of the transmitted image (BMP, PNG, JPEG, RAW 8 bit, RAW 32 bit)
        int width; ///< Width of the image stream
        int height; ///< Width of the image stream
        std::vector<uint8_t> data; ///< Buffer for the incoming bytestream
        uint64_t start; //time when we started receiving data

        void ready()
        {
            std::lock_guard<std::mutex> image_read_guard(read_mutex);
            is_ready = true;
            is_unread = true;
            last_image.data = data;
            last_image.height = height;
            last_image.width = width;
            last_image.quality = quality;
            last_image.type = type;
        }

        bool read(MavLinkVideoClient::MavLinkVideoFrame& image)
        {
            std::lock_guard<std::mutex> image_read_guard(read_mutex);

            if (is_ready && is_unread) {
                is_ready = false;
                is_unread = false;
                image = last_image;
                image.progress = 1;
                return true;
            }
            else {
                // return info about the image including progress.
                image.height = this->height;
                image.width = this->width;
                image.progress = static_cast<float>(packetsArrived) / static_cast<float>(packets);
                image.quality = this->quality;
                image.type = this->type;
                return false;
            }
        }

    private:
        bool is_unread = false; //was inage data returned by getReceievedImage() call
        bool is_ready = false;
        MavLinkVideoClient::MavLinkVideoFrame last_image;
        std::mutex read_mutex;
    } incoming_image;
};

class MavLinkVideoServerImpl : public MavLinkNodeImpl
{
public:
    MavLinkVideoServerImpl(int localSystemId, int localComponentId);
    ~MavLinkVideoServerImpl();

    bool hasVideoRequest(MavLinkVideoServer::MavLinkVideoRequest& req);

    void sendFrame(uint8_t data[], uint32_t data_size, uint16_t width, uint16_t height, uint8_t image_type, uint8_t image_quality);

protected:
    virtual void handleMessage(std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& message);

private:
    MavLinkVideoServer::MavLinkVideoRequest image_request_;
    std::mutex state_mutex;
};
}

#endif
