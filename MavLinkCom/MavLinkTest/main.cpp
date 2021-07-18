// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

// PX4.cpp : Defines the entry point for the console application.

#include "Utils.hpp"
#include "FileSystem.hpp"
#include "MavLinkTcpServer.hpp"
#include "MavLinkConnection.hpp"
#include "MavLinkVehicle.hpp"
#include "MavLinkMessages.hpp"
#include "MavLinkLog.hpp"
#include "Commands.h"
#include <iostream>
#include <vector>
#include <string.h>
#include <functional>
#include <mutex>
#include <map>
#include <ctime>
STRICT_MODE_OFF
#include "json.hpp"
STRICT_MODE_ON
#include "UnitTests.h"

#include <filesystem>
using namespace std::filesystem;

/* enable math defines on Windows */

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923 // pi/2
#endif

static const int pixhawkVendorId = 9900; ///< Vendor ID for Pixhawk board (V2 and V1) and PX4 Flow
static const int pixhawkFMUV4ProductId = 18; ///< Product ID for Pixhawk V2 board
static const int pixhawkFMUV2ProductId = 17; ///< Product ID for Pixhawk V2 board
static const int pixhawkFMUV5ProductId = 50; ///< Product ID for Pixhawk V5 board
static const int pixhawkFMUV2OldBootloaderProductId = 22; ///< Product ID for Bootloader on older Pixhawk V2 boards
//static const int pixhawkFMUV1ProductId = 16;     ///< Product ID for PX4 FMU V1 board

#define MAV_AUTOPILOT_ENUM_END (static_cast<uint8_t>(MAV_AUTOPILOT::MAV_AUTOPILOT_ASLUAV) + 1)
#define MAV_TYPE_ENUM_END (static_cast<uint8_t>(MAV_TYPE::MAV_TYPE_ADSB) + 1)
#define MAV_STATE_ENUM_END (static_cast<uint8_t>(MAV_STATE::MAV_STATE_POWEROFF) + 1)

typedef mavlink_utils::Utils Utils;
typedef mavlink_utils::FileSystem FileSystem;
typedef unsigned int uint;
using namespace mavlinkcom;

struct FlagName
{
public:
    int Flag;
    const char* Name;
};

class PortAddress
{
public:
    std::string addr;
    int port;
};

#ifdef _WIN32
#include <Windows.h>
void DebugOutput(const char* message, ...)
{
    va_list args;
    va_start(args, message);

    std::unique_ptr<char[]> buffer(new char[8000]);
    vsprintf(buffer.get(), message, args);
    OutputDebugStringA(buffer.get());
    OutputDebugStringA("\n");
    fflush(stdout);

    va_end(args);
}
#else
// how do you write to the debug output windows on Unix ?
__attribute__((__format__(__printf__, 1, 0))) void DebugOutput(const char* message, ...)
{
    va_list args;
    va_start(args, message);

    std::unique_ptr<char[]> buffer(new char[8000]);
    IGNORE_FORMAT_STRING_ON
    vsprintf(buffer.get(), message, args);
    IGNORE_FORMAT_STRING_OFF
    fflush(stdout);

    va_end(args);
}
#endif

const int LocalSystemId = 166;
const int LocalLogViewerSystemId = 167;
const int LocalComponentId = 1;

#define DEFAULT_BUFLEN 512
std::string defaultLocalAddress{ "127.0.0.1" };

// The remote app is connected to Pixhawk, and is also "serving" UDP packets, this tells us what remote
// connection to create to talke to that server.
bool offboard = false;
bool udp = false;
bool tcp = false;
PortAddress offboardEndPoint;
#define DEFAULT_OFFBOARD_UDP_PORT 14560
#define DEFAULT_OFFBOARD_TCP_PORT 4560

// SITL setup info
const uint8_t sim_sysid = 142;
const int sim_compid = 42;
#define DEFAULT_SITL_PORT 14580
bool sitl = false;
PortAddress sitlEndPoint;

// The local ethernet interface to use (default localhost).
PortAddress localEndPoint;

// this is used if you want to connect MavLinkTest to the serial port of the Pixhawk directly
bool serial = false;
std::string comPort;
int baudRate = 115200;

// server mode on UDP is when you want another app to connect to Pixhawk and publish data back to this process.
// this server will be listening for UDP packets, this is mutually exclusive with 'offboard' as this
// server will become the primary "droneConnection".  For example, jMAVSim can talk to this server
// using their the -qgc option.  Server mode on TCP means mavlinktest will do an "accept" socket which is
// what PX4 is waiting for when it is running in TCP mode.  Here the serverEndPoint is different from the
// offboardEndPoint.  The serverEndPoint specifies which local address to use in case your computer has
// multiple network interfaces.
bool server = false;

bool connectLogViewer = false;
PortAddress logViewerEndPoint;
#define DEFAULT_LOGVIEWER_PORT 14388

// These are used to echo the mavlink messages to other 3rd party apps like QGC or LogViewer.
std::vector<PortAddress> proxyEndPoints;
#define DEFAULT_PROXY_PORT 14580

// this switch controls whether we turn off the RC remote active link loss detection
// if you do not have radio connected this is needed to stop "failsafe" override in pixhawk
// from kicking in when you try and fly.
bool noRadio = false;
bool unitTest = false;
bool verbose = false;
bool nsh = false;
bool noparams = false;
std::string logDirectory;
std::string ifaceName;
bool jsonLogFormat = false;
bool csvLogFormat = false;
bool convertExisting = false;
std::vector<int> filterTypes;
std::shared_ptr<MavLinkFileLog> inLogFile;
std::shared_ptr<MavLinkFileLog> outLogFile;
std::thread telemetry_thread;
bool telemetry = false;
std::mutex logLock;
std::stringstream initScript;

std::shared_ptr<MavLinkConnection> droneConnection;
std::shared_ptr<MavLinkConnection> logConnection;
std::shared_ptr<MavLinkVehicle> mavLinkVehicle;
std::shared_ptr<MavLinkNode> hilNode; // for simulation.

std::vector<PortAddress> usedPorts;

void ConvertLogFileToJson(std::string logFile)
{
    std::string fullPath = FileSystem::getFullPath(logFile);
    printf("Converting logfile to json: %s...", fullPath.c_str());
    try {
        MavLinkMessage msg;

        MavLinkFileLog log;
        log.openForReading(fullPath);

        path jsonPath(logFile);
        jsonPath.replace_extension(".json");

        MavLinkFileLog jsonLog;
        jsonLog.openForWriting(jsonPath.generic_string(), true);

        uint64_t timestamp;
        while (log.read(msg, timestamp)) {
            jsonLog.write(msg, timestamp);
        }
        jsonLog.close();
        printf("done\n");
    }
    catch (std::exception& ex) {
        printf("error: %s\n", ex.what());
    }
}

class CsvWriter
{
    std::ofstream csvFile;
    bool begin;
    std::string delimiter;

public:
    CsvWriter(std::string fileName, std::string tabDelimiter)
    {
        csvFile.open(fileName.c_str());
        this->delimiter = tabDelimiter;
    }
    ~CsvWriter()
    {
        csvFile.close();
    }
    void BeginRow()
    {
        begin = true;
    }
    void WriteValue(const std::string& value)
    {
        if (begin) {
            begin = false;
        }
        else {
            csvFile << delimiter;
        }
        csvFile << value;
    }
    void WriteValue(double value)
    {
        if (begin) {
            begin = false;
        }
        else {
            csvFile << delimiter;
        }
        csvFile << value;
    }
    void EndRow()
    {
        csvFile << std::endl;
    }
};

