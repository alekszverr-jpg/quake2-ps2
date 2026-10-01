"""Exercise actual VU packet state and sprite submission with host adapters."""
import os
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'build/transparency-host'
out.mkdir(parents=True, exist_ok=True)
vu = (root / 'src/ps2/renderer/vu1.cpp').read_text()
vcl = (root / 'src/ps2/renderer/vu1progs/textured_triangles.vcl').read_text()

def extract(source, signature):
    match = re.search(r'^' + re.escape(signature) + r'\(.*?^\}', source, re.M | re.S)
    assert match, signature
    return match.group(0)

constants = '\n'.join(re.findall(r'^constexpr int k(?:MaxVertsPerBatch|BatchHeaderAddr|GifTagsAddr|VertexDataAddr|ChunkChainQwords|ChainTailQwords).*?;',vu,re.M))
assert int(re.search(r'#define kVertexData\s+(\d+)',vcl)[1]) == 11
copy = vcl.split('; The GIF tags were prepared')[1].split('; One triangle')[0]
assert len(re.findall(r'^\s*lqi ',copy,re.M)) == 10
assert len(re.findall(r'^\s*sqi ',copy,re.M)) == 10
(out / 'packet.inc').write_text(constants + '\n' + extract(vu,'u64 MakeTestData') + '\n' + extract(vu,'void AddBatchChunk'))
gs = (root / 'src/ps2/renderer/gs.cpp').read_text()
begin = extract(gs, 'void BeginFrame')
assert begin.index('clear.DepthBuffer(') < begin.index('clear.Clear(')
(out / 'depth.inc').write_text(extract(gs,'u64 DepthBufferData'))
view = (root / 'src/ps2/renderer/render_view.cpp').read_text()
(out / 'sprite.inc').write_text(extract(view,'void DrawSpriteModel'))
texture = (root / 'src/ps2/renderer/texture.cpp').read_text()
(out / 'alpha.inc').write_text(extract(texture,'u8 GsTextureAlpha'))
binary = out / ('test.exe' if os.name == 'nt' else 'test')
flags = [] if os.name == 'nt' else ['-fsanitize=address,undefined']
subprocess.run([os.environ.get('CXX','g++'),'-std=c++17','-Wall','-Wextra','-Werror',
                *flags,'-I'+str(out),str(root/'tests/transparency/test.cpp'),'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True)
