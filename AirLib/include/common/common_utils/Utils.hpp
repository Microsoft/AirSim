// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef common_utils_Utils_hpp
#define common_utils_Utils_hpp

#include "StrictMode.hpp"
#include <chrono>
#include <thread>
#include <memory>
#include <string>
#include <cstdarg>
#include <cstring>
#include <array>
#include <sstream>
#include <fstream>
#include <vector>
#include <ctime>
#include <random>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <bitset>
#include "type_utils.hpp"

#ifndef _WIN32
#include <limits.h> // needed for CHAR_BIT used below
#endif

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <math.h>
//#include <cmath>

#ifndef M_PIf
#define M_PIf static_cast<float>(3.1415926535897932384626433832795028841972)
#endif

#ifndef M_PI
#define M_PI static_cast<double>(3.1415926535897932384626433832795028841972)
#endif

#ifndef M_PIl
#define M_PIl static_cast<long double>(3.1415926535897932384626433832795028841972)
#endif

#define EARTH_RADIUS (6378137.0f)

/*
    This file is collection of routines that can be included in ANY project just
    by dropping in common_utils.hpp. Therefore there should not be any dependency 
    in the code below other than STL. The code should be able to compilable on
    all major platforms.
*/

#ifndef _MSC_VER
__attribute__((__format__(__printf__, 1, 0))) static int _vscprintf(const char* format, va_list pargs)
{
    int retval;
    va_list argcopy;
    va_copy(argcopy, pargs);
    IGNORE_FORMAT_STRING_ON
    retval = vsnprintf(NULL, 0, format, argcopy);
    IGNORE_FORMAT_STRING_OFF
    va_end(argcopy);
    return retval;
}
#endif

// Call this on a function parameter to suppress the unused paramter warning
template <class T>
inline void unused(T const& result)
{
    static_cast<void>(result);
}

namespace common_utils
{

class Utils
{
private:
    typedef std::chrono::system_clock system_clock;
    typedef std::chrono::steady_clock steady_clock;
    typedef std::string string;
    typedef std::stringstream stringstream;
    //this is not required for most compilers
    typedef unsigned int uint;
    template <typename T>
    using time_point = std::chrono::time_point<T>;

public:
    class Logger
    {
    public:
        virtual void log(int level, const std::string& message)
        {
            if (level >= 0)
                std::cout << message << std::endl;
            else
                std::cerr << message << std::endl;
        }
    };

    static void enableImmediateConsoleFlush()
    {
        //disable buffering
        setbuf(stdout, NULL);
    }

    template <typename T>
    static T getRandomFromGaussian(T stddev = 1, T mean = 0)
    {
        static std::default_random_engine random_gen;
        static std::normal_distribution<T> gaussian_dist(0.0f, 1.0f);

        return gaussian_dist(random_gen) * stddev + mean;
    }

    static constexpr double degreesToRadians(double degrees)
    {
        return static_cast<double>(M_PIl * degrees / 180.0);
    }
    static constexpr float degreesToRadians(float degrees)
    {
        return static_cast<float>(M_PI * degrees / 180.0f);
    }
    static constexpr double radiansToDegrees(double radians)
    {
        return static_cast<double>(radians * 180.0 / M_PIl);
    }
    static constexpr float radiansToDegrees(float radians)
    {
        return static_cast<float>(radians * 180.0f / M_PI);
    }

    static bool startsWith(const string& s, const string& prefix)
    {
        return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
    }

    template <template <class, class, class...> class TContainer, typename TKey, typename TVal, typename... Args>
    static const TVal& findOrDefault(const TContainer<TKey, TVal, Args...>& m, TKey const& key, const TVal& default_val)
    {
        typename TContainer<TKey, TVal, Args...>::const_iterator it = m.find(key);
        if (it == m.end())
            return default_val;
        return it->second;
    }

    template <template <class, class, class...> class TContainer, typename TKey, typename TVal, typename... Args>
    static const TVal& findOrDefault(const TContainer<TKey, TVal, Args...>& m, TKey const& key)
    {
        static TVal default_val;
        return findOrDefault(m, key, default_val);
    }

