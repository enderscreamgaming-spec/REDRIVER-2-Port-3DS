# REDRIVER2 3DS

This directory contains the Nintendo 3DS backend for REDRIVER2.

## Files

- `redriver2_3ds.cpp` starts libctru, configures the runtime, and enters the
  game loop.
- `psyx_3ds_*` files provide the 3DS Psy-X platform layer.
- `shims/` contains compatibility headers used by the rebuild.
- `tools/` contains local helper scripts.
- `packaging/` contains metadata for CIA builds.

## Build

```sh
make
```

Build an installable CIA:

```sh
make cia
```

Build options can be overridden from the command line:

```sh
make RENDER_SCALE=3 DRAW_DISTANCE=34 FOG=1
make QUICK_DRIVE=1
make SMOKE_TEST=1
```

## Windows Helpers

Check the 3DS build environment:

```powershell
.\tools\check_3ds_env.ps1
```

Prepare an SD layout:

```powershell
.\tools\package_sd.ps1 -SdRoot E:\ -DataSource C:\path\to\DRIVER2
```

Extract files from a PlayStation disc image:

```sh
python tools/extract_psx_iso.py C:/path/to/disc.bin --path DRIVER2 --out C:/path/to/output
```

## Game Data

The executable expects the original game data at:

```text
sdmc:/3ds/redriver2/DRIVER2/
```

The CIA contains the program only. Keep the `DRIVER2` data folder on the SD card
when launching the installed title.
