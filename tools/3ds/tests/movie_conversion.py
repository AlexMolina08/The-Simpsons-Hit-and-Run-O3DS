#!/usr/bin/env python3
"""Decode generated fixtures; catch PCM loss, tail truncation and damaged headers."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from convert_old3ds_movies import convert
from setup_sd import find_ffmpeg
from validate_movies import header, validate

ffmpeg = find_ffmpeg(None)
with tempfile.TemporaryDirectory() as folder:
    root = Path(folder)
    source, movie = root / 'fixture.mkv', root / 'fixture.3dm'
    subprocess.run([ffmpeg, '-hide_banner', '-loglevel', 'error', '-f', 'lavfi', '-i',
                    'color=c=blue:s=320x240:r=30000/1001:d=0.2', '-f', 'lavfi', '-i',
                    'sine=frequency=440:sample_rate=24000:duration=0.45', '-c:v', 'ffv1', '-c:a', 'pcm_s16le', str(source)], check=True)
    convert(ffmpeg, source, movie)
    result = validate(ffmpeg, source, movie)
    assert result['held_tail_frames'] > 0 and result['audio_seconds'] >= 0.45
    data = bytearray(movie.read_bytes())
    data[100] ^= 1
    movie.write_bytes(data)
    try:
        validate(ffmpeg, source, movie)
    except ValueError as error:
        assert 'PCM differs' in str(error)
    else:
        raise AssertionError('Changed audio accepted')
    for data in (b'', b'invalid movie', data[:-1]):
        movie.write_bytes(data)
        try:
            header(movie)
        except ValueError:
            pass
        else:
            raise AssertionError('Invalid bounds accepted')
print('PASS: full MPEG decode, PCM identity, held audio tail and corrupt-container rejection.')
