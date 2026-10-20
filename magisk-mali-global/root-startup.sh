#!/system/bin/sh
# Gate only this early-init adapter by its actual command/runtime interfaces.
# Never invoke a boot stage as a probe and never gate on a version/build hash.
root_startup_reason() {
 case "${KSU:-}:${APATCH:-}" in true:*|1:*|*:true|*:1) echo other_root_backend;return;;esac
 command -v magisk >/dev/null 2>&1 || { echo other_root_backend;return; }
 command -v timeout >/dev/null 2>&1 || { echo root_probe_tools_missing;return; }
 root_help=$(timeout 5 magisk --help 2>&1) || :
 # Some builds group callbacks under --[init trigger] rather than spelling
 # each option with its -- prefix in the usage text.
 case "$root_help" in *post-fs-data*) ;;*) echo startup_command_interfaces_missing;return;;esac
 case "$root_help" in *--path*) ;;*) echo startup_command_interfaces_missing;return;;esac
 root_tmp=$(timeout 5 magisk --path 2>/dev/null) || { echo root_runtime_unavailable;return; }
 case "$root_tmp" in /*) ;;*) echo root_runtime_unavailable;return;;esac
 case "$root_tmp" in /|*'/../'*|*/..|*'//'*) echo root_runtime_unavailable;return;;esac
 case "$root_tmp" in *[!a-zA-Z0-9_./-]*) echo root_runtime_unavailable;return;;esac
 [ -d "$root_tmp" ] && [ -x "$root_tmp/magisk" ] || { echo root_runtime_unavailable;return; }
 echo supported
}
root_startup_supported() { [ "$(root_startup_reason)" = supported ]; }
