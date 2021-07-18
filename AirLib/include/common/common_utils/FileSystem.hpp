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

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif

namespace common_utils
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

    static std::string createDirectory(const std::string& fullPath);

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

    static std::string getExecutableFolder();

    static std::string getAppDataFolder()
    {
        return ensureFolder(combine(getUserDocumentsFolder(), ProductFolderName));
    }

    static std::string ensureFolder(const std::string& fullpath)
    {
        // make sure this directory exists.
        return createDirectory(fullpath);
    }

    static std::string ensureFolder(const std::string& parentFolder, const std::string& child)
    {
        // make sure this directory exists.
        return createDirectory(combine(parentFolder, child));
    }

    static std::string combine(const std::string& parentFolder, const std::string& child)
    {
        if (child.size() == 0)
            return parentFolder;

        size_t len = parentFolder.size();
        if (parentFolder.size() > 0 && parentFolder[len - 1] == kPathSeparator) {
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
        size_t pos = path.find_last_of('/');
        if (pos != std::string::npos) {
            path.erase(pos, size - pos);
        }
    }

    static std::string getFileExtension(const std::string& str)
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
        return str.substr(i, len - i);
    }

    static std::string getLogFolderPath(bool folder_timestamp, const std::string& parent = "")
    {
        std::string logfolder = folder_timestamp ? Utils::to_string(Utils::now()) : "";
        std::string parent_folder = (parent == "") ? getAppDataFolder() : parent;
        std::string fullPath = combine(parent_folder, logfolder);
        ensureFolder(fullPath);

        return fullPath;
    }

    static std::string getLogFileNamePath(const std::string& fullPath, const std::string& prefix, const std::string& suffix, const std::string& extension,
                                          bool file_timestamp)
    {
        //TODO: because this bug we are using alternative code with stringstream
        //https://answers.unrealengine.com/questions/664905/unreal-crashes-on-two-lines-of-extremely-simple-st.html

        std::string filename;
        filename.append(ensureFolder(fullPath))
            .push_back(kPathSeparator);
        filename.append(prefix)
            .append(suffix)
            .append(file_timestamp ? Utils::to_string(Utils::now()) : "")
            .append(extension);

        return filename;

        //std::stringstream filename_ss;
        //filename_ss << ensureFolder(fullPath) << kPathSeparator << prefix << suffix << timestamp << extension;
        //return filename_ss.str();
    }

    static void openTextFile(const std::string& filepath, std::ifstream& file)
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

    static void createBinaryFile(const std::string& filepath, std::ofstream& file)
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

    static void createTextFile(const std::string& filepath, std::ofstream& file)
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

    static std::string createLogFile(const std::string& suffix, std::ofstream& flog)
    {
        std::string log_folderpath = common_utils::FileSystem::getLogFolderPath(false);
        std::string filepath = getLogFileNamePath(log_folderpath, "log_", suffix, ".tsv", true);
        createTextFile(filepath, flog);

        Utils::log(Utils::stringf("log file started: %s", filepath.c_str()));
        flog.exceptions(flog.exceptions() | std::ios::failbit | std::ifstream::badbit);
        return filepath;
    }

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

    static void appendLineToFile(const std::string& filepath, const std::string& line)
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