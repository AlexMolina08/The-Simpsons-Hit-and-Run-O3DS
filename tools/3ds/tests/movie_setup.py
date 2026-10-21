#!/usr/bin/env python3
"""Exercise SD setup routing without converting copyrighted assets."""
from pathlib import Path
import importlib.util,sys,tempfile
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import setup_sd,convert_old3ds_movies
with tempfile.TemporaryDirectory() as tmp:
 root=Path(tmp);game=root/'game';game.mkdir()
 for name in ['art','scripts','movies']:(game/name).mkdir()
 (game/'scripts.rcf').write_bytes(b'test');(game/'movies/fmv1A.rmv').write_bytes(b'test')
 for skip in [False,True]:
  argv=['setup_sd',str(game),'--out',str(root/'sd'),'--old3ds','--no-romfs','--force-movies']+(['--no-movies'] if skip else [])
  with patch('prepare_lid_audio.prepare',return_value=[]),patch.object(sys,'argv',argv),patch.object(setup_sd,'find_ffmpeg',return_value='ffmpeg'),patch.object(setup_sd,'convert_movie',side_effect=AssertionError('Old 3DS selected H.264')),patch.object(convert_old3ds_movies,'convert',return_value={'status':'converted','bytes':52}) as converter:
   setup_sd.main()
   assert converter.call_count==(0 if skip else 1)
   if not skip:
    assert converter.call_args.args[2]==(root/'sd/3ds/simpsons/movies/fmv1A.3dm').resolve()
    assert converter.call_args.args[3] is True
print('PASS: Old 3DS setup routes movies to software conversion; explicit skip and forced conversion remain supported.')
