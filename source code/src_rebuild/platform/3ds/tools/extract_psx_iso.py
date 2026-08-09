#!/usr/bin/env python3
import argparse
import os
from pathlib import Path


SECTOR_CANDIDATES = (
    (2352, 24),  # PlayStation MODE2/2352 Form 1 user data
    (2352, 16),  # MODE1/2352 user data
    (2048, 0),   # plain ISO
)


class IsoReader:
    def __init__(self, image_path):
        self.image_path = Path(image_path)
        self.file = self.image_path.open("rb")
        self.sector_size, self.data_offset = self._detect_layout()

    def close(self):
        self.file.close()

    def _detect_layout(self):
        for sector_size, data_offset in SECTOR_CANDIDATES:
            self.file.seek(16 * sector_size + data_offset)
            pvd = self.file.read(2048)
            if len(pvd) >= 7 and pvd[1:6] == b"CD001":
                return sector_size, data_offset

        raise RuntimeError("Could not find an ISO9660 primary volume descriptor")

    def read_extent(self, lba, size):
        remaining = size
        sector = lba
        while remaining > 0:
            self.file.seek(sector * self.sector_size + self.data_offset)
            data = self.file.read(2048)
            chunk = data[:min(remaining, 2048)]
            if not chunk:
                raise RuntimeError(f"Short read at LBA {sector}")
            yield chunk
            remaining -= len(chunk)
            sector += 1

    def read_extent_bytes(self, lba, size):
        return b"".join(self.read_extent(lba, size))

    def root_record(self):
        pvd = self.read_extent_bytes(16, 2048)
        return DirectoryRecord.from_bytes(pvd[156:156 + pvd[156]])


class DirectoryRecord:
    def __init__(self, name, lba, size, flags):
        self.name = name
        self.lba = lba
        self.size = size
        self.flags = flags

    @property
    def is_dir(self):
        return (self.flags & 2) != 0

    @staticmethod
    def from_bytes(data):
        if not data or data[0] == 0:
            return None

        name_len = data[32]
        raw_name = data[33:33 + name_len]

        if raw_name == b"\x00":
            name = "."
        elif raw_name == b"\x01":
            name = ".."
        else:
            name = raw_name.decode("ascii", errors="replace")
            name = name.split(";", 1)[0]

        return DirectoryRecord(
            name=name,
            lba=int.from_bytes(data[2:6], "little"),
            size=int.from_bytes(data[10:14], "little"),
            flags=data[25],
        )


def sanitize_iso_name(name):
    invalid = '<>:"/\\|?*'
    cleaned = "".join("_" if ch in invalid or ord(ch) < 32 else ch for ch in name)
    cleaned = cleaned.strip()
    if cleaned in ("", ".", ".."):
        raise RuntimeError(f"Unsafe ISO name: {name!r}")
    return cleaned


def iter_directory(reader, record):
    data = reader.read_extent_bytes(record.lba, record.size)
    offset = 0
    while offset < len(data):
        length = data[offset]
        if length == 0:
            offset = ((offset // 2048) + 1) * 2048
            continue

        child = DirectoryRecord.from_bytes(data[offset:offset + length])
        offset += length
        if child and child.name not in (".", ".."):
            yield child


def find_path(reader, root_record, iso_path):
    current = root_record
    parts = [part for part in iso_path.replace("\\", "/").split("/") if part]
    for part in parts:
        match = None
        for child in iter_directory(reader, current):
            if child.name.upper() == part.upper():
                match = child
                break
        if not match:
            raise RuntimeError(f"Path not found in image: {iso_path}")
        current = match
    return current


def extract_record(reader, record, output_dir):
    output_dir = Path(output_dir)

    if record.is_dir:
        output_dir.mkdir(parents=True, exist_ok=True)
        for child in iter_directory(reader, record):
            child_name = sanitize_iso_name(child.name)
            extract_record(reader, child, output_dir / child_name)
        return

    output_dir.parent.mkdir(parents=True, exist_ok=True)
    with output_dir.open("wb") as out:
        for chunk in reader.read_extent(record.lba, record.size):
            out.write(chunk)


def main():
    parser = argparse.ArgumentParser(description="Extract files from a PSX ISO/BIN image.")
    parser.add_argument("image", help="Path to .bin or .iso image")
    parser.add_argument("--out", required=True, help="Output directory")
    parser.add_argument("--path", default="", help="Optional ISO path to extract, for example DRIVER2")
    args = parser.parse_args()

    reader = IsoReader(args.image)
    try:
        root = reader.root_record()
        record = find_path(reader, root, args.path) if args.path else root
        extract_record(reader, record, args.out)
        print(f"Extracted {args.path or '/'} to {os.path.abspath(args.out)}")
        print(f"Detected sector layout: {reader.sector_size} bytes, data offset {reader.data_offset}")
    finally:
        reader.close()


if __name__ == "__main__":
    main()
