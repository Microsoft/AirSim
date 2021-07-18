// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "MavLinkLog.hpp"
#include "Utils.hpp"
#include <chrono>

using namespace mavlinkcom;
using namespace mavlink_utils;

#define MAVLINK_STX_MAVLINK1 0xFE // marker for old protocol
#define MAVLINK_STX 253 // marker for mavlink 2

uint64_t MavLinkFileLog::getTimeStamp()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

MavLinkFileLog::MavLinkFileLog()
{
    ptr_ = nullptr;
    reading_ = false;
    writing_ = false;
    json_ = false;
}

MavLinkFileLog::~MavLinkFileLog()
{
    close();
}

bool MavLinkFileLog::isOpen()
{
    return reading_ || writing_;
}
void MavLinkFileLog::openForReading(const std::string& filename)
{
    close();
    file_name_ = filename;
    ptr_ = fopen(filename.c_str(), "rb");
    if (ptr_ == nullptr) {
        throw std::runtime_error(Utils::stringf("Could not open the file %s, error=%d", filename.c_str(), errno));
    }

    fseek(ptr_, 0, SEEK_SET);
    reading_ = true;
    writing_ = false;
}
void MavLinkFileLog::openForWriting(const std::string& filename, bool json)
{
    close();
    json_ = json;
    file_name_ = filename;
    ptr_ = fopen(filename.c_str(), "wb");
    if (ptr_ == nullptr) {
        throw std::runtime_error(Utils::stringf("Could not open the file %s, error=%d", filename.c_str(), errno));
    }
    if (json) {
        fprintf(ptr_, "{ \"rows\": [\n");
    }
    reading_ = false;
    writing_ = true;
}

void MavLinkFileLog::close()
{
    FILE* temp = ptr_;
    if (json_ && ptr_ != nullptr) {
        fprintf(ptr_, "    {}\n"); // so that trailing comma on last row isn't a problem.
        fprintf(ptr_, "]}\n");
    }
    ptr_ = nullptr;
    if (temp != nullptr) {
        fclose(temp);
    }
    reading_ = false;
    writing_ = false;
}

uint64_t FlipEndianness(uint64_t v)
{
    uint64_t result = 0;
    uint64_t shift = v;
    const int size = sizeof(uint64_t);
    for (int i = 0; i < size; i++) {
        uint64_t low = (shift & 0xff);
        result = (result << 8) + low;
        shift >>= 8;
    }
    return result;
}

void MavLinkFileLog::write(const mavlinkcom::MavLinkMessage& msg, uint64_t timestamp)
{
    if (ptr_ != nullptr) {
        if (reading_) {
            throw std::runtime_error("Log file was opened for reading");
        }
        if (json_) {
            MavLinkMessageBase* strongTypedMsg = MavLinkMessageBase::lookup(msg);
            if (strongTypedMsg != nullptr) {
                strongTypedMsg->timestamp = timestamp;
                std::string line = strongTypedMsg->toJSon();
                {
                    std::lock_guard<std::mutex> lock(log_lock_);
                    fprintf(ptr_, "    %s\n", line.c_str());
                }
                delete strongTypedMsg;
            }
        }
        else {
            if (timestamp == 0) {
                timestamp = getTimeStamp();
            }
            // for compatibility with QGroundControl we have to save the time field in big endian.
            // todo: mavlink2 support?
            timestamp = FlipEndianness(timestamp);

            uint8_t magic = msg.magic;
            if (magic != MAVLINK_STX_MAVLINK1) {
                // has to be one or the other!
                magic = MAVLINK_STX;
            }

            std::lock_guard<std::mutex> lock(log_lock_);
            fwrite(&timestamp, sizeof(uint64_t), 1, ptr_);
            fwrite(&magic, 1, 1, ptr_);
            fwrite(&msg.len, 1, 1, ptr_);
            fwrite(&msg.seq, 1, 1, ptr_);
            fwrite(&msg.sysid, 1, 1, ptr_);
            fwrite(&msg.compid, 1, 1, ptr_);

            if (magic == MAVLINK_STX_MAVLINK1) {
                uint8_t msgid = msg.msgid & 0xff; // truncate to mavlink 2 msgid
                fwrite(&msgid, 1, 1, ptr_);
            }
            else {
                // 24 bits.
                uint8_t msgid = msg.msgid & 0xFF;
                fwrite(&msgid, 1, 1, ptr_);
                msgid = (msg.msgid >> 8) & 0xFF;
                fwrite(&msgid, 1, 1, ptr_);
                msgid = (msg.msgid >> 16) & 0xFF;
                fwrite(&msgid, 1, 1, ptr_);
            }

            fwrite(&msg.payload64, 1, msg.len, ptr_);
            fwrite(&msg.checksum, sizeof(uint16_t), 1, ptr_);
        }
    }
}

bool MavLinkFileLog::read(mavlinkcom::MavLinkMessage& msg, uint64_t& timestamp)
{
    if (ptr_ != nullptr) {
        if (writing_) {
            throw std::runtime_error("Log file was opened for writing");
        }
        uint64_t time;

        size_t s = fread(&time, 1, sizeof(uint64_t), ptr_);
        if (s < sizeof(uint64_t)) {
            int hr = errno;
            return false;
        }

        timestamp = FlipEndianness(time);

        s = fread(&msg.magic, 1, 1, ptr_);
        if (s == 0) {
            return false;
        }
        s = fread(&msg.len, 1, 1, ptr_);
        if (s == 0) {
            return false;
        }
        s = fread(&msg.seq, 1, 1, ptr_);
        if (s == 0) {
            return false;
        }
        s = fread(&msg.sysid, 1, 1, ptr_);
        if (s == 0) {
            return false;
        }
        s = fread(&msg.compid, 1, 1, ptr_);
        if (s == 0) {
            return false;
        }

        if (msg.magic == MAVLINK_STX_MAVLINK1) {
            uint8_t msgid = 0;
            s = fread(&msgid, 1, 1, ptr_);
            msg.msgid = msgid;
        }
        else {
            // 24 bits.
            uint8_t msgid = 0;
            s = fread(&msgid, 1, 1, ptr_);
            msg.msgid = msgid;
            s = fread(&msgid, 1, 1, ptr_);
            msg.msgid |= (msgid << 8);
            s = fread(&msgid, 1, 1, ptr_);
            msg.msgid |= (msgid << 16);
        }

        if (s < 1) {
            return false;
        }
        s = fread(&msg.payload64, 1, msg.len, ptr_);
        if (s < msg.len) {
            return false;
        }
        s = fread(&msg.checksum, 1, sizeof(uint16_t), ptr_);
        if (s < sizeof(uint16_t)) {
            return false;
        }
        return true;
    }
    return false;
}
