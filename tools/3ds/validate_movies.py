#!/usr/bin/env python3
"""Fully decode Old 3DS movies and compare timing and PCM with retail originals."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from setup_sd import MOVIE_HEADER, MOVIE_MAGIC, find_ffmpeg
from movie_names import select_movies


def header(path):
    with path.open('rb') as stream:
        data = stream.read(MOVIE_HEADER.size)
    if len(data) != MOVIE_HEADER.size:
        raise ValueError(f'{path.name}: truncated header')
    magic, version, width, height, num, den, frames, rate, channels, reserved, audio, audio_bytes, video, video_bytes = MOVIE_HEADER.unpack(data)
    if (magic, version, width, height, num, den, reserved) != (MOVIE_MAGIC, 2, 320, 240, 30000, 1001, 0):
        raise ValueError(f'{path.name}: unsupported movie format')
    if not frames or not video_bytes or audio != MOVIE_HEADER.size or video != audio + audio_bytes or video + video_bytes != path.stat().st_size:
        raise ValueError(f'{path.name}: invalid stream bounds')
    if (audio_bytes and (rate, channels) != (32000, 2)) or audio_bytes % 4 or (not audio_bytes and (rate or channels)):
        raise ValueError(f'{path.name}: invalid PCM format')
    return frames, audio_bytes, video


def decoded_frames(ffmpeg, path, source=False):
    command = [ffmpeg, '-hide_banner', '-loglevel', 'error', '-xerror', '-err_detect', 'explode', '-i', str(path), '-map', '0:v:0', '-an']
    if source:
        command += ['-vf', 'fps=30000/1001']
    result = subprocess.run(command + ['-progress', 'pipe:1', '-nostats', '-f', 'null', '-'], capture_output=True, text=True, check=True)
    frames = [int(value) for value in re.findall(r'^frame=(\d+)$', result.stdout, re.M)]
    if not frames or frames[-1] < 1:
        raise ValueError(f'{path.name}: no decoded frames')
    return frames[-1]


def source_pcm(ffmpeg, source):
    command = [ffmpeg, '-hide_banner', '-loglevel', 'error', '-xerror', '-i', str(source), '-vn', '-map', '0:a:0', '-ac', '2', '-ar', '32000', '-c:a', 'pcm_s16le', '-f', 's16le', '-']
    digest, size = hashlib.sha256(), 0
    with tempfile.TemporaryFile() as errors:
        with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=errors) as process:
            while block := process.stdout.read(1024 * 1024):
                digest.update(block)
                size += len(block)
            if process.wait():
                errors.seek(0)
                raise ValueError(f'{source.name}: audio decode failed: {errors.read().decode(errors="replace")}')
    return size, digest.hexdigest()


def validate(ffmpeg, source, movie):
    frames, audio_bytes, video_offset = header(movie)
    audio_hash = hashlib.sha256()
    with tempfile.TemporaryDirectory(prefix='srr-movie-check-') as folder:
        video = Path(folder) / 'video.m1v'
        with movie.open('rb') as stream:
            stream.seek(MOVIE_HEADER.size)
            remaining = audio_bytes
            while remaining:
                block = stream.read(min(remaining, 1024 * 1024))
                audio_hash.update(block)
                remaining -= len(block)
            with video.open('wb') as output:
                shutil.copyfileobj(stream, output)
        # Check sequence dimensions/rate and every MPEG picture type, then decode
        # the complete elementary stream. A valid header alone proves little.
        data = video.read_bytes()
        sequence = data.find(b'\0\0\1\xb3')
        if sequence < 0:
            raise ValueError(f'{movie.name}: missing MPEG sequence')
        info = data[sequence + 4:sequence + 8]
        if len(info) != 4 or (int.from_bytes(info[:3], 'big') >> 12, int.from_bytes(info[:3], 'big') & 4095, info[3] & 15) != (320, 240, 4):
            raise ValueError(f'{movie.name}: invalid MPEG dimensions or frame rate')
        pictures = list(re.finditer(b'\x00\x00\x01\x00', data))
        if len(pictures) != frames or any(((data[p.end() + 1] >> 3) & 7) not in (1, 2) for p in pictures):
            raise ValueError(f'{movie.name}: picture count mismatch or unsupported B frames')
        if decoded_frames(ffmpeg, video) != frames:
            raise ValueError(f'{movie.name}: decoded frame count differs from header')
    source_frames = decoded_frames(ffmpeg, source, source=True)
    if audio_bytes:
        expected_bytes, expected_hash = source_pcm(ffmpeg, source)
        if (expected_bytes, expected_hash) != (audio_bytes, audio_hash.hexdigest()):
            raise ValueError(f'{movie.name}: PCM differs from the complete retail audio')
    else:
        streams = subprocess.run([ffmpeg, '-hide_banner', '-i', str(source)], capture_output=True, text=True).stderr
        if 'Audio:' in streams:
            raise ValueError(f'{movie.name}: missing source audio')
    expected_frames = max(source_frames, (audio_bytes * 30000 + 128000 * 1001 - 1) // (128000 * 1001))
    if frames != expected_frames:
        raise ValueError(f'{movie.name}: video is truncated or has unexpected padding ({frames} != {expected_frames})')
    with movie.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    return {'file': movie.name, 'sha256': digest, 'bytes': movie.stat().st_size, 'decoded_frames': frames,
            'source_frames_at_2997': source_frames, 'held_tail_frames': frames - source_frames,
            'video_seconds': round(frames * 1001 / 30000, 6), 'audio_seconds': audio_bytes / 128000,
            'pcm_sha256': audio_hash.hexdigest(), 'pcm_matches_source': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--movies', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--ffmpeg')
    args = parser.parse_args()
    try:
        sources = select_movies(args.game / 'movies', '.rmv')
        movies = dict(select_movies(args.movies, '.3dm'))
    except ValueError as error:
        parser.error(str(error))
    if {name for name, _ in sources} != set(movies):
        parser.error('Converted movies must match the retail movie set, including gvuglogo when present')
    ffmpeg = find_ffmpeg(args.ffmpeg)
    results = []
    for name, source in sources:
        result = validate(ffmpeg, source, movies[name])
        result['file'] = name + '.3dm'
        results.append(result)
        print(f'Checked {source.name}: {result["decoded_frames"]} frames; complete PCM matches', flush=True)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps({'movies': results, 'validation': 'Complete host decode, stream bounds, frame counts, source timing and exact PCM comparison; console playback remains a separate check'}, indent=2) + '\n')


if __name__ == '__main__':
    main()
