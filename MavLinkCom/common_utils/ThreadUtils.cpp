#include "ThreadUtils.hpp"
#include "StrictMode.hpp"
#include <codecvt>

#ifdef _WIN32
#include <windows.h> // SetThreadPriority and GetCurrentThread
#else
#include <pthread.h>
#endif

using namespace mavlink_utils;

// make the current thread run with maximum priority.
bool CurrentThread::setMaximumPriority()
{
#ifdef _WIN32
    HANDLE thread = GetCurrentThread();
    // THREAD_PRIORITY_HIGHEST is too high and makes animation a bit jumpy.
    int rc = SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    if (rc == 0) {
        rc = GetLastError();
        return false;
    }
    return true;
#elif defined(__APPLE__)
    // TODO: How to handle POSIX thread priorities on OSX?
    return true;
#else
    int policy;
    struct sched_param param;
    int err = pthread_getschedparam(pthread_self(), &policy, &param);
    if (err != 0) return false;
    int maxPriority = sched_get_priority_max(policy);
    err = pthread_setschedprio(pthread_self(), maxPriority);
    return err == 0;
#endif
}

#ifdef _WIN32
typedef HRESULT(WINAPI* SetThreadDescriptionFunction)(_In_ HANDLE hThread, _In_ PCWSTR lpThreadDescription);
static SetThreadDescriptionFunction setThreadDescriptionFunction = nullptr;
#endif

// setThreadName is a helper function that is useful when debugging because your threads
// show up in the debugger with the name you set which makes it easier to find the threads
// that you are interested in.
bool CurrentThread::setThreadName(const std::string& name)
{
#ifdef _WIN32
    // unfortunately this is only available on Windows 10, and AirSim is not limited to that.
    if (setThreadDescriptionFunction == nullptr) {
        HINSTANCE hGetProcIDDLL = LoadLibrary(L"Kernel32");
        FARPROC func = GetProcAddress(hGetProcIDDLL, "SetThreadDescription");
        if (func != nullptr) {
            setThreadDescriptionFunction = (SetThreadDescriptionFunction)func;
        }
    }
    if (setThreadDescriptionFunction != nullptr) {
        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
        std::wstring wide_path = converter.from_bytes(name);
        auto rc = (*setThreadDescriptionFunction)(GetCurrentThread(), wide_path.c_str());
        return S_OK == rc;
    }
    return false;
#elif defined(__APPLE__)
    return 0 == pthread_setname_np(name.c_str());
#else
    return 0 == pthread_setname_np(pthread_self(), name.c_str());
#endif
}
