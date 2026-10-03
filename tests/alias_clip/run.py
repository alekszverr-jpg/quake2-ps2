"""Differential-test indexed MD2 clipping against the production generic clipper."""
import os
from pathlib import Path
import re
import subprocess
root = Path(__file__).resolve().parents[2]
out = root / 'build/alias-clip-host'
out.mkdir(parents=True, exist_ok=True)
source = (root / 'src/ps2/renderer/render_view.cpp').read_text()
def function(prefix):
    match = re.search(r'^' + re.escape(prefix) + r'.*?^\}', source, re.M | re.S)
    if not match: raise RuntimeError(prefix)
    return match.group(0)
parts = []
for prefix in ['union ClipDists', 'struct alignas(16) ClipVertex', 'struct alignas(16) AliasClipData', 'struct alignas(16) PreparedAliasVertex', 'struct AliasTexCoord']:
    parts.append(function(prefix) + ';')
(out/'types.inc').write_text('\n'.join(parts))
for name, prefixes in [('generic.inc',['int ClipAgainstPlane(', 'inline u32 PackFloatColor(', 'inline void EmitScratchVertex(const ClipVertex & v,', 'inline void EmitScratchVertex(const ClipVertex & v)', 'void SubmitWorldTriangle(', 'void SetClipDistances(ClipDists &']), ('alias.inc',['void PrepareAliasTexCoords(', 'inline AliasTexCoord AliasTexCoordsAt(', 'inline void SubmitOpaqueAliasTriangle(', 'void PrepareAliasClipData(', 'void SubmitAliasTriangle('])]:
    (out/name).write_text('\n'.join(function(prefix) for prefix in prefixes))
binary = out / ('test.exe' if os.name == 'nt' else 'test')
flags = [] if os.name == 'nt' else ['-fsanitize=address,undefined']
subprocess.run([os.environ.get('CXX','g++'), '-std=c++17','-Wall','-Wextra','-Werror', *flags, '-I'+str(out), str(root/'tests/alias_clip/test.cpp'), '-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True)
