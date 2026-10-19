"""Build the direct API32 callback probe using a generated core CMake project.

The core project contains private link input paths. Only the resulting probe
and this source are used for testing; no link inputs are copied or distributed.
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
if a.work.exists():
    raise SystemExit('Use a fresh build directory')
project = a.work.resolve() / 'project'
project.mkdir(parents=True)
text = a.core_project.read_text(encoding='utf-8')
source = Path(__file__).with_name('output_delay_probe.cpp').resolve().as_posix()
text, count = re.subn(r'add_library\(codec2_rx_component SHARED [^\n]+',
        'add_executable(codec2_rx_component "' + source + '")', text)
if count != 1:
    raise SystemExit('Unknown core project format')
text = text.replace('codec2_rx_component', 'output_delay_probe32').replace(' -Wl,-Bsymbolic', '')
(project / 'CMakeLists.txt').write_text(text, encoding='utf-8')
subprocess.run(['cmake', '-S', str(project), '-B', str(a.work / 'arm'), '-G', 'Ninja',
    '-DCMAKE_TOOLCHAIN_FILE=' + str(a.ndk / 'build/cmake/android.toolchain.cmake'),
    '-DANDROID_ABI=armeabi-v7a', '-DANDROID_PLATFORM=android-32', '-DCMAKE_SKIP_RPATH=ON'], check=True)
subprocess.run(['cmake', '--build', str(a.work / 'arm')], check=True)
