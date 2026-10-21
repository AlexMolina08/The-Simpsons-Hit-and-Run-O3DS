#!/usr/bin/env python3
"""Prepare ten complete retail character greetings for waking a running 3DS."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile

from prepare_home_menu import archive_member
from setup_sd import find_ffmpeg

RATE = 24000
MEMBERS = (
    r'homer\w_greeting_hom_hanging.rsd', r'homer\w_greeting_hom_happen.rsd',
    r'bart\w_greeting_brt_buddy.rsd', r'bart\w_greeting_brt_dude.rsd',
    r'lisa\w_greeting_lis_hi.rsd', r'lisa\w_greeting_lis_how.rsd',
    r'marge\w_greeting_mrg_hello.rsd', r'marge\w_greeting_mrg_how.rsd',
    r'apu\w_greeting_apu_fine.rsd', r'apu\w_greeting_apu_loyal.rsd',
)


def prepare(data, ffmpeg=None):
    output = data / 'sound/3ds-lid'
    output.mkdir(parents=True, exist_ok=True)
    results = []
    with tempfile.TemporaryDirectory(prefix='srr-lid-audio-') as temporary:
        root = Path(temporary)
        for index, member in enumerate(MEMBERS):
            source = archive_member(data / 'dialog.rcf', member)
            (root / 'source.rsd').write_bytes(source)
            pcm = root / 'clip.pcm'
            # Preserve the complete line, with gentle level matching and tiny
            # edge fades. No duration limit or mid-word truncation is applied.
            subprocess.run([find_ffmpeg(ffmpeg), '-hide_banner', '-loglevel', 'error', '-y',
                            '-i', str(root / 'source.rsd'), '-af',
                            'loudnorm=I=-18:TP=-3:LRA=7,afade=t=in:d=0.004,areverse,afade=t=in:d=0.012,areverse',
                            '-ar', str(RATE), '-ac', '1', '-c:a', 'pcm_s16le', '-f', 's16le', str(pcm)], check=True)
            body = pcm.read_bytes()
            if not 0 < len(body) <= RATE * 3 * 2 or len(body) % 2:
                raise ValueError(f'{member}: greeting exceeds the bounded three-second PCM format')
            samples = struct.unpack('<' + str(len(body) // 2) + 'h', body)
            peak = max(abs(value) for value in samples)
            if not 32 < peak < 27000:
                raise ValueError(f'{member}: greeting is silent or clips')
            encoded = struct.pack('<4sII', b'LID1', RATE, len(body) // 2) + body
            (output / f'{index:02}.bin').write_bytes(encoded)
            results.append({'index': index, 'member': member, 'seconds': len(body) / (RATE * 2),
                            'source_sha256': hashlib.sha256(source).hexdigest(),
                            'sha256': hashlib.sha256(encoded).hexdigest(), 'peak_pcm16': peak})
    (output / 'manifest.json').write_text(json.dumps({'rate': RATE, 'channels': 1, 'bits': 16, 'clips': results}, indent=2) + '\n')
    return results
