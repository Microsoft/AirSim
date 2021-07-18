#include "SimJoyStick.h"

#if defined _WIN32 || defined _WIN64

#include "DirectInputJoystick.h"

struct SimJoyStick::impl
{
public:
    void getJoyStickState(int index, SimJoyStick::State& state, const AxisMaps& maps)
    {
        if (index >= kMaxControllers) {
            state.is_initialized = false;
            return;
        }

        if (controllers_[index] == nullptr) {
            controllers_[index].reset(new DirectInputJoyStick());
            if (!controllers_[index]->initialize(index)) {
                state.is_initialized = false;
                state.message = controllers_[index]->getState(false).message;
                return;
            }
            state.is_initialized = true;
        }

        const DirectInputJoyStick::JoystickState& di_state = controllers_[index]->getState();
        const DirectInputJoyStick::JoystickInfo& joystick_info = controllers_[index]->getJoystickInfo();

        state.is_valid = di_state.is_valid;

        state.left_x = getAxisValue(AxisMap::AxisType::LeftX, maps.left_x, di_state, joystick_info.pid_vid);
        state.left_y = getAxisValue(AxisMap::AxisType::LeftY, maps.left_y, di_state, joystick_info.pid_vid);
        state.right_x = getAxisValue(AxisMap::AxisType::RightX, maps.right_x, di_state, joystick_info.pid_vid);
        state.right_y = getAxisValue(AxisMap::AxisType::RightY, maps.right_y, di_state, joystick_info.pid_vid);
        state.right_z = getAxisValue(AxisMap::AxisType::RightZ, maps.right_z, di_state, joystick_info.pid_vid);
        state.left_z = getAxisValue(AxisMap::AxisType::LeftZ, maps.left_z, di_state, joystick_info.pid_vid);

        state.slider0 = static_cast<float>(di_state.slider0);
        state.slider1 = static_cast<float>(di_state.slider1);

        state.pov0 = static_cast<float>(di_state.pov0);
        state.pov1 = static_cast<float>(di_state.pov1);
        state.pov2 = static_cast<float>(di_state.pov2);
        state.pov3 = static_cast<float>(di_state.pov3);

        state.pid_vid = joystick_info.pid_vid;

        state.buttons = 0;
        for (int i = 0; i < sizeof(int) * 8; ++i) {
            state.buttons |= ((di_state.buttons[i] & 0x80) == 0 ? 0 : 1) << i;
        }
    }

    void setAutoCenter(int index, double strength)
    {
        if (index >= 0)
            controllers_[index]->setAutoCenter(strength);
    }

    void setWheelRumble(int index, double strength)
    {
        if (index >= 0)
            controllers_[index]->setWheelRumble(strength);
    }

private:
    float getMappedValue(AxisMap::AxisType axis_type, const AxisMap& map, const DirectInputJoyStick::JoystickState& di_state, const std::string& device_pid_vid)
    {
        AxisMap::AxisType rc_axis;
        if (map.rc_axis == AxisMap::AxisType::Auto) {
            if (device_pid_vid == "" || device_pid_vid == "VID_0483&PID_5710") { //RCs like FrSky Taranis
                switch (axis_type) {
                case AxisMap::AxisType::LeftX:
                    rc_axis = AxisMap::AxisType::RightX;
                    break;
                case AxisMap::AxisType::LeftY:
                    rc_axis = AxisMap::AxisType::LeftX;
                    break;
                case AxisMap::AxisType::LeftZ:
                    rc_axis = AxisMap::AxisType::RightY;
                    break;
                case AxisMap::AxisType::RightX:
                    rc_axis = AxisMap::AxisType::LeftY;
                    break;
                case AxisMap::AxisType::RightY:
                    rc_axis = AxisMap::AxisType::LeftZ;
                    break;
                case AxisMap::AxisType::RightZ:
                    rc_axis = AxisMap::AxisType::RightZ;
                    break;
                default:
                    throw std::invalid_argument("Unsupported axis_type in getMappedValue");
                }
            }
            else if (device_pid_vid == "VID_0401&PID_0401") { //Flysky FS-SM100 RC USB adapter
                switch (axis_type) {
                case AxisMap::AxisType::LeftX:
                    rc_axis = AxisMap::AxisType::RightY;
                    break;
                case AxisMap::AxisType::LeftY:
                    rc_axis = AxisMap::AxisType::LeftX;
                    break;
                case AxisMap::AxisType::LeftZ:
                    rc_axis = AxisMap::AxisType::RightX;
                    break;
                case AxisMap::AxisType::RightX:
                    rc_axis = AxisMap::AxisType::LeftY;
                    break;
                case AxisMap::AxisType::RightY:
                    rc_axis = AxisMap::AxisType::LeftZ;
                    break;
                case AxisMap::AxisType::RightZ:
                    rc_axis = AxisMap::AxisType::RightZ;
                    break;
                default:
                    throw std::invalid_argument("Unsupported axis_type in getMappedValue");
                }
            }
            else { //Xbox controllers
                rc_axis = axis_type;
            }
        }
        else
            rc_axis = map.rc_axis;

        long result;
        switch (rc_axis) {
        case AxisMap::AxisType::LeftX:
            result = di_state.x;
            break;
        case AxisMap::AxisType::LeftY:
            result = di_state.y;
            break;
        case AxisMap::AxisType::LeftZ:
            result = di_state.z;
            break;
        case AxisMap::AxisType::RightX:
            result = di_state.rx;
            break;
        case AxisMap::AxisType::RightY:
            result = di_state.ry;
            break;
        case AxisMap::AxisType::RightZ:
            result = di_state.rz;
            break;
        default:
            throw std::invalid_argument("Unsupported rc_axis in getMappedValue");
        }

        return static_cast<float>(result);
    }