void ConvertLogFileToCsv(std::string logFile, int filter)
{
    std::string fullPath = FileSystem::getFullPath(logFile);
    printf("Converting logfile to csv: %s...", fullPath.c_str());
    try {
        MavLinkMessage msg;

        MavLinkFileLog log;
        log.openForReading(fullPath);

        path jsonPath(logFile);
        jsonPath.replace_extension(".csv");

        CsvWriter csv(jsonPath.generic_string(), "\t");

        bool headers = true;
        uint64_t timestamp;
        while (log.read(msg, timestamp)) {
            if (msg.msgid == filter) {
                MavLinkMessageBase* strongTypedMsg = MavLinkMessageBase::lookup(msg);
                if (strongTypedMsg != nullptr) {
                    strongTypedMsg->timestamp = timestamp;
                    std::string line = strongTypedMsg->toJSon();
                    line = line.substr(0, line.size() - 1); // remove trailing comma.
                    // parse the json
                    nlohmann::json doc;
                    std::stringstream ss;
                    ss << line;
                    ss >> doc;
                    auto name = doc["name"].get<std::string>();
                    if (headers) {
                        headers = false;
                        csv.BeginRow();
                        for (auto it = doc.begin(); it != doc.end(); ++it) {
                            auto v = it.value();
                            if (v.is_object()) {
                                // flatten inner mavlink object
                                for (auto itm = v.begin(); itm != v.end(); ++itm) {
                                    csv.WriteValue(itm.key());
                                }
                            }
                            else {
                                csv.WriteValue(it.key());
                            }
                        }
                        csv.EndRow();
                    }
                    csv.BeginRow();
                    for (auto it = doc.begin(); it != doc.end(); ++it) {
                        auto v = it.value();
                        if (v.is_object()) {
                            // flatten inner mavlink object
                            for (auto itm = v.begin(); itm != v.end(); ++itm) {
                                auto vm = itm.value();
                                if (vm.is_number()) {
                                    csv.WriteValue(vm.get<double>());
                                }
                                else if (vm.is_string()) {
                                    csv.WriteValue(vm.get<std::string>());
                                }
                                else {
                                    // todo
                                    csv.WriteValue("");
                                }
                            }
                        }
                        else {
                            auto m = it.value();

                            if (m.is_number()) {
                                csv.WriteValue(m.get<double>());
                            }
                            else if (m.is_string()) {
                                csv.WriteValue(m.get<std::string>());
                            }
                            else {
                                // todo
                                csv.WriteValue("");
                            }
                        }
                    }
                    csv.EndRow();

                    delete strongTypedMsg;
                }
            }
        };
        printf("done\n");
    }
    catch (std::exception& ex) {
        printf("error: %s\n", ex.what());
    }
}

void LoadInitScript(std::string fileName)
{

    std::ifstream fs;
    std::string line;
    FileSystem::openTextFile(fileName, fs);
    while (!fs.eof()) {
        std::getline(fs, line);
        if (line.size() > 0) {
            initScript << line << std::endl;
        }
    }
}

void ConvertLogFilesToJson(std::string directory)
{
    if (directory == "") {
        printf("Please provide the -logdir option\n");
        return;
    }
    printf("converting log files in: %s\n", directory.c_str());
    auto fullPath = FileSystem::getFullPath(directory);
    if (!FileSystem::isDirectory(fullPath)) {
        printf("-logdir:%s, does not exist\n", fullPath.c_str());
    }
    path dirPath(fullPath);

    for (directory_iterator next(dirPath), end; next != end; ++next) {
        auto path = next->path();
        auto ext = path.extension();
        if (ext == ".mavlink") {
            ConvertLogFileToJson(path.generic_string());
        }
    }
}

void ConvertLogFilesToCsv(std::string directory)
{
    if (filterTypes.size() != 1) {
        printf("When converting to csv, you must provide a single -filter value\n");
        return;
    }
    if (directory == "") {
        printf("Please provide the -logdir option\n");
        return;
    }

    int filter = filterTypes[0];

    printf("extracting csv data for %d from log files in: %s\n", filter, directory.c_str());

    auto fullPath = FileSystem::getFullPath(directory);
    if (!FileSystem::isDirectory(fullPath)) {
        printf("-logdir:%s, does not exist\n", fullPath.c_str());
    }
    path dirPath(fullPath);

    for (directory_iterator next(dirPath), end; next != end; ++next) {
        auto path = next->path();
        auto ext = path.extension();
        if (ext == ".mavlink") {
            ConvertLogFileToCsv(path.generic_string(), filter);
        }
    }
}

void OpenLogFiles()
{
    if (logDirectory.size() > 0) {
        std::time_t result = std::time(nullptr);
        auto local = std::localtime(&result);

        auto path = FileSystem::getFullPath(logDirectory);
        FileSystem::ensureFolder(path);

        path = FileSystem::combine(path, "logs");
        FileSystem::ensureFolder(path);

        std::string today = Utils::stringf("%04d-%02d-%02d", local->tm_year + 1900, local->tm_mon + 1, local->tm_mday);
        path = FileSystem::combine(path, today);
        FileSystem::ensureFolder(path);

        const char* ext = jsonLogFormat ? "json" : "mavlink";
        std::string input = Utils::stringf("%02d-%02d-%02d-input.%s", local->tm_hour, local->tm_min, local->tm_sec, ext);
        auto infile = FileSystem::combine(path, input);
        inLogFile = std::make_shared<MavLinkFileLog>();
        inLogFile->openForWriting(infile, jsonLogFormat);

        std::string output = Utils::stringf("%02d-%02d-%02d-output.%s", local->tm_hour, local->tm_min, local->tm_sec, ext);
        auto outfile = FileSystem::combine(path, output);
        outLogFile = std::make_shared<MavLinkFileLog>();
        outLogFile->openForWriting(outfile, jsonLogFormat);
    }
}
void CloseLogFiles()
{
    if (inLogFile != nullptr) {
        inLogFile->close();
        inLogFile = nullptr;
    }
    if (outLogFile != nullptr) {
        outLogFile->close();
        outLogFile = nullptr;
    }
}

