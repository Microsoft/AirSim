// in header only mode, control library is not available
#ifndef AIRLIB_HEADER_ONLY
//if using Unreal Build system then include precompiled header file first
#ifdef AIRLIB_PCH
#include "AirSim.h"
#endif

#include "FileSystem.hpp"
#include <codecvt>
#include <fstream>

#ifdef _WIN32
#include <Shlobj.h>
#include <direct.h>
#include <stdlib.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/param.h> // MAXPATHLEN definition
#include <sys/stat.h> // get mkdir, stat
#endif

using namespace mavlink_utils;

// File names are unicode (std::wstring), because users can create folders containing unicode characters on both
// Windows, OSX and Linux.
std::string FileSystem::createDirectory(std::string fullPath)
{

#ifdef _WIN32
#ifndef ONECORE
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    std::wstring wide_path = converter.from_bytes(fullPath);
    int hr = CreateDirectory(wide_path.c_str(), NULL);
    if (hr == 0) {
        hr = GetLastError();
        if (hr != ERROR_ALREADY_EXISTS) {
            throw std::invalid_argument(Utils::stringf("Error creating directory, hr=%d", hr));
        }
    }
#endif
#else
    mkdir(fullPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
    return fullPath;
}

void FileSystem::remove(std::string fileName)
{

#ifdef _WIN32
    // WIN32 will create the wrong file names if we don't first convert them to UTF-16.
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    std::wstring wide_path = converter.from_bytes(fileName);
    if (!DeleteFile(wide_path.c_str())) {
        int hr = GetLastError();
        if (hr != ERROR_FILE_NOT_FOUND) {
            throw std::runtime_error(Utils::stringf("Failed to delete file '%s', error=%d.", fileName.c_str(), hr));
        }
    }
#else

    int hr = unlink(fileName.c_str());
    if (hr != 0) {
        hr = errno;
        if (hr != ENOENT) {
            throw std::runtime_error(Utils::stringf("Failed to delete file '%s', error=%d.", fileName.c_str(), hr));
        }
    }
#endif
}

std::string FileSystem::getUserDocumentsFolder()
{
#ifdef _WIN32
#ifndef ONECORE
    // Windows users can move the Documents folder to any location they want
    // SHGetFolderPath knows how to find it.
    wchar_t szPath[MAX_PATH];

    if (0 == SHGetFolderPath(NULL,
                             CSIDL_MYDOCUMENTS | CSIDL_FLAG_CREATE,
                             NULL,
                             0,
                             szPath)) {
        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
        return converter.to_bytes(szPath);
    }
#endif
// fall back in case SHGetFolderPath failed for some reason.
#endif
    return combine(getUserHomeFolder(), "Documents");
}

std::string FileSystem::getFullPath(const std::string fileName)
{
#ifdef _WIN32
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    std::wstring wide_path = converter.from_bytes(fileName);

    // convert from std::path '/' to windows backslash.
    size_t length = wide_path.size();
    for (size_t i = 0; i < length; i++) {
        if (wide_path[i] == '/') {
            wide_path[i] = kPathSeparator;
        }
    }

    wchar_t szPath[MAX_PATH];
    int len = GetFullPathName(wide_path.c_str(), MAX_PATH, szPath, NULL);
    if (len == 0) {
        int hr = GetLastError();
        throw std::runtime_error(Utils::stringf("getFullPath of '%s' failed with error=%d.", fileName.c_str(), hr));
    }

    szPath[len] = '\0';
    return converter.to_bytes(szPath);

#else

    char buf[PATH_MAX];
    char* cwd = getcwd(buf, PATH_MAX);

    std::string path = cwd;

    size_t size = fileName.size();
    if (size == 0) {
        return path;
    }
    if (fileName[0] == kPathSeparator) {
        return fileName;
    }

    return resolve(path, fileName);
#endif
}

bool FileSystem::isDirectory(const std::string path)
{
#ifdef _WIN32
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    std::wstring wide_path = converter.from_bytes(path);
    unsigned long attrib = GetFileAttributes(wide_path.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat buf;
    int rc = ::stat(path.c_str(), &buf);
    return rc == 0 && S_ISDIR(buf.st_mode);
#endif
}

bool FileSystem::exists(const std::string path)
{
#ifdef _WIN32
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    std::wstring wide_path = converter.from_bytes(path);
    unsigned long attrib = GetFileAttributes(wide_path.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES);
#else
    struct stat buf;
    int rc = ::stat(path.c_str(), &buf);
    return rc == 0;
#endif
}
#endif