    static Logger* getSetLogger(Logger* logger = nullptr)
    {
        static Logger logger_default_;
        static Logger* logger_;

        if (logger != nullptr)
            logger_ = logger;
        else if (logger_ == nullptr)
            logger_ = &logger_default_;

        return logger_;
    }

    static constexpr int kLogLevelInfo = 0;
    static constexpr int kLogLevelWarn = -1;
    static constexpr int kLogLevelError = -2;
    static void log(std::string message, int level = kLogLevelInfo)
    {
        if (level >= getSetMinLogLevel())
            getSetLogger()->log(level, message);
    }
    static int getSetMinLogLevel(bool set_or_get = false,
                                 int set_min_log_level = std::numeric_limits<int>::min())
    {
        static int min_log_level = std::numeric_limits<int>::min();
        if (set_or_get)
            min_log_level = set_min_log_level;

        return min_log_level;
    }

    template <typename T>
    static int sign(T val)
    {
        return T(0) < val ? 1 : (T(0) > val ? -1 : 0);
    }

    /// Limits absolute value whole preserving sign
    template <typename T>
    static T limitAbsValue(T val, T min_value, T max_value)
    {
        T val_abs = std::abs(val);
        T val_limited = std::max(val_abs, min_value);
        val_limited = std::min(val_limited, max_value);
        return sign(val) * val_limited;
    }

    /// Limits absolute value whole preserving sign
    template <typename T>
    static T clip(T val, T min_value, T max_value)
    {
        return std::max(min_value, std::min(val, max_value));
    }

    template <typename Range>
    static const string printRange(Range&& range, const string& delim = ", ",
                                   const string& prefix = "(", const string& suffix = ")")
    {
        return printRange(std::begin(range), std::end(range), delim, prefix, suffix);
    }
    template <typename Iterator>
    static const string printRange(Iterator start, Iterator last, const string& delim = ", ",
                                   const string& prefix = "(", const string& suffix = ")")
    {
        stringstream ss;
        ss << prefix;

        for (Iterator i = start; i != last; ++i) {
            if (i != start)
                ss << delim;
            ss << *i;
        }

        ss << suffix;
        return ss.str();
    }

    static std::string getFileExtension(const string& str)
    {
        int len = static_cast<int>(str.size());
        const char* ptr = str.c_str();
        int i = 0;
        for (i = len - 1; i >= 0; i--) {
            if (ptr[i] == '.')
                break;
        }
        if (i < 0) return "";
        return str.substr(i, len - i);
    }

#ifndef _MSC_VER
    __attribute__((__format__(__printf__, 1, 0)))
#endif
    static string
    stringf(const char* format, ...)
    {
        va_list args;
        va_start(args, format);

        IGNORE_FORMAT_STRING_ON
        auto size = _vscprintf(format, args) + 1U;
        IGNORE_FORMAT_STRING_OFF
        std::unique_ptr<char[]> buf(new char[size]);

#ifndef _MSC_VER
        IGNORE_FORMAT_STRING_ON
        vsnprintf(buf.get(), size, format, args);
        IGNORE_FORMAT_STRING_OFF
#else
        vsnprintf_s(buf.get(), size, _TRUNCATE, format, args);
#endif

        va_end(args);

        return string(buf.get());
    }

    static string trim(const string& str, char ch)
    {
        int len = static_cast<int>(str.size());
        const char* ptr = str.c_str();
        int i = 0;
        for (i = 0; i < len; i++) {
            if (ptr[i] != ch)
                break;
        }
        int j = 0;
        for (j = len - 1; j >= i; j--) {
            if (ptr[j] != ch)
                break;
        }
        if (i > j) return "";
        return str.substr(i, j - i + 1);
    }
    static std::vector<std::string> split(const string& s, const char* splitChars, int numSplitChars)
    {
        auto start = s.begin();
        std::vector<string> result;
        for (auto it = s.begin(); it != s.end(); it++) {
            char ch = *it;
            bool split = false;
            for (int i = 0; i < numSplitChars; i++) {
                if (ch == splitChars[i]) {
                    split = true;
                    break;
                }
            }
            if (split) {
                if (start < it) {
                    result.push_back(string(start, it));
                }
                start = it;
                start++;
            }
        }
        if (start < s.end()) {
            result.push_back(string(start, s.end()));
        }
        return result;
    }

