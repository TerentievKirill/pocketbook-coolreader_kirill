"""Probe the production startup entry point with an emulated child executable."""
from pathlib import Path
import hashlib
import os
import shlex
import shutil
import subprocess


def probe_startup(application, runtime, qemu, output, welcome, entry_source=None):
    command = [qemu, '-L', runtime, '-E', 'LD_LIBRARY_PATH=' + str(runtime / 'lib'),
               '-E', 'LD_BIND_NOW=1', '-E', 'LD_DEBUG=libs', application]
    if entry_source:
        directory = output / 'startup-entry'
        directory.mkdir(exist_ok=True)
        entry = directory / 'cr3-pb.app'
        shutil.copy2(entry_source, entry)
        child = directory / 'cr3-pb-executable.app'
        child.write_text('#!/bin/sh\nexec ' + shlex.join(str(x) for x in command) + ' "$@"\n')
        child.chmod(0o644)  # Permissions produced by the 1.0.3 OTA extractor.
        command = ['sh', entry]
    try:
        result = subprocess.run([str(x) for x in command + [welcome]], capture_output=True,
                                text=True, errors='replace', timeout=8, env=dict(os.environ, LC_ALL='C'))
        report = {'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr}
    except subprocess.TimeoutExpired as error:
        def text(value):
            return value.decode(errors='replace') if isinstance(value, bytes) else value or ''
        report = {'exit_code': None, 'timeout': True, 'stdout': text(error.stdout), 'stderr': text(error.stderr)}
    report['control_transferred'] = 'transferring control:' in report['stderr']
    if entry_source:
        report['entrypoint_sha256'] = hashlib.sha256(Path(entry_source).read_bytes()).hexdigest()
    return report