const static FlagName MavSysSensorFlags[] = {
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_3D_GYRO), "MAV_SYS_STATUS_SENSOR_3D_GYRO - 0x01 3D gyro" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_3D_ACCEL), "MAV_SYS_STATUS_SENSOR_3D_ACCEL - 0x02 3D accelerometer" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_3D_MAG), "MAV_SYS_STATUS_SENSOR_3D_MAG - 0x04 3D magnetometer" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_ABSOLUTE_PRESSURE), "MAV_SYS_STATUS_SENSOR_ABSOLUTE_PRESSURE - 0x08 absolute pressure" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_DIFFERENTIAL_PRESSURE), "MAV_SYS_STATUS_SENSOR_DIFFERENTIAL_PRESSURE - 0x10 differential pressure" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_GPS), "MAV_SYS_STATUS_SENSOR_GPS - 0x20 GPS" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_OPTICAL_FLOW), "MAV_SYS_STATUS_SENSOR_OPTICAL_FLOW - 0x40 optical flow" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_VISION_POSITION), "MAV_SYS_STATUS_SENSOR_VISION_POSITION - 0x80 computer vision position" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_LASER_POSITION), "MAV_SYS_STATUS_SENSOR_LASER_POSITION - 0x100 laser based position" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_EXTERNAL_GROUND_TRUTH), "MAV_SYS_STATUS_SENSOR_EXTERNAL_GROUND_TRUTH - 0x200 external ground truth (Vicon or Leica)" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_ANGULAR_RATE_CONTROL), "MAV_SYS_STATUS_SENSOR_ANGULAR_RATE_CONTROL - 0x400 3D angular rate control" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_ATTITUDE_STABILIZATION), "MAV_SYS_STATUS_SENSOR_ATTITUDE_STABILIZATION - 0x800 attitude stabilization" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_YAW_POSITION), "MAV_SYS_STATUS_SENSOR_YAW_POSITION - 0x1000 yaw position" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_Z_ALTITUDE_CONTROL), "MAV_SYS_STATUS_SENSOR_Z_ALTITUDE_CONTROL - 0x2000 z/altitude control" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_XY_POSITION_CONTROL), "MAV_SYS_STATUS_SENSOR_XY_POSITION_CONTROL - 0x4000 x/y position control" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_MOTOR_OUTPUTS), "MAV_SYS_STATUS_SENSOR_MOTOR_OUTPUTS - 0x8000 motor outputs / control" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_RC_RECEIVER), "MAV_SYS_STATUS_SENSOR_RC_RECEIVER - 0x10000 rc receiver" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_3D_GYRO2), "MAV_SYS_STATUS_SENSOR_3D_GYRO2 - 0x20000 2nd 3D gyro" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_3D_ACCEL2), "MAV_SYS_STATUS_SENSOR_3D_ACCEL2 - 0x40000 2nd 3D accelerometer" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_SENSOR_3D_MAG2), "MAV_SYS_STATUS_SENSOR_3D_MAG2 - 0x80000 2nd 3D magnetometer" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_GEOFENCE), "MAV_SYS_STATUS_GEOFENCE - 0x100000 geofence" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_AHRS), "MAV_SYS_STATUS_AHRS - 0x200000 AHRS subsystem health" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_TERRAIN), "MAV_SYS_STATUS_TERRAIN - 0x400000 Terrain subsystem health" },
    { static_cast<int>(MAV_SYS_STATUS_SENSOR::MAV_SYS_STATUS_REVERSE_MOTOR), "MAV_SYS_STATUS_REVERSE_MOTOR - 0x800000 Motors are reversed" },
    { 0, NULL }
};

const static char* AutoPilotNames[]{
    "MAV_AUTOPILOT_GENERIC, Generic autopilot, full support for everything",
    "MAV_AUTOPILOT_RESERVED, Reserved for future use.",
    "MAV_AUTOPILOT_SLUGS, SLUGS autopilot, http://slugsuav.soe.ucsc.edu",
    "MAV_AUTOPILOT_ARDUPILOTMEGA, ArduPilotMega / ArduCopter, http://diydrones.com",
    "MAV_AUTOPILOT_OPENPILOT, OpenPilot, http://openpilot.org",
    "MAV_AUTOPILOT_GENERIC_WAYPOINTS_ONLY, Generic autopilot only supporting simple waypoints",
    "MAV_AUTOPILOT_GENERIC_WAYPOINTS_AND_SIMPLE_NAVIGATION_ONLY, Generic autopilot supporting waypoints and other simple navigation commands",
    "MAV_AUTOPILOT_GENERIC_MISSION_FULL, Generic autopilot supporting the full mission command set",
    "MAV_AUTOPILOT_INVALID,  No valid autopilot, e.g. a GCS or other MAVLink component",
    "MAV_AUTOPILOT_PPZ, PPZ UAV - http://nongnu.org/paparazzi",
    "MAV_AUTOPILOT_UDB, UAV Dev Board",
    "MAV_AUTOPILOT_FP, FlexiPilot",
    "MAV_AUTOPILOT_PX4, PX4 Autopilot - http://pixhawk.ethz.ch/px4/",
    "MAV_AUTOPILOT_SMACCMPILOT, SMACCMPilot - http://smaccmpilot.org",
    "MAV_AUTOPILOT_AUTOQUAD, AutoQuad -- http://autoquad.org",
    "MAV_AUTOPILOT_ARMAZILA,  Armazila -- http://armazila.com",
    "MAV_AUTOPILOT_AEROB, Aerob -- http://aerob.ru",
    "MAV_AUTOPILOT_ASLUAV, ASLUAV autopilot -- http://www.asl.ethz.ch"
};

const static char* MavTypeNames[]{
    "MAV_TYPE_GENERIC-  Generic micro air vehicle.",
    "MAV_TYPE_FIXED_WING-  Fixed wing aircraft.",
    "MAV_TYPE_QUADROTOR-  Quadrotor",
    "MAV_TYPE_COAXIAL-  Coaxial helicopter",
    "MAV_TYPE_HELICOPTER-  Normal helicopter with tail rotor.",
    "MAV_TYPE_ANTENNA_TRACKER-  Ground installation",
    "MAV_TYPE_GCS-  Operator control unit / ground control station",
    "MAV_TYPE_AIRSHIP-  Airship, controlled",
    "MAV_TYPE_FREE_BALLOON-  Free balloon, uncontrolled",
    "MAV_TYPE_ROCKET-  Rocket",
    "MAV_TYPE_GROUND_ROVER-  Ground rover",
    "MAV_TYPE_SURFACE_BOAT-  Surface vessel, boat, ship",
    "MAV_TYPE_SUBMARINE-  Submarine",
    "MAV_TYPE_HEXAROTOR-  Hexarotor",
    "MAV_TYPE_OCTOROTOR-  Octorotor",
    "MAV_TYPE_TRICOPTER-  Octorotor",
    "MAV_TYPE_FLAPPING_WING-  Flapping wing",
    "MAV_TYPE_KITE-  Flapping wing",
    "MAV_TYPE_ONBOARD_CONTROLLER-  Onboard companion controller",
    "MAV_TYPE_VTOL_DUOROTOR-  Two-rotor VTOL using control surfaces in vertical operation in addition. Tailsitter.",
    "MAV_TYPE_VTOL_QUADROTOR-  Quad-rotor VTOL using a V-shaped quad config in vertical operation. Tailsitter.",
    "MAV_TYPE_VTOL_TILTROTOR-  Tiltrotor VTOL",
    "MAV_TYPE_VTOL_RESERVED2-  VTOL reserved 2",
    "MAV_TYPE_VTOL_RESERVED3-  VTOL reserved 3",
    "MAV_TYPE_VTOL_RESERVED4-  VTOL reserved 4",
    "MAV_TYPE_VTOL_RESERVED5-  VTOL reserved 5",
    "MAV_TYPE_GIMBAL-  Onboard gimbal",
    "MAV_TYPE_ADSB-  Onboard ADSB peripheral"
};