    // split a line into tokens using any of the given separators as token separators.
    // this method also understands quoted string literals (either single or double quotes) and returns the
    // quoted value without the quotes, and this value can contain separators.
    static std::vector<std::string> tokenize(const std::string& line, const char* separators, int numSeparators)
    {
        auto start = line.begin();
        std::vector<std::string> result;
        auto end = line.end();
        for (auto it = line.begin(); it != end;) {
            bool split = false;
            char ch = *it;
            if (ch == '\'' || ch == '"') {
                // skip quoted literal
                if (start < it) {
                    result.push_back(string(start, it));
                }
                it++;
                start = it;
                for (; it != end; it++) {
                    if (*it == ch) {
                        break;
                    }
                }
                split = true;
            }
            else {
                for (int i = 0; i < numSeparators; i++) {
                    if (ch == separators[i]) {
                        split = true;
                        break;
                    }
                }
            }
            if (split) {
                if (start < it) {
                    result.push_back(string(start, it));
                }
                start = it;
                if (start < end) start++;
            }
            if (it != end) {
                it++;
            }
        }
        if (start < end) {
            result.push_back(string(start, end));
        }
        return result;
    }

    static string toLower(const string& str)
    {
        auto len = str.size();
        std::unique_ptr<char[]> buf(new char[len + 1U]);
        str.copy(buf.get(), len, 0);
        buf[len] = '\0';
#ifdef _WIN32
        _strlwr_s(buf.get(), len + 1U);
#else
        char* p = buf.get();
        for (int i = len; i > 0; i--) {
            *p = tolower(*p);
            p++;
        }
        *p = '\0';
#endif
        string lower = buf.get();
        return lower;
    }
    //http://stackoverflow.com/a/28703383/207661
    template <typename R>
    static constexpr R bitmask(unsigned int const onecount)
    {
        //  return (onecount != 0)
        //      ? (static_cast<R>(-1) >> ((sizeof(R) * CHAR_BIT) - onecount))
        //      : 0;
        return static_cast<R>(-(onecount != 0)) & (static_cast<R>(-1) >> ((sizeof(R) * CHAR_BIT) - onecount));
    }

    static void cleanupThread(std::thread& th)
    {
        if (th.joinable()) {
            Utils::log("thread was cleaned up!", kLogLevelWarn);
            th.detach();
        }
    }

    static inline int floorToInt(float x)
    {
        return static_cast<int>(std::floor(x));
    }

    template <typename T>
    static constexpr T nan()
    {
        return std::numeric_limits<T>::quiet_NaN();
    }
    template <typename T>
    static constexpr T max()
    {
        return std::numeric_limits<T>::max();
    }
    template <typename T>
    static constexpr T min()
    {
        return std::numeric_limits<T>::min();
    }

    template <typename T>
    static void setValue(T arr[], size_t length, const T& val)
    {
        std::fill(arr, arr + length, val);
    }

    template <typename T, size_t N>
    static void setValue(T (&arr)[N], const T& val)
    {
        std::fill(arr, arr + N, val);
    }

    template <class T, size_t N>
    static std::size_t length(const T (&)[N])
    {
        return N;
    }

    static void saveToFile(string file_name, const char* data, uint size)
    {
        std::ofstream file(file_name, std::ios::binary);
        file.write(data, size);
    }
    template <typename Container>
    static typename std::enable_if<type_utils::is_container<Container>::value, void>::type
    append(Container& to, const Container& from)
    {
        using std::begin;
        using std::end;
        to.insert(end(to), begin(from), end(from));
    }
    template <typename Container>
    static typename std::enable_if<type_utils::is_container<Container>::value, void>::type
    copy(const Container& from, Container& to)
    {
        using std::begin;
        using std::end;
        std::copy(begin(from), end(from), begin(to));
    }
    template <typename T>
    static void copy(const T* from, T* to, uint count)
    {
        std::copy(from, from + count, to);
    }

