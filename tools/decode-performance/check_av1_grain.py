"""Compare normalized hardware output with a libdav1d film-grain reference.

Hardware input is planar Y/U/V, every sample stored as a little-endian uint16,
as emitted by the component precision probe. Eight-bit samples are 0..255;
ten-bit samples are 0..1023. No scaling, shifts or error tolerance are applied.
This checks picture precision, not playback cadence or HDMI signaling.
"""
import argparse
import array
import hashlib
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("encoded", type=Path)
    parser.add_argument("hardware", type=Path)
    parser.add_argument("--depth", type=int, choices=[8, 10], required=True)
    parser.add_argument("--grain", type=int, choices=[0, 1], required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="av1-grain-reference-") as work:
        reference = Path(work) / "reference.raw"
        subprocess.run([
            "ffmpeg", "-v", "error", "-y", "-c:v", "libdav1d",
            "-filmgrain", str(args.grain), "-i", str(args.encoded),
            "-pix_fmt", "yuv420p10le" if args.depth == 10 else "yuv420p",
            "-f", "rawvideo", str(reference),
        ], check=True)
        expected = reference.read_bytes()
        if args.depth == 8:
            samples = array.array("H", list(expected))
            import sys
            if sys.byteorder != "little":
                samples.byteswap()
            expected = samples.tobytes()
        actual = args.hardware.read_bytes()
        if actual != expected:
            raise SystemExit("FAIL: hardware pixels differ from the requested grain reference")
        print("PASS: all samples exact; bytes=", len(actual),
              "sha256=", hashlib.sha256(actual).hexdigest())


if __name__ == "__main__":
    main()