const static FlagName ModeFlagNames[]{
    { static_cast<int>(MAV_MODE_FLAG::MAV_MODE_FLAG_TEST_ENABLED), "MAV_MODE_FLAG_TEST_ENABLED - system has a test mode enabled. This flag is intended for temporary system tests and should not be used for stable implementations." },
    { static_cast<int>(MAV_MODE_FLAG::MAV_MODE_FLAG_AUTO_ENABLED), "MAV_MODE_FLAG_AUTO_ENABLED - autonomous mode enabled, system finds its own goal positions. Guided flag can be set or not, depends on the actual implementation." },
    { static_cast<int>(MAV_MODE_FLAG::MAV_MODE_FLAG_GUIDED_ENABLED), "MAV_MODE_FLAG_GUIDED_ENABLED - guided mode enabled, system flies MISSIONs / mission items." },
    { static_cast<int>(MAV_MODE_FLAG::MAV_MODE_FLAG_STABILIZE_ENABLED), "MAV_MODE_FLAG_STABILIZE_ENABLED - system stabilizes electronically its attitude (and optionally position). It needs however further control inputs to move around." },
    { static_cast<int>(MAV_MODE_FLAG::MAV_MODE_FLAG_HIL_ENABLED), "MAV_MODE_FLAG_HIL_ENABLED - hardware in the loop simulation. All motors / actuators are blocked, but internal software is full operational." },
    { static_cast<int>(MAV_MODE_FLAG::MAV_MODE_FLAG_MANUAL_INPUT_ENABLED), "MAV_MODE_FLAG_MANUAL_INPUT_ENABLED - remote control input is enabled." },
    { static_cast<int>(MAV_MODE_FLAG::MAV_MODE_FLAG_SAFETY_ARMED), "MAV_MODE_FLAG_SAFETY_ARMED - MAV safety set to armed. Motors are enabled / running / can start. Ready to fly." },
    { 0, NULL }
};

const static char* MavStateNames[]{
    "MAV_STATE_UNINIT - Uninitialized system, state is unknown.",
    "MAV_STATE_BOOT - System is booting up.",
    "MAV_STATE_CALIBRATING - System is calibrating and not flight-ready.",
    "MAV_STATE_STANDBY - System is grounded and on standby. It can be launched any time.",
    "MAV_STATE_ACTIVE - System is active and might be already airborne. Motors are engaged.",
    "MAV_STATE_CRITICAL - System is in a non-normal flight mode. It can however still navigate.",
    "MAV_STATE_EMERGENCY - System is in a non-normal flight mode. It lost control over parts or over the whole airframe. It is in mayday and going down.",
    "MAV_STATE_POWEROFF - System just initialized its power-down sequence, will shut down now.",
    "MAV_STATE_ENUM_END - ",
};

enum PX4_CUSTOM_MAIN_MODE
{
    PX4_CUSTOM_MAIN_MODE_MANUAL = 1,
    PX4_CUSTOM_MAIN_MODE_ALTCTL,
    PX4_CUSTOM_MAIN_MODE_POSCTL,
    PX4_CUSTOM_MAIN_MODE_AUTO,
    PX4_CUSTOM_MAIN_MODE_ACRO,
    PX4_CUSTOM_MAIN_MODE_OFFBOARD,
    PX4_CUSTOM_MAIN_MODE_STABILIZED,
    PX4_CUSTOM_MAIN_MODE_RATTITUDE
};

enum PX4_CUSTOM_SUB_MODE_AUTO
{
    PX4_CUSTOM_SUB_MODE_AUTO_READY = 1,
    PX4_CUSTOM_SUB_MODE_AUTO_TAKEOFF,
    PX4_CUSTOM_SUB_MODE_AUTO_LOITER,
    PX4_CUSTOM_SUB_MODE_AUTO_MISSION,
    PX4_CUSTOM_SUB_MODE_AUTO_RTL,
    PX4_CUSTOM_SUB_MODE_AUTO_LAND,
    PX4_CUSTOM_SUB_MODE_AUTO_RTGS,
    PX4_CUSTOM_SUB_MODE_AUTO_FOLLOW_TARGET
};

const static FlagName CustomModeNames[]{
    { PX4_CUSTOM_MAIN_MODE_MANUAL, "PX4_CUSTOM_MAIN_MODE_MANUAL" },
    { PX4_CUSTOM_MAIN_MODE_ALTCTL, "PX4_CUSTOM_MAIN_MODE_ALTCTL" },
    { PX4_CUSTOM_MAIN_MODE_POSCTL, "PX4_CUSTOM_MAIN_MODE_POSCTL" },
    { PX4_CUSTOM_MAIN_MODE_AUTO, "PX4_CUSTOM_MAIN_MODE_AUTO" },
    { PX4_CUSTOM_MAIN_MODE_ACRO, "PX4_CUSTOM_MAIN_MODE_ACRO" },
    { PX4_CUSTOM_MAIN_MODE_OFFBOARD, "PX4_CUSTOM_MAIN_MODE_OFFBOARD" },
    { PX4_CUSTOM_MAIN_MODE_STABILIZED, "PX4_CUSTOM_MAIN_MODE_STABILIZED" },
    { PX4_CUSTOM_MAIN_MODE_RATTITUDE, "PX4_CUSTOM_MAIN_MODE_RATTITUDE" },
    { 0, nullptr }
};

const static FlagName CustomSubModeNames[]{
    { PX4_CUSTOM_SUB_MODE_AUTO_READY, "PX4_CUSTOM_SUB_MODE_AUTO_READY" },
    { PX4_CUSTOM_SUB_MODE_AUTO_TAKEOFF, "PX4_CUSTOM_SUB_MODE_AUTO_TAKEOFF" },
    { PX4_CUSTOM_SUB_MODE_AUTO_LOITER, "PX4_CUSTOM_SUB_MODE_AUTO_LOITER" },
    { PX4_CUSTOM_SUB_MODE_AUTO_MISSION, "PX4_CUSTOM_SUB_MODE_AUTO_MISSION" },
    { PX4_CUSTOM_SUB_MODE_AUTO_RTL, "PX4_CUSTOM_SUB_MODE_AUTO_RTL" },
    { PX4_CUSTOM_SUB_MODE_AUTO_LAND, "PX4_CUSTOM_SUB_MODE_AUTO_LAND" },
    { PX4_CUSTOM_SUB_MODE_AUTO_RTGS, "PX4_CUSTOM_SUB_MODE_AUTO_RTGS" },
    { PX4_CUSTOM_SUB_MODE_AUTO_FOLLOW_TARGET, "PX4_CUSTOM_SUB_MODE_AUTO_FOLLOW_TARGET" },
    { 0, nullptr }
};

void PrintFlags(const FlagName* flagNames, int value)
{

    for (int i = 0;; i++) {
        if (flagNames[i].Name == NULL)
            break;

        if ((value & flagNames[i].Flag) != 0) {
            printf("    %s\n", flagNames[i].Name);
        }
    }
}

void PrintEnum(const FlagName* enumNames, int value)
{

    for (int i = 0;; i++) {
        if (enumNames[i].Name == NULL)
            break;

        if (value == enumNames[i].Flag) {
            printf("    %s\n", enumNames[i].Name);
        }
    }
}

void PrintSystemStatus(MavLinkSysStatus& status)
{
    printf("System status:\n");

    printf("    onboard sensors present:\n");
    PrintFlags(MavSysSensorFlags, status.onboard_control_sensors_present);

    printf("    onboard sensors enabled:\n");
    PrintFlags(MavSysSensorFlags, status.onboard_control_sensors_enabled);

    printf("    CPU load %d\n", static_cast<int>(status.load));

    printf("    battery voltage %d millivolts\n", static_cast<int>(status.voltage_battery));
    printf("    battery current %d milliamps\n", static_cast<int>(status.current_battery));
    printf("    drop_rate_comm = %d\n", static_cast<int>(status.drop_rate_comm));
    printf("    errors_comm = %d\n", static_cast<int>(status.errors_comm));
    printf("    battery_remaining = %d\n", static_cast<int>(status.battery_remaining));
}

void PrintCustomMode(const MavLinkHeartbeat& heartbeat)
{
    if (heartbeat.autopilot == static_cast<uint8_t>(MAV_AUTOPILOT::MAV_AUTOPILOT_PX4)) {
        int custom = (heartbeat.custom_mode >> 16);
        int mode = (custom & 0xff);
        int submode = (custom >> 8);
        PrintEnum(CustomModeNames, mode);
        PrintEnum(CustomSubModeNames, submode);
    }
    else {
        Utils::log(Utils::stringf("    Custom mode=%x", heartbeat.custom_mode));
    }
}

