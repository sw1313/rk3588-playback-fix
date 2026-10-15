"""Build RGA 1.10.6 against Android 12 VNDK headers and verified vendor link libraries.

Only upstream RGA libraries and our probes enter the ZIP. The vendor libraries
are link inputs; they are never redistributed. Install contracts must be captured
from the verified device before packaging.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "magisk-rga"
SOURCE_SHA = "dae723cfc3cf4d076f21e7f5c609647408f86ac612a94ceb889163959eeb2f0b"
VNDK_REVISION = "e1ba8043f8368220b412c18ee8392850ab009aae"
VNDK_CONTENT_SHA = "fad8528cbe9f2750622b0058b6f88ac6c68a5b0f4d3dcdc56f33a9bc87c7b9b7"
LINK_LIBS = ["libc++.so", "libui.so", "libutils.so", "libcutils.so", "libhardware.so",
             "libsync.so", "libgralloctypes.so", "libhidlbase.so", "android.hardware.graphics.mapper@4.0.so"]
SOURCES = ["core/utils/android_utils/src/android_utils.cpp", "core/utils/drm_utils/src/drm_utils.cpp",
           "core/utils/utils.cpp", "core/RockchipRga.cpp", "core/GrallocOps.cpp", "core/NormalRga.cpp",
           "core/NormalRgaApi.cpp", "core/RgaApi.cpp", "core/RgaUtils.cpp", "core/RgaUtils_symbol.cpp",
           "core/rga_gralloc.cpp", "core/rga_sync.cpp", "core/platform_gralloc4.cpp",
           "im2d_api/src/im2d_log.cpp", "im2d_api/src/im2d_debugger.cpp", "im2d_api/src/im2d_context.cpp",
           "im2d_api/src/im2d_job.cpp", "im2d_api/src/im2d_csc.cpp", "im2d_api/src/im2d_impl.cpp",
           "im2d_api/src/im2d.cpp"]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def extract(archive, destination, expected, content_hash=False):
    actual = digest(archive)
    if content_hash:
        # Gitiles changes gzip/tar timestamps between downloads of the same
        # commit. Verify every file's name and bytes, not compression metadata.
        with tarfile.open(archive) as source:
            entries = {}
            for member in source.getmembers():
                if member.isfile():
                    if member.name in entries:
                        raise SystemExit("Duplicate archive file")
                    entries[member.name] = hashlib.sha256(source.extractfile(member).read()).hexdigest()
            manifest = "".join(entries[name] + "  " + name + "\n" for name in sorted(entries))
            actual = hashlib.sha256(manifest.encode("utf-8")).hexdigest()
    if actual != expected:
        raise SystemExit("Archive checksum mismatch: " + str(archive))
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive) as source:
        for member in source.getmembers():
            target = (destination / member.name).resolve()
            if not target.is_relative_to(destination.resolve()):
                raise SystemExit("Unsafe archive path")
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(source.extractfile(member).read())
            # The source release has no required links; never follow tar links.


def run(*args):
    subprocess.run([str(x) for x in args], check=True)


def quote(path):
    return '"' + Path(path).as_posix() + '"'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ndk", required=True, type=Path)
    parser.add_argument("--source-archive", required=True, type=Path,
                        help="Official librga_1.10.6_[3].tar.gz from the upstream README source share")
    parser.add_argument("--vendor-root", required=True, type=Path,
                        help="Private vendor64/vendor32 link input directories")
    parser.add_argument("--headers-archive", type=Path)
    parser.add_argument("--work", type=Path, default=ROOT / "build" / "rga")
    args = parser.parse_args()
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    archive = args.headers_archive
    if archive is None:
        archive = work / "vndk32-headers.tar.gz"
        if not archive.exists():
            url = "https://android.googlesource.com/platform/prebuilts/vndk/v32/+archive/" + VNDK_REVISION + "/arm64/include.tar.gz"
            with urllib.request.urlopen(url, timeout=60) as response:
                archive.write_bytes(response.read())
    extract(args.source_archive, work / "source", SOURCE_SHA)
    extract(archive, work / "headers", VNDK_CONTENT_SHA, content_hash=True)
    source = work / "source" / "librga"
    headers = work / "headers"
    project = work / "project"
    project.mkdir(exist_ok=True)
    includes = [source, source / "include", source / "im2d_api", source / "core", source / "core/hardware",
                source / "core/utils", source / "core/3rdparty/libdrm/include/drm", source / "core/3rdparty/android_hal"]
    names = {"include", "include_vndk", "include_types", "include_private", "include_outside_system"}
    includes += sorted(p for p in headers.rglob("*") if p.is_dir() and p.name in names
                       and "external/libcxx" not in p.as_posix() and "bionic" not in p.as_posix())
    includes += sorted(p for p in (headers / "generated-headers").rglob("gen") if (p / "android").exists())
    includes += [headers / "system/libfmq/base", headers / "frameworks/native/libs/binder/ndk/include_cpp",
                 headers / "frameworks/native/libs/binder/ndk/include_ndk"]
    (MODULE / "bin").mkdir(exist_ok=True)
    vendor_root = args.vendor_root.resolve()
    llvm = args.ndk.resolve() / "toolchains/llvm/prebuilt/windows-x86_64/bin"
    for abi, label, vendor, library_dir, generated in [
            ("arm64-v8a", "arm64", "vendor64", "lib64", "android_vendor.32_arm64_armv8-a_shared"),
            ("armeabi-v7a", "arm", "vendor32", "lib", "android_vendor.32_arm_armv8-a_shared")]:
        link = vendor_root / vendor
        for name in LINK_LIBS + ["libnativewindow.so"]:
            if not (link / name).is_file():
                raise SystemExit("Missing vendor link input: " + str(link / name))
        cmake = "cmake_minimum_required(VERSION 3.12)\nproject(rga_vendor LANGUAGES C CXX)\n"
        cmake += "add_library(rga SHARED\n" + "\n".join(quote(source / x) for x in SOURCES) + ")\n"
        cmake += "target_include_directories(rga PRIVATE\n" + "\n".join(quote(p) for p in includes) + ")\n"
        options = ["-O2", "-fno-exceptions", "-fno-rtti", "-nostdinc++", "-Wno-c++11-narrowing", "-Wno-deprecated-declarations",
                   "-isystem" + (headers / "external/libcxx/include").as_posix(),
                   "-isystem" + (headers / "generated-headers/bionic/libc/libc" / generated / "gen/include").as_posix(),
                   "-ffile-prefix-map=" + work.as_posix() + "=rga-build",
                   "-ffile-prefix-map=" + ROOT.as_posix() + "=module"]
        cmake += "target_compile_options(rga PRIVATE " + " ".join('"' + x + '"' for x in options) + ")\n"
        cmake += 'target_compile_definitions(rga PRIVATE ANDROID ANDROID_8 ANDROID_7_DRM USE_HARDWARE_ROCKCHIP RK_DRM_GRALLOC=1 USE_GRALLOC_4=1 USE_AHARDWAREBUFFER=1 UN_NEED_GL LOG_TAG="librga")\n'
        cmake += "target_link_options(rga PRIVATE -nostdlib++ -Wl,--no-undefined -Wl,-Bsymbolic-functions)\n"
        cmake += "target_link_libraries(rga PRIVATE log dl m " + " ".join(quote(link / x) for x in LINK_LIBS) + ")\n"
        cmake += "add_executable(rga_probe " + quote(MODULE / "src/rga_probe.cpp") + ")\n"
        for attribute in ["INCLUDE_DIRECTORIES", "COMPILE_OPTIONS", "COMPILE_DEFINITIONS"]:
            cmake += "get_target_property(rga_" + attribute + " rga " + attribute + ")\n"
        cmake += "target_include_directories(rga_probe PRIVATE ${rga_INCLUDE_DIRECTORIES})\n"
        cmake += "target_compile_options(rga_probe PRIVATE ${rga_COMPILE_OPTIONS})\n"
        cmake += "target_compile_definitions(rga_probe PRIVATE ${rga_COMPILE_DEFINITIONS})\n"
        cmake += "target_link_libraries(rga_probe PRIVATE dl android " + quote(link / "libnativewindow.so") + ")\n"
        cmake += "add_executable(mediacodec_probe " + quote(MODULE / "src/mediacodec_probe.c") + ")\n"
        cmake += 'target_compile_options(mediacodec_probe PRIVATE "-ffile-prefix-map=' + ROOT.as_posix() + '=module")\n'
        cmake += "target_link_libraries(mediacodec_probe PRIVATE mediandk)\n"
        (project / "CMakeLists.txt").write_text(cmake, encoding="utf-8")
        build = work / label
        run("cmake", "-S", project, "-B", build, "-G", "Ninja",
            "-DCMAKE_TOOLCHAIN_FILE=" + str(args.ndk.resolve() / "build/cmake/android.toolchain.cmake"),
            "-DANDROID_ABI=" + abi, "-DANDROID_PLATFORM=android-32", "-DANDROID_STL=none",
            "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_SKIP_RPATH=ON")
        run("cmake", "--build", build, "--parallel", "6")
        destination = MODULE / "system/vendor" / library_dir
        destination.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(build / "librga.so", destination / "librga.so")
        probe = MODULE / "bin" / ("probe64" if label == "arm64" else "probe32")
        shutil.copyfile(build / "rga_probe", probe)
        run(llvm / "llvm-strip.exe", "--strip-unneeded", destination / "librga.so", probe)
    licenses = MODULE / "licenses"
    licenses.mkdir(exist_ok=True)
    shutil.copyfile(source / "COPYING", licenses / "Apache-2.0")
    (MODULE / "UPSTREAM.json").write_text(json.dumps({
        "project": "airockchip/librga", "version": "1.10.6_[3]", "source_share": "https://console.zbox.filez.com/l/fuGojC",
        "source_sha256": SOURCE_SHA, "vndk_headers_revision": VNDK_REVISION, "vndk_headers_content_sha256": VNDK_CONTENT_SHA,
        "android_api": 32, "abis": ["arm64-v8a", "armeabi-v7a"],
        "source_modifications": "None; rebuilt Android gralloc4 configuration against matching system ABI."
    }, indent=2) + "\n", encoding="utf-8")
    package()


def package():
    for name in ["system/vendor/lib64/librga.so", "system/vendor/lib/librga.so", "bin/probe64", "bin/probe32",
                 "contracts/vendor.sha256", "contracts/baseline.sh", "contracts/arm64-symbols.txt", "contracts/arm-symbols.txt"]:
        if not (MODULE / name).is_file():
            raise SystemExit("Missing package input: " + name)
    excluded = {"checksums.sha256", "status.txt", "disable"}
    files = sorted(p for p in MODULE.rglob("*") if p.is_file() and p.name not in excluded)
    for path in files:
        if path.suffix in {".sh", ".md", ".txt", ".prop", ".sha256", ".cpp", ".c", ".json"}:
            path.write_bytes(path.read_bytes().replace(b"\r\n", b"\n"))
    checksums = "".join(digest(p) + "  " + p.relative_to(MODULE).as_posix() + "\n" for p in files)
    (MODULE / "checksums.sha256").write_bytes(checksums.encode("ascii"))
    files.append(MODULE / "checksums.sha256")
    output = ROOT / "dist/rk3588-rga-upgrade-0.1.0-rga1.10.6.zip"
    output.parent.mkdir(exist_ok=True)
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in files:
            name = path.relative_to(MODULE).as_posix()
            info = zipfile.ZipInfo(name, (2026, 10, 14, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = (0o100755 if path.suffix == ".sh" or name.startswith("bin/") else 0o100644) << 16
            archive.writestr(info, path.read_bytes())
    print(output)
    print("SHA256 " + digest(output))


if __name__ == "__main__":
    main()
