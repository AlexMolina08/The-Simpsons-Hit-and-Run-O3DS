"""Select a language present in a private PC copy, independently of system locale."""
from pathlib import Path
import re
import subprocess

LANGUAGES = {
    'en': {'name': 'English', 'archive': 'dialog.rcf', 'track': 0},
    'fr': {'name': 'Français', 'archive': 'dialogf.rcf', 'track': 1},
    'es': {'name': 'Español', 'archive': 'dialogs.rcf', 'track': 3},
}


def select_language(game, requested=None):
    game = Path(game)
    files = {path.name.lower(): path for path in game.iterdir() if path.is_file()}
    available = [code for code, profile in LANGUAGES.items() if profile['archive'] in files]
    if not available:
        raise ValueError('The PC copy is missing a supported dialogue archive')
    code = requested or available[0]
    if code not in available:
        raise ValueError(f'This copy does not include {code}. Available languages: {", ".join(available)}')
    return {'code': code, **LANGUAGES[code], 'path': files[LANGUAGES[code]['archive']]}


def movie_audio_track(ffmpeg, source, language='en'):
    """Keep single-stream logos; require the chosen stream in multilingual clips."""
    probe = subprocess.run([str(ffmpeg), '-hide_banner', '-i', str(source)],
                           capture_output=True, text=True, errors='replace').stderr
    streams = len(re.findall(r'Audio:', probe))
    if not streams:
        return None
    track = LANGUAGES[language]['track'] if streams > 1 else 0
    if track >= streams:
        raise ValueError(f'{Path(source).name}: selected {language} audio track is missing')
    return track