void PrintHeartbeat(const MavLinkMessage& msg)
{

    MavLinkHeartbeat heartbeat;
    heartbeat.decode(msg);

    Utils::log("Connected:\n");
    Utils::log(Utils::stringf("    Version=%d\n", static_cast<int>(heartbeat.mavlink_version)));

    if (heartbeat.type < MAV_TYPE_ENUM_END) {
        Utils::log(Utils::stringf("    Type=%s\n", MavTypeNames[heartbeat.type]));
    }

    if (heartbeat.autopilot < MAV_AUTOPILOT_ENUM_END) {
        Utils::log(Utils::stringf("    Autopilot=%s\n", AutoPilotNames[heartbeat.autopilot]));
    }

    if (heartbeat.system_status < MAV_STATE_ENUM_END) {
        Utils::log(Utils::stringf("    State=%s\n", MavStateNames[heartbeat.system_status]));
    }

    Utils::log("    Base mode:\n");
    PrintFlags(ModeFlagNames, heartbeat.base_mode);

    PrintCustomMode(heartbeat);

    Utils::log(Utils::stringf("    VEHICLE SYSTEM ID: %i\n", msg.sysid));
    Utils::log(Utils::stringf("    VEHICLE COMPONENT ID: %i\n", msg.compid));
}

uint32_t gCustom = 0;

void CheckHeartbeat(const MavLinkMessage& msg)
{
    MavLinkHeartbeat heartbeat;
    heartbeat.decode(msg);

    if (gCustom != heartbeat.custom_mode) {
        gCustom = heartbeat.custom_mode;
        PrintCustomMode(heartbeat);
    }
}

void mavlink_quaternion_to_dcm(const float quaternion[4], float dcm[3][3])
{
    double a = quaternion[0];
    double b = quaternion[1];
    double c = quaternion[2];
    double d = quaternion[3];
    double aSq = a * a;
    double bSq = b * b;
    double cSq = c * c;
    double dSq = d * d;
    dcm[0][0] = static_cast<float>(aSq + bSq - cSq - dSq);
    dcm[0][1] = static_cast<float>(2 * (b * c - a * d));
    dcm[0][2] = static_cast<float>(2 * (a * c + b * d));
    dcm[1][0] = static_cast<float>(2 * (b * c + a * d));
    dcm[1][1] = static_cast<float>(aSq - bSq + cSq - dSq);
    dcm[1][2] = static_cast<float>(2 * (c * d - a * b));
    dcm[2][0] = static_cast<float>(2 * (b * d - a * c));
    dcm[2][1] = static_cast<float>(2 * (a * b + c * d));
    dcm[2][2] = static_cast<float>(aSq - bSq - cSq + dSq);
}
void mavlink_dcm_to_euler(const float dcm[3][3], float* roll, float* pitch, float* yaw)
{
    float phi, theta, psi;
    theta = asin(-dcm[2][0]);

    if (fabsf(theta - static_cast<float>(M_PI_2)) < 1.0e-3f) {
        phi = 0.0f;
        psi = (atan2f(dcm[1][2] - dcm[0][1],
                      dcm[0][2] + dcm[1][1]) +
               phi);
    }
    else if (fabsf(theta + static_cast<float>(M_PI_2)) < 1.0e-3f) {
        phi = 0.0f;
        psi = atan2f(dcm[1][2] - dcm[0][1],
                     dcm[0][2] + dcm[1][1] - phi);
    }
    else {
        phi = atan2f(dcm[2][1], dcm[2][2]);
        psi = atan2f(dcm[1][0], dcm[0][0]);
    }

    *roll = phi;
    *pitch = theta;
    *yaw = psi;
}
void mavlink_quaternion_to_euler(const float quaternion[4], float* roll, float* pitch, float* yaw)
{
    float dcm[3][3];
    mavlink_quaternion_to_dcm(quaternion, dcm);
    mavlink_dcm_to_euler(dcm, roll, pitch, yaw);
}

extern void mavlink_euler_to_quaternion(float roll, float pitch, float yaw, float quaternion[4]);

void PrintUsage()
{
    printf("Usage: PX4 options\n");
    printf("Connects to PX4 either over udp or serial COM port\n");
    printf("Options: \n");
    printf("    -udp[:ipaddr[:port]]                   - connect to remote drone at this udp address (default port localhost:14550)\n");
    printf("    -tcp[:ipaddr[:port]]                   - connect to remote drone at this tcp address (default port localhost:4560)\n");
    printf("    -local[:ipaddr]                        - connect to remote drone via this local address (default localhost)\n");
    printf("    -server                                - start mavlink server on specified -udp or -tcp local address\n");
    printf("    -serial[:comPortName][, baudrate]]     - open serial port\n");
    printf("    -sitl[:ipaddr[:port]]                  - when talking to a SITL PX4 provide socket port number here for control channel (default localhost:14580)\n");
    printf("    -logviewer:ipaddr[:port]               - for sending mavlink information to Log Viewer\n");
    printf("    -proxy:ipaddr[:port]                   - send all mavlink messages to and from remote node\n");
    printf("    -local:ipaddr                          - specify local NIC address (default 127.0.0.1)\n");
    printf("    -logdir:filename                       - specify local directory where mavlink logs are stored (default is no log files)\n");
    printf("    -logformat:json                        - the default is binary .mavlink, if you specify this option you will get mavlink logs in json\n");
    printf("    -convert:[json,csv]                    - convert all existing .mavlink log files in the logdir to the specified -logformat\n");
    printf("    -filter:msid,msgid,...                 - while converting .mavlink log extract only the given mavlink message ids\n");
    printf("    -noradio							   - disables RC link loss failsafe\n");
    printf("    -nsh                                   - enter NuttX shell immediately on connecting with PX4\n");
    printf("    -telemetry                             - generate telemetry mavlink messages for logviewer\n");
    printf("    -wifi:iface                            - add wifi rssi to the telemetry using given wifi interface name (e.g. wplsp0)\n");
    printf("If no arguments it will find a COM port matching the name 'PX4'\n");
    printf("You can specify -proxy multiple times with different port numbers to proxy drone messages out to multiple listeners\n");
}

