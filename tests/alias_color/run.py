"""Compare production MD2 vertex preparation with the frozen Alpha135 loop."""
import os
from pathlib import Path
import re
import struct
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'build/alias-color-host'
out.mkdir(parents=True, exist_ok=True)
source = (root / 'src/ps2/renderer/render_view.cpp').read_text()

def function(prefix):
    match = re.search(r'^' + re.escape(prefix) + r'.*?^\}', source, re.M | re.S)
    if not match:
        raise RuntimeError(prefix)
    return match.group(0)

parts = [function('struct alignas(16) PreparedAliasVertex') + ';',
         function('struct alignas(16) AliasCachedColor') + ';',
         function('inline u32 PackFloatColor('),
         function('math::Vec3 AliasShellOffset('),
         function('math::Vec4 AliasShellVertexColor(')]
shade = function('math::Vec4 AliasVertexColor(')
parts.append(shade.replace('{\n', '{\n    ++shadeCalls;\n', 1))
parts += ['constexpr int kAliasColorCount = static_cast<int>(ArrayLength(kAliasNormals));',
          'static AliasCachedColor s_aliasColors[kAliasColorCount];',
          'static bool s_aliasColorReady[kAliasColorCount];',
          function('void BeginAliasColors('), function('inline const AliasCachedColor & CachedAliasColor(')]
(out / 'production.inc').write_text('\n'.join(parts))
start = source.index('    timer.Switch(ModelProfile::Vertices);', source.index('void DrawAliasModel('))
end = source.index('    timer.Switch(ModelProfile::Triangles);', start)
(out / 'vertices.inc').write_text(source[start:end].replace('    timer.Switch(ModelProfile::Vertices);\n', ''))
assert source.index('const bool clipAlias = clipViewWeapon || shell || translucent;') < start

flags = [] if os.name == 'nt' else ['-fsanitize=address,undefined']
for optimization in ['-O0', '-O2']:
    binary = out / ('test-' + optimization[1:] + ('.exe' if os.name == 'nt' else ''))
    subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++17', optimization,
        '-Wall', '-Wextra', '-Werror', '-Wdouble-promotion', *flags,
        '-I' + str(root / 'src'), '-I' + str(out),
        str(root / 'tests/alias_color/test.cpp'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

# Optional local workload evidence: no assets are needed by CI or committed.
files = list((root / 'baseq2/models').rglob('*.md2'))
vertices = normals = frames = 0
for path in files:
    data = path.read_bytes()
    h = struct.unpack_from('<17i', data)
    if h[0] != 844121161 or h[1] != 8:
        continue
    for frame in range(h[10]):
        offset = h[14] + frame * h[4] + 40
        indexes = data[offset + 3:offset + 4 * h[6]:4]
        vertices += len(indexes)
        normals += len({n if n < 162 else 0 for n in indexes})
        frames += 1
if frames:
    print(f'Local MD2 assets: {len(files)} files, {frames} frames, '
          f'{vertices} vertex colours -> {normals} unique normal colours; '
          'operation count only, not an FPS result')
