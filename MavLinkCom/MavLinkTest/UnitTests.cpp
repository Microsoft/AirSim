// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "UnitTests.h"
#include <thread>
#include <chrono>
#include "Utils.hpp"
#include "FileSystem.hpp"
#include "MavLinkVehicle.hpp"
#include "MavLinkMessages.hpp"
#include "MavLinkConnection.hpp"
#include "MavLinkVideoStream.hpp"
#include "MavLinkTcpServer.hpp"
#include "MavLinkFtpClient.hpp"
#include "Semaphore.hpp"

STRICT_MODE_OFF
#include "json.hpp"
STRICT_MODE_ON

#include <iostream>

using namespace mavlink_utils;
using namespace mavlinkcom;

extern std::string replaceAll(std::string s, char toFind, char toReplace);

void UnitTests::RunAll(std::string comPort, int boardRate)
{
    com_port_ = comPort;
    baud_rate_ = boardRate;
    if (comPort == "") {
        throw std::runtime_error("unit tests need a serial connection to Pixhawk, please specify -serial argument");
    }

    RunTest("UdpPingTest", [=] { UdpPingTest(); });
    RunTest("TcpPingTest", [=] { TcpPingTest(); });
    RunTest("SendImageTest", [=] { SendImageTest(); });
    RunTest("SerialPx4Test", [=] { SerialPx4Test(); });
    RunTest("FtpTest", [=] { FtpTest(); });
    RunTest("JSonLogTest", [=] { JSonLogTest(); });
}

void UnitTests::RunTest(const std::string& name, TestHandler handler)
{
    printf("%s: start\n", name.c_str());
    try {
        handler();
        printf("------------- %s test passed --------------------\n", name.c_str());
    }
    catch (const std::exception& e) {
        printf("------------- %s test failed: %s --------------- \n", name.c_str(), e.what());
    }
}

void UnitTests::UdpPingTest()
{

    auto localConnection = MavLinkConnection::connectLocalUdp("jMavSim", "127.0.0.1", 14588);

    Semaphore received;
    auto id = localConnection->subscribe([&](std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& msg) {
        printf("    Received message %d\n", msg.msgid);
        received.post();
    });

    auto remoteConnection = MavLinkConnection::connectRemoteUdp("jMavSim", "127.0.0.1", "127.0.0.1", 14588);

    // send a heartbeat
    MavLinkHeartbeat hb;
    hb.autopilot = 0;
    hb.base_mode = 0;
    hb.custom_mode = 0;
    hb.mavlink_version = 3;
    hb.system_status = 1;
    hb.type = 1;

    auto node = std::make_shared<MavLinkNode>(166, 1);
    node->connect(remoteConnection);
    node->sendMessage(hb);

    if (!received.timed_wait(2000)) {
        throw std::runtime_error("heartbeat not received after 2 seconds");
    }
    localConnection->unsubscribe(id);
    localConnection->close();
    remoteConnection->close();
    node->close();
}

void UnitTests::TcpPingTest()
{

    const int testPort = 45166;

    std::shared_ptr<MavLinkTcpServer> server = std::make_shared<MavLinkTcpServer>("127.0.0.1", testPort);

    std::future<int> future = std::async(std::launch::async, [=] {
        // accept one incoming connection
        std::shared_ptr<MavLinkConnection> con = server->acceptTcp("test");
        std::shared_ptr<MavLinkNode> serverNode = std::make_shared<MavLinkNode>(1, 1);
        serverNode->connect(con);

        // send a heartbeat to the client
        MavLinkHeartbeat hb;
        hb.autopilot = 0;
        hb.base_mode = 0;
        hb.custom_mode = 0;
        hb.mavlink_version = 3;
        hb.system_status = 1;
        hb.type = 1;
        serverNode->sendMessage(hb);
        return 0;
    });

    Semaphore received;
    auto client = MavLinkConnection::connectTcp("local", "127.0.0.1", "127.0.0.1", testPort);
    client->subscribe([&](std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& msg) {
        printf("Received msg %d\n", msg.msgid);
        received.post();
    });

    if (!received.timed_wait(2000)) {
        throw std::runtime_error("heartbeat not received from server");
    }

    client->close();
}

void UnitTests::SerialPx4Test()
{
    auto connection = MavLinkConnection::connectSerial("px4", com_port_, baud_rate_);

    int count = 0;
    Semaphore received;

    auto id = connection->subscribe([&](std::shared_ptr<MavLinkConnection> con, const MavLinkMessage& msg) {
        //printf("    Received message %d\n", static_cast<int>(msg.msgid));
        count++;
        if (msg.msgid == 0) {
            received.post();
        }
    });

    if (!received.timed_wait(5000)) {
        throw std::runtime_error("MavLink heartbeat is not being received over serial, please make sure PX4 is plugged in and the unit test is using the right COM port.");
    }

    printf("Received %d mavlink messages over serial port\n", count);
    connection->unsubscribe(id);
    connection->close();
}

