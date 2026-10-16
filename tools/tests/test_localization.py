"""Check native resource parity and existing English module-facing messages."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
resources = root / 'lsposed/app/src/main/res'
en = {n.attrib['name']: n.text or '' for n in ET.parse(resources/'values/strings.xml').getroot()}
zh = {n.attrib['name']: n.text or '' for n in ET.parse(resources/'values-zh/strings.xml').getroot()}
assert en.keys() == zh.keys()
for key in en:
    assert en[key] and zh[key]
    assert re.findall(r'%\d+\$[a-z]', en[key]) == re.findall(r'%\d+\$[a-z]', zh[key]), key
    if key != 'language_button':
        assert not re.search(r'[\u3400-\u9fff]', en[key]), key
messages = 0
for component in ['magisk-decode', 'magisk-mali-global', 'magisk']:
    for path in (root/component).glob('*'):
        if path.suffix != '.sh' and path.name != 'module.prop':
            continue
        for line in path.read_text(encoding='utf-8').splitlines():
            if re.search(r'\b(?:echo|abort|ui_print)\b|^(?:name|description)=', line):
                assert not re.search(r'[\u3400-\u9fff]', line), path.name
                messages += 1
print(f'PASS: {len(en)} native resource pairs; {messages} existing English module-facing lines')
