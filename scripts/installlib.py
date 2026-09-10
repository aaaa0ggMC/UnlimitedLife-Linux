"""Manifest-based Linux library installation; no build commands run here."""
import fcntl
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import tempfile
import uuid

PROFILES = {
    'AGE': ('AGE', 'AGE'),
    'alib5': ('alib5', 'aaaa0ggmcLib'),
    'alib6': ('alib6', 'aaaa0ggmcLib6'),
}
STATE = '.ul-install'


def allowed(name, rel):
    header, lib = PROFILES[name]
    p = PurePosixPath(rel)
    if p.is_absolute() or '..' in p.parts or str(p) != rel:
        return False
    return (rel.startswith(f'include/{header}/') or
            (name == 'alib6' and (rel.startswith('share/alib6/modules/') or
             rel == 'share/alib6/repository/packages/a/alib6/xmake.lua' or
             (len(p.parts) == 3 and p.parts[0] == 'modules' and
              p.parts[2].endswith(('.cppm', '.cppm.meta-info'))))) or
            rel == f'lib/pkgconfig/{lib}.pc' or
            rel == f'lib/{lib}.sym' or
            rel == f'lib/lib{lib}.so' or
            (rel.startswith(f'lib/lib{lib}.so.') and '/' not in rel[4:]))


def safe_path(prefix, rel):
    p = prefix
    for part in PurePosixPath(rel).parts:
        p = p / part
        if p.is_symlink():
            raise RuntimeError(f'Refusing symlink: {p}')
    return p


def digest(p):
    if not p.exists():
        return None
    if not p.is_file() or p.is_symlink():
        raise RuntimeError(f'Expected a regular file: {p}')
    return hashlib.sha256(p.read_bytes()).hexdigest()


def inventory(root, name):
    result = {}
    if root.exists():
        header, lib = PROFILES[name]
        header_dir = safe_path(root, f'include/{header}')
        lib_dir = safe_path(root, 'lib')
        candidates = list(header_dir.rglob('*')) if header_dir.exists() else []
        candidates += list(lib_dir.glob(f'lib{lib}.so*')) if lib_dir.exists() else []
        candidates += [safe_path(root, f'lib/{lib}.sym')]
        candidates += [safe_path(root, f'lib/pkgconfig/{lib}.pc')]
        if name == 'alib6':
            owned = safe_path(root, 'share/alib6')
            candidates += list(owned.rglob('*')) if owned.exists() else []
            # Old xmake installations used a shared modules/<hash> namespace.
            # Claim only pairs whose metadata explicitly identifies alib6.
            legacy = safe_path(root, 'modules')
            for meta in legacy.glob('*/*.cppm.meta-info'):
                safe_path(root, meta.relative_to(root).as_posix())
                data = json.loads(meta.read_text())
                module = data.get('name', '')
                if module == 'alib6' or module.startswith('alib6.'):
                    rel = 'modules/' + data.get('file', '')
                    if not allowed(name, rel) or root / rel != meta.with_suffix(''):
                        raise RuntimeError(f'Invalid legacy module metadata: {meta}')
                    candidates += [meta, safe_path(root, rel)]
        for p in candidates:
            rel = p.relative_to(root).as_posix()
            if allowed(name, rel) and (p.is_file() or p.is_symlink()):
                safe_path(root, rel)
                result[rel] = digest(p)
    return result


def load_manifest(prefix, name):
    p = safe_path(prefix, f'{STATE}/{name}.json')
    if not p.exists():
        return None
    data = json.loads(p.read_text())
    if data.get('name') != name or data.get('schema') != 1:
        raise RuntimeError(f'Invalid manifest: {p}')
    files = data['files']
    if not isinstance(files, dict) or any(not allowed(name, k) for k in files):
        raise RuntimeError(f'Unsafe manifest paths: {p}')
    return files


