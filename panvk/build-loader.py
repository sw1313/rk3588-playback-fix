"""Build the API32 compatibility loader; the RC3 payload has the separately documented address-width fix."""
from pathlib import Path
import os
import subprocess
import sys

project = Path(__file__).resolve().parent
ndk_root = os.environ.get("ANDROID_NDK_HOME") or os.environ.get("ANDROID_NDK_ROOT")
if not ndk_root:
    raise SystemExit("Set ANDROID_NDK_HOME to Android NDK 28.2.13676358")
host = "windows-x86_64" if sys.platform == "win32" else "linux-x86_64"
bin_dir = Path(ndk_root) / "toolchains/llvm/prebuilt" / host / "bin"
suffix = ".cmd" if sys.platform == "win32" else ""
compiler = bin_dir / ("aarch64-linux-android32-clang" + suffix)
cxx = bin_dir / ("aarch64-linux-android32-clang++" + suffix)
common = ["-O2", "-fPIC", "-fvisibility=hidden",
    "-Wall", "-Wextra", "-Werror", "-I" + str(project / "include"),
    "-ffile-prefix-map=" + project.as_posix() + "=/src"]
subprocess.run([str(compiler), *common, "-c", str(project / "eden_rc3_loader.c"),
    "-o", str(project / "loader.o")], check=True)
subprocess.run([str(cxx), *common, "-std=c++17", "-c", str(project / "gralloc_bridge.cpp"),
    "-o", str(project / "bridge.o")], check=True)
for name in ("csf_group_adapter", "csf_group_core"):
    subprocess.run([str(compiler), *common, "-c", str(project / (name+".c")),
        "-o", str(project / (name+".o"))],check=True)
subprocess.run([
    str(cxx), "-shared", "-static-libstdc++", str(project / "loader.o"),
    str(project / "bridge.o"), str(project / "csf_group_adapter.o"),
    str(project / "csf_group_core.o"), "-o", str(project / "libvulkan_eden_rc3.so"),
    "-Wl,-z,max-page-size=16384", "-Wl,--exclude-libs,ALL", "-ldl", "-llog",
], check=True)
