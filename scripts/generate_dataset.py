"""Generate distinct, deterministic uncompressed 24-bit BMPs without dependencies."""
import argparse
import random
import struct
from pathlib import Path


def generate(directory, count, width, height, seed):
    if min(count, width, height) < 1:
        raise ValueError('count, width and height must be positive')
    directory.mkdir(parents=True, exist_ok=True)
    paths = [directory / f'image_{i:05d}.bmp' for i in range(count)]
    if any(path.exists() for path in paths):
        raise ValueError('dataset already exists; choose a new directory')
    stride = (width * 3 + 3) & ~3
    size = stride * height
    header = struct.pack('<2sIHHIIIIHHIIIIII', b'BM', 54 + size, 0, 0, 54,
                         40, width, height, 1, 24, 0, size, 2835, 2835, 0, 0)
    rng = random.Random(seed)
    for path in paths:
        with path.open('wb') as target:
            target.write(header)
            for _ in range(height):
                target.write(rng.randbytes(width * 3))
                target.write(bytes(stride - width * 3))
    print(f'Generated {count} BMPs: {width}x{height}, seed={seed}, {directory}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--count', type=int, default=100)
    parser.add_argument('--width', type=int, default=1920)
    parser.add_argument('--height', type=int, default=1080)
    parser.add_argument('--seed', type=int, default=42)
    args = parser.parse_args()
    generate(args.directory, args.count, args.width, args.height, args.seed)
