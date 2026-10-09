from pathlib import Path
import re,os,subprocess
root=Path(__file__).resolve().parents[2];out=root/'build/sound-precache-host';out.mkdir(parents=True,exist_ok=True)
functions=[]
for path,names in [('src/client/cl_parse.c',['CL_RegisterSounds']),('src/client/snd_dma.c',['S_BeginRegistration','S_RegisterSound','S_EndRegistration','S_PurgeLevelSounds'])]:
 src=(root/path).read_text()
 for name in names:
  functions.append(re.search(r'^(?:void|sfx_t \*) '+name+r'\([^\n]*\)\n\{.*?^\}',src,re.M|re.S).group(0))
(out/'production.inc').write_text('\n'.join(functions))
# All literal base-player muzzle sounds and all five random variants covered.
src=(root/'src/client/cl_fx.c').read_text().split('case MZ_PHALANX:')[0]
required=set(re.findall(r'S_RegisterSound\("([^\"]+)"\)',src))
required.update('weapons/machgf%db.wav'%i for i in range(1,6))
listed=set(re.findall(r'"([^\"]+\.wav)"',(root/'src/client/weapon_sounds.h').read_text()))
assert required<=listed,required-listed
assert len(listed)==34
# Instrument only disk reads after the production cache-hit early return.
mem=(root/'src/client/snd_mem.c').read_text()
assert mem.index('if (sc)')<mem.index('soundTicks = (unsigned)clock();')<mem.index('PS2_FrameSoundIO(')
flags=[] if os.name=='nt' else ['-fsanitize=address,undefined']
exe=out/('test.exe' if os.name=='nt' else 'test')
subprocess.run(['g++','-std=c++17','-DPS2_QUAKE=1','-Wall','-Wextra','-Werror',*flags,'-I'+str(root/'src'),'-I'+str(out),str(root/'tests/sound_precache/test.cpp'),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
