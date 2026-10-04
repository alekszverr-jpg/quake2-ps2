import os
from pathlib import Path
import re
import subprocess
root = Path(__file__).resolve().parents[2]
out = root/'build/seal-corners-host'
out.mkdir(parents=True,exist_ok=True)
source = (root/'src/ps2/renderer/render_view.cpp').read_text()
def extract(source,prefix):
    match=re.search(r'^'+re.escape(prefix)+r'.*?^\}',source,re.M|re.S)
    if not match: raise RuntimeError(prefix)
    return match.group(0)
(out/'types.inc').write_text('\n'.join(extract(source,p)+';' for p in
    ['union ClipDists','struct alignas(16) ClipVertex','struct CachedLitVertex']))
(out/'triangle.inc').write_text(extract((root/'src/ps2/renderer/model.h').read_text(),'struct ModelTriangle')+';')
(out/'seals.inc').write_text('\n'.join(extract(source,p) for p in
    ['void UnpackCachedColor(','void CacheSealCornerIndices(','void GatherPolyCrackSeals(']))
binary=out/('test.exe' if os.name=='nt' else 'test')
flags=[] if os.name=='nt' else ['-fsanitize=address,undefined']
subprocess.run([os.environ.get('CXX','g++'),'-std=c++17','-Wall','-Wextra','-Werror',
    *flags,'-I'+str(root/'src'),'-I'+str(out),str(root/'tests/seal_corners/test.cpp'),'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True)
