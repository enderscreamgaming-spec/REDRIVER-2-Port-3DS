# Nintendo 3DS Port

This tree contains a work-in-progress Nintendo 3DS backend for REDRIVER2.

## Layout

- `src_rebuild/platform/3ds/` contains the 3DS launcher, renderer, audio, input,
  filesystem shims, and Makefile.
- `src_rebuild/platform/3ds/shims/` contains small compatibility headers for
  desktop APIs used by the original rebuild.
- `src_rebuild/platform/3ds/tools/` contains helper scripts for checking the
  build environment, extracting data from a PlayStation image, and preparing an
  SD card layout.
- `src_rebuild/platform/3ds/packaging/` contains metadata used for CIA builds.

Local toolchains, emulators, disc dumps, SD-card staging folders, and compiled
outputs are intentionally ignored by Git.

## Requirements

- devkitPro with devkitARM and libctru
- `3dstools` from devkitPro for `.3dsx` builds
- `makerom` on `PATH` for `.cia` builds

## Build

From `src_rebuild/platform/3ds`:

```sh
make
```

To build an installable CIA:

```sh
make cia
```

Useful build options:

```sh
make RENDER_SCALE=3 DRAW_DISTANCE=34 FOG=1
make QUICK_DRIVE=1
make SMOKE_TEST=1
```

## SD Data

The executable expects the original game data at:

```text
sdmc:/3ds/redriver2/DRIVER2/
```

Prepare an SD layout on Windows:

```powershell
.\tools\package_sd.ps1 -SdRoot E:\ -DataSource C:\path\to\DRIVER2
```

`DataSource` may point to an extracted `DRIVER2` directory, a project directory
with `data\DRIVER2`, or a zip archive containing that layout.

## CIA Notes

The CIA contains the program only. It does not embed the original game data, so
the SD data folder above is still required after installing the CIA with FBI or a
compatible title manager.
