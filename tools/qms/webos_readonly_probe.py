"""Bounded webOS SSAP diagnostics. No input, power, picture-setting or Luna writes.

Requires websocket-client. Output and pairing key must stay in private storage.
The only state change is optional, explicitly requested TV client registration.
Sources: home-assistant-libs/aiowebostv and Maassoft/ColorControl read endpoints.
"""
import argparse
import json
from pathlib import Path
import ssl
import time

import websocket

READ_PERMISSIONS = [
    'READ_UPDATE_INFO', 'READ_SETTINGS', 'READ_POWER_STATE',
    'READ_INPUT_DEVICE_LIST', 'READ_RUNNING_APPS', 'READ_LGE_TV_INPUT_EVENTS',
]
QUERIES = [
    ('system', 'system/getSystemInfo', {}),
    ('services', 'api/getServiceList', {}),
    ('software', 'com.webos.service.update/getCurrentSWInformation', {}),
    ('foreground', 'com.webos.applicationManager/getForegroundAppInfo', {}),
    ('inputs', 'tv/getExternalInputList', {}),
    ('power', 'com.webos.service.tvpower/power/getPowerState', {}),
    ('dimensions', 'settings/getSystemSettings', {
        'category': 'dimensionInfo',
        'keys': ['input', '_3dStatus', 'colorSystem', 'dynamicRange', 'gameInput'],
    }),
    ('picture_mode', 'settings/getSystemSettings', {
        'category': 'picture', 'keys': ['pictureMode'],
    }),
]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host', required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--key-file', type=Path)
    p.add_argument('--pair', action='store_true', help='Request TV approval, read permissions only')
    p.add_argument('--observe-seconds', type=int, default=0)
    a = p.parse_args()
    if not 0 <= a.observe_seconds <= 120:
        p.error('observation must be bounded to 0..120 seconds')
    if a.pair and not a.key_file:
        p.error('--pair requires a private --key-file destination')
    a.output.parent.mkdir(parents=True, exist_ok=True)
    result = {'read_only_queries': True, 'events': [], 'requests': {}}
    ws = websocket.create_connection(
        f'wss://{a.host}:3001', timeout=5, suppress_origin=True,
        sslopt={'cert_reqs': ssl.CERT_NONE, 'check_hostname': False},
    )  # webOS uses a local self-signed certificate.

    def send(message):
        ws.send(json.dumps(message))

    def receive(ident):
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            message = json.loads(ws.recv())
            if message.get('id') == ident or (ident == 'hello' and message.get('type') == 'hello'):
                return message
            result['events'].append({'monotonic_ns': time.monotonic_ns(),
                                     'wall_ns': time.time_ns(), 'message': message})
        raise TimeoutError('SSAP response deadline exceeded')

    try:
        send({'id': 'hello', 'type': 'hello', 'payload': {}})
        result['hello'] = receive('hello')
        # New webOS requires this pre-registration query.
        send({'id': 'pre_system', 'type': 'request', 'uri': 'ssap://system/getSystemInfo', 'payload': {}})
        result['pre_system'] = receive('pre_system')
        key = None
        if a.key_file and a.key_file.exists():
            key = json.loads(a.key_file.read_text(encoding='utf-8'))['client_key']
        if key or a.pair:
            payload = {'forcePairing': False, 'pairingType': 'PROMPT',
                       'manifest': {'manifestVersion': 1, 'appVersion': '1.0',
                                    'permissions': READ_PERMISSIONS}}
            if key:
                payload['client-key'] = key
            send({'id': 'register_0', 'type': 'register', 'payload': payload})
            message = receive('register_0')
            if message.get('payload', {}).get('pairingType') == 'PROMPT':
                print('TV_PAIRING_PROMPT: approve on TV; only READ permissions requested.', flush=True)
                deadline = time.monotonic() + 90
                ws.settimeout(2)
                while time.monotonic() < deadline:
                    try:
                        message = receive('register_0')
                        break
                    except websocket.WebSocketTimeoutException:
                        continue
                else:
                    raise TimeoutError('TV approval not received')
                ws.settimeout(5)
            if message.get('type') != 'registered':
                raise RuntimeError('TV registration refused: ' + str(message.get('error', 'unknown')))
            new_key = message.get('payload', {}).get('client-key')
            if new_key:
                a.key_file.parent.mkdir(parents=True, exist_ok=True)
                a.key_file.write_text(json.dumps({'client_key': new_key}), encoding='utf-8')
                a.key_file.chmod(0o600)
            result['registered'] = True  # Never include client key in diagnostic output.
            print('TV_REGISTERED_READ_ONLY', flush=True)
        for name, uri, payload in QUERIES:
            sent_ns = time.time_ns()
            send({'id': name, 'type': 'request', 'uri': 'ssap://' + uri, 'payload': payload})
            message = receive(name)
            result['requests'][name] = message
            result.setdefault('request_timing', {})[name] = {'sent_wall_ns': sent_ns,
                                                           'received_wall_ns': time.time_ns()}
            status = 'OK' if message.get('type') == 'response' and message.get('payload', {}).get('returnValue', True) else message.get('error', 'UNAVAILABLE')
            print(name + ': ' + str(status), flush=True)
        if a.observe_seconds:
            for name in ['foreground', 'inputs', 'picture_mode']:
                _, uri, payload = next(x for x in QUERIES if x[0] == name)
                send({'id': 'observe_' + name, 'type': 'subscribe', 'uri': 'ssap://' + uri,
                      'payload': dict(payload, subscribe=True)})
            print('TV_READ_ONLY_OBSERVATION_STARTED', flush=True)
            deadline = time.monotonic() + a.observe_seconds
            ws.settimeout(1)
            while time.monotonic() < deadline:
                try:
                    message = json.loads(ws.recv())
                    result['events'].append({'monotonic_ns': time.monotonic_ns(),
                                             'wall_ns': time.time_ns(), 'message': message})
                except websocket.WebSocketTimeoutException:
                    pass
    finally:
        ws.close()
        def redact(value):
            if isinstance(value, dict):
                return {k: redact(v) for k, v in value.items() if k not in ('client-key', 'client_key')}
            if isinstance(value, list):
                return [redact(x) for x in value]
            return value
        a.output.write_text(json.dumps(redact(result), ensure_ascii=False, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