    float getAxisValue(AxisMap::AxisType axis_type, const AxisMap& map, const DirectInputJoyStick::JoystickState& di_state, const std::string& device_pid_vid)
    {
        float val = getMappedValue(axis_type, map, di_state, device_pid_vid);

        //normalize min to max --> 0 to 1
        val = (val - map.min_val) / (map.max_val - map.min_val);

        switch (map.direction) {
        case AxisMap::AxisDirection::Auto:
            if (
                ((device_pid_vid == "" || device_pid_vid == "VID_0483&PID_5710") &&
                 (axis_type == AxisMap::AxisType::LeftZ || axis_type == AxisMap::AxisType::RightY)) ||
                ((device_pid_vid != "" && device_pid_vid != "VID_0483&PID_5710" && device_pid_vid != "VID_0401&PID_0401") &&
                 (axis_type == AxisMap::AxisType::LeftY)))
                val = 1 - val;
            break;
        case AxisMap::AxisDirection::Normal:
            break;
        case AxisMap::AxisDirection::Reverse:
            val = 1 - val;
            break;
        default:
            throw std::invalid_argument("Unsupported map.direction in getAxisValue");
        }

        //normalize 0 to 1 --> -1 to 1
        val = 2 * val - 1;

        return val;
    }

private:
    static constexpr unsigned int kMaxControllers = 4;
    std::unique_ptr<DirectInputJoyStick> controllers_[kMaxControllers];
};

#else

#include <limits>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sstream>
#include <iostream>
//#include <libudev.h>
#include "unistd.h"

//implementation for unsupported OS
struct SimJoyStick::impl
{
private:
    class JoystickEvent
    {
    public:
        /** Minimum value of axes range */
        static const short MIN_AXES_VALUE = -32768;

        /** Maximum value of axes range */
        static const short MAX_AXES_VALUE = 32767;

        /**
       * The timestamp of the event, in milliseconds.
       */
        unsigned int time;

        /**
       * The value associated with this joystick event.
       * For buttons this will be either 1 (down) or 0 (up).
       * For axes, this will range between MIN_AXES_VALUE and MAX_AXES_VALUE.
       */
        short value;

        /**
       * The event type.
       */
        unsigned char type;

        /**
       * The axis/button number.
       */
        unsigned char number;

        /**
       * Returns true if this event is the result of a button press.
       */
        bool isButton()
        {
            static constexpr unsigned char JS_EVENT_BUTTON = 0x01; // button pressed/released
            return (type & JS_EVENT_BUTTON) != 0;
        }

        /**
       * Returns true if this event is the result of an axis movement.
       */
        bool isAxis()
        {
            static constexpr unsigned char JS_EVENT_AXIS = 0x02; // joystick moved
            return (type & JS_EVENT_AXIS) != 0;
        }

        /**
       * Returns true if this event is part of the initial state obtained when
       * the joystick is first connected to.
       */
        bool isInitialState()
        {
            static constexpr unsigned char JS_EVENT_INIT = 0x80; // initial state of device
            return (type & JS_EVENT_INIT) != 0;
        }

        /**
       * The ostream inserter needs to be a friend so it can access the
       * internal data structures.
       */
        friend std::ostream& operator<<(std::ostream& os, const JoystickEvent& e);
    };

public:
    ~impl()
    {
        if (fd_ >= 0)
            close(fd_);
    }

    static float normalizeAxisVal(int axis_val, bool wide, bool zero2One, bool reversed)
    {
        float min_val = wide ? -32768 : -16384;
        float max_val = wide ? 32767 : 16383;

        float val = (axis_val - min_val) / (max_val - min_val);
        if (zero2One) {
            if (reversed)
                val = 1 - val;
        }
        else {
            val = 2 * val - 1;
            if (reversed)
                val *= -1;
        }

        return val;
    }

