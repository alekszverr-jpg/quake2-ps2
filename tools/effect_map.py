"""Generate original MAP/WAL assets for the small PS2 flowing-surface fixture.

No commercial game assets are read or bundled. Compile with yquake2/maptools;
pass --verify after QBSP/QVIS/QRAD to check the compiled fixture.
"""
import argparse
from pathlib import Path
import re
import struct

FONT = {
    "A": [14,17,17,31,17,17,17], "C": [14,17,16,16,16,17,14],
    "F": [31,16,16,30,16,16,16], "H": [17,17,17,31,17,17,17],
    "M": [17,27,21,21,17,17,17], "N": [17,25,21,19,17,17,17],
    "D": [30,17,17,17,17,17,30], "E": [31,16,16,30,16,16,31],
    "G": [14,17,16,23,17,17,15], "I": [31,4,4,4,4,4,31],
    "L": [16,16,16,16,16,16,31], "O": [14,17,17,17,17,17,14],
    "P": [30,17,17,30,16,16,16], "Q": [14,17,17,17,21,18,13],
    "R": [30,17,17,30,20,18,17], "S": [15,16,16,14,1,1,30],
    "T": [31,4,4,4,4,4,4], "U": [17,17,17,17,17,17,14],
}


def wal(root, name, width, height, pixels):
    levels = [bytes(pixels)]
    for level in range(1, 4):
        w, h = width >> level, height >> level
        previous = levels[-1]
        levels.append(bytes(previous[(y*2)*(w*2)+x*2] for y in range(h) for x in range(w)))
    offsets, cursor = [], 100
    for level in levels:
        offsets.append(cursor)
        cursor += len(level)
    header = struct.pack("<32sII4I32siii", name.encode(), width, height, *offsets, b"", 0, 0, 0)
    target = root / "textures" / (name + ".wal")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(header + b"".join(levels))


def label(root, text):
    w, h = 128, 32
    pixels = [1] * (w*h)
    left = (w - len(text)*12 + 2)//2
    for letter, char in enumerate(text):
        for y, row in enumerate(FONT[char]):
            for x in range(5):
                if row & (1 << (4-x)):
                    for dy in range(2):
                        for dx in range(2):
                            pixels[(9+y*2+dy)*w + left+letter*12+x*2+dx] = 15
    wal(root, "ps2test/" + text.lower(), w, h, pixels)


