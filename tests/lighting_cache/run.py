"""Exercise the production chunk allocator, invalidation and chain pinning."""
import os
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'build/lighting-cache-host'
out.mkdir(parents=True, exist_ok=True)
source = (root / 'src/ps2/renderer/render_view.cpp').read_text()

def extract(text, prefix):
    match = re.search(r'^' + re.escape(prefix) + r'.*?^\}', text, re.M | re.S)
    if not match:
        raise RuntimeError(prefix)
    return match.group(0)

(out / 'vertex.inc').write_text(extract(source, 'struct CachedLitVertex') + ';')
(out / 'triangle.inc').write_text(extract(
    (root / 'src/ps2/renderer/model.h').read_text(), 'struct ModelTriangle') + ';')
start = source.index('constexpr int kMaxCachedVertsPerTriangle =')
end = source.index('// Sutherland-Hodgman', start)
(out / 'cache.inc').write_text(source[start:end] + '\n' +
    extract(source, 'void PinLitTextureChains('))
# Integration ordering is essential to protect later chains and seal reads.
draw = extract(source, 'void DrawTextureChains(')
assert draw.index('PinLitTextureChains();') < draw.index('GatherPolyTriangles(')
assert 'BeginLitCacheFrame();' in extract(source, 'void RenderFrame(')

binary = out / ('test.exe' if os.name == 'nt' else 'test')
flags = [] if os.name == 'nt' else ['-fsanitize=address,undefined']
subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++17', '-Wall', '-Wextra',
                '-Werror', '-Wconversion', *flags, '-I' + str(out),
                str(root / 'tests/lighting_cache/test.cpp'), '-o', str(binary)], check=True)
subprocess.run([str(binary)], check=True)
