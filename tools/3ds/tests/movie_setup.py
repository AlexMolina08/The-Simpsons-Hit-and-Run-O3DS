#!/usr/bin/env python3
"""Exercise SD setup routing without converting copyrighted assets."""
from pathlib import Path
import importlib.util,sys,tempfile,json,io
from contextlib import redirect_stderr,redirect_stdout
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import setup_sd,convert_old3ds_movies,validate_movies
from movie_names import REQUIRED_MOVIES,select_movies
with tempfile.TemporaryDirectory() as tmp:
 root=Path(tmp);game=root/'game';game.mkdir()
 for name in ['art','scripts','movies']:(game/name).mkdir()
 (game/'scripts.rcf').write_bytes(b'test');(game/'movies/fmv1A.RMV').write_bytes(b'test')
 for skip in [False,True]:
  argv=['setup_sd',str(game),'--out',str(root/'sd'),'--old3ds','--no-romfs','--force-movies']+(['--no-movies'] if skip else [])
  with patch('prepare_lid_audio.prepare',return_value=[]),patch.object(sys,'argv',argv),patch.object(setup_sd,'find_ffmpeg',return_value='ffmpeg'),patch.object(setup_sd,'convert_movie',side_effect=AssertionError('Old 3DS selected H.264')),patch.object(convert_old3ds_movies,'convert',return_value={'status':'converted','bytes':52}) as converter:
   setup_sd.main()
   assert converter.call_count==(0 if skip else 1)
   if not skip:
    assert converter.call_args.args[2]==(root/'sd/3ds/simpsons/movies/fmv1A.3dm').resolve()
    assert converter.call_args.args[3] is True
with tempfile.TemporaryDirectory() as tmp:
 root=Path(tmp);game=root/'game';sources=game/'movies';converted=root/'converted'
 sources.mkdir(parents=True);converted.mkdir()
 for name in REQUIRED_MOVIES:
  (sources/(name+'.rmv')).write_bytes(b'original fixture')
  (converted/(name+'.3dm')).write_bytes(b'converted fixture')
 (converted/'fmv1A.3dm').rename(converted/'FMV1A.3DM')
 assert len(select_movies(sources,'.rmv'))==16
 def check(count):
  argv=['validate_movies',str(game),'--movies',str(converted),'--out',str(root/'audit.json')]
  with patch.object(sys,'argv',argv),patch.object(validate_movies,'find_ffmpeg',return_value='ffmpeg'),patch.object(validate_movies,'validate',side_effect=lambda ffmpeg,source,movie:{'file':movie.name,'decoded_frames':1}) as validator,redirect_stdout(io.StringIO()):
   validate_movies.main()
  assert validator.call_count==count
  results=json.loads((root/'audit.json').read_text())['movies']
  assert results[1]['file']=='fmv1A.3dm'
  assert len(results)==count
 check(16)
 # A regional logo is retained, but cannot substitute for a story cinematic.
 (sources/'gvuglogo.rmv').write_bytes(b'optional fixture')
 (converted/'gvuglogo.3dm').write_bytes(b'optional fixture')
 check(17)
 (sources/'fmv8.rmv').unlink();(sources/'unrelated.rmv').write_bytes(b'not a story clip')
 try:select_movies(sources,'.rmv')
 except ValueError as error:assert 'fmv8.rmv' in str(error)
 else:raise AssertionError('An unrelated 17th movie replaced a missing story clip')
 # Simulate distinct names even on case-insensitive developer filesystems.
 duplicate=[converted/'fmv8.3dm',converted/'FMV8.3DM']
 with patch.object(Path,'iterdir',return_value=iter(duplicate)),patch.object(Path,'is_file',return_value=True):
  try:select_movies(converted,'.3dm')
  except ValueError as error:assert 'Duplicate' in str(error)
  else:raise AssertionError('Case-insensitive duplicate movies accepted')
 (sources/'fmv8.rmv').write_bytes(b'original fixture')
 (sources/'gvuglogo.rmv').unlink()
 argv=['validate_movies',str(game),'--movies',str(converted),'--out',str(root/'audit.json')]
 with patch.object(sys,'argv',argv),patch.object(validate_movies,'validate') as validator,redirect_stderr(io.StringIO()):
  try:validate_movies.main()
  except SystemExit as error:assert error.code==2
  else:raise AssertionError('Optional conversion accepted without its source')
  validator.assert_not_called()
print('PASS: software-converter routing, 16 required cinematics, optional regional logo, and missing/duplicate movie rejection.')
