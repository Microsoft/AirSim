// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "MavLinkConnection.hpp"
#include "Utils.hpp"

#include <cstdio>
#include <iostream>
#include <memory>
#include <sstream>
#include <locale>
#include <codecvt>

using namespace mavlinkcom;
using namespace mavlinkcom_impl;

std::vector<SerialPortInfo> MavLinkConnection::findSerialPorts(int vid, int pid)
{
    unused(vid); // avoid warning: unused parameter
    unused(pid); // avoid warning: unused parameter
    // todo: alternative:  probably need to do an lstat on '/dev/serial/by-id' and find
    // something that looks like PX4 and return that name, or follow the symbolic link to /dev/ttyACM0...

    std::string bash_command =
        "bash -c '"
        "for sysdevpath in $(find /sys/bus/usb/devices/usb*/ -name dev); do "
        "syspath=\"${sysdevpath%/dev}\"; devname=\"$(udevadm info -q name -p $syspath)\"; "
        "[[ \"$devname\" == \"bus/\"* ]] && continue; "
        "eval \"$(udevadm info -q property --export -p $syspath)\"; "
        "[[ -z \"$ID_SERIAL\" ]] && continue; "
        "echo \"/dev/$devname \"$ID_SERIAL\"\"; "
        "done"
        "'";

    std::vector<SerialPortInfo> ports;
    std::string command_result;
    {
        std::array<char, 128> buffer;
        std::shared_ptr<FILE> pipe(popen(bash_command.c_str(), "r"), pclose);
        if (!pipe) throw std::runtime_error("popen() failed!");
        while (!feof(pipe.get())) {
            if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
                command_result += buffer.data();
        }
    }

    {
        std::stringstream ss(command_result);
        std::string port_name, display_name;
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

        while (std::getline(ss, port_name, ' ') && std::getline(ss, display_name)) {
            SerialPortInfo portInfo;
            portInfo.pid = 0;
            portInfo.vid = 0;
            portInfo.displayName = converter.from_bytes(display_name);
            portInfo.portName = converter.from_bytes(port_name);

            ports.push_back(portInfo);
        }
    }

    return ports;
}
