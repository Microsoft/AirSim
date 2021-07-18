// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef MavLinkCom_MavLinkVideoStream_hpp
#define MavLinkCom_MavLinkVideoStream_hpp

#include <vector>
#include <memory>
#include "MavLinkConnection.hpp"
#include "MavLinkNode.hpp"
#include <mutex>

namespace mavlinkcom_impl
{
class MavLinkVideoClientImpl;
class MavLinkVideoServerImpl;
}
namespace mavlinkcom
{

// This class implements the client side of a video stream request
class MavLinkVideoClient : public MavLinkNode
{
public:
    MavLinkVideoClient(int localSystemId, int localComponentId);
    ~MavLinkVideoClient();

    struct MavLinkVideoFrame
    {
        std::vector<uint8_t> data;
        int quality; ///< Quality of the transmitted image (percentage)
        int type; ///< Type of the transmitted image (BMP, PNG, JPEG, RAW 8 bit, RAW 32 bit)
        int width; ///< Width of the image stream
        int height; ///< Width of the image stream
        float progress; ///< while frame is being assembled this returns the progress between 0 and 1.
    };

    void requestVideo(int camera_id, float every_n_sec, bool save_locally);

    // Call this function to get the most recent frame.
    // Returns false if there is no new frame available yet.
    bool readNextFrame(MavLinkVideoFrame& image);
};

class MavLinkVideoServer : public MavLinkNode
{
public:
    MavLinkVideoServer(int localSystemId, int localComponentId);
    ~MavLinkVideoServer();

    struct MavLinkVideoRequest
    {
        int camera_id;
        float every_n_sec = 0;
        bool save_locally = false;
        bool valid = false;
    };

    // poll this function to see if there is a valid image request, it returns false if there is no request.
    bool hasVideoRequest(MavLinkVideoRequest& req);

    // call this to send the image back over the connection given to start function.
    void sendFrame(uint8_t data[], uint32_t data_size, uint16_t width, uint16_t height, uint8_t image_type, uint8_t image_quality);
};
}
#endif
