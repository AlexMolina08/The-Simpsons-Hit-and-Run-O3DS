#!/usr/bin/env python3
"""Convert only movie assets for the Old 3DS software decoder."""

import argparse
import concurrent.futures
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from setup_sd import MOVIE_HEADER, MOVIE_MAGIC, find_ffmpeg, probe

STAMP = "mpeg1 320x240 30000/1001 q3 bf0 g15 / pcm16 stereo 32k v2 audio-tail-pad aspect-preserved"


def convert(ffmpeg, source, destination, force=False):
    stamp = destination.with_suffix(".3dm.txt")
    if not force and destination.exists() and stamp.exists() and stamp.read_text() == STAMP and destination.stat().st_mtime >= source.stat().st_mtime:
        return {"file": source.name, "status": "unchanged", "bytes": destination.stat().st_size}
    _, has_audio = probe(ffmpeg, source)
    with tempfile.TemporaryDirectory(prefix="srr2-old3ds-movie-") as folder:
        video = Path(folder) / "video.m1v"
        pcm = Path(folder) / "audio.pcm"
        result = subprocess.run([
            ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-i", str(source), "-an",
            "-vf", "fps=30000/1001,scale=320:240:force_original_aspect_ratio=decrease:force_divisible_by=2:flags=lanczos,pad=320:240:(ow-iw)/2:(oh-ih)/2,setsar=1", "-c:v", "mpeg1video",
            "-pix_fmt", "yuv420p", "-q:v", "3", "-g", "15", "-bf", "0",
            "-threads", "1", "-progress", "pipe:1", "-nostats", "-f", "mpeg1video", str(video),
        ], check=True, capture_output=True, text=True)
        frames = [int(line.partition("=")[2]) for line in result.stdout.splitlines() if line.startswith("frame=")]
        if not frames or frames[-1] < 1:
            raise RuntimeError(f"{source.name}: no encoded video frames")
        if has_audio:
            subprocess.run([
                ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-i", str(source), "-vn",
                "-map", "0:a:0", "-ac", "2", "-ar", "32000", "-c:a", "pcm_s16le",
                "-f", "s16le", str(pcm),
            ], check=True)
        audio_bytes = pcm.stat().st_size if has_audio else 0
        # Some PC replacements have a longer audio tail than their video stream.
        # Hold the last image until that PCM finishes; never cut the source video.
        audio_frames = (audio_bytes * 30000 + 128000 * 1001 - 1) // (128000 * 1001)
        if audio_frames > frames[-1]:
            subprocess.run([
                ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-i", str(source), "-an",
                "-vf", "fps=30000/1001,scale=320:240:force_original_aspect_ratio=decrease:force_divisible_by=2:flags=lanczos,pad=320:240:(ow-iw)/2:(oh-ih)/2,setsar=1,tpad=stop_mode=clone:stop=-1",
                "-frames:v", str(audio_frames), "-c:v", "mpeg1video", "-pix_fmt", "yuv420p",
                "-q:v", "3", "-g", "15", "-bf", "0", "-threads", "1", "-f", "mpeg1video", str(video),
            ], check=True)
            frames[-1] = audio_frames
        video_bytes = video.stat().st_size
        header = MOVIE_HEADER.pack(
            MOVIE_MAGIC, 2, 320, 240, 30000, 1001, frames[-1],
            32000 if audio_bytes else 0, 2 if audio_bytes else 0, 0,
            MOVIE_HEADER.size, audio_bytes, MOVIE_HEADER.size + audio_bytes, video_bytes,
        )
        destination.parent.mkdir(parents=True, exist_ok=True)
        temporary = destination.with_suffix(".part")
        with temporary.open("wb") as output:
            output.write(header)
            if audio_bytes:
                with pcm.open("rb") as source_audio:
                    shutil.copyfileobj(source_audio, output)
            with video.open("rb") as source_video:
                shutil.copyfileobj(source_video, output)
        os.replace(temporary, destination)
        stamp.write_text(STAMP)
        return {"file": source.name, "status": "converted", "frames": frames[-1], "bytes": destination.stat().st_size}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game", type=Path, help="Installed PC game folder; originals are read only")
    parser.add_argument("--out", type=Path, required=True, help="Movie output folder; no other game files are changed")
    parser.add_argument("--only", nargs="*", help="Optional movie stems, for a small initial transfer")
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--ffmpeg")
    args = parser.parse_args()
    movies = sorted(path for path in (args.game / "movies").glob('*')
                    if path.is_file() and path.suffix.lower() == '.rmv')
    if args.only:
        movies = [movie for movie in movies if movie.stem.lower() in {name.lower() for name in args.only}]
    if not movies:
        parser.error("No matching PC movie files found")
    ffmpeg = find_ffmpeg(args.ffmpeg)
    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        tasks = [pool.submit(convert, ffmpeg, movie, args.out / (movie.stem + ".3dm")) for movie in movies]
        for task in concurrent.futures.as_completed(tasks):
            result = task.result()
            results.append(result)
            print(json.dumps(result), flush=True)
    print(json.dumps({"movies": len(results), "total_bytes": sum(result["bytes"] for result in results)}))


if __name__ == "__main__":
    main()
