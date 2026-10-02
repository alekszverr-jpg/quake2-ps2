import os
from pathlib import Path
import re
import subprocess
root = Path(__file__).resolve().parents[2]
out = root / "build/video-menu-host"
out.mkdir(parents=True, exist_ok=True)
source = (root / "src/client/qmenu.c").read_text()
functions = []
for signature in ["void Slider_DoSlide", "void SpinControl_DoSlide", "void Menu_SlideItem"]:
    match = re.search(r"^" + signature + r"\(.*?^\}", source, re.M | re.S)
    if not match: raise RuntimeError(signature)
    functions.append(match.group(0))
(out / "slide.inc").write_text("\n".join(functions))
gs_source = (root / "src/ps2/renderer/gs.cpp").read_text()
depth = re.search(r"^int AllocateDepthBuffer\(.*?^\}", gs_source, re.M | re.S)
if not depth: raise RuntimeError("AllocateDepthBuffer")
(out / "depth.inc").write_text(depth.group(0))
ref_source = (root / "src/ps2/renderer/ref.cpp").read_text()
glyph = re.search(r"^void DrawGlyph\(.*?^\}", ref_source, re.M | re.S)
if not glyph: raise RuntimeError("DrawGlyph")
(out / "glyph.inc").write_text(glyph.group(0))
weapon_source = (root / "src/client/cl_ents.c").read_text()
weapon = re.search(r"^void CL_AddViewWeapon\(.*?^\}", weapon_source, re.M | re.S)
if not weapon: raise RuntimeError("CL_AddViewWeapon")
(out / "weapon.inc").write_text(weapon.group(0))
binary = out / ("test.exe" if os.name == "nt" else "test")
flags = [] if os.name == "nt" else ["-fsanitize=address,undefined"]
subprocess.run([os.environ.get("CXX", "g++"), "-std=c++17", "-Wall", "-Wextra", "-Werror", *flags,
               "-I" + str(root / "src"), "-I" + str(out), str(root / "tests/video_menu/test.cpp"),
               "-o", str(binary)], check=True)
subprocess.run([str(binary)], check=True)
