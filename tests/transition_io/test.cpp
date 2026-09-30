#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <filesystem>
#define MAX_OSPATH 256
#define SFF_SUBDIR 8
#define SFF_HIDDEN 2
#define SFF_RDONLY 4
#include "ps2/system/file_search.h"
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif
static ps2::sys::FileSearch s_fileSearch;
static std::string gameDir;
const char * FS_Gamedir() { return gameDir.c_str(); }
void Com_DPrintf(const char *, ...) {}
void Com_sprintf(char * dst, int size, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(dst, size, fmt, args);
    va_end(args);
}
struct Sound { void * cache; const char * name; };
static int num_sfx = 3;
static Sound known_sfx[3];
static int pcm[3], freed;
static bool stopped;
void S_StopAllSounds() { stopped = true; }
void Z_Free(void * ptr) { assert(stopped && ptr); ++freed; }
#include "transition.inc"
static void Write(const std::string & name) {
    FILE * f = fopen(name.c_str(), "wb");
    assert(f);
    fputs("saved level", f);
    fclose(f);
}
int main(int argc, char ** argv) {
    assert(argc == 2);
    gameDir = argv[1];
    std::filesystem::create_directories(gameDir);
    char path[MAX_OSPATH];
    Com_sprintf(path, sizeof(path), "%s/save/current/", FS_Gamedir());
    FS_CreatePath(path);
    const std::string dir = gameDir + "/save/current/";
    Write(dir + "base1.sav"); Write(dir + "base2.sav");
    Write(dir + "base1.sv2"); Write(dir + "server.ssv"); Write(dir + "game.ssv");
    Write(dir + "keep.txt");
    std::filesystem::create_directory(dir + "folder.sav");
    Write(dir + ".hidden.sav");
    int count = 0;
    char * found = Sys_FindFirst((dir + "*.sav").c_str(), 0, SFF_SUBDIR | SFF_HIDDEN);
    while (found) { ++count; found = Sys_FindNext(0, SFF_SUBDIR | SFF_HIDDEN); }
    Sys_FindClose(); assert(count == 2);
    assert(!Sys_FindFirst((dir + "none*.sav").c_str(), 0, 0)); Sys_FindClose();
    SV_WipeSavegame("current");
    assert(!std::filesystem::exists(dir + "base1.sav"));
    assert(!std::filesystem::exists(dir + "base2.sav"));
    assert(!std::filesystem::exists(dir + "base1.sv2"));
    assert(!std::filesystem::exists(dir + "server.ssv"));
    assert(!std::filesystem::exists(dir + "game.ssv"));
    assert(std::filesystem::exists(dir + "keep.txt"));
    for (int i = 0; i < num_sfx; ++i) known_sfx[i] = { &pcm[i], "sound" };
    S_PurgeLevelSounds();
    assert(stopped && freed == num_sfx);
    for (const auto & s : known_sfx) assert(!s.cache && s.name);
    S_PurgeLevelSounds(); assert(freed == num_sfx);
    puts("Save directories, unit archive cleanup and PCM purge passed");
}
