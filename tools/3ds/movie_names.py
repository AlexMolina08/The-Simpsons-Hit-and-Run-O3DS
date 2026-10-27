"""Canonical retail cinematics shared by native validation and packaging."""
from pathlib import Path


REQUIRED_MOVIES = (
    'credits', 'fmv1A', 'fmv2', 'fmv3', 'fmv4', 'fmv5', 'fmv6', 'fmv7',
    'fmv8', 'foxlogo', 'gracie', 'intro', 'loot', 'radlogo', 'tele', 'vuglogo',
)
OPTIONAL_MOVIES = ('gvuglogo',)


def select_movies(directory, suffix):
    """Require every gameplay clip; retain the optional regional logo if present."""
    directory = Path(directory)
    found = {}
    for path in sorted(directory.iterdir()) if directory.is_dir() else ():
        if not path.is_file() or path.suffix.lower() != suffix.lower():
            continue
        key = path.stem.lower()
        if key in found:
            raise ValueError(f'Duplicate movie names: {found[key].name} and {path.name}')
        found[key] = path
    missing = [name + suffix for name in REQUIRED_MOVIES if name.lower() not in found]
    if missing:
        raise ValueError('Missing required movies: ' + ', '.join(missing))
    return [(name, found[name.lower()]) for name in REQUIRED_MOVIES + OPTIONAL_MOVIES
            if name.lower() in found]