    void getJoyStickState(int index, SimJoyStick::State& state, const AxisMaps& maps)
    {
        unused(maps);

        static constexpr bool blocking = false;

        //if this is new index
        if (index != last_index_) {
            //getJoystickInfo(1, manufacturerID, productID, state.message);

            //close previous one
            if (fd_ >= 0)
                close(fd_);

            //open new device
            std::stringstream devicePath;
            devicePath << "/dev/input/js" << index;

            fd_ = open(devicePath.str().c_str(), blocking ? O_RDONLY : O_RDONLY | O_NONBLOCK);
            state.is_initialized = fd_ >= 0;
            last_index_ = index;
        }

        //if open was successful
        if (fd_ >= 0) {
            //read the device
            int bytes = read(fd_, &event_, sizeof(event_));

            //if we didn't had valid read
            if (bytes == -1 || bytes != sizeof(event_)) {
                // NOTE if this condition is not met, we're probably out of sync and this
                // Joystick instance is likely unusable
                //TODO: set below to false?
                //state.is_valid = false;
            }
            else {
                state.is_valid = true;

                if (event_.isButton()) {
                    if (event_.value == 0)
                        state.buttons &= ~(1 << event_.number);
                    else
                        state.buttons |= (1 << event_.number);
                }
                else if (event_.isAxis()) {
                    if (device_type > 0) { //RCs like FrSky Taranis
                        switch (event_.number) {
                        case 0:
                            state.left_y = event_.value;
                            break;
                        case 1:
                            state.right_x = event_.value;
                            break;
                        case 2:
                            state.right_y = event_.value;
                            break;
                        case 3:
                            state.left_x = event_.value;
                            break;
                        default:
                            break;
                        }
                    }
                    else { //XBox
                        switch (event_.number) {
                        case 0:
                            state.left_x = normalizeAxisVal(event_.value, true, false, false);
                            break;
                        case 1:
                            state.left_y = normalizeAxisVal(event_.value, true, false, true);
                            break;
                        case 2:
                            state.left_z = normalizeAxisVal(event_.value, true, false, false);
                            break;
                        case 3:
                            state.right_x = normalizeAxisVal(event_.value, true, false, false);
                            break;
                        case 4:
                            state.right_y = normalizeAxisVal(event_.value, true, false, false);
                            break;
                        case 5:
                            state.right_z = normalizeAxisVal(event_.value, true, false, false);
                            break;

                        default:
                            break;
                        }
                    }
                }
                //else ignore
            }
        }
        else
            state.is_valid = false;
    }

    void setAutoCenter(unsigned int index, double strength)
    {
        unused(index);
        unused(strength);
        //TODO: implement this for linux
    }

    void setWheelRumble(unsigned int index, double strength)
    {
        unused(index);
        unused(strength);
        //TODO: implement this for linux
    }

    // bool getJoystickInfo(int index, std::string& manufacturerID, std::string& productID, std::string& message)
    // {
    //     manufacturerID = productID = "";
    //     // Use udev to look up the product and manufacturer IDs
    //     struct udev *udev = udev_new();
    //     if (udev) {
    //         char sysname[32];
    //         std::snprintf(sysname, sizeof(sysname), "js%u", index);
    //         struct udev_device *dev = udev_device_new_from_subsystem_sysname(udev, "input", sysname);
    //         dev = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
    //         if (!dev)
    //         {
    //             message = "Unable to find parent USB device";
    //             return false;
    //         }

    //         std::stringstream ss;
    //         ss << std::hex << udev_device_get_sysattr_value(dev, "idVendor");
    //         ss >> manufacturerID;

    //         ss.clear();
    //         ss.str("");
    //         ss << std::hex << udev_device_get_sysattr_value(dev, "idProduct");
    //         ss >> productID;

    //         udev_device_unref(dev);
    //     }
    //     else
    //     {
    //         message = "Cannot create udev";
    //         return false;
    //     }
    //     udev_unref(udev);
    //     return true;
    // }

private:
    int last_index_ = -1;
    int fd_ = -1;
    JoystickEvent event_;
    std::string manufacturerID, productID;
    int device_type = 0;
};

#endif

SimJoyStick::SimJoyStick()
{
    pimpl_.reset(new impl());
}
SimJoyStick::~SimJoyStick()
{
    //required for pimpl
}

void SimJoyStick::getJoyStickState(int index, SimJoyStick::State& state) const
{
    if (index < 0) {
        state.is_initialized = false;
        state.is_valid = false;
        return;
    }

    //TODO: anyway to workaround const_cast?
    const_cast<SimJoyStick*>(this)->pimpl_->getJoyStickState(index, state, axis_maps);
}

void SimJoyStick::setAutoCenter(int index, double strength)
{
    pimpl_->setAutoCenter(index, strength);
}

void SimJoyStick::setWheelRumble(int index, double strength)
{
    pimpl_->setWheelRumble(index, strength);
}