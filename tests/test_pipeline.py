"""Integration checks against an independent pixel-level reference."""
import importlib.util
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / ('filter.exe' if (ROOT / 'filter.exe').exists() else 'filter')
spec = importlib.util.spec_from_file_location('generate', ROOT / 'scripts/generate_dataset.py')
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


def pixels(path):
    data = path.read_bytes()
    width, height = struct.unpack_from('<ii', data, 18)
    stride = (width * 3 + 3) & ~3
    offset = struct.unpack_from('<I', data, 10)[0]
    return [[list(data[offset + y * stride + x * 3:offset + y * stride + x * 3 + 3])
             for x in range(width)] for y in reversed(range(height))]


def reference(image, name):
    height, width = len(image), len(image[0])
    kernels = {'blur': [1] * 9,
               'sharpen': [0, -1, 0, -1, 5, -1, 0, -1, 0],
               'edges': [-1, -1, -1, -1, 8, -1, -1, -1, -1]}
    result = []
    for y in range(height):
        row = []
        for x in range(width):
            if name == 'gray':
                row.append([sum(image[y][x]) // 3] * 3)
            elif name == 'exposure':
                row.append([min(255, channel * 2) for channel in image[y][x]])
            else:
                kernel = kernels[name]
                values = [image[min(height - 1, max(0, y + dy))][min(width - 1, max(0, x + dx))]
                          for dy in (-1, 0, 1) for dx in (-1, 0, 1)]
                row.append([min(255, max(0, int(sum(p[c] * k for p, k in zip(values, kernel)) /
                                               (9 if name == 'blur' else 1)))) for c in range(3)])
        result.append(row)
    return result


class PipelineTests(unittest.TestCase):
    def test_filters_and_execution_modes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / 'input'
            generator.generate(source, 5, 7, 3, 42)
            expected = {}
            for path in sorted(source.glob('*.bmp')):
                image = pixels(path)
                for name in ('blur', 'sharpen', 'gray', 'exposure', 'edges'):
                    for threads in (1, 2, 4, 8):
                        out = root / 'single.bmp'
                        subprocess.run([BINARY, path, out, name, str(threads)], check=True, capture_output=True)
                        self.assertEqual(pixels(out), reference(pixels(path), name))
                    image = reference(image, name)
                expected[path.name] = image
            for mode in ('file-first', 'stage-first', 'pipeline'):
                for threads in (1, 2, 4, 8):
                    out = root / f'{mode}-{threads}'
                    command = [BINARY, '--batch', source, out, mode, str(threads), '--csv']
                    result = subprocess.run(command, check=True, capture_output=True, text=True)
                    fields = result.stdout.strip().split(',')
                    self.assertEqual(fields[:3], [mode, str(threads), '5'])
                    self.assertEqual(len(fields), 7)
                    self.assertTrue(all(float(value) >= 0 for value in fields[3:]))
                    for path in out.glob('*.bmp'):
                        self.assertEqual(pixels(path), expected[path.name])
                    self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)

    def test_invalid_arguments(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / 'input'
            generator.generate(source, 1, 3, 3, 42)
            image = next(source.glob('*.bmp'))
            output = Path(tmp) / 'output'
            batch = [BINARY, '--batch', source, output]
            commands = [batch + ['bad-mode', '1', '--csv'],
                        batch + ['file-first', '1', '--bad-flag'],
                        batch + ['file-first', '1']]
            for threads in ('0', '-2', '2x', 'abc'):
                commands.append([BINARY, image, output, 'blur', threads])
                commands.append(batch + ['file-first', threads, '--csv'])
            for command in commands:
                with self.subTest(command=command):
                    result = subprocess.run(command, capture_output=True)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertFalse(output.exists())

    def test_demo_rejects_extra_arguments(self):
        with tempfile.TemporaryDirectory() as tmp:
            samples = Path(tmp) / 'samples'
            samples.mkdir()
            result = subprocess.run([BINARY, '--demo', 'unexpected'], cwd=tmp, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(list(samples.iterdir()), [])

    def test_invalid_bmp(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'bad.bmp'
            path.write_bytes(b'BM')
            result = subprocess.run([BINARY, path, Path(tmp) / 'out.bmp', 'blur'], capture_output=True)
            self.assertNotEqual(result.returncode, 0)


if __name__ == '__main__':
    unittest.main()
