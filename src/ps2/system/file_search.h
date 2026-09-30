#pragma once

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

// POSIX IO follows the same newlib backend as fopen (host: or mass:).
namespace ps2::sys {
class FileSearch final {
    DIR * directory = nullptr;
    char base[MAX_OSPATH] = {};
    char pattern[MAX_OSPATH] = {};
    char result[MAX_OSPATH] = {};

    static bool Matches(const char * mask, const char * name) {
        const char * star = nullptr;
        const char * retry = nullptr;
        while (*name) {
            if (*mask == '?' || *mask == *name) { ++mask; ++name; }
            else if (*mask == '*') { star = mask++; retry = name; }
            else if (star) { mask = star + 1; name = ++retry; }
            else { return false; }
        }
        while (*mask == '*') ++mask;
        return *mask == '\0';
    }
public:
    void Close() {
        if (directory) closedir(directory);
        directory = nullptr;
    }
    char * Next(unsigned musthave, unsigned canthave) {
        if (!directory) return nullptr;
        while (dirent * entry = readdir(directory)) {
            const char * name = entry->d_name;
            if (!std::strcmp(name, ".") || !std::strcmp(name, "..") ||
                !Matches(pattern, name)) continue;
            const int len = std::snprintf(result, sizeof(result), "%s/%s", base, name);
            if (len < 0 || len >= static_cast<int>(sizeof(result))) continue;
            struct stat info;
            if (stat(result, &info) != 0) continue;
            unsigned attributes = 0;
            if (S_ISDIR(info.st_mode)) attributes |= SFF_SUBDIR;
            if (name[0] == '.') attributes |= SFF_HIDDEN;
            if (!(info.st_mode & S_IWUSR)) attributes |= SFF_RDONLY;
            if ((attributes & musthave) != musthave || (attributes & canthave)) continue;
            return result;
        }
        return nullptr;
    }
    char * First(const char * path, unsigned musthave, unsigned canthave) {
        Close();
        if (!path || std::strlen(path) >= sizeof(base)) return nullptr;
        const char * slash = std::strrchr(path, '/');
        if (slash) {
            const size_t length = static_cast<size_t>(slash - path);
            std::memcpy(base, path, length);
            base[length] = '\0';
            if (!length) std::strcpy(base, "/");
            std::strcpy(pattern, slash + 1);
        } else {
            std::strcpy(base, ".");
            std::strcpy(pattern, path);
        }
        directory = opendir(base);
        return Next(musthave, canthave);
    }
};
} // namespace ps2::sys
