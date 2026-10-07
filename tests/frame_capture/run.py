import os,re,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out=root/'build/frame-capture-host'; out.mkdir(parents=True,exist_ok=True)
src=(root/'src/ps2/renderer/ref.cpp').read_text()
names=['RestoreFrameProfiles','PrepareFrameCapture','CollectFrameCapture','DrawFrameCaptureResult']
text='\n'.join(re.search(r'^void '+name+r'\(\).*?^\}',src,re.M|re.S).group(0) for name in names)
text+='\n'+re.search(r'^extern "C" void PS2_FramePhase\(int phase\).*?^\}',src,re.M|re.S).group(0)
(out/'production.inc').write_text(text)
assert src.index('ps2::gs::EndFrame();')<src.index('CollectFrameCapture();')
assert 's_frameRequest->value==0.0f' in src
common=(root/'src/common/common.c').read_text()
assert re.search(r'PS2_FramePhase\(PS2_FRAME_SERVER\);\s*SV_Frame\(msec\);\s*PS2_FramePhase\(PS2_FRAME_OTHER\)',common)
assert re.search(r'PS2_FramePhase\(PS2_FRAME_CLIENT\);\s*CL_Frame\(msec\);\s*PS2_FramePhase\(PS2_FRAME_OTHER\)',common)
menu=(root/'src/client/menu.c').read_text(); assert '"capture frame 10s"' in menu and 'mode == 5 ? 1 : 0' in menu
flags=[] if os.name=='nt' else ['-fsanitize=address,undefined']
exe=out/('test.exe' if os.name=='nt' else 'test')
subprocess.run(['g++','-std=c++17','-DPS2_PROFILE=1','-Wall','-Wextra','-Wconversion','-Werror',*flags,'-I'+str(root/'src'),'-I'+str(out),str(root/'tests/frame_capture/test.cpp'),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
# The shared header must remain valid in C89, with no release-link dependency.
(out/'release.c').write_text('#include "ps2/frame_capture.h"\nint main(void) { PS2_FramePhase(PS2_FRAME_RENDER); return 0; }\n')
subprocess.run(['gcc','-std=c89','-pedantic','-Wall','-Wextra','-Werror','-DPS2_PROFILE=0','-I'+str(root/'src'),str(out/'release.c'),'-o',str(out/'release-test')],check=True)