def plan(prefix, stage, name, adopt=False):
    old = load_manifest(prefix, name)
    if adopt and old is not None:
        raise RuntimeError('--adopt-existing is only for the first managed installation')
    if stage is None and old is None:
        raise RuntimeError(f'No managed installation for {name} at {prefix}')
    if old is None:
        old = inventory(prefix, name) if adopt else {}
    new = inventory(stage, name) if stage else {}
    if stage is not None:
        header, lib = PROFILES[name]
        if f'lib/lib{lib}.so' not in new or not any(k.startswith(f'include/{header}/') for k in new):
            raise RuntimeError('Staging is incomplete: expected headers and shared library')
        if name == 'alib6' and (not any(k.startswith('share/alib6/modules/') and k.endswith('.meta-info') for k in new)
                               or 'share/alib6/repository/packages/a/alib6/xmake.lua' not in new):
            raise RuntimeError('Staging is incomplete: expected alib6 module metadata and package recipe')
    changes = []
    for rel in sorted(old.keys() | new.keys()):
        current = digest(safe_path(prefix, rel))
        if rel in old and current is not None and current != old[rel]:
            raise RuntimeError(f'Installed file was modified; refusing to overwrite/delete: {rel}')
        if rel not in old and current is not None:
            raise RuntimeError(f'Unmanaged file already exists: {rel}; use --adopt-existing for the first migration')
        if current != new.get(rel):
            changes.append(('remove' if rel not in new else 'install', rel))
    return new, changes


def atomic_copy(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    fd, temp = tempfile.mkstemp(prefix='.ul-install-', dir=destination.parent)
    os.close(fd)
    try:
        shutil.copy2(source, temp)
        os.replace(temp, destination)
    finally:
        if os.path.exists(temp):
            os.unlink(temp)


def apply(prefix, stage, name, adopt=False, dry_run=False):
    # Prefix itself and every existing parent must be real directories.
    prefix = Path(os.path.abspath(prefix))
    for p in (prefix, *prefix.parents):
        if p.is_symlink():
            raise RuntimeError(f'Refusing symlink prefix: {p}')
    if prefix == Path('/'):
        raise RuntimeError('Installing directly into / is not supported')
    if dry_run:
        _, changes = plan(prefix, stage, name, adopt)
        for action, rel in changes:
            print(f'{action:7} {prefix / rel}')
        print(f'Dry run: {len(changes)} file changes; no installation files written.')
        return
    state = safe_path(prefix, STATE)
    state.mkdir(parents=True, exist_ok=True)
    lock = safe_path(prefix, f'{STATE}/lock')
    with lock.open('a') as handle:
        fcntl.flock(handle, fcntl.LOCK_EX)
        new, changes = plan(prefix, stage, name, adopt)
        manifest = safe_path(prefix, f'{STATE}/{name}.json')
        if not changes and stage is not None and load_manifest(prefix, name) == new:
            print(f'{name}: already up to date.')
            return
        manifest_rel = manifest.relative_to(prefix).as_posix()
        transaction = uuid.uuid4().hex
        backup = safe_path(prefix, f'{STATE}/backups/{name}-{transaction}')
        backup.mkdir(parents=True)
        paths = [rel for _, rel in changes] + [manifest_rel]
        existing = []
        # Finish all backups before touching installed files.
        for rel in paths:
            p = safe_path(prefix, rel)
            if p.exists():
                atomic_copy(p, backup / rel)
                existing.append(rel)
        (backup / 'transaction.json').write_text(json.dumps({'paths': paths, 'existing': existing}, indent=2))
        try:
            for action, rel in changes:
                p = safe_path(prefix, rel)
                if action == 'remove':
                    p.unlink()
                else:
                    atomic_copy(stage / rel, p)
            if stage is None:
                manifest.unlink()
            else:
                draft = backup / 'new-manifest.json'
                draft.write_text(json.dumps({'schema': 1, 'name': name, 'files': new}, indent=2) + '\n')
                atomic_copy(draft, manifest)
        except BaseException:
            for rel in reversed(paths):
                p = safe_path(prefix, rel)
                if rel in existing:
                    atomic_copy(backup / rel, p)
                elif p.is_file():
                    p.unlink()
            raise
        print(f'{name}: {len(changes)} file changes applied. Backup: {backup}')