    static const char* to_string(time_point<steady_clock> t)
    {
        time_t tt = system_clock::to_time_t(std::chrono::time_point_cast<system_clock::duration>(system_clock::now() + (t - steady_clock::now())));
        return ctime(&tt);
    }

    static time_point<system_clock> now()
    {
        return system_clock::now();
    }

    static std::time_t to_time_t(const std::string& str, bool is_dst = false, const std::string& format = "%Y-%m-%d %H:%M:%S")
    {
        std::tm t;
        t.tm_isdst = is_dst ? 1 : 0;
        std::istringstream ss(str);
        ss >> std::get_time(&t, format.c_str());
        return mktime(&t);

        /* GCC doesn't implement put_time yet
        stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d-%H-%M-%S");
        return ss.str();
        */
    }

    static string to_string(time_t tt, const char* format = "%Y-%m-%d-%H-%M-%S")
    {
        char str[1024];
        if (std::strftime(str, sizeof(str), format, std::localtime(&tt)))
            return string(str);
        else
            return string();
    }

    static string to_string(time_point<system_clock> time, const char* format = "%Y-%m-%d-%H-%M-%S")
    {
        time_t tt = system_clock::to_time_t(time);
        char str[1024];
        if (std::strftime(str, sizeof(str), format, std::localtime(&tt)))
            return string(str);
        else
            return string();
    }
    static string getLogFileTimeStamp()
    {
        return to_string(now(), "%Y%m%d%H%M%S");
    }
    static int to_integer(std::string s)
    {
        return atoi(s.c_str());
    }
    static string getEnv(const string& var)
    {
        char* ptr = std::getenv(var.c_str());
        return ptr ? ptr : "";
    }

    static uint64_t getUnixTimeStamp(const std::time_t* t = nullptr)
    {
        //if specific time is not passed then get current time
        std::time_t st = t == nullptr ? std::time(nullptr) : *t;
        auto secs = static_cast<std::chrono::seconds>(st).count();
        return static_cast<uint64_t>(secs);
    }

    //high precision time in seconds since epoch
    static double getTimeSinceEpochSecs(std::chrono::system_clock::time_point* t = nullptr)
    {
        using Clock = std::chrono::system_clock; //high res clock has epoch since boot instead of since 1970 for VC++
        return std::chrono::duration<double>((t != nullptr ? *t : Clock::now()).time_since_epoch()).count();
    }
    static uint64_t getTimeSinceEpochNanos(std::chrono::system_clock::time_point* t = nullptr)
    {
        using Clock = std::chrono::system_clock; //high res clock has epoch since boot instead of since 1970 for VC++
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   (t != nullptr ? *t : Clock::now())
                       .time_since_epoch())
            .count();
    }

    template <typename T>
    static void clear(std::queue<T>& q, size_t max_elements = SIZE_MAX)
    {
        while (!q.empty() && max_elements > 0) {
            q.pop();
            --max_elements;
        }
    }

    template <typename T>
    static const std::vector<T>& emptyVector()
    {
        static const std::vector<T> empty_vector;
        return empty_vector;
    }

    static const std::string& emptyString()
    {
        static std::string empty = "";
        return empty;
    }

    static constexpr float kelvinToCelcius(float kelvin)
    {
        return kelvin - 273.15f;
    }
    static constexpr float celciusToKelvin(float celcius)
    {
        return celcius + 273.15f;
    }

    template <typename TReal>
    static constexpr TReal epsilon()
    {
        return std::numeric_limits<TReal>::epsilon();
    }

    //implements relative method - do not use for comparing with zero
    //use this most of the time, tolerance needs to be meaningful in your context
    template <typename TReal>
    static bool isApproximatelyEqual(TReal a, TReal b, TReal tolerance = epsilon<TReal>())
    {
        TReal diff = std::fabs(a - b);
        if (diff <= tolerance)
            return true;

        if (diff < std::fmax(std::fabs(a), std::fabs(b)) * tolerance)
            return true;

        return false;
    }