bool ParseCommandLine(int argc, const char* argv[])
{
    const char* logDirOption = "logdir";
    const char* logformatOption = "logformat";
    //const char* outLogFileOption = "outlogfile";
    const char* wifiOption = "wifi";
    const char* initOption = "init";
    const char* filterOption = "filter";
    const char* convertOption = "convert";

    // parse command line
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (arg[0] == '-' || arg[0] == '/') {
            std::string option(arg + 1);
            std::vector<std::string> parts = Utils::split(option, ":,", 2);
            std::string lower = Utils::toLower(parts[0]);
            if (lower == "udp") {
                udp = true;
                offboard = true;
                offboardEndPoint.port = DEFAULT_OFFBOARD_UDP_PORT;
                if (parts.size() > 1) {
                    offboardEndPoint.addr = parts[1];
                    if (parts.size() > 2) {
                        offboardEndPoint.port = atoi(parts[2].c_str());
                    }
                }
            }
            else if (lower == "tcp") {
                tcp = true;
                offboard = true;
                offboardEndPoint.port = DEFAULT_OFFBOARD_TCP_PORT;
                if (parts.size() > 1) {
                    offboardEndPoint.addr = parts[1];
                    if (parts.size() > 2) {
                        offboardEndPoint.port = atoi(parts[2].c_str());
                    }
                }
            }
            else if (lower == "local") {
                localEndPoint.port = 0; // any port.
                if (parts.size() > 1) {
                    localEndPoint.addr = parts[1];
                    if (parts.size() > 2) {
                        localEndPoint.port = atoi(parts[2].c_str());
                    }
                }
            }
            else if (lower == "server") {
                server = true;
            }
            else if (lower == "sitl") {
                sitl = true;
                sitlEndPoint.port = 0; // any port.
                if (parts.size() > 1) {
                    sitlEndPoint.addr = parts[1];
                    if (parts.size() > 2) {
                        sitlEndPoint.port = atoi(parts[2].c_str());
                    }
                }
            }
            else if (lower == "proxy") {
                PortAddress ep;
                ep.port = DEFAULT_PROXY_PORT;

                if (parts.size() > 1) {
                    ep.addr = parts[1];
                    if (parts.size() > 2) {
                        ep.port = atoi(parts[2].c_str());
                    }
                }
                proxyEndPoints.push_back(ep);
            }
            else if (lower == "logviewer") {
                connectLogViewer = true;
                logViewerEndPoint.port = DEFAULT_LOGVIEWER_PORT;
                if (parts.size() > 1) {
                    logViewerEndPoint.addr = parts[1];
                    if (parts.size() > 2) {
                        logViewerEndPoint.port = atoi(parts[2].c_str());
                    }
                }
            }
            else if (lower == logDirOption) {
                if (parts.size() > 1) {
                    std::string fileName(arg + 1 + strlen(logDirOption) + 1);
                    logDirectory = fileName;
                }
            }
            else if (lower == logformatOption) {

                if (parts.size() > 1) {
                    std::string format(arg + 1 + strlen(logformatOption) + 1);
                    format = Utils::toLower(format);
                    if (format == "json") {
                        jsonLogFormat = true;
                    }
                    else {

                        printf("### Error: invalid logformat '%s', expecting 'json'\n", format.c_str());
                        return false;
                    }
                }
            }
            else if (lower == convertOption) {
                convertExisting = true;
                if (parts.size() > 1) {
                    std::string format(arg + 1 + strlen(convertOption) + 1);
                    format = Utils::toLower(format);
                    if (format == "json") {
                        jsonLogFormat = true;
                    }
                    else if (format == "csv") {
                        csvLogFormat = true;
                    }
                    else {
                        printf("### Error: invalid format '%s', expecting 'json'\n", format.c_str());
                        return false;
                    }
                }
            }
            else if (lower == filterOption) {
                if (parts.size() > 1) {
                    std::string filters(arg + 1 + strlen(filterOption) + 1);
                    std::vector<std::string> fparts = Utils::split(filters, ",", 1);
                    for (auto ptr = fparts.begin(), end = fparts.end(); ptr != end; ptr++) {
                        std::string f = *ptr;
                        try {
                            long ft = std::stol(f);
                            filterTypes.push_back(ft);
                        }
                        catch (std::exception&) {
                            printf("expecting integer filter messagid, but found %s\n", f.c_str());
                            return false;
                        }
                    }
                }
            }
#if defined(USE_CPP_FILESYSTEM)
            else if (lower == initOption) {

                if (parts.size() > 1) {
                    std::string fileName(arg + 1 + strlen(initOption) + 1);
                    LoadInitScript(fileName);
                }
            }
#endif
            else if (lower == "local") {
                if (parts.size() > 1) {
                    defaultLocalAddress = parts[1];
                }
            }
            else if (lower == "serial") {
                serial = true;
                if (parts.size() > 1) {
                    comPort = parts[1];
                    if (parts.size() > 2) {
                        baudRate = atoi(parts[2].c_str());
                        if (baudRate == 0) {
                            printf("### Error: invalid baud rate in -serial argument\n");
                            return false;
                        }
                    }
                }
            }
            else if (lower == "h" || lower == "?" || lower == "help" || lower == "-help") {
                return false;
            }
            else if (lower == "noradio") {
                noRadio = true;
            }
            else if (lower == "test") {
                unitTest = true;
            }
            else if (lower == "verbose") {
                verbose = true;
            }
            else if (lower == "nsh") {
                nsh = true;
            }
            else if (lower == "noparams") {
                noparams = true;
            }
            else if (lower == "telemetry") {
                telemetry = true;
            }
            else if (lower == wifiOption) {
                if (parts.size() > 1) {
                    std::string name(arg + 1 + strlen(wifiOption) + 1);
                    ifaceName = name;
                }
            }
            else {
                printf("### Error: unexpected argument: %s\n", arg);
                return false;
            }
        }
        else {
            printf("### Error: unexpected argument: %s\n", arg);
            return false;
        }
    }
    return true;
}

void HexDump(uint8_t* buffer, uint len)
{
    for (uint i = 0; i < len; i += 16) {

        uint j = 0;
        for (j = i; j < i + 16 && j < len; j++) {
            uint8_t b = buffer[i + j];
            printf("%02x ", b);
        }
        while (j < 16) {
            printf("   ");
        }
        for (j = i; j < i + 16 && j < len; j++) {
            uint8_t b = buffer[j];
            if (b < 0x20 || b == 0x7f || b == 0xfe) {
                b = '.';
            }
            printf("%c", b);
        }
        while (j < 16) {
            printf(" ");
        }
        printf("\n");
    }
}

std::shared_ptr<MavLinkConnection> connectProxy(const PortAddress& endPoint, std::string name)
{
    printf("Connecting to UDP Proxy address %s:%d\n", endPoint.addr.c_str(), endPoint.port);

    std::shared_ptr<MavLinkConnection> proxyConnection = MavLinkConnection::connectRemoteUdp(name, localEndPoint.addr, endPoint.addr, endPoint.port);

    // forward all PX4 messages to the remote proxy and all messages from remote proxy to PX4.
    droneConnection->join(proxyConnection);

    return proxyConnection;
}

std::string findPixhawk()
{

    auto result = MavLinkConnection::findSerialPorts(0, 0);
    for (auto iter = result.begin(); iter != result.end(); iter++) {
        SerialPortInfo info = *iter;
        if (info.vid == pixhawkVendorId) {
            if (info.pid == pixhawkFMUV4ProductId || info.pid == pixhawkFMUV2ProductId || info.pid == pixhawkFMUV2OldBootloaderProductId || info.pid == pixhawkFMUV5ProductId) {
                printf("Auto Selecting COM port: %S\n", info.displayName.c_str());

                std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
                std::string portName_str = converter.to_bytes(info.portName);
                return portName_str;
            }
        }
    }
    return "";
}

std::shared_ptr<MavLinkConnection> connectSerial()
{
    std::string name = comPort;
    printf("Connecting to serial port %s, baudrate=%d\n", name.c_str(), baudRate);
    return MavLinkConnection::connectSerial("drone", name, baudRate, "sh /etc/init.d/rc.usb\n");
}

std::shared_ptr<MavLinkConnection> connectOffboard()
{
    printf("Connecting to offboard drone at address %s:%d\n", offboardEndPoint.addr.c_str(), offboardEndPoint.port);
    return MavLinkConnection::connectRemoteUdp("drone", defaultLocalAddress, offboardEndPoint.addr, offboardEndPoint.port);
}

