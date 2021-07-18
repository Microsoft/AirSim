// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <iostream>
#include <string>
#include "vehicles/multirotor/api/MultirotorRpcLibServer.hpp"
#include "vehicles/multirotor/firmwares/mavlink/MavLinkMultirotorApi.hpp"
#include "common/Settings.hpp"

using namespace std;
using namespace msr::airlib;

/*
    This is a sample code demonstrating how to deploy rpc server on-board
    real drone so we can use same APIs on real vehicle that we used in simulation.
    This demonstration is designed for PX4 powered drone.
*/

int main(int argc, const char* argv[])
{
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " is_simulation" << std::endl;
        std::cout << "\t where is_simulation = 0 or 1" << std::endl;
        cout << "Start the DroneServer using the 'PX4' settings in ~/Documents/AirSim/settings.json." << endl;
        return 1;
    }

    bool is_simulation = std::atoi(argv[1]) == 1;
    if (is_simulation)
        std::cout << "You are running in simulation mode." << std::endl;
    else
        std::cout << "WARNING: This is not simulation!" << std::endl;

    AirSimSettings::MavLinkConnectionInfo connection_info;

    // read settings and override defaults
    auto settings_full_filepath = Settings::getUserDirectoryFullPath("settings.json");
    Settings& settings = Settings::singleton().loadJSonFile(settings_full_filepath);
    Settings child;
    if (settings.isLoadSuccess()) {
        Settings vehicles;
        if (settings.hasKey("Vehicles")) {
            settings.getChild("Vehicles", vehicles);
            vehicles.getChild("PX4", child);
        }
        else {
            settings.getChild("PX4", child);
        }

        // allow json overrides on a per-vehicle basis.
        connection_info.sim_sysid = static_cast<uint8_t>(child.getInt("SimSysID", connection_info.sim_sysid));
        connection_info.sim_compid = child.getInt("SimCompID", connection_info.sim_compid);

        connection_info.vehicle_sysid = static_cast<uint8_t>(child.getInt("VehicleSysID", connection_info.vehicle_sysid));
        connection_info.vehicle_compid = child.getInt("VehicleCompID", connection_info.vehicle_compid);

        connection_info.offboard_sysid = static_cast<uint8_t>(child.getInt("OffboardSysID", connection_info.offboard_sysid));
        connection_info.offboard_compid = child.getInt("OffboardCompID", connection_info.offboard_compid);

        connection_info.logviewer_ip_address = child.getString("LogViewerHostIp", connection_info.logviewer_ip_address);
        connection_info.logviewer_ip_port = child.getInt("LogViewerPort", connection_info.logviewer_ip_port);
        connection_info.logviewer_ip_sport = child.getInt("LogViewerSendPort", connection_info.logviewer_ip_sport);

        connection_info.qgc_ip_address = child.getString("QgcHostIp", connection_info.qgc_ip_address);
        connection_info.qgc_ip_port = child.getInt("QgcPort", connection_info.qgc_ip_port);

        connection_info.control_ip_address = child.getString("ControlIp", connection_info.control_ip_address);
        connection_info.control_port_local = child.getInt("ControlPort", connection_info.control_port_local);
        connection_info.control_port_local = child.getInt("ControlPortLocal", connection_info.control_port_local);
        connection_info.control_port_remote = child.getInt("ControlPortRemote", connection_info.control_port_remote);

        connection_info.local_host_ip = child.getString("LocalHostIp", connection_info.local_host_ip);

        connection_info.use_serial = child.getBool("UseSerial", connection_info.use_serial);
        connection_info.udp_address = child.getString("UdpIp", connection_info.udp_address);
        connection_info.udp_port = child.getInt("UdpPort", connection_info.udp_port);
        connection_info.use_tcp = child.getBool("UseTcp", connection_info.use_tcp);
        connection_info.tcp_port = child.getInt("TcpPort", connection_info.tcp_port);
        connection_info.serial_port = child.getString("SerialPort", connection_info.serial_port);
        connection_info.baud_rate = child.getInt("SerialBaudRate", connection_info.baud_rate);
        connection_info.model = child.getString("Model", connection_info.model);
        connection_info.logs = child.getString("Logs", connection_info.logs);
    }
    else {
        std::cout << "Could not load settings from " << Settings::singleton().getFullFilePath() << std::endl;
        return 3;
    }

    MavLinkMultirotorApi api;
    api.initialize(connection_info, nullptr, is_simulation);
    api.reset();

    ApiProvider api_provider(nullptr);
    api_provider.insert_or_assign("", &api, nullptr);
    msr::airlib::MultirotorRpcLibServer server(&api_provider, connection_info.local_host_ip);

    //start server in async mode
    server.start(false, 4);

    if (connection_info.use_tcp) {
        std::cout << "Server connected to MavLink TCP endpoint at " << connection_info.local_host_ip << ":" << connection_info.tcp_port << std::endl;
    }
    else {
        std::cout << "Server connected to MavLink UDP endpoint at " << connection_info.local_host_ip << ":" << connection_info.udp_port << std::endl;
    }
    std::cout << "Hit Ctrl+C to terminate." << std::endl;

    std::vector<std::string> messages;
    while (true) {
        //check messages
        api.getStatusMessages(messages);
        if (messages.size() > 1) {
            for (const auto& message : messages) {
                std::cout << message << std::endl;
            }
        }

        constexpr static std::chrono::milliseconds MessageCheckDurationMillis(100);
        std::this_thread::sleep_for(MessageCheckDurationMillis);

        api.sendTelemetry();
    }

    return 0;
}
