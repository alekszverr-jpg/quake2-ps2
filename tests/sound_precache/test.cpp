#include <cassert>
#include <cstdio>
#include <cstring>
#include "client/weapon_sounds.h"
struct sfxcache_t { int length=1,width=1; };
struct sfx_t { char name[80]; int registration_sequence; sfxcache_t *cache; char *truename; };
static sfx_t known_sfx[128]; static int num_sfx=0,s_registration_sequence=0;
static bool sound_started=true,s_registering=false;
static int reads=0,live=0,pumps=0;
const int MAX_SOUNDS=4,CS_SOUNDS=0;
struct { char configstrings[MAX_SOUNDS][80]={}; sfx_t *sound_precache[MAX_SOUNDS]={}; } cl;
sfx_t * S_FindName(const char *name,bool) {
 for(int i=0;i<num_sfx;++i) if(!std::strcmp(known_sfx[i].name,name)) return &known_sfx[i];
 int i=0; while(i<num_sfx && known_sfx[i].name[0]) ++i;
 if(i==num_sfx) { assert(num_sfx<128); ++num_sfx; }
 std::strcpy(known_sfx[i].name,name); return &known_sfx[i];
}
sfxcache_t * S_LoadSound(sfx_t *s) {
 if(s->cache) return s->cache;
 ++reads; ++live; s->cache=new sfxcache_t; return s->cache;
}
void Z_Free(sfxcache_t *p) { delete p; --live; }
void Z_Free(char *) { assert(false); }
using byte=unsigned char;
void Com_PageInMemory(byte *,int) {}
void S_StopAllSounds() {}
void Sys_SendKeyEvents() { ++pumps; }
sfx_t * S_RegisterSound(const char *);
void CL_RegisterTEntSounds() { S_RegisterSound("world/ric1.wav"); }
void S_BeginRegistration();
void S_EndRegistration();
#include "production.inc"
int main() {
 std::strcpy(cl.configstrings[1],"weapons/hyprbl1a.wav"); // deduplicates preload
 std::strcpy(cl.configstrings[2],"world/map.wav");
 CL_RegisterSounds(); assert(!s_registering && reads==36 && live==36 && pumps==36);
 const int initial=reads;
 for(auto name:ps2_weapon_sounds) for(int i=0;i<20;++i) S_RegisterSound(name);
 assert(reads==initial && live==36); // firing cannot first-load these sounds
 auto *hyper=S_RegisterSound("weapons/hyprbl1a.wav");
 auto *oldMap=S_RegisterSound("world/map.wav");
 for(int map=0;map<40;++map) {
  S_PurgeLevelSounds(); assert(live==0 && hyper->cache==nullptr);
  std::strcpy(cl.configstrings[2],"world/nextmap.wav");
  CL_RegisterSounds(); assert(live==36 && num_sfx<=37);
  assert(oldMap->name[0]==0);
  auto *again=S_RegisterSound("weapons/hyprbl1a.wav"); assert(again==hyper);
  const int before=reads;
  for(auto name:ps2_weapon_sounds) S_RegisterSound(name);
  assert(reads==before && live==36);
 }
 S_PurgeLevelSounds(); assert(live==0);
 sound_started=false; assert(S_RegisterSound("weapons/hyprbl1a.wav")==nullptr);
 std::puts("Weapon registration/preload/dedup/no combat reads/40 level purges/bounded slots PASS");
}