std::shared_ptr<MavLinkConnection> connectServer(std::string name)
{
    if (localEndPoint.addr == "") {
        localEndPoint.addr = defaultLocalAddress;
    }
    if (localEndPoint.port == 0) {
        localEndPoint.port = offboardEndPoint.port;
    }

    std::shared_ptr<MavLinkConnection> serverConnection;

    if (tcp) {
        if (localEndPoint.port == 0) {
            localEndPoint.port = DEFAULT_OFFBOARD_TCP_PORT;
        }
        printf("Waiting for drone to connect to server at address %s:%d...\n", localEndPoint.addr.c_str(), localEndPoint.port);
        MavLinkTcpServer server(localEndPoint.addr, localEndPoint.port);
        serverConnection = server.acceptTcp(name);
    }
    else {
        if (localEndPoint.port == 0) {
            localEndPoint.port = DEFAULT_OFFBOARD_UDP_PORT;
        }
        printf("Starting UDP Server on address %s:%d\n", localEndPoint.addr.c_str(), localEndPoint.port);
        serverConnection = MavLinkConnection::connectLocalUdp(name, localEndPoint.addr, localEndPoint.port);
    }

    if (droneConnection != nullptr) {
        // then we have a serial connection as the primary droneConnection, so publish all PX4 messages out to the server
        droneConnection->join(serverConnection);
    }
    else {
        // no local serial connection, so this is the primary droneConnection.
        droneConnection = serverConnection;
    }

    return serverConnection;
}

void runTelemetry()
{
    while (telemetry) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (droneConnection != nullptr) {
            MavLinkTelemetry tel;
            tel.wifiInterfaceName = ifaceName.c_str();
            droneConnection->getTelemetry(tel);
            tel.compid = LocalComponentId;
            tel.sysid = LocalSystemId;
            if (logConnection != nullptr) {
                logConnection->sendMessage(tel);
            }
        }
    }
}

void startTelemetry()
{
    Utils::cleanupThread(telemetry_thread);
    telemetry_thread = std::thread(&runTelemetry);
}

void stopTelemetry()
{
    telemetry = false;
    if (telemetry_thread.joinable()) {
        telemetry_thread.join();
    }
}

bool connect()
{
    if (offboard && serial) {
        printf("Cannot connect to local -serial pixhawk and -udp drone at the same time \n");
        return false;
    }
    if (!offboard && !serial) {
        printf("Must specify one of -serial, -udp or -tcp otherwise we don't have a drone connection\n");
        return false;
    }

    if (offboardEndPoint.addr == "") {
        offboardEndPoint.addr = defaultLocalAddress;
    }

    if (localEndPoint.addr == "") {
        localEndPoint.addr = defaultLocalAddress;
    }

    if (serial) {
        droneConnection = connectSerial();
    }
    else if (!server) {
        droneConnection = connectOffboard();
        usedPorts.push_back(offboardEndPoint);
    }

    if (server) {
        std::shared_ptr<MavLinkConnection> serverConnection = connectServer("server");
        usedPorts.push_back(localEndPoint);
    }

    return true;
}

