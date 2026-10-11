import os,re,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=root/'build/object-fx-host';out.mkdir(parents=True,exist_ok=True)
src=(root/'src/client/cl_fx.c').read_text()
funcs=['CL_BlasterTrail','CL_DiminishingTrail','CL_RocketTrail']
(out/'production.inc').write_text('\n'.join(re.search(r'^void '+n+r'\([^\n]*\)\n\{.*?^\}',src,re.M|re.S).group(0) for n in funcs))
flags=[] if os.name=='nt' else ['-fsanitize=address,undefined']
for profile in [0,1]:
 exe=out/('test%d'%profile+('.exe' if os.name=='nt' else ''))
 subprocess.run(['g++','-std=c++17','-DPS2_PROFILE=%d'%profile,'-Wall','-Wextra','-Werror',*flags,'-I'+str(root/'src'),'-I'+str(out),str(root/'tests/object_fx/test.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
ents=(root/'src/client/cl_ents.c').read_text()
assert ents.index('// add automatic particle trails')<ents.index('PS2_FrameObjectPart(PS2_OBJECT_AUTO_FX)')<ents.index('PS2_FrameObjectPart(PS2_OBJECT_BASE)')
