"""Hash source and configuration so test evidence cannot migrate between builds."""
from pathlib import Path
import hashlib
import subprocess

REQUIRED_TESTS = {
    'ota-package', 'reference-initialization', 'css-null-ancestor', 'skin-language-symbol',
    'dependency-compatibility', 'web-tls', 'standby-frontlight',
    'document-render', 'reading-state-migration', 'dictionary-safety', 'history-persistence', 'archive-boundaries', 'atomic-faults',
}

def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def source_metadata(root):
    files = subprocess.check_output(
        ['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard'], cwd=root
    ).split(b'\0')
    digest = hashlib.sha256()
    for name in sorted(set(files) - {b''}):
        path = root / name.decode('utf-8')
        digest.update(name + b'\0')
        if path.is_symlink():
            import os
            digest.update(b'link\0' + os.readlink(path).encode())
        elif path.is_file():
            digest.update(str(path.stat().st_mode & 0o777).encode() + b'\0' + path.read_bytes())
        else:
            digest.update(b'deleted')
    return {
        'commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
        'dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=root)),
        'tree_sha256': digest.hexdigest(),
    }

def verified_report(report, binary, root, build):
    if report.get('status') != 'passed' or set(report.get('passed', [])) != REQUIRED_TESTS:
        raise ValueError('Required ARM regressions have not all passed')
    if report.get('binary_sha256') != sha256(binary):
        raise ValueError('Regression report does not match this binary')
    if report.get('source') != source_metadata(root):
        raise ValueError('Source changed since regression tests ran')
    if report.get('configuration_sha256') != sha256(build / 'crengine/CMakeFiles/crengine.dir/flags.make'):
        raise ValueError('Build configuration changed since regression tests ran')
    return report
