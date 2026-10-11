#include <cassert>
#include <initializer_list>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "ps2/frame_capture.h"
using vec3_t=float[3];
struct cparticle_t { cparticle_t *next; vec3_t org,vel,accel; float time,alpha,alphavel; int color; };
struct centity_t { int trailcount; };
static cparticle_t pool[256], *free_particles, *active_particles;
static struct { int time; } cl={500};
constexpr int EF_GIB=1,EF_GREENGIB=2,EF_ROCKET=4,PARTICLE_GRAVITY=40;
static int normals=0;
void VectorCopy(const float*a,float*b) { for(int i=0;i<3;++i) b[i]=a[i]; }
void VectorSubtract(const float*a,const float*b,float*c) { for(int i=0;i<3;++i)c[i]=a[i]-b[i]; }
void VectorScale(const float*a,float scale,float*b) { for(int i=0;i<3;++i)b[i]=a[i]*scale; }
void VectorAdd(const float*a,const float*b,float*c) { for(int i=0;i<3;++i)c[i]=a[i]+b[i]; }
void VectorClear(float*a) { for(int i=0;i<3;++i)a[i]=0; }
float VectorNormalize(float*a) { ++normals; float len=std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]); if(len)for(int i=0;i<3;++i)a[i]/=len;return len; }
#define frand() ((rand() & 32767) / 32767.0)
#define crand() (2.0 * ((rand() & 32767) / 32767.0) - 1.0)
static ps2_fx_counters_t counters={};
#if PS2_PROFILE
extern "C" ps2_fx_counters_t *PS2_FrameFXCounters() { return &counters; }
#endif
#include "reference.inc"
#include "production.inc"
void reset(int available,int seed) {
 std::memset(pool,0,sizeof(pool)); active_particles=nullptr;
 free_particles=available?pool:nullptr;
 for(int i=0;i<available;++i) pool[i].next=i+1<available?&pool[i+1]:nullptr;
 std::srand(seed); normals=0; counters={};
}
void call(int type,bool reference,vec3_t start,vec3_t end,centity_t *cent,int flags) {
 if(type==0) { if(reference)CL_BlasterTrail_Reference(start,end); else CL_BlasterTrail(start,end); }
 if(type==1) { if(reference)CL_DiminishingTrail_Reference(start,end,cent,flags); else CL_DiminishingTrail(start,end,cent,flags); }
 if(type==2) { if(reference)CL_RocketTrail_Reference(start,end,cent); else CL_RocketTrail(start,end,cent); }
}
int main() {
 int scenarios=0;
 for(int type=0;type<3;++type)for(float distance:{0.f,.1f,1.f,14.f,500.f})for(int available:{0,1,5,256})for(int flags:{EF_GIB,EF_GREENGIB,EF_ROCKET})for(int trail:{100,850,950}) {
  vec3_t start={1,2,3},end={distance+1,2,3}; centity_t cent={trail};
  reset(available,17); call(type,true,start,end,&cent,flags);
  cparticle_t expected[256]; std::memcpy(expected,pool,sizeof(pool));
  auto *expectedFree=free_particles; auto *expectedActive=active_particles;
  int expectedTrail=cent.trailcount,expectedRand=std::rand(),referenceNormals=normals;
  reset(available,17); cent.trailcount=trail; call(type,false,start,end,&cent,flags);
  assert(!std::memcmp(expected,pool,sizeof(pool)) && free_particles==expectedFree && active_particles==expectedActive);
  assert(cent.trailcount==expectedTrail && std::rand()==expectedRand);
  if(!available) assert(normals==0 && referenceNormals>0);
#if PS2_PROFILE
  int emitted=0; for(auto*p=active_particles;p;p=p->next) ++emitted;
  assert(counters.particles==emitted);
  if(!available) assert(counters.exhausted==1);
#endif
  ++scenarios;
 }
 std::printf("%d trail scenarios preserve particles/RNG/trail state; exhausted pool skips normalization PASS\n",scenarios);
}
