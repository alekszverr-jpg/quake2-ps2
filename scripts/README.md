# Helper scripts

## prepare_baseq2.ps1

Prepares the Quake II game data directory used by the PS2 port.

The script copies the contents of `Quake2Game/BASEQ2` into `baseq2/` and creates the target directory if needed.

Usage from PowerShell:

```powershell
./scripts/prepare_baseq2.ps1
```

Optional arguments:

```powershell
./scripts/prepare_baseq2.ps1 <source-path> <target-path>
```

If the source or target paths are relative, they are resolved relative to the repository root.

## check_baseq2.ps1

Checks whether the Quake II game data directory exists and whether the expected file layout is present.

Usage from PowerShell:

```powershell
./scripts/check_baseq2.ps1
```

Expected files:
- baseq2/pak0.pak
- baseq2/pak1.pak (optional)
- baseq2/video/
- baseq2/players/
