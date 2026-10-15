"""Build pinned MPP Android libraries and a guarded Magisk package.

Requires JDK-independent CMake, Ninja, Git, Python 3 and a complete Android NDK.
Vendor libraries are not copied into this repository or the resulting ZIP.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

REVISION = "c08762ebfadeb4e986d2fed993bc7a54862d3ebe"
TAG = "1.1.0"
ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "magisk-mpp"


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ndk", default=os.environ.get("ANDROID_NDK_HOME"), required=not os.environ.get("ANDROID_NDK_HOME"))
    parser.add_argument("--work", type=Path, default=ROOT / "build" / "mpp")
    args = parser.parse_args()
    ndk = Path(args.ndk).resolve()
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    source = work / "source"
    if not source.exists():
        run("git", "clone", "--depth", "1", "--branch", TAG, "https://github.com/rockchip-linux/mpp.git", source)
    actual = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if actual != REVISION:
        raise SystemExit("MPP source does not match the pinned revision")
    if subprocess.check_output(["git", "-C", str(source), "diff", "--name-only"], text=True).strip():
        raise SystemExit("Tracked MPP source modifications are not supported by this build")
    # Keep the verified upstream checkout clean; build compatibility additions
    # from a separate source tree so rebuilding cannot modify release inputs.
    patched = work / "source-compat"
    if patched.exists():
        raise SystemExit("Use a fresh work directory for the compatibility build")
    shutil.copytree(source, patched, ignore=shutil.ignore_patterns(".git"))
    for patch in sorted((MODULE / "compat").glob("*.patch")):
        run("git", "-C", patched, "--git-dir=" + str(source / ".git"),
            "--work-tree=" + str(patched), "apply", patch)
    shutil.copyfile(MODULE / "compat" / "mpi_legacy_prep.h", patched / "mpp" / "mpi_legacy_prep.h")
    # Upstream uses .git to generate its runtime revision. A source-only copy
    # has no VCS directory, so declare the already-verified base plus patches.
    cmake_file = patched / "CMakeLists.txt"
    cmake_text = cmake_file.read_text(encoding='utf-8')
    needle = 'set(VERSION_INFO        "\\\"unknown mpp version for missing VCS info\\\"")'
    replacement = 'set(VERSION_INFO "\\\"c08762eb MPP 1.1.0 + RK3588 compatibility 0.4.0\\\"")'
    if cmake_text.count(needle) != 1:
        raise SystemExit("Unexpected upstream revision-generation template")
    cmake_file.write_bytes(cmake_text.replace(needle, replacement).encode())
    source = patched
    toolchain = ndk / "build" / "cmake" / "android.toolchain.cmake"
    host = "windows-x86_64" if os.name == "nt" else "darwin-x86_64" if os.uname().sysname == "Darwin" else "linux-x86_64"
    llvm = ndk / "toolchains" / "llvm" / "prebuilt" / host
    extension = ".exe" if os.name == "nt" else ""
    (MODULE / "bin").mkdir(exist_ok=True)
    maps = " ".join("-ffile-prefix-map=" + path.as_posix() + "=" + label for path, label in [(source, "mpp"), (work, "build"), (ROOT, "module")])
    for label, abi, target, library_dir, probe in [
        ("arm64", "arm64-v8a", "aarch64-linux-android32", "lib64", "probe64"),
        ("arm", "armeabi-v7a", "armv7a-linux-androideabi32", "lib", "probe32"),
    ]:
        build = work / label
        run("cmake", "-S", source, "-B", build, "-G", "Ninja",
            "-DCMAKE_TOOLCHAIN_FILE=" + str(toolchain), "-DANDROID_ABI=" + abi,
            "-DANDROID_PLATFORM=android-32", "-DANDROID_STL=c++_static", "-DCMAKE_BUILD_TYPE=Release",
            "-DMPP_PROJECT_NAME=mpp", "-DVPU_PROJECT_NAME=vpu", "-DBUILD_TEST=OFF",
            "-DCMAKE_C_FLAGS=" + maps, "-DCMAKE_CXX_FLAGS=" + maps)
        run("cmake", "--build", build, "--target", "mpp", "vpu", "--parallel", "6")
        destination = MODULE / "system" / "vendor" / library_dir
        destination.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(build / "mpp" / "libmpp.so", destination / "libmpp.so")
        shutil.copyfile(build / "mpp" / "legacy" / "libvpu.so", destination / "libvpu.so")
        run(llvm / "bin" / ("llvm-strip" + extension), "--strip-unneeded", destination / "libmpp.so")
        run(llvm / "bin" / ("llvm-strip" + extension), "--strip-unneeded", destination / "libvpu.so")
        run(llvm / "bin" / ("clang" + extension), "--target=" + target,
            "--sysroot=" + str(llvm / "sysroot"), "-O2", "-fPIE", "-pie", "-s",
            "-I" + str(source / "inc"), "-ffile-prefix-map=" + ROOT.as_posix() + "=module",
            MODULE / "src" / "compat_probe.c", "-ldl", "-o", MODULE / "bin" / probe)
        run(llvm / "bin" / ("clang" + extension), "--target=" + target,
            "--sysroot=" + str(llvm / "sysroot"), "-O2", "-fPIE", "-pie", "-s",
            "-I" + str(source / "inc"), "-ffile-prefix-map=" + ROOT.as_posix() + "=module",
            MODULE / "src" / "hdr_lifetime_probe.c", "-ldl", "-o",
            MODULE / "bin" / ("hdr_lifetime" + ("64" if label == "arm64" else "32")))
    (MODULE / "licenses").mkdir(exist_ok=True)
    for name in ("Apache-2.0", "MIT"):
        shutil.copyfile(source / "LICENSES" / name, MODULE / "licenses" / name)
    package()


def package():
    for name in ("system/vendor/lib64/libmpp.so", "system/vendor/lib/libmpp.so", "system/vendor/lib64/libvpu.so", "system/vendor/lib/libvpu.so", "bin/probe64", "bin/probe32", "contracts/arm64-symbols.txt", "contracts/arm-symbols.txt", "contracts/vendor.sha256"):
        if not (MODULE / name).is_file():
            raise SystemExit("Missing module payload: " + name)
    active = [MODULE / "system" / "vendor" / abi / lib for abi in ("lib", "lib64") for lib in ("libmpp.so", "libvpu.so")]
    (MODULE / "contracts" / "active.sha256").write_bytes("".join(hashlib.sha256(p.read_bytes()).hexdigest() + "  /vendor/" + p.parent.name + "/" + p.name + "\n" for p in active).encode())
    paths = sorted(path for path in MODULE.rglob("*") if path.is_file() and path.name not in {"checksums.sha256", "status.txt", "disable", "resume-codec2"})
    # Hash the same LF-normalized bytes that Magisk will extract.
    for path in paths:
        if path.suffix in {".sh", ".md", ".txt", ".prop", ".sha256", ".c", ".json"}:
            data = path.read_bytes().replace(b"\r\n", b"\n")
            path.write_bytes(data)
    checksums = "".join(hashlib.sha256(path.read_bytes()).hexdigest() + "  " + path.relative_to(MODULE).as_posix() + "\n" for path in paths)
    (MODULE / "checksums.sha256").write_bytes(checksums.encode("ascii"))
    paths.append(MODULE / "checksums.sha256")
    destination = ROOT / "dist"
    destination.mkdir(exist_ok=True)
    properties = dict(line.split("=", 1) for line in (MODULE / "module.prop").read_text(encoding='utf-8').splitlines() if "=" in line)
    archive = destination / ("rk3588-mpp-upgrade-" + properties["version"] + ".zip")
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as output:
        for path in paths:
            name = path.relative_to(MODULE).as_posix()
            data = path.read_bytes()
            if path.suffix in {".sh", ".md", ".txt", ".prop", ".sha256", ".c", ".json"}:
                data = data.replace(b"\r\n", b"\n")
            info = zipfile.ZipInfo(name, (2026, 8, 11, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = (0o100755 if path.suffix == ".sh" or name.startswith("bin/") else 0o100644) << 16
            output.writestr(info, data)
    print(archive)
    print("SHA256 " + hashlib.sha256(archive.read_bytes()).hexdigest())


if __name__ == "__main__":
    main()
