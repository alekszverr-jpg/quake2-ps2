"""Apply the single audited 64-bit alignment fix to pinned legacy QRAD."""
from pathlib import Path
import sys

root = Path(sys.argv[1])
path = root / "src/qrad3/trace.c"
source = path.read_text()
old = "tnodes = (tnode_t *)(((int)tnodes + 31)&~31);"
new = "tnodes = (tnode_t *)(((uintptr_t)tnodes + 31u) & ~(uintptr_t)31u);"
assert source.count(old) == 1, "Pinned QRAD alignment site changed"
path.write_text("#include <stdint.h>\n" + source.replace(old,new))
(root / "release").mkdir(exist_ok=True)
print("Patched QRAD pointer alignment to uintptr_t; source otherwise unchanged")
