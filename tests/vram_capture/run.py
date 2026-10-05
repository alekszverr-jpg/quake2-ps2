import os,re,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out=root/'build/vram-capture-host'
out.mkdir(parents=True,exist_ok=True)
src=(root/'src/ps2/renderer/ref.cpp').read_text()
match=re.search(r'^void CollectVramCapture\(\).*?^\}',src,re.M|re.S)
assert match
(out/'collect.inc').write_text(match.group(0))
(out/'panel.inc').write_text(re.search(r'^void DrawVramCaptureResult\(\).*?^\}',src,re.M|re.S).group(0))
assert src.index('ps2::gs::EndFrame();')<src.index('CollectVramCapture();')
menu=(root/'src/client/menu.c').read_text()
assert '"capture vram 10s"' in menu and 'mode == 3 ? 1 : 0' in menu
flags=[] if os.name=='nt' else ['-fsanitize=address,undefined']
exe=out/('test.exe' if os.name=='nt' else 'test')
subprocess.run(['g++','-std=c++17','-DPS2_PROFILE=1','-Wall','-Wextra','-Wconversion','-Werror',*flags,
    '-I'+str(root/'tests/vram/stubs'),'-I'+str(root/'src'),'-I'+str(out),
    str(root/'tests/vram_capture/test.cpp'),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
