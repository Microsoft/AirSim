// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef common_utils_FileSystem_hpp
#define common_utils_FileSystem_hpp

#include <codecvt>
#include <fstream>
#include <string>
#include "Utils.hpp"

// This defines a default folder name for all the files created by AirLib so they
// are all gathered nicely in one place in the user's documents folder.
#ifndef ProductFolderName
#define ProductFolderName "AirSim"
#endif

namespace mavlink_utils
{
class FileSystem
{
    typedef unsigned int uint;

public:
    // please use the combine() method instead.
    static const char kPathSeparator =
#ifdef _WIN32
        '\\';
#else
        '/';
#endif

    static std::string createDirectory(std::string fullPath);

    static std::string getUserHomeFolder()
    {
//Windows uses USERPROFILE, Linux uses HOME
#ifdef _WIN32
        std::wstring userProfile = _wgetenv(L"USERPROFILE");
        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
        return converter.to_bytes(userProfile);
#else
        return std::getenv("HOME");
#endif
    }

    static std::string getUserDocumentsFolder();

    static std::string getAppDataFolder()
    {
        return ensureFolder(combine(getUserDocumentsFolder(), ProductFolderName));
    }

    static std::string ensureFolder(std::string fullpath)
    {
        // make sure this directory exists.
        return createDirectory(fullpath);
    }

    // resolves the absolute path given a base path and a relative name, handles "..".
    static std::string resolve(std::string path, std::string rel)
    {
        if (rel.size() > 0 && rel[0] == kPathSeparator) {
            // absolute path overrides cwd.
            return rel;
        }

        auto currentPath = path;
        auto srcPath = rel;
        std::string sep;
        sep.push_back(kPathSeparator);
        auto vector = Utils::split(srcPath, sep.c_str(), 1);
        for (auto iter = vector.begin(); iter != vector.end(); iter++) {
            if (*iter == "..") {
                FileSystem::removeLeaf(currentPath);
            }
            else if (*iter == ".") {
                // ignore this
            }
            else {
                if (currentPath.size() == 0 || currentPath[currentPath.size() - 1] != kPathSeparator) {
                    currentPath.append(sep);
                }
                currentPath.append(*iter);
            }
        }

        // PX4 doesn't like relative names (it horks the board, no more responses after that)
        if (currentPath.size() == 0 || currentPath[0] != kPathSeparator) {
            currentPath = sep + currentPath;
        }
        return currentPath;
    }

    static std::string getFullPath(const std::string path);

    static bool isDirectory(const std::string path);

    static bool exists(const std::string path);

    static std::string getTempFolder()
    {
#ifdef _WIN32
        std::wstring userProfile = _wgetenv(L"TEMP");
        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
        return converter.to_bytes(userProfile);
#else
        return std::string("/tmp");
#endif
    }

    static std::string combine(const std::string parentFolder, const std::string child)
    {
        size_t len = parentFolder.size();
        if (len > 0 && parentFolder[len - 1] == kPathSeparator) {
            // parent already ends with '/'
            return parentFolder + child;
        }
        len = child.size();
        if (len > 0 && child[0] == kPathSeparator) {
            // child already starts with '/'
            return parentFolder + child;
        }
        return parentFolder + kPathSeparator + child;
    }

    static void removeLeaf(std::string& path)
    {
        size_t size = path.size();
        size_t pos = path.find_last_of(kPathSeparator);
        if (pos != std::string::npos) {
            path.erase(pos, size - pos);
        }
    }

    static std::string getFileName(std::string fullPath)
    {
        size_t size = fullPath.size();
        size_t pos = fullPath.find_last_of(kPathSeparator);
        if (pos != std::string::npos) {
            pos++;
            return fullPath.substr(pos, size - pos);
        }
        return fullPath;
    }

    static std::string getFileExtension(const std::string str)
    {
        // bugbug: this is not unicode safe.
        int len = static_cast<int>(str.size());
        const char* ptr = str.c_str();
        int i = 0;
        for (i = len - 1; i >= 0; i--) {
            if (ptr[i] == '.')
                break;
        }
        if (i < 0) return "";
        auto ui = static_cast<uint>(i);
        return str.substr(ui, len - ui);
    }

    static std::string getLogFileNamePath(std::string prefix, std::string suffix, std::string extension, bool add_timestamp)
    {
        std::string logfolder = Utils::to_string(Utils::now(), "%Y-%m-%d");
        std::string fullPath = combine(getAppDataFolder(), logfolder);
        std::string timestamp = add_timestamp ? Utils::to_string(Utils::now()) : "";
        std::stringstream filename_ss;
        filename_ss << ensureFolder(fullPath) << kPathSeparator << prefix << suffix << timestamp << extension;
        return filename_ss.str();
    }

    static void openTextFile(std::string filepath, std::ifstream& file)
    {

#ifdef _WIN32
        // WIN32 will create the wrong file names if we don't first convert them to UTF-16.
        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
        std::wstring wide_path = converter.from_bytes(filepath);
        file.open(wide_path, std::ios::in);
#else
        file.open(filepath, std::ios::in);
#endif
    }

    static void createBinaryFile(std::string filepath, std::ofstream& file)
    {

#ifdef _WIN32
        // WIN32 will create the wrong file names if we don't first convert them to UTF-16.
        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
        std::wstring wide_path = converter.from_bytes(filepath);
        file.open(wide_path, std::ios::binary | std::ios::trunc);
#else
        file.open(filepath, std::ios::binary | std::ios::trunc);
#endif
    }

    static void createTextFile(std::string filepath, std::ofstream& file)
    {

#ifdef _WIN32
        // WIN32 will create the wrong file names if we don't first convert them to UTF-16.
        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
        std::wstring wide_path = converter.from_bytes(filepath);
        file.open(wide_path, std::ios::out | std::ios::trunc);
#else
        file.open(filepath, std::ios::trunc);
#endif

        if (file.fail())
            throw std::ios_base::failure(std::strerror(errno));
    }

    static std::string createLogFile(std::string suffix, std::ofstream& flog)
    {
        std::string filepath = getLogFileNamePath("log_", suffix, ".tsv", true);
        createTextFile(filepath, flog);

        Utils::log(Utils::stringf("log file started: %s", filepath.c_str()));
        flog.exceptions(flog.exceptions() | std::ios::failbit | std::ifstream::badbit);
        return filepath;
    }

    static void remove(std::string fileName);

    static std::string readLineFromFile(std::ifstream& file)
    {
        std::string line;
        try {
            std::getline(file, line);
        }
        catch (...) {
            if (!file.eof())
                throw;
        }
        return line;
    }

    static void appendLineToFile(std::string filepath, std::string line)
    {
        std::ofstream file;
#ifdef _WIN32
        // WIN32 will create the wrong file names if we don't first convert them to UTF-16.
        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
        std::wstring wide_path = converter.from_bytes(filepath);
        file.open(wide_path, std::ios::out | std::ios::app);
#else
        file.open(filepath, std::ios::out | std::ios::app);
#endif
        if (file.fail())
            throw std::ios_base::failure(std::strerror(errno));
        file.exceptions(file.exceptions() | std::ios::failbit | std::ifstream::badbit);
        file << line << std::endl;
    }
};
}
#endif