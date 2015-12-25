/**
 Copyright (c) 2015-present, Facebook, Inc.
 All rights reserved.

 This source code is licensed under the BSD-style license found in the
 LICENSE file in the root directory of this source tree. An additional grant
 of patent rights can be found in the PATENTS file in the same directory.
 */

#include <libutil/FSUtil.h>

#include <sys/stat.h>
#include <climits>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include "bsd_glob.h"

#include <sstream>

using libutil::FSUtil;

bool FSUtil::
TestForPresence(std::string const &path)
{
    return ::access(path.c_str(), F_OK) == 0;
}

bool FSUtil::
TestForRead(std::string const &path)
{
    return ::access(path.c_str(), R_OK) == 0;
}

bool FSUtil::
TestForWrite(std::string const &path)
{
    return ::access(path.c_str(), W_OK) == 0;
}

bool FSUtil::
TestForExecute(std::string const &path)
{
    return ::access(path.c_str(), X_OK) == 0;
}

bool FSUtil::
TestForDirectory(std::string const &path)
{
    struct stat st;
    if (::stat(path.c_str(), &st) < 0)
        return false;
    else
        return S_ISDIR(st.st_mode);
}

bool FSUtil::
TestForSymlink(std::string const &path)
{
    struct stat st;
    if (::lstat(path.c_str(), &st) < 0)
        return false;
    else
        return S_ISLNK(st.st_mode);
}

std::string FSUtil::
GetDirectoryName(std::string const &path)
{
    std::string copy(path);
    return ::dirname(&copy[0]);
}

std::string FSUtil::
GetBaseName(std::string const &path)
{
    std::string copy(path);
    return ::basename(&copy[0]);
}

std::string FSUtil::
GetBaseNameWithoutExtension(std::string const &path)
{
    std::string base = GetBaseName(path);
    if (base.empty())
        return base;

    size_t pos = base.rfind('.');
    if (pos == std::string::npos)
        return std::string();

    return base.substr(0, pos);
}

std::string FSUtil::
GetRelativePath(std::string const &path, std::string const &to)
{
    std::string::size_type po = 0;
    std::string::size_type oo = 0;

    std::string result;

    bool found = false;
    while (!found) {
        std::string::size_type npo = path.find('/', po);
        std::string::size_type noo = to.find('/', oo);

        std::string spo = path.substr(po, (npo == std::string::npos ? path.size() : npo) - po);
        std::string soo = to.substr(oo, (noo == std::string::npos ? to.size() : noo) - oo);

        if (spo == soo) {
            po = (npo == std::string::npos ? std::string::npos : npo + 1);
            oo = (noo == std::string::npos ? std::string::npos : noo + 1);
        } else {
            break;
        }

        if (npo == std::string::npos || noo == std::string::npos) {
            break;
        }
    }

    while (oo != std::string::npos && oo != to.size()) {
        result += "../";
        oo = to.find('/', oo + 1);
    }

    if (po != std::string::npos && po != path.size()) {
        result += path.substr(po);
    }

    return result;
}

std::string FSUtil::
GetFileExtension(std::string const &path)
{
    std::string base = GetBaseName(path);
    if (base.empty())
        return base;

    size_t pos = base.rfind('.');
    if (pos == std::string::npos)
        return std::string();

    return base.substr(pos + 1);
}

bool FSUtil::
IsFileExtension(std::string const &path, std::string const &extension,
        bool insensitive)
{
    std::string pathExtension = GetFileExtension(path);
    if (pathExtension.empty())
        return extension.empty();

    if (insensitive) {
        return ::strcasecmp(pathExtension.c_str(), extension.c_str()) == 0;
    } else {
        return pathExtension == extension;
    }
}

bool FSUtil::
IsFileExtension(std::string const &path,
        std::initializer_list <std::string> const &extensions,
        bool insensitive)
{
    std::string pathExtension = GetFileExtension(path);
    if (pathExtension.empty())
        return false;

    for (auto const &extension : extensions) {
        bool match = false;
        if (insensitive) {
            match = ::strcasecmp(pathExtension.c_str(), extension.c_str()) == 0;
        } else {
            match = pathExtension == extension;
        }
        if (match)
            return true;
    }

    return false;
}

bool FSUtil::
IsAbsolutePath(std::string const &path)
{
    return !path.empty() && path[0] == '/';
}

std::string FSUtil::
ResolveRelativePath(std::string const &path, std::string const &workingDirectory)
{
    if (IsAbsolutePath(path)) {
        return path;
    } else {
        return NormalizePath(workingDirectory + "/" + path);
    }
}

std::string FSUtil::
ResolvePath(std::string const &path)
{
    char realPath[PATH_MAX + 1];
    if (::realpath(path.c_str(), realPath) == nullptr)
        return std::string();
    else
        return realPath;
}

bool FSUtil::
Touch(std::string const &path)
{
    if (TestForWrite(path))
        return true;

    FILE *fp = std::fopen(path.c_str(), "w");
    if (fp == nullptr)
        return false;

    std::fclose(fp);
    return true;
}

bool FSUtil::
Remove(std::string const &path)
{
    if (::unlink(path.c_str()) < 0) {
        if (::unlink(path.c_str()) < 0)
            return false;
    }
    return true;
}

