"""Test allocation-free effects and the production dynamic triangle path."""
import os
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / "build/view-effects-host"
out.mkdir(parents=True, exist_ok=True)
source = (root / "src/ps2/renderer/render_view.cpp").read_text()
functions = []
for signature in ["u32 AddWorldLights", "u32 SelectTriangleLights", "void SelectSurfaceLights", "float DynamicTriangleEdgeSquared", "void SubmitDynamicallyLitTriangle",
                  "u8 ViewBlendByte", "void RenderViewBlend", "math::Vec3 AliasShellColor",
                  "math::Vec3 AliasShellOffset", "math::Vec4 AliasShellVertexColor",
                  "void DrawTranslucentSurface"]:
    match = re.search(r"^" + re.escape(signature) + r"\(.*?^\}", source, re.M | re.S)
    if not match:
        raise RuntimeError("Cannot extract " + signature)
    functions.append(match.group(0))
(out / "effects.inc").write_text("\n".join(functions))
binary = out / ("test.exe" if os.name == "nt" else "test")
flags = [] if os.name == "nt" else ["-fsanitize=address,undefined"]
subprocess.run([os.environ.get("CXX", "g++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-Wconversion", "-Wsign-conversion", *flags,
                "-I" + str(root / "src"), "-I" + str(out),
                str(root / "tests/view_effects/test.cpp"), "-o", str(binary)], check=True)
subprocess.run([str(binary)], check=True)
