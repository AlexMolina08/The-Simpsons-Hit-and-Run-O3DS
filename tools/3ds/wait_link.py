#!/usr/bin/env python3
"""Wait for the official 3dslink UDP discovery reply without launching anything."""
import argparse
import json
import socket
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--timeout', type=float, default=300)
parser.add_argument('--address', default='255.255.255.255')
args = parser.parse_args()
deadline = time.monotonic() + args.timeout
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
    receiver.bind(('', 17491))
    receiver.settimeout(2)
    sender.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    while time.monotonic() < deadline:
        # Same discovery request, reply prefix and port as devkitPro 3dslink.
        try:
            sender.sendto(b'3dsboot', (args.address, 17491))
        except OSError:
            time.sleep(min(2, max(0, deadline-time.monotonic())))
            continue
        try:
            data, peer = receiver.recvfrom(256)
        except socket.timeout:
            continue
        if data.startswith(b'boot3ds'):
            print(json.dumps({'ready': True, 'ip': peer[0]}), flush=True)
            break
    else:
        print(json.dumps({'ready': False, 'reason': 'discovery timeout'}), flush=True)
        raise SystemExit(1)