    //supply tolerance that is meaningful in your context
    //for example, default tolerance may not work if you are comparing double with float
    template <typename TReal>
    static bool isApproximatelyZero(TReal a, TReal tolerance = epsilon<TReal>())
    {
        if (std::fabs(a) <= tolerance)
            return true;
        return false;
    }

    //use this when you want to be on safe side
    //for example, don't start rover unless signal is above 1
    template <typename TReal>
    static bool isDefinitelyLessThan(TReal a, TReal b, TReal tolerance = epsilon<TReal>())
    {
        TReal diff = a - b;
        if (diff < tolerance)
            return true;

        if (diff < std::fmax(std::fabs(a), std::fabs(b)) * tolerance)
            return true;

        return false;
    }
    template <typename TReal>
    static bool isDefinitelyGreaterThan(TReal a, TReal b, TReal tolerance = epsilon<TReal>())
    {
        TReal diff = a - b;
        if (diff > tolerance)
            return true;

        if (diff > std::fmax(std::fabs(a), std::fabs(b)) * tolerance)
            return true;

        return false;
    }

    //implements ULP method
    //use this when you are only concerned about floating point precision issue
    //for example, if you want to see if a is 1.0 by checking if its within
    //10 closest representable floating point numbers around 1.0.
    template <typename TReal>
    static bool isWithinPrecisionInterval(TReal a, TReal b, unsigned int interval_size = 1)
    {
        TReal min_a = a - (a - std::nextafter(a, std::numeric_limits<TReal>::lowest())) * interval_size;
        TReal max_a = a + (std::nextafter(a, std::numeric_limits<TReal>::max()) - a) * interval_size;

        return min_a <= b && max_a >= b;
    }

    static void DebugBreak()
    {
#ifdef _MSC_VER
        __debugbreak();
#else
//TODO: Use GCC and Clang version from https://github.com/scottt/debugbreak
#endif
    }

    //convert strongly typed enum to underlying scaler types
    template <typename E>
    static constexpr typename std::underlying_type<E>::type toNumeric(E e)
    {
        return static_cast<typename std::underlying_type<E>::type>(e);
    }
    template <typename E>
    static constexpr E toEnum(typename std::underlying_type<E>::type u)
    {
        return static_cast<E>(u);
    }

    // check whether machine is little endian
    static bool isLittleEndian()
    {
        int intval = 1;
        unsigned char* uval = reinterpret_cast<unsigned char*>(&intval);
        return uval[0] == 1;
    }

    static void writePFMfile(const float* const image_data, int width, int height, const std::string& path, float scalef = 1)
    {
        std::ofstream file(path.c_str(), std::ios::binary);

        std::string bands;
        float fvalue; // scale factor and temp value to hold pixel value
        bands = "Pf"; // grayscale

        // sign of scalefact indicates endianness, see pfm specs
        if (isLittleEndian())
            scalef = -scalef;

        // insert header information
        file << bands << "\n";
        file << width << " ";
        file << height << "\n";
        file << scalef << "\n";

        if (bands == "Pf") { // handle 1-band image
            for (int i = 0; i < height; i++) {
                for (int j = 0; j < width; ++j) {
                    fvalue = image_data[i * width + j];
                    file.write(reinterpret_cast<char*>(&fvalue), sizeof(fvalue));
                }
            }
        }

        file.close();
    }

    static void writePPMfile(const uint8_t* const image_data, int width, int height, const std::string& path)
    {
        std::ofstream file(path.c_str(), std::ios::binary);

        // Header information
        file << "P6\n"; // Magic type for PPM files
        file << width << " " << height << "\n";
        file << "255\n"; // Max color value

        auto write_binary = [&file](const uint8_t& data) {
            file.write(reinterpret_cast<const char*>(&data), sizeof(data));
        };

        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                int id = (i * width + j) * 3; // Pixel index

                // Image is in BGR, write as RGB
                write_binary(image_data[id + 2]); // R
                write_binary(image_data[id + 1]); // G
                write_binary(image_data[id]); // B
            }
        }

        file.close();
    }

    template <typename T>
    static std::string toBinaryString(const T& x)
    {
        std::stringstream ss;
        ss << std::bitset<sizeof(T) * 8>(x);
        return ss.str();
    }
};

} //namespace
#endif
