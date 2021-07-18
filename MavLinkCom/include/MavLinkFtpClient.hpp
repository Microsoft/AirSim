// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef MavLinkCom_MavLinkFtpClient_hpp
#define MavLinkCom_MavLinkFtpClient_hpp

#include <memory>
#include <string>
#include <vector>
#include "MavLinkNode.hpp"

namespace mavlinkcom_impl
{
class MavLinkFtpClientImpl;
}

namespace mavlinkcom
{

struct MavLinkFileInfo
{
public:
    std::string name;
    int size;
    bool is_directory;
};

struct MavLinkFtpProgress
{
public:
    bool cancel = false;
    bool complete = false;
    int error = 0;
    std::string message;
    uint64_t current = 0; // current progress towards the goal.
    uint64_t goal = 0; // goal we are trying to reach or 0 if the goal is indeteminate.
    double average_rate = 0;
    double longest_delay = 0;
    int message_count;
};

class MavLinkFtpClient : public MavLinkNode
{
public:
    MavLinkFtpClient(int localSystemId, int localComponentId);
    ~MavLinkFtpClient();

    bool isSupported();

    void list(MavLinkFtpProgress& progress, const std::string& remotePath, std::vector<MavLinkFileInfo>& files);
    void get(MavLinkFtpProgress& progress, const std::string& remotePath, const std::string& localPath);
    void put(MavLinkFtpProgress& progress, const std::string& remotePath, const std::string& localPath);
    void remove(MavLinkFtpProgress& progress, const std::string& remotePath);
    void mkdir(MavLinkFtpProgress& progress, const std::string& remotePath);
    void rmdir(MavLinkFtpProgress& progress, const std::string& remotePath);

    void cancel(); // cancel any pending operation.
};
}

#endif
