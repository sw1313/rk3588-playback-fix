"""Build two API32 diagnostics, without packaging firmware or installing anything."""
import argparse
from pathlib import Path
import subprocess
import os


def compile_command(args, log):
    result = subprocess.run([str(a) for a in args], capture_output=True, text=True)
    log.write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise SystemExit(f"Compilation failed; see {log}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("ndk", "headers", "private-libs", "drm-headers", "work"):
        parser.add_argument("--" + name, type=Path, required=True)
    a = parser.parse_args()
    source = Path(__file__).resolve().parent
    work = a.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    host = "windows-x86_64" if os.name == "nt" else "darwin-x86_64" if os.uname().sysname == "Darwin" else "linux-x86_64"
    ext = ".exe" if os.name == "nt" else ""
    llvm = a.ndk.resolve() / "toolchains/llvm/prebuilt" / host / "bin"
    h = a.headers.resolve()
    v = a.private_libs.resolve()
    libraries = [v / name for name in (
        "libui.so", "android.hardware.graphics.mapper@4.0.so", "libhidlbase.so",
        "libutils.so", "libcutils.so", "libc++.so")]
    if not all(p.is_file() for p in libraries):
        raise SystemExit("Missing original API32 ARM64 private link inputs")
    include_names = {"include", "include_vndk", "include_types", "include_private", "include_outside_system"}
    includes = sorted(p for p in h.rglob("*") if p.is_dir() and p.name in include_names
                      and "external/libcxx" not in p.as_posix() and "bionic" not in p.as_posix())
    includes += sorted(p for p in (h / "generated-headers").rglob("gen") if (p / "android").exists())
    includes += [h / "system/libfmq/base", h / "frameworks/native/libs/binder/ndk/include_cpp",
                 h / "frameworks/native/libs/binder/ndk/include_ndk"]
    common = ["--target=aarch64-linux-android32", "-O2"]
    args = [llvm / ("clang++" + ext), *common, "-std=c++17", "-fno-exceptions", "-fno-rtti",
            "-nostdinc++", "-nostdlib++", "-DANDROID", "-Wno-deprecated-declarations",
            "-isystem" + str(h / "external/libcxx/include"),
            "-isystem" + str(h / "generated-headers/bionic/libc/libc/android_vendor.32_arm_armv8-a_shared/gen/include")]
    args += ["-I" + str(p) for p in includes]
    args += [source / "mapper_hdr_probe.cpp", "-o", work / "mapper_hdr_probe", "-Wl,--no-undefined", *libraries]
    compile_command(args, work / "mapper-build.log")
    compile_command([llvm / ("clang" + ext), *common, "-Wall", "-Wextra", "-Werror",
                     "-I" + str(a.drm_headers.resolve()), source / "drm_hdr_probe.c",
                     "-o", work / "drm_hdr_probe"], work / "drm-build.log")
    print("Built diagnostics only:", work)


if __name__ == "__main__":
    main()
