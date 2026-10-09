import os,re,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out=root/'build/frame-capture-host'; out.mkdir(parents=True,exist_ok=True)
src=(root/'src/ps2/renderer/ref.cpp').read_text()
names=['RestoreFrameProfiles','PrepareFrameCapture','CollectFrameCapture','DrawSceneCaptureResult','DrawRenderCaptureResult','DrawFrameCaptureResult']
text='\n'.join(re.search(r'^void '+name+r'\(\).*?^\}',src,re.M|re.S).group(0) for name in names)
text+='\n'+re.search(r'^extern "C" void PS2_FramePhase\(int phase\).*?^\}',src,re.M|re.S).group(0)
text+='\n'+re.search(r'^extern "C" void PS2_VramCaptureEligible\(int eligible\).*?^\}',src,re.M|re.S).group(0)
text+='\n'+re.search(r'^extern "C" void PS2_FrameSoundIO\(unsigned ticks\).*?^\}',src,re.M|re.S).group(0)
text+='\n'+re.search(r'^extern "C" void PS2_FrameRenderPart\(int part\).*?^\}',src,re.M|re.S).group(0)
for name,args in [('PS2_FrameScenePart','int part'),('PS2_FrameSceneCounts','int entities,int particles,int lights')]:
 text+='\n'+re.search(r'^extern "C" void '+name+r'\('+re.escape(args)+r'\).*?^\}',src,re.M|re.S).group(0)
(out/'production.inc').write_text(text)
end=src[src.index('void PS2_EndFrame()'):]; assert end.index('ps2::gs::EndFrame();')<end.index('CollectFrameCapture();')
assert 's_frameRequest->value==0.0f' in src
common=(root/'src/common/common.c').read_text()
assert re.search(r'PS2_FramePhase\(PS2_FRAME_SERVER\);\s*SV_Frame\(msec\);\s*PS2_FramePhase\(PS2_FRAME_OTHER\)',common)
assert re.search(r'PS2_FramePhase\(PS2_FRAME_CLIENT\);\s*CL_Frame\(msec\);\s*PS2_FramePhase\(PS2_FRAME_OTHER\)',common)
menu=(root/'src/client/menu.c').read_text(); assert '"capture frame 10s"' in menu and 'mode >= 5 && mode <= 7 ? 1 : 0' in menu
flags=[] if os.name=='nt' else ['-fsanitize=address,undefined']
exe=out/('test.exe' if os.name=='nt' else 'test')
subprocess.run(['g++','-std=c++17','-DPS2_PROFILE=1','-Wall','-Wextra','-Wconversion','-Werror',*flags,'-I'+str(root/'src'),'-I'+str(out),str(root/'tests/frame_capture/test.cpp'),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
# The shared header must remain valid in C89, with no release-link dependency.
(out/'release.c').write_text('#include "ps2/frame_capture.h"\nint main(void) { PS2_FramePhase(PS2_FRAME_RENDER); PS2_FrameRenderPart(PS2_RENDER_BEGIN); PS2_FrameScenePart(PS2_SCENE_CAMERA); PS2_FrameSceneCounts(1,2,3); return 0; }\n')
subprocess.run(['gcc','-std=c89','-pedantic','-Wall','-Wextra','-Werror','-DPS2_PROFILE=0','-I'+str(root/'src'),str(out/'release.c'),'-o',str(out/'release-test')],check=True)

screen=(root/'src/client/cl_scrn.c').read_text()
assert screen.count('PS2_VramCaptureEligible(0);')==2
assert 'cl.refresh_prepped && !scr_draw_loading && !cl.cinematictime' in screen
assert 'if (!s_captureEligible) CollectFrameCapture();' in src

assert '!= 0.0F ? 6 : 5;' in menu and '"capture render 10s"' in menu

gs=(root/'src/ps2/renderer/gs.cpp').read_text()
flush=gs[gs.index('void FlushPending2D()'):gs.index('bool In2DMode()')]
assert flush.index('vu1::Flush();')<flush.index('overlayStart=timing::Now()')<flush.index('pkt.SendNormal();')<flush.index('overlaySubmitMicros+=')
assert 's_timingStats = {};' in gs
assert 'PS2_FrameRenderPart(PS2_RENDER_BEGIN)' in src and 'PS2_FrameRenderPart(PS2_RENDER_VIEW)' in src
assert 'PS2_FrameRenderPart(PS2_RENDER_3D)' in src and 'PS2_FrameRenderPart(PS2_RENDER_HUD)' in src

ents=(root/'src/client/cl_ents.c').read_text()
for stage,call in [('CAMERA','CL_CalcViewValues();'),('OBJECTS','CL_AddPacketEntities(&cl.frame);'),('EFFECTS','CL_AddTEnts();'),('PARTICLES','CL_AddParticles();'),('LIGHTS','CL_AddDLights();'),('STYLES','CL_AddLightStyles();')]:
 assert ents.index('PS2_FrameScenePart(PS2_SCENE_'+stage+')')<ents.index('    '+call)
view=(root/'src/client/cl_view.c').read_text()
assert view.index('PS2_FrameScenePart(PS2_SCENE_SORT)')<view.index('        qsort(cl.refdef.entities,')<view.index('PS2_FrameSceneCounts(')<view.index('    re.RenderFrame(&cl.refdef);')
assert '"capture scene 10s"' in menu and '>= 2.0F ? 7 :' in menu