bool FSUtil::
CreateDirectory(std::string const &path)
{
    std::vector<std::string> components;

    std::string current = path;
    while (current != GetDirectoryName(current)) {
        std::string component = GetBaseName(current);
        components.push_back(component);

        current = GetDirectoryName(current);
    }

    for (auto it = components.rbegin(); it != components.rend(); ++it) {
        std::string const &component = *it;
        if (it != components.rbegin()) {
            current += "/";
        }
        current += component;

        if (::mkdir(current.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0 && errno != EEXIST) {
            return false;
        }
    }

    return true;
}

std::string FSUtil::
GetCurrentDirectory()
{
    char path[PATH_MAX + 1];
    if (::getcwd(path, sizeof(path)) == nullptr) {
        path[0] = '\0';
    }
    return path;
}

bool FSUtil::
EnumerateDirectory(std::string const &path, std::string const &pattern,
        std::function <bool(std::string const &)> const &cb, bool insensitive)
{
    bsd_glob_t  glob;
    std::string matchPattern;

    if (pattern.empty()) {
        matchPattern = "*";
    } else {
        matchPattern = pattern;
    }

    if (::bsd_glob((path + "/" + matchPattern).c_str(),
                BSD_GLOB_TILDE | BSD_GLOB_BRACE | BSD_GLOB_MARK |
                BSD_GLOB_NOESCAPE | (insensitive ? BSD_GLOB_CASEFOLD : 0),
                nullptr, &glob) != 0)
        return false;

    for (size_t n = 0; n < glob.gl_pathc; n++) {
        std::string path = GetBaseName(glob.gl_pathv[n]);

        // 
        // Skip past . and ..
        //
        if (path == "." || path == "..")
            continue;

        if (!cb(path))
            break;
    }

    ::bsd_globfree(&glob);

    return true;
}

bool FSUtil::
EnumerateRecursive(std::string const &path, std::string const &pattern,
    std::function <bool(std::string const &)> const &cb, bool insensitive)
{
    EnumerateDirectory(path, pattern,
        [&](std::string const &filename) -> bool
        {
            std::string full = path + "/" + filename;
            cb(full);
            return true;
        }, insensitive);

    EnumerateDirectory(path, std::string(),
        [&](std::string const &filename) -> bool
        {
            std::string full = path + "/" + filename;
            if (TestForDirectory(full) && !TestForSymlink(full)) {
                EnumerateRecursive(full, pattern, cb, insensitive);
            }
            return true;
        }, false);

    return true;
}

std::string FSUtil::
FindExecutable(std::string const &name)
{
    if (name.empty())
        return std::string();

    char const *paths = ::getenv("PATH");
    if (paths == nullptr)
        return std::string();

    return FindExecutable(name, paths);
}

std::string FSUtil::
FindExecutable(std::string const &name, std::string const &paths)
{
    std::string exePath = FindFile(name, paths);

    if (exePath.empty())
        return std::string();

    if (TestForExecute(exePath))
        return ResolvePath(exePath);

    return std::string();
}

std::string FSUtil::
FindExecutable(std::string const &name, string_vector const &paths)
{
    std::string exePath = FindFile(name, paths);

    if (exePath.empty())
        return std::string();

    if (TestForExecute(exePath)) {
        return NormalizePath(exePath);
    }

    return std::string();
}

std::string FSUtil::
FindFile(std::string const &name, std::string const &paths)
{
    if (name.empty())
        return std::string();

    string_vector      vpaths;
    string_set         seen;
    std::string        path;
    std::istringstream is(paths);

    while (std::getline(is, path, ':')) {
        if (seen.find(path) != seen.end())
            continue;

        vpaths.push_back(path);
        seen.insert(path);
    }

    return FindFile(name, vpaths);
}

std::string FSUtil::
FindFile(std::string const &name, string_vector const &paths)
{
    if (name.empty())
        return std::string();

    for (auto const &path : paths) {
        std::string filePath = path + "/" + name;
        if (TestForPresence(filePath)) {
            return NormalizePath(filePath);
        }
    }

    return std::string();
}

static size_t
SimplePathNormalize(char const *in, char *out, size_t outSize, char separator,
        char const *invalidCharSet, bool dontWantRoot, bool relative,
        char replacementChar)
{
    char const *i = in;
    char *o = out;

    while (i[0] != 0) {
        if (i[0] == separator) {
            while (i[1] != 0 && i[1] == separator)
                i++;

            i++;
            if (o == out || *(o - 1) != separator) {
                if (o != out || !dontWantRoot)
                    *o++ = separator, *o = 0;
            }
        } else if (!relative && (i == in || i[-1] == separator) && i[0] == '.') {
            if (i[1] == '.' && (i[2] == separator || i[2] == 0)) {
                int cnt = 2;
                for (;;) {
                    if (cnt > 0 && *o == separator) {
                        if (o == out || --cnt == 0) {
                            o++;
                            break;
                        }
                    } else if (o == out) {
                        if (!dontWantRoot)
                            *o++ = separator, *o = 0;
                        break;
                    }
                    *o-- = 0;
                }

                i += (i[2] == 0 ? 2 : 3);
            } else if (i[1] == separator || i[1] == 0) {
                if (o == out) {
                    *o++ = '.', *o++ = separator;
                }
                *o = 0, i += (i[1] == 0 ? 1 : 2);
            } else {
                i++, *o++ = '.', *o = 0;
            }
        } else {
            if (invalidCharSet != NULL
                && strchr(invalidCharSet, *i) != NULL)
                *o++ = replacementChar, i++;
            else
                *o++ = *i++;
        }
    }

    *o = 0;

    return (o - out);
}

static size_t
POSIXPathNormalize(char const * in, char *out, size_t outSize, bool relative)
{
    return SimplePathNormalize(in, out, outSize, '/', NULL, false, relative, '-');
}

std::string FSUtil::
NormalizePath(std::string const &path)
{
    std::string outputPath;

    if (path.empty())
        return std::string();

    outputPath.resize(path.size() * 2);
    size_t size = ::POSIXPathNormalize(path.c_str(),
            &outputPath[0], outputPath.size(), path[0] != '/');
    outputPath.resize(size);

    return outputPath;
}
