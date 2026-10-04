import os
from pathlib import Path
import re
import subprocess
root = Path(__file__).resolve().parents[2]
out = root / 'build/lighting-lod-host'
out.mkdir(parents=True, exist_ok=True)
source = (root/'src/ps2/renderer/render_view.cpp').read_text()
parts = []
for prefix in ['u32 TriangleLightingKey(', 'void BuildCachedLitTriangle(']:
    match = re.search(r'^'+re.escape(prefix)+r'.*?^\}', source, re.M|re.S)
    if not match: raise RuntimeError(prefix)
    parts.append(match.group(0))
(out/'lighting.inc').write_text('\n'.join(parts))
binary = out / ('test.exe' if os.name == 'nt' else 'test')
flags = [] if os.name == 'nt' else ['-fsanitize=address,undefined']
for profile in [0,1]:
    subprocess.run([os.environ.get('CXX','g++'), '-std=c++17', '-Wall','-Wextra','-Werror',
        '-DPS2_PROFILE='+str(profile), *flags, '-I'+str(root/'src'), '-I'+str(out),
        str(root/'tests/lighting_lod/test.cpp'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)],check=True)
