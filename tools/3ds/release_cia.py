#!/usr/bin/env python3
"""Build a versioned complete CIA and publish it to GitHub Releases."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

from release_tools import locate
from release_version import next_version, parse

REPO = Path(__file__).resolve().parents[2]
DEFAULT_REPOSITORY = 'AlexMolina08/The-Simpsons-Hit-and-Run-O3DS'


def run(command, capture=False, check=True):
    return subprocess.run([str(part) for part in command], cwd=REPO, text=True,
                          stdout=subprocess.PIPE if capture else None,
                          stderr=subprocess.PIPE if capture else None, check=check)


def git(*arguments):
    return run(['git', *arguments], capture=True).stdout.strip()


def digest(path):
    with path.open('rb') as file:
        return hashlib.file_digest(file, 'sha256').hexdigest()


def verify_artifact(cia, manifest, commit):
    metadata = json.loads(manifest.read_text())
    if metadata.get('source_commit') != commit:
        raise RuntimeError('The saved CIA belongs to another source commit; choose a new version and rebuild')
    if metadata['cia_bytes'] != cia.stat().st_size or metadata['cia_sha256'] != digest(cia):
        raise RuntimeError('The saved CIA failed its size/SHA-256 check; do not upload it')
    return metadata


def remote_for(repository):
    for name in git('remote').splitlines():
        url = git('remote', 'get-url', name).removesuffix('.git')
        if url in (f'https://github.com/{repository}', f'git@github.com:{repository}'):
            return name
    raise RuntimeError(f'No Git remote matches {repository}; configure one before publishing')


def release_info(repository, tag):
    result = run(['gh', 'api', f'repos/{repository}/releases/tags/{tag}'], capture=True, check=False)
    if result.returncode == 0:
        return json.loads(result.stdout)
    if '404' in result.stderr:
        # Some API versions omit drafts from the tag endpoint. The authenticated
        # release list includes them, which keeps interrupted uploads resumable.
        listed = run(['gh', 'api', f'repos/{repository}/releases?per_page=100',
                      '--paginate', '--slurp'], capture=True)
        for page in json.loads(listed.stdout):
            for release in page:
                if release['tag_name'] == tag:
                    return release
        return None
    raise RuntimeError(result.stderr.strip() or 'Cannot check the existing GitHub release')


def wait_for_release(repository, tag):
    for attempt in range(5):
        info = release_info(repository, tag)
        if info is not None:
            return info
        if attempt < 4:
            time.sleep(attempt + 1)
    raise RuntimeError('GitHub has not exposed the new draft yet; retry with --resume after a short wait')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', help='For example 0.1.1-rc.1; prompts with the next version when omitted')
    parser.add_argument('--repo', default=DEFAULT_REPOSITORY)
    parser.add_argument('--data', type=Path, default=REPO / 'sdcard/3ds/simpsons')
    parser.add_argument('--movies', type=Path)
    parser.add_argument('--game', type=Path, help='Optional retail PC installation to prepare before building')
    parser.add_argument('--makerom', type=Path)
    parser.add_argument('--bannertool', type=Path)
    parser.add_argument('--notes-file', type=Path)
    parser.add_argument('--stable', action='store_true', help='Publish a stable release instead of a test release')
    parser.add_argument('--no-upload', action='store_true', help='Build and verify locally without pushing or publishing')
    parser.add_argument('--resume', action='store_true', help='Resume upload of this version without rebuilding its CIA')
    parser.add_argument('--dry-run', action='store_true', help='Show the release plan without changing files or GitHub')
    args = parser.parse_args()
    if sys.version_info < (3, 11):
        raise RuntimeError('Python 3.11 or later is required')
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', args.repo):
        parser.error('--repo must be OWNER/REPOSITORY')
    if git('branch', '--show-current') != 'main':
        raise RuntimeError('Switch to main before releasing; the launcher never rewrites or merges branches')
    head = commit = git('rev-parse', 'HEAD')
    dirty = bool(git('status', '--porcelain'))
    if dirty and not args.dry_run:
        raise RuntimeError('Commit the source changes on main before releasing. No files were staged or committed automatically')
    if not git('config', 'user.name') or not git('config', 'user.email'):
        raise RuntimeError('Configure your Git name and email before creating a release tag')
    tags = git('tag', '--list', 'v*').splitlines()
    if not args.no_upload:
        if not shutil.which('gh'):
            raise RuntimeError('Install GitHub CLI and run gh auth login once before publishing')
        run(['gh', 'auth', 'status'], capture=True)
        listed = run(['gh', 'release', 'list', '--repo', args.repo, '--limit', '100', '--json', 'tagName'], capture=True)
        tags += [release['tagName'] for release in json.loads(listed.stdout)]
    default = next_version(tags)
    requested = args.version
    if requested is None:
        if args.resume:
            parser.error('--resume requires --version so the intended upload is unambiguous')
        requested = input(f'Release version [{default}]: ').strip() if sys.stdin.isatty() else default
    version, numbers = parse(requested or default)
    if args.stable and '-' in version:
        parser.error('A stable release cannot use an alpha, beta or rc version suffix')
    tag = 'v' + version
    folder = REPO / 'out/releases' / tag
    cia = folder / f'Simpsons-Hit-and-Run-3DS-{tag}.cia'
    manifest = cia.with_suffix('.manifest.json')
    checksum = cia.with_suffix('.sha256')
    movies = args.movies or args.data / 'movies'
    if not movies.is_dir() and (REPO / 'out/old3ds-movies').is_dir():
        movies = REPO / 'out/old3ds-movies'
    print(f'\nVersion: {tag}\nCheckout: {commit}\nOutput:  {cia}\nGitHub:  {args.repo}\nMode:    {"stable" if args.stable else "test release"}\n', flush=True)
    if args.dry_run:
        print('Plan: check clean main; prepare tools; build; package; verify; tag; push; upload; verify GitHub hashes; publish.')
        if dirty:
            print('Source changes must be committed before the real run.')
        return
    existing = None if args.no_upload else release_info(args.repo, tag)
    if existing and not args.resume:
        raise RuntimeError(f'{tag} already exists. Use a new version or --resume; published versions are never overwritten')
    if existing and not existing['draft']:
        raise RuntimeError(f'{tag} is already public; publish changes under a new version')
    if folder.exists() and not args.resume:
        raise RuntimeError(f'{folder} already exists. Use --resume or choose a new version')
    if args.resume:
        if not cia.is_file() or not manifest.is_file():
            raise RuntimeError('No complete CIA and manifest exist for this version; choose a new version to rebuild')
        saved = json.loads(manifest.read_text())
        commit = saved.get('source_commit', '')
        if not re.fullmatch(r'[0-9a-f]{40}', commit):
            raise RuntimeError('The saved release has no valid source commit')
        ancestor = run(['git', 'merge-base', '--is-ancestor', commit, head], capture=True, check=False)
        if ancestor.returncode:
            raise RuntimeError('The saved release source is not part of main; refusing to resume')
        # Resume uploads the exact original artifact. Later tooling fixes on main
        # do not change its recorded source revision or move the release tag.
        metadata = verify_artifact(cia, manifest, commit)
        print(f"Resuming the saved CIA from source {commit}", flush=True)
    else:
        makerom, bannertool = locate(REPO, args.makerom, args.bannertool)
        if args.game:
            if args.data.name != 'simpsons' or args.data.parent.name != '3ds':
                raise RuntimeError('--game requires a --data path ending in 3ds/simpsons')
            run([sys.executable, 'tools/3ds/setup_sd.py', args.game.resolve(), '--old3ds',
                 '--out', args.data.resolve().parents[1]])
            movies = args.data / 'movies' if args.movies is None else args.movies
        print('Building original-model executable...', flush=True)
        run(['bash', 'tools/3ds/build_old3ds.sh'])
        print('Packaging complete game and movies...', flush=True)
        run([sys.executable, 'tools/3ds/build_cia.py', '--version', version, '--data', args.data.resolve(),
             '--movies', movies.resolve(), '--makerom', makerom, '--bannertool', bannertool, '--out', cia])
        metadata = json.loads(manifest.read_text())
        metadata.update(source_commit=commit, elf_sha256=digest(REPO / 'out/old3ds-build/code/SRR2.elf'))
        manifest.write_text(json.dumps(metadata, indent=2) + '\n')
        verify_artifact(cia, manifest, commit)
    if git('status', '--porcelain') or git('rev-parse', 'HEAD') != head:
        raise RuntimeError('Source changed during the build. The CIA is kept locally; publishing was stopped')
    if args.no_upload:
        print(f'\nVerified local CIA: {cia}\nSHA-256: {metadata["cia_sha256"]}')
        return
    remote = remote_for(args.repo)
    local_tag = run(['git', 'rev-parse', '--verify', f'refs/tags/{tag}^{{commit}}'], capture=True, check=False)
    if local_tag.returncode == 0:
        if local_tag.stdout.strip() != commit:
            raise RuntimeError(f'Tag {tag} points to another commit; tags are never moved')
    else:
        run(['git', 'tag', '-a', tag, '-m', f'Release {tag}', commit])
    run(['git', 'push', remote, 'main', f'refs/tags/{tag}'])
    if not existing:
        notes = args.notes_file.resolve() if args.notes_file else None
        if notes is None:
            notes = folder / 'release-notes.md'
            notes.write_text(f'''The Simpsons: Hit & Run for Nintendo 3DS {tag}.

Optimized for Nintendo 3DS and 2DS. Install with FBI; no separate game-data copy is needed. Saves and settings use `sd:/3ds/simpsons/`. Audio requires your console's DSP firmware at `sd:/3ds/dspfirm.cdc`.

Source: `{commit}`. Build and packaging checks passed.

The checksum and manifest identify the exact CIA and bundled assets.
''')
        command = ['gh', 'release', 'create', tag, '--repo', args.repo, '--target', commit, '--draft',
                   '--title', f'The Simpsons: Hit & Run for Nintendo 3DS {tag}', '--notes-file', notes]
        if not args.stable:
            command.append('--prerelease')
        run(command)
    info = wait_for_release(args.repo, tag)
    present = {asset['name']: asset for asset in info['assets']}
    pending = []
    for file in (cia, manifest, checksum):
        if file.name in present:
            if present[file.name].get('digest') != 'sha256:' + digest(file):
                raise RuntimeError(f'The draft already contains a different {file.name}; it will not be overwritten')
        else:
            pending.append(file)
    if pending:
        print('Uploading release files...', flush=True)
        run(['gh', 'release', 'upload', tag, *pending, '--repo', args.repo])
    info = wait_for_release(args.repo, tag)
    uploaded = {asset['name']: asset for asset in info['assets']}
    for file in (cia, manifest, checksum):
        asset = uploaded.get(file.name)
        if not asset or asset['state'] != 'uploaded' or asset['size'] != file.stat().st_size or asset.get('digest') != 'sha256:' + digest(file):
            raise RuntimeError(f'GitHub verification failed for {file.name}. The release remains a draft')
    run(['gh', 'release', 'edit', tag, '--repo', args.repo, '--draft=false',
         '--prerelease=' + str(not args.stable).lower(), '--latest=' + str(args.stable).lower()])
    url = f'https://github.com/{args.repo}/releases/tag/{tag}'
    print(f'\nPublished: {url}\nCIA: {cia}\nSHA-256: {metadata["cia_sha256"]}', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, ValueError, subprocess.CalledProcessError, OSError, KeyboardInterrupt) as error:
        print(f'\nRelease stopped: {error}\nExisting releases and local build files were preserved.', file=sys.stderr)
        sys.exit(1)
