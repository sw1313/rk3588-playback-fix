"""Build the API32 direct C2 HEVC EOS ownership probe.

Use the generated core CMake project for headers and private link inputs.
No link-input library is copied into the probe package.
"""
import argparse
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--core-project', type=Path, required=True)
p.add_argument('--ndk', type=Path, required=True)
p.add_argument('--work', type=Path, required=True)
a = p.parse_args()
work = a.work.resolve()
if work.exists():
    raise SystemExit('Use a fresh build directory')
project = work / 'project'
project.mkdir(parents=True)
text = a.core_project.read_text(encoding='utf-8')
source = Path(__file__).with_name('eos_pending_probe.cpp').resolve().as_posix()
text, count = re.subn(r'add_library\(codec2_rx_component SHARED [^\n]+',
    'add_executable(codec2_rx_component "' + source + '")', text)
if count != 1:
    raise SystemExit('Unknown core project format')
text = text.replace('codec2_rx_component', 'eos_pending_probe32').replace(' -Wl,-Bsymbolic', '')
text += '\ntarget_link_libraries(eos_pending_probe32 PRIVATE mediandk)\n'
(project / 'CMakeLists.txt').write_text(text, encoding='utf-8')
subprocess.run(['cmake', '-S', str(project), '-B', str(work / 'arm'), '-G', 'Ninja',
    '-DCMAKE_TOOLCHAIN_FILE=' + str(a.ndk / 'build/cmake/android.toolchain.cmake'),
    '-DANDROID_ABI=armeabi-v7a', '-DANDROID_PLATFORM=android-32', '-DCMAKE_SKIP_RPATH=ON'], check=True)
subprocess.run(['cmake', '--build', str(work / 'arm')], check=True)