def brush(lo, hi, texture="grid", flags=0, shift=(0, 0)):
    # Q2 MAP plane normal = cross(p0-p1, p2-p1); normals face outwards.
    lines = ["{"]
    for axis in range(3):
        for sign in (-1, 1):
            center = [(lo[i]+hi[i])//2 for i in range(3)]
            center[axis] = lo[axis] if sign < 0 else hi[axis]
            a, b = center.copy(), center.copy()
            a[(axis+1) % 3] += 64
            b[(axis+2) % 3] += 64*sign
            points = " ".join("( " + " ".join(map(str, p)) + " )" for p in (a, center, b))
            lines.append(f"{points} ps2test/{texture} {shift[0]} {shift[1]} 0 1 1 0 {flags} 0")
    return "\n".join(lines + ["}"])


def entity(properties, brushes=()):
    return "\n".join(["{", *(f'"{k}" "{v}"' for k, v in properties.items()), *brushes, "}"])


def diagnostic_models(root):
    """Original NPOT cutout sprite and two-frame MD2; no stock assets needed."""
    def tga(name, width, height, pixels):
        target = root / name
        target.parent.mkdir(parents=True, exist_ok=True)
        header = struct.pack('<BBBHHBHHHHBB', 0,0,2,0,0,0,0,0,width,height,32,0x28)
        target.write_bytes(header + bytes(pixels))

    frames = []
    for frame in range(2):
        w, h = 96, 80
        pixels = []
        for y in range(h):
            for x in range(w):
                radius = ((x-48)/42)**2 + ((y-40)/34)**2
                # Transparent border and central hole expose both alpha testing
                # and depth handling. Different frames keep the hole stationary.
                alpha = 255 if 0.15 < radius < 1 else 0
                b,g,r = (32,200,255) if frame == 0 else (255,180,32)
                if (x//8+y//8) % 2: b,g,r = b//2,g//2,r//2
                pixels += [b,g,r,alpha]
        name = f'sprites/ps2test/disc{frame}.tga'
        tga(name, w, h, pixels)
        frames.append(struct.pack('<4i64s', w,h,48,40,name.encode()))
    (root / 'sprites/ps2test/disc.sp2').write_bytes(struct.pack('<4sii',b'IDS2',2,2)+b''.join(frames))

    skin = 'models/ps2test/check.tga'
    pixels = []
    for y in range(64):
        for x in range(64):
            v = 240 if (x//8+y//8) % 2 else 48
            pixels += [v,v,v,255]
    tga(skin,64,64,pixels)
    st = struct.pack('<8h',0,0,63,0,63,63,0,63)
    quads = [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
    tris = b''.join(struct.pack('<6h',*(q[i] for i in indices),*indices)
                    for q in quads for indices in ((0,1,2),(0,2,3)))
    xyz = [(0,0,0),(96,0,0),(96,96,0),(0,96,0),
           (0,0,128),(96,0,128),(96,96,128),(0,96,128)]
    anim = b''.join(struct.pack('<6f16s',.5,.5,.5,-24,-24,-32,f'frame{i}'.encode()) +
                    b''.join(struct.pack('<4B',x,y,z-i*8 if z else z,5) for x,y,z in xyz)
                    for i in range(2))
    ofs_skin,ofs_st,ofs_tris,ofs_frames = 68,132,148,292
    ofs_gl = ofs_frames + len(anim)
    header = struct.pack('<4s16i',b'IDP2',8,64,64,72,1,8,4,12,1,2,
                         ofs_skin,ofs_st,ofs_tris,ofs_frames,ofs_gl,ofs_gl+4)
    (root / 'models/ps2test/cube.md2').write_bytes(header +
        struct.pack('<64s',skin.encode())+st+tris+anim+struct.pack('<i',0))


def generate(root):
    root.mkdir(parents=True, exist_ok=True)
    # QRAD requires a colormap even with zero bounces. This original grayscale
    # compiler-only PCX is NOT installed with the map or allowed to override
    # the user's game colormap; runtime uses the game's existing palette.
    header = bytearray(128)
    header[:4] = bytes((10,5,1,8))
    struct.pack_into("<6H",header,4,0,0,1,0,2,1)
    header[65] = 1
    struct.pack_into("<4H",header,66,2,1,2,1)
    palette = bytes(min(i*17,255) for i in range(256) for _ in range(3))
    (root / "pics").mkdir(exist_ok=True)
    (root / "pics/colormap.pcx").write_bytes(header + bytes((0,15,12)) + palette)
    wal(root, "ps2test/grid", 64, 64,
        [8 if x % 16 == 0 or y % 16 == 0 else 4 for y in range(64) for x in range(64)])
    wal(root, "ps2test/stripes", 64, 64,
        [15 if ((x//8) % 2) else 2 for y in range(64) for x in range(64)])
    for text in ("STATIC", "OPAQUE", "GLASS", "DOOR", "MODEL", "ALPHA", "SPRITE", "FADE"):
        label(root, text)
    diagnostic_models(root)

    solids = [
        brush((-400,-400,-16), (400,400,0)),
        brush((-400,-400,192), (400,400,208)),
        brush((-400,-400,0), (-384,400,192)),
        brush((384,-400,0), (400,400,192)),
        brush((-384,-400,0), (384,-384,192)),
        brush((-384,384,0), (384,400,192)),
    ]
    for x, flags, name in ((-248,0,"static"), (0,64,"opaque"), (248,96,"glass")):
        solids.append(brush((x-104,360,16), (x+104,368,128), "stripes", flags))
        solids.append(brush((x-64,356,144), (x+64,360,176), name, shift=(64-x,176)))
    # A stationary grid behind the transparent sample reveals its transparency.
    solids.append(brush((144,380,16), (352,384,128)))
    solids.append(brush((380,-64,144), (384,64,176), "door", shift=(64,176)))
    # New samples face the opposite side of the same room. The grid behind
    # them exposes transparency; a low foreground bar tests opaque occlusion.
    for x,name in ((-264,'model'),(-88,'alpha'),(88,'sprite'),(264,'fade')):
        solids.append(brush((x-64,-384,136),(x+64,-380,168),name,shift=(64-x,168)))
        solids.append(brush((x-56,-268,24),(x+56,-260,72)))
    entities = [entity({"classname":"worldspawn", "message":"PS2 Flowing Surface Test"}, solids),
                entity({"classname":"info_player_start", "origin":"0 -240 32", "angle":"90"})]
    for x in (-256,0,256):
        entities.append(entity({"classname":"light", "origin":f"{x} 128 160", "light":"400"}))
    entities.append(entity({"classname":"light", "origin":"0 -192 160", "light":"400"}))
    entities.append(entity({"classname":"func_door", "angle":"90", "speed":"64", "wait":"3", "lip":"8"},
                           [brush((368,-64,16), (376,64,128), "stripes", 64)]))
    for style,x in enumerate((-264,-88,88,264)):
        entities.append(entity({'classname':'misc_ps2sample','origin':f'{x} -312 88','style':str(style)}))
    # Overlap two faded sprites, nearer first: no depth writes must prevent
    # it from suppressing the later rear sample when the client groups models.
    entities.append(entity({'classname':'misc_ps2sample','origin':'276 -340 100','style':'3'}))
    maps = root / "maps"
    maps.mkdir(exist_ok=True)
    (maps / "ps2flow.map").write_text("// Original PS2 diagnostic room, generated by tools/effect_map.py\n" +
                                      "\n".join(entities) + "\n")
    print("Generated ps2flow MAP, original WALs, MD2 and NPOT cutout sprite in", root)


def verify(root):
    path = root / "maps/ps2flow.bsp"
    data = path.read_bytes()
    assert struct.unpack_from("<4si", data) == (b"IBSP",38)
    lumps = [struct.unpack_from("<ii", data, 8+i*8) for i in range(19)]
    for offset, size in lumps:
        assert offset >= 0 and size >= 0 and offset+size <= len(data)
    eo, es = lumps[0]
    entities = data[eo:eo+es].decode("ascii")
    assert '"info_player_start"' in entities and '"func_door"' in entities
    assert not re.search(r'"monster_', entities)
    assert entities.count('"misc_ps2sample"') == 5
    md2 = (root / 'models/ps2test/cube.md2').read_bytes()
    hdr = struct.unpack_from('<4s16i',md2)
    assert hdr[:2] == (b'IDP2',8) and hdr[10] == 2 and hdr[16] == len(md2)
    assert hdr[14]+hdr[4]*hdr[10] == hdr[15]
    for p in range(hdr[13],hdr[14],12):
        triangle = struct.unpack_from('<6h',md2,p)
        assert all(0 <= v < hdr[6] for v in triangle[:3])
        assert all(0 <= v < hdr[7] for v in triangle[3:])
    sp2 = (root / 'sprites/ps2test/disc.sp2').read_bytes()
    assert struct.unpack_from('<4sii',sp2) == (b'IDS2',2,2) and len(sp2) == 172
    for p in (12,92):
        w,h,ox,oy,name = struct.unpack_from('<4i64s',sp2,p)
        image = (root / name.split(b'\0')[0].decode()).read_bytes()
        assert struct.unpack_from('<HH',image,12) == (w,h)
        assert (w,h,ox,oy) == (96,80,48,40)
        assert set(image[21::4]) == {0,255} and len(image) == 18+w*h*4
    to, ts = lumps[5]
    texture_info = [struct.unpack_from("<8fii32si",data,p) for p in range(to,to+ts,76)]
    fo, fs = lumps[6]
    used = {}
    for p in range(fo,fo+fs,20):
        face = struct.unpack_from("<Hhihh4Bi",data,p)
        info = texture_info[face[4]]
        flags, name = info[8], info[10].split(b"\0")[0].decode()
        assert not flags & 8, "Fixture must not use SURF_WARP"
        used[flags] = used.get(flags,0)+1
        assert (root / "textures" / (name+".wal")).is_file(), name
        if not flags & 48:
            assert face[-1] >= 0, "Opaque test faces require lightmaps"
    assert all(used.get(flags,0)>0 for flags in (0,64,96)), used
    assert lumps[7][1] > 0, "Missing lightmaps"
    assert lumps[3][1] > 0, "Missing visibility"
    assert lumps[13][1] == 96, "Expected world plus one inline door model"
    assert not (root / "maps/ps2flow.lin").exists(), "Leaking room"
    # Check collision leaf contents at the player's center and hull corners.
    po, _ = lumps[1]; no, _ = lumps[4]; lo, _ = lumps[8]; mo, _ = lumps[13]
    head = struct.unpack_from("<i",data,mo+36)[0]
    for x in (-16,0,16):
        for y in (-256,-240,-224):
            for z in (8,32,64):
                node = head
                while node >= 0:
                    plane, front, back = struct.unpack_from("<iii",data,no+node*28)
                    nx,ny,nz,dist = struct.unpack_from("<4f",data,po+plane*20)
                    node = front if nx*x+ny*y+nz*z >= dist else back
                contents = struct.unpack_from("<i",data,lo+(-1-node)*28)[0]
                assert not contents & 1, "Player hull starts in solid"
    print("Verified sealed BSP38: spawn collision, PVS, lighting, inline door and face flags",used)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="baseq2 output directory")
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    if args.verify:
        verify(args.output)
    else:
        generate(args.output)