void connectSitl(std::shared_ptr<MavLinkVehicle> mavLinkVehicle)
{

    if (sitlEndPoint.port != 0) {
        sitlEndPoint.port = DEFAULT_SITL_PORT;
    }

    // need a retry loop here because we don't know how quickly px4 will start accepting these connections...
    for (int retries = 60; retries >= 0; retries--) {
        try {
            auto gcsConnection = mavlinkcom::MavLinkConnection::connectRemoteUdp("gcs",
                                                                                 localEndPoint.addr,
                                                                                 sitlEndPoint.addr,
                                                                                 sitlEndPoint.port);
            mavLinkVehicle->connect(gcsConnection);
        }
        catch (std::exception&) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

bool setupDrone()
{
    if (droneConnection == nullptr) {
        // failed to connect
        return false;
    }

    if (verbose) {
        droneConnection->subscribe([=](std::shared_ptr<MavLinkConnection> con, const MavLinkMessage& msg) {
            printf("Received msg %d from drone\n", static_cast<int>(msg.msgid));
        });
    }

    if (outLogFile != nullptr) {
        droneConnection->startLoggingSendMessage(outLogFile);
    }

    if (sitl) {
        // then we need 2 mavlink channels, one for sending/receiving HIL_* messages and the other
        // for controlling the drone.
        hilNode = std::make_shared<mavlinkcom::MavLinkNode>(sim_sysid, sim_compid);
        hilNode->connect(droneConnection);
        hilNode->startHeartbeat();

        printf("TODO: for SITL to work, mavlinktest needs to simulate HIL_SENSOR messages...\n");

        // this is the control channel.
        mavLinkVehicle = std::make_shared<MavLinkVehicle>(LocalSystemId, LocalComponentId);
        connectSitl(mavLinkVehicle);
    }
    else {
        mavLinkVehicle = std::make_shared<MavLinkVehicle>(LocalSystemId, LocalComponentId);
        mavLinkVehicle->connect(droneConnection);
    }

    mavLinkVehicle->startHeartbeat();

    if (connectLogViewer) {
        if (logViewerEndPoint.addr == "") {
            logViewerEndPoint.addr = defaultLocalAddress;
        }
        logConnection = connectProxy(logViewerEndPoint, "log");
        usedPorts.push_back(logViewerEndPoint);
        if (serial && telemetry) {
            startTelemetry();
        }
    }
    else {
        logConnection = nullptr;
    }

    for (auto ptr = proxyEndPoints.begin(), end = proxyEndPoints.end(); ptr != end; ptr++) {
        bool ok = true;
        PortAddress proxyEndPoint = *ptr;
        for (auto ep = usedPorts.begin(), endep = usedPorts.end(); ep != endep; ep++) {
            PortAddress used = *ep;
            if (used.addr == proxyEndPoint.addr && used.port == proxyEndPoint.port) {
                printf("Cannot proxy to address that is already used: %s:%d\n", used.addr.c_str(), used.port);
                ok = false;
            }
        }
        if (ok) {
            usedPorts.push_back(proxyEndPoint);
            connectProxy(proxyEndPoint, "proxy");
        }
    }

    return true;
}

void checkPulse()
{
    MavLinkHeartbeat heartbeat;
    if (!mavLinkVehicle->waitForHeartbeat().wait(100000, &heartbeat)) {
        throw std::runtime_error("Received no heartbeat from PX4 after 100 seconds");
    }
}

const char* IgnoreStateTable[] = {
    "Baro #0 fail:  STALE!",
    nullptr
};

void handleStatus(const MavLinkStatustext& statustext)
{
    std::string msg = statustext.text;
    for (size_t i = 0; IgnoreStateTable[i] != nullptr; i++) {
        if (msg == IgnoreStateTable[i]) {
            return;
        }
    }

    std::string safeText(statustext.text, 50);
    Utils::log(Utils::stringf("STATUS: sev=%d, '%s'\n", static_cast<int>(statustext.severity), safeText.c_str()));
}

int console(std::stringstream& script)
{

    std::string line;
    std::shared_ptr<MavLinkNode> logViewer = nullptr;
    Command* currentCommand = nullptr;
    OrbitCommand* orbit = new OrbitCommand();
    SendImageCommand* sendImage = nullptr;
    NshCommand* nshCommand = new NshCommand();

    std::vector<Command*> cmdTable;
    Command::setAllCommand(&cmdTable);

    cmdTable.push_back(new ArmDisarmCommand());
    cmdTable.push_back(new TakeOffCommand());
    cmdTable.push_back(new LandCommand());
    cmdTable.push_back(new MissionCommand());
    cmdTable.push_back(new LoiterCommand());
    cmdTable.push_back(new CapabilitiesCommand());
    cmdTable.push_back(new RtlCommand());
    cmdTable.push_back(new GetParamsCommand());
    cmdTable.push_back(new GetSetParamCommand());
    cmdTable.push_back(new StatusCommand());
    cmdTable.push_back(new PositionCommand());
    cmdTable.push_back(new HilCommand());
    cmdTable.push_back(new FakeGpsCommand());
    cmdTable.push_back(new RequestImageCommand());
    cmdTable.push_back(new FtpCommand());
    cmdTable.push_back(new PlayLogCommand());
    cmdTable.push_back(new DumpLogCommandsCommand());
    cmdTable.push_back(nshCommand);
    // this is advanced command that can get us into trouble on real drone, so remove it for now.
    //cmdTable.push_back(new AltHoldCommand());
    cmdTable.push_back(sendImage = new SendImageCommand());
    cmdTable.push_back(new SetMessageIntervalCommand());
    cmdTable.push_back(new BatteryCommand());
    cmdTable.push_back(new WaitForAltitudeCommand());

    droneConnection->subscribe([=](std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& message) {
        MavLinkStatustext statustext;
        if (inLogFile != nullptr && inLogFile->isOpen()) {
            std::lock_guard<std::mutex> lock(logLock);
            inLogFile->write(message);
        }
        switch (message.msgid) {
        case MavLinkHeartbeat::kMessageId:
            CheckHeartbeat(message);
            break;
        case MavLinkAttitudeTarget::kMessageId:
            /*
            MavLinkAttitudeTarget target;
            target.decode(message);

            float pitch, roll, yaw;
            mavlink_quaternion_to_euler(target.q, &roll, &pitch, &yaw);
            float q2[4];
            mavlink_euler_to_quaternion(roll, pitch, yaw, q2);*/

            //DebugOutput("q1 : %f\t%f\t%f\t%g", target.q[0], target.q[1], target.q[2], target.q[3]);
            //DebugOutput("q2 : %f\t%f\t%f\t%g", q2[0], q2[1], q2[2], q2[3]);
            //DebugOutput("target roll: %f\tpitch: %f\tyaw:%f\tthrust: %f", roll, pitch, yaw, target.thrust);
            break;
        case MavLinkStatustext::kMessageId: // MAVLINK_MSG_ID_STATUSTEXT:
            statustext.decode(message);
            handleStatus(statustext);
            break;
        default:
            break;
        }
    });

    if (logConnection != nullptr) {
        logViewer = std::make_shared<MavLinkNode>(LocalLogViewerSystemId, LocalComponentId);
        logViewer->connect(logConnection);
        orbit->setLogViewer(logViewer);
        sendImage->setLogViewer(logViewer);
    }

    // this stops us from being able to connect to SITL mode PX4.
    //checkPulse();

    int retries = 0;
    while (retries++ < 5) {
        try {
            if (mavLinkVehicle->isLocalControlSupported()) {
                cmdTable.push_back(new GotoCommand());
                cmdTable.push_back(new RotateCommand());
                cmdTable.push_back(orbit);
                cmdTable.push_back(new SquareCommand());
                cmdTable.push_back(new WiggleCommand());
            }
            break;
        }
        catch (std::exception& e) {
            printf("isLocalControlSupported failed: %s\n", e.what());
        }
    }

    if (noRadio) {
        MavLinkParameter p = mavLinkVehicle->getCachedParameter("NAV_RCL_ACT");
        if (p.value != 0) {
            p.value = 0;
            mavLinkVehicle->setParameter(p);
        }
    }

    if (nsh) {
        currentCommand = nshCommand;
        currentCommand->Execute(mavLinkVehicle);
    }
    else {
        if (!noparams) {
            printf("Downloading drone parameters so we know how to control it properly...\n");
            try {
                mavLinkVehicle->getParamList();
            }
            catch (std::exception& e) {
                printf("%s\n", e.what());
            }
        }
        mavLinkVehicle->setStabilizedFlightMode();
    }

    printf("Ready...\n");
    script << "status\n";

    while (!std::cin.eof()) {

        if (!script.eof()) {
            std::getline(script, line);
        }
        else {
            printf("mavlink> ");
            std::getline(std::cin, line);
        }

        line = mavlink_utils::Utils::trim(line, ' ');

        if (line.length() > 0) {
            if (line.length() == 0) {
                continue;
            }
            std::vector<std::string> args = Command::parseArgs(line);
            std::string cmd = args[0];

            if (cmd == "x") {
                break;
            }
            else if (cmd == "disconnect") {
                mavLinkVehicle->close();
            }
            else if (cmd == "connect") {
                connect();
            }
            else if (cmd == "?" || cmd == "help") {
                for (size_t i = 0; i < cmdTable.size(); i++) {
                    Command* command = cmdTable[i];
                    if (args.size() > 1 && args[1] == command->Name) {
                        command->PrintHelp();
                        break;
                    }
                    else {
                        printf("%s\n", command->Name.c_str());
                    }
                }
            }
            else {
                Command* selected = Command::create(args);
                //add command text in log
                if (selected != nullptr && inLogFile != nullptr && inLogFile->isOpen()) {
                    auto str = std::string(Command::kCommandLogPrefix) + line;
                    MavLinkStatustext st;
                    strncpy(st.text, str.c_str(), 50);
                    MavLinkMessage m;
                    st.encode(m);
                    droneConnection->prepareForSending(m);
                    std::lock_guard<std::mutex> lock(logLock);
                    inLogFile->write(m);
                }

                if (currentCommand != nullptr && currentCommand != selected) {
                    // close previous command.
                    currentCommand->Close();
                }
                currentCommand = selected;

                if (currentCommand != NULL) {
                    try {
                        currentCommand->Execute(mavLinkVehicle);
                    }
                    catch (const std::exception& e) {
                        const char* reason = e.what();
                        if (reason == nullptr) {
                            reason = "(unknown)";
                        }
                        printf("Error: %s\n", reason);
                    }
                }
                else {
                    printf("Unknown command.  Type '?' to get list of commands\n");
                }
            }
        }
    }

    stopTelemetry();
    logViewer = nullptr;
    droneConnection = nullptr;
    logConnection = nullptr;
    mavLinkVehicle = nullptr;
    CloseLogFiles();
    return 0;
}

void completion(int state)
{
}

int main(int argc, const char* argv[])
{
    if (!ParseCommandLine(argc, argv)) {
        PrintUsage();
        return 1;
    }

#if defined(USE_CPP_FILESYSTEM)
    if (convertExisting) {
        if (jsonLogFormat) {
            ConvertLogFilesToJson(logDirectory);
        }
        else if (csvLogFormat) {
            ConvertLogFilesToCsv(logDirectory);
        }
        else {
            //FilterLogFiles(logDirectory);
        }
        return 0;
    }
#endif

    OpenLogFiles();

    if (serial) {
        if (comPort.size() == 0 || comPort == "*") {
            comPort = findPixhawk();
            if (comPort == "") {
                printf("### Error: PX4 not found on your SerialPort, or it is not available");
                return 1;
            }
        }
    }

    try {
        if (unitTest) {
            UnitTests test;
            test.RunAll(comPort, baudRate);
            return 0;
        }

        if (!connect()) {
            return 1;
        }

        if (!setupDrone()) {
            return 1;
        }

        return console(initScript);
    }
    catch (const std::exception& e) {
        printf("Exception: %s\n", e.what());
    }

    CloseLogFiles();
}
