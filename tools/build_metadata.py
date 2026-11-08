"""Hash source and configuration so test evidence cannot migrate between builds."""
from pathlib import Path
import hashlib
import subprocess
import json
import os
from targets import DEFAULT_TARGET, load_target, target_evidence, firmware_path, verify_build_target
from runtime_compatibility import runtime_fingerprint

REQUIRED_TESTS = {
    'shared-device-profiles',
    'frontlight-status', 'ota-ui', 'ota-package', 'reference-initialization', 'css-null-ancestor', 'skin-language-symbol',
    'dependency-compatibility', 'web-tls', 'standby-frontlight',
    'gray-buffer-copy',
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

def verified_report(report, binary, root, build, target=None):
    target = target or load_target(DEFAULT_TARGET)
    verify_build_target(build, target)
    if report.get("target") != target_evidence(target):
        raise ValueError("Regression report belongs to a different target/profile")
    if report.get("runtime_compatibility", {}).get("status") != "passed":
        raise ValueError("Firmware compatibility verification has not passed")
    runtime_files = report['runtime_compatibility'].get('files_sha256')
    if not runtime_files or runtime_fingerprint(firmware_path(target) / 'runtime', runtime_files) != runtime_files:
        raise ValueError('Firmware runtime changed since regression tests ran')
    if report.get('status') != 'passed' or set(report.get('passed', [])) != REQUIRED_TESTS:
        raise ValueError('Required ARM regressions have not all passed')
    if report.get('binary_sha256') != sha256(binary):
        raise ValueError('Regression report does not match this binary')
    if target.get('device_registry') and (
            not report.get('startup_probe', {}).get('control_transferred')
            or report['startup_probe'].get('entrypoint_sha256') != sha256(root / target['package']['assets'] / 'runtime-entry.app')):
        raise ValueError('Primary startup did not use the packaged entry point')
    if report.get('source') != source_metadata(root):
        raise ValueError('Source changed since regression tests ran')
    if report.get('configuration_sha256') != sha256(build / 'crengine/CMakeFiles/crengine.dir/flags.make'):
        raise ValueError('Build configuration changed since regression tests ran')
    return report


def verified_firmware_reports(binary, root, build, target, primary):
    candidates = {entry['id']: entry for entry in json.loads((root / 'config/firmware-candidates.json').read_text())['devices']}
    directory = Path(os.environ.get('CR3_QUALIFICATION_DIR', root / 'work/qualification'))
    reports = {}
    for name in target.get('qualification_candidates', []):
        report = json.loads((build / 'firmware-qualification' / (name + '.json')).read_text())
        if (report.get('status') != 'laboratory-checks-passed' or report.get('baseline_binary_sha256') != sha256(binary)
                or report.get('baseline_validation') != primary or report.get('device') != candidates[name]
                or not report.get('startup_probe', {}).get('control_transferred')
                or report.get('loader_resolution', {}).get('exit_code') != 0
                or report.get('inkview_imports', {}).get('status') != 'passed'):
            raise ValueError('Missing, failed or mismatched firmware evidence: ' + name)
        if (target.get('device_registry') and report.get('startup_probe', {}).get('entrypoint_sha256')
                != sha256(root / target['package']['assets'] / 'runtime-entry.app')):
            raise ValueError('Firmware startup did not use the packaged entry point: ' + name)
        for tool_name, digest in report['inspection_tools_sha256'].items():
            if sha256(root / 'tools' / tool_name) != digest:
                raise ValueError('Firmware inspection tools changed: ' + name)
        files = report['runtime_compatibility'].get('files_sha256')
        if (report['runtime_compatibility'].get('status') != 'passed' or not files
                or runtime_fingerprint(directory / name / 'runtime', files) != files):
            raise ValueError('Qualified firmware files changed: ' + name)
        probes = report.get('regression_probes', [])
        expected = {'reference-initialization', 'css-null-ancestor', 'dependency-compatibility',
                    'dictionary-safety', 'history-persistence', 'archive-boundaries', 'atomic-faults',
                    'reading-state-migration', 'document-render', 'skin-language-symbol',
                    'frontlight-status', 'standby-frontlight', 'shared-device-profiles'}
        if {probe['name'] for probe in probes} != expected or any(probe['status'] != 'passed' for probe in probes):
            raise ValueError('Firmware software probes have not all passed: ' + name)
        reports[name] = report
    return reports