class ImageServer
{
public:
    ImageServer(std::shared_ptr<MavLinkConnection> con)
    {
        stream = std::make_shared<MavLinkVideoServer>(1, 1);
        stream->connect(con);
        con->subscribe([&](std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& msg) {
            MavLinkVideoServer::MavLinkVideoRequest req;
            if (stream->hasVideoRequest(req)) {
                printf("    server received request for video at %f frames every second\n", req.every_n_sec);

                int* image = new int[10000];
                int size = sizeof(int) * 10000;
                for (int i = 0; i < 10000; i++) {
                    image[i] = i;
                }
                stream->sendFrame(reinterpret_cast<uint8_t*>(image), size, 100, 100, 0, 0);
                delete[] image;
            }
        });
    }

    std::shared_ptr<MavLinkVideoServer> stream;
};

void UnitTests::SendImageTest()
{

    const int testPort = 42316;
    std::string testAddr = "127.0.0.1";

    std::shared_ptr<MavLinkTcpServer> server = std::make_shared<MavLinkTcpServer>(testAddr, testPort);

    std::future<int> future = std::async(std::launch::async, [=] {
        // this is the server code, it will accept 1 connection from a client on port 14588
        // for this unit test we are expecting a request to send an image.
        auto con = server->acceptTcp("test");
        this->server_ = new ImageServer(con);
        return 0;
    });

    // add a drone connection so the mavLinkCom can use it to send requests to the above server.
    auto drone = MavLinkConnection::connectTcp("drone", testAddr, testAddr, testPort);

    MavLinkVideoClient client{ 150, 1 };
    client.connect(drone);
    client.requestVideo(1, 1, false);

    MavLinkVideoClient::MavLinkVideoFrame image;
    int retries = 100;
    while (!client.readNextFrame(image) && retries-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (retries <= 0) {
        // hmmm
        throw std::runtime_error("no image received after timeout");
    }
    else {
        std::vector<unsigned char> raw = image.data;
        int* img = reinterpret_cast<int*>(raw.data());

        for (int i = 0, n = static_cast<int>(raw.size() / 4); i < n; i++) {
            if (img[i] != i) {
                throw std::runtime_error("corrupt image data received");
            }
        }
    }
    printf("    Received image %d bytes, width %d and height %d ok\n", static_cast<int>(image.data.size()), image.width, image.height);

    return;
}

void UnitTests::VerifyFile(MavLinkFtpClient& ftp, const std::string& dir, const std::string& name, bool exists, bool isdir)
{
    MavLinkFtpProgress progress;
    std::vector<MavLinkFileInfo> files;
    ftp.list(progress, dir, files);
    if (progress.error != 0) {
        throw std::runtime_error(Utils::stringf("unexpected error %d: '%s' from ftp list '%s' command",
                                                progress.error,
                                                progress.message.c_str(),
                                                dir.c_str()));
    }

    bool found = false;
    for (auto ptr = files.begin(), end = files.end(); ptr != end; ptr++) {
        MavLinkFileInfo i = *ptr;
        if (isdir == i.is_directory && i.name == name) {
            found = true;
        }
    }
    if (!found && exists) {
        throw std::runtime_error(Utils::stringf("The %s '%s' not found in '%s', but it should be there",
                                                isdir ? "dir" : "file",
                                                name.c_str(),
                                                dir.c_str()));
    }
    else if (found && !exists) {
        throw std::runtime_error(Utils::stringf("The %s '%s' was found in '%s', but it should not have been",
                                                isdir ? "dir" : "file",
                                                name.c_str(),
                                                dir.c_str()));
    }
}

void UnitTests::FtpTest()
{

    std::shared_ptr<MavLinkConnection> connection = MavLinkConnection::connectSerial("px4", com_port_, baud_rate_);

    MavLinkFtpClient ftp{ 166, 1 };
    ftp.connect(connection);

    try {

        MavLinkFtpProgress progress;
        std::vector<MavLinkFileInfo> files;

        // ================ ls

        ftp.list(progress, "/fs/microsd", files);
        if (progress.error != 0) {
            throw std::runtime_error(Utils::stringf("unexpected error %d: '%s' from ftp list '/fs/microsd' command - does your pixhawk have an sd card?",
                                                    progress.error,
                                                    progress.message.c_str()));
        }
        else {
            printf("Found %d files in '/fs/microsd' folder\n", static_cast<int>(files.size()));
        }

        // ================ put file

        auto tempPath = FileSystem::getTempFolder();
        tempPath = FileSystem::combine(tempPath, "ftptest.txt");
        std::ofstream stream(tempPath);

        const char* TestPattern = "This is line %d\n";

        for (int i = 0; i < 100; i++) {
            std::string line = Utils::stringf("%s %i", TestPattern, i);
            stream << line;
        }

        stream.close();

        std::string remotePath = "/fs/microsd/ftptest.txt";
        std::string localPath = tempPath;
#if defined(_WIN32)
        // I wish there was a cleaner way to do this, but I can't use tempPath.native() because on windows that is a wstring and on our linux build it is a string.
        replaceAll(localPath, '/', '\\');
#endif

        ftp.put(progress, remotePath, localPath);

        if (progress.error != 0) {
            throw std::runtime_error(Utils::stringf("unexpected error %d: '%s' from ftp put command",
                                                    progress.error,
                                                    progress.message.c_str()));
        }
        else {
            printf("put succeeded\n");
        }

        FileSystem::remove(tempPath);

        VerifyFile(ftp, "/fs/microsd", "ftptest.txt", true, false);

        // ================ get file
        ftp.get(progress, remotePath, localPath);

        if (progress.error != 0) {
            throw std::runtime_error(Utils::stringf("unexpected error %d: '%s' from ftp get command",
                                                    progress.error,
                                                    progress.message.c_str()));
        }

        // verify the file contents.
        std::ifstream istream(tempPath);

        int count = 0;
        std::string line;
        std::getline(istream, line);
        while (line.size() > 0) {
            line += '\n';
            std::string expected = Utils::stringf("%s %i", TestPattern, count);
            if (line != expected) {
                throw std::runtime_error(Utils::stringf("ftp local file contains unexpected contents '%s' on line %d\n", line.c_str(), count));
            }
            count++;
            std::getline(istream, line);
        }

        printf("get succeeded\n");

        istream.close();

        // ================ remove file
        FileSystem::remove(tempPath);

        ftp.remove(progress, remotePath);

        if (progress.error != 0) {
            throw std::runtime_error(Utils::stringf("unexpected error %d: '%s' from ftp remove command",
                                                    progress.error,
                                                    progress.message.c_str()));
        }
        else {
            printf("remove succeeded\n");
        }

        VerifyFile(ftp, "/fs/microsd", "ftptest.txt", false, false);

        // ================ make directory
        // D:\px4\src\lovettchris\Firmware\rootfs\fs\microsd
        ftp.mkdir(progress, "/fs/microsd/testrmdir");
        if (progress.error != 0) {
            throw std::runtime_error(Utils::stringf("unexpected error %d: '%s' from ftp mkdir '/fs/microsd/testrmdir' command",
                                                    progress.error,
                                                    progress.message.c_str()));
        }

        VerifyFile(ftp, "/fs/microsd", "testrmdir", true, true);

        // ================ remove directory
        ftp.rmdir(progress, "/fs/microsd/testrmdir");
        if (progress.error != 0) {
            throw std::runtime_error(Utils::stringf("unexpected error %d: '%s' from ftp rmdir '/fs/microsd/testrmdir' command",
                                                    progress.error,
                                                    progress.message.c_str()));
        }

        VerifyFile(ftp, "/fs/microsd", "testrmdir", false, true);
    }
    catch (...) {
        ftp.close();
        connection->close();
        connection = nullptr;
        throw;
    }

    ftp.close();
    connection->close();
    connection = nullptr;
}

void UnitTests::JSonLogTest()
{
    auto connection = MavLinkConnection::connectSerial("px4", com_port_, baud_rate_);

    MavLinkFileLog log;

    auto tempPath = FileSystem::getTempFolder();
    tempPath = FileSystem::combine(tempPath, "test.mavlink");
    log.openForWriting(tempPath, true);

    int count = 0;
    Semaphore received;

    auto id = connection->subscribe([&](std::shared_ptr<MavLinkConnection> con, const MavLinkMessage& msg) {
        count++;
        log.write(msg);
        if (count > 50) {
            received.post();
        }
    });

    if (!received.timed_wait(30000)) {
        throw std::runtime_error("PX4 is not sending 50 messages in 30 seconds.");
    }

    connection->unsubscribe(id);
    connection->close();
    log.close();

    // Now verification
    nlohmann::json doc;

    std::ifstream s;
    FileSystem::openTextFile(tempPath, s);
    if (!s.fail()) {
        s >> doc;
    }
    else {
        throw std::runtime_error(Utils::stringf("Cannot open json file at '%s'.", tempPath.c_str()));
    }

    if (doc.count("rows") == 1) {
        nlohmann::json rows = doc["rows"].get<nlohmann::json>();
        int found = 0;
        int imu = 0;
        if (rows.is_array()) {
            size_t size = rows.size();
            for (size_t i = 0; i < size; i++) {
                auto ptr = rows[i];
                if (ptr.is_object()) {
                    if (ptr.count("name") == 1) {
                        auto name = ptr["name"].get<std::string>();
                        if (name == "HIGHRES_IMU") {
                            imu++;
                        }
                        found++;
                    }
                }
            }
        }

        printf("found %d valid rows in the json file, and %d HIGHRES_IMU records\n", found, imu);
    }
}