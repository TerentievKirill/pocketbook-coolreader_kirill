#!/usr/bin/env python3
"""Run ARM regressions using the configured build and extracted firmware runtime."""
from pathlib import Path
import argparse, datetime, io, json, os, re, shlex, subprocess, zipfile
import struct, zlib, shutil
import http.server, ssl, threading
from build_metadata import sha256, source_metadata
from targets import DEFAULT_TARGET, load_target, firmware_path, tool, target_evidence, verify_build_target
from runtime_compatibility import check_runtime
from runtime_startup import probe_startup
root = Path(__file__).resolve().parent.parent
os.chdir(root)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--target', default=DEFAULT_TARGET)
parser.add_argument('--build', type=Path)
parser.add_argument('--application', type=Path)
args = parser.parse_args()
profile = load_target(args.target)
build = (args.build or root / profile['build']['directory']).resolve()
firmware = firmware_path(profile)
application = args.application or build / 'package/cr3-pb.app'
report_path = build / 'test-results.json'
report_path.unlink(missing_ok=True)
verify_build_target(build, profile)
source_before = source_metadata(root)
compiler = tool(profile, 'g++')
bundled_qemu = root / 'work/host/usr/bin/qemu-arm'
qemu = Path(os.environ.get('CR3_QEMU') or (str(bundled_qemu) if bundled_qemu.is_file() else shutil.which('qemu-arm') or 'qemu-arm'))
runtime = firmware / 'runtime'
out = build / 'arm-regressions'
out.mkdir(exist_ok=True)
makeflags = (build / 'crengine/CMakeFiles/crengine.dir/flags.make').read_text()
def flags(name):
    return shlex.split(re.search(r'^' + name + r' = (.*)$', makeflags, re.M).group(1))
options = flags('CXX_DEFINES') + flags('CXX_INCLUDES') + flags('CXX_FLAGS')
options += ['-I' + str(build / 'cr3gui')]
if profile['build']['cmake'].get('CR3_SHARED_PACKAGE') == 'ON':
    options += ['-DCR3_SHARED_PACKAGE=1']
dependencies = json.loads((build / 'dependency-link.json').read_text())
libraries = [build / p for p in ['crengine/libcrengine.a', 'tinydict/libtinydict.a', 'thirdparty/chmlib/libchmlib.a', 'thirdparty/antiword/libantiword.a']]
libraries += [Path(p) for p in dependencies['archives']]
class StreamingBuffer(io.BytesIO):
    def seekable(self): return False
    def seek(self, *args): raise io.UnsupportedOperation('streaming ZIP')
for name, entry, method, streaming in [
    ('utf8-stored.zip', 'Тест/章节-é.xhtml', zipfile.ZIP_STORED, False),
    ('utf8-deflated.zip', 'Тест/章节-é.xhtml', zipfile.ZIP_DEFLATED, False),
    ('descriptor-stored.zip', 'chapter.xhtml', zipfile.ZIP_STORED, True),
    ('descriptor-deflated.zip', 'chapter.xhtml', zipfile.ZIP_DEFLATED, True),
    ('ascii.zip', 'chapter.xhtml', zipfile.ZIP_DEFLATED, False),
]:
    target = StreamingBuffer() if streaming else io.BytesIO()
    with zipfile.ZipFile(target, 'w', method) as z: z.writestr(entry, b'payload')
    (out / name).write_bytes(target.getvalue())
legacy = io.BytesIO()
legacy_name = 'Тест.xhtml'.encode('cp866')
placeholder = 'x' * len(legacy_name)
with zipfile.ZipFile(legacy, 'w') as z:
    info = zipfile.ZipInfo(placeholder); info.create_system = 0
    z.writestr(info, b'payload')
(out / 'legacy.zip').write_bytes(legacy.getvalue().replace(placeholder.encode(), legacy_name))
# Replace a UTF-8 character with an invalid leading byte, preserving header lengths.
bad = (out / 'utf8-stored.zip').read_bytes().replace('Т'.encode(), b'\xc0\x80')
(out / 'invalid-utf8.zip').write_bytes(bad)
# OTA fixtures cover verified extraction, corruption, traversal and release identity.
shared_ota = profile['build']['cmake'].get('CR3_SHARED_PACKAGE') == 'ON'
compatibility = '632 U632.6.11.2001\n634 U634.6.10.3425\n700 U700.6.10.3209\n743G U743g.6.11.1683\n'
def add_shared_identity(entries, api=False):
    if shared_ota:
        entries['package-family.txt'] = 'pocketbook-armv7-softfp\n'
        entries['firmware-compatibility.txt'] = compatibility
        if api:
            entries['firmware-api-compatibility.txt'] = '632 610\n634 610\n700 610\n743G 610\n'
    return entries
for name in ('good', 'wrong-version', 'traversal', 'missing-binary', 'wrong-family', 'wrong-firmware', 'good-api', 'wrong-api'):
    with zipfile.ZipFile(out / ('ota-' + name + '.zip'), 'w', zipfile.ZIP_DEFLATED) as archive:
        entries = {'release-version.txt': '1.0.0\n', 'bin/cr3-pb.app': 'binary', 'bin/cr3-pb-executable.app': 'ELF payload', 'cr3-defaults.ini': 'defaults', 'skins/pb631fw5.cr3skin': 'skin'}
        add_shared_identity(entries)
        if name == 'wrong-family': entries['package-family.txt'] = 'pocketbookhf\n'
        if name == 'wrong-firmware': entries['firmware-compatibility.txt'] = '632 U632.6.9.1\n'
        if name == 'wrong-version': entries['release-version.txt'] = '0.9.0\n'
        if name == 'traversal': entries['../escaped'] = 'bad'
        if name == 'missing-binary': del entries['bin/cr3-pb.app']
        if name in ('good-api', 'wrong-api'):
            entries['firmware-api-compatibility.txt'] = '632 ' + ('610' if name == 'good-api' else '612') + '\n'
        for entry, data in entries.items(): archive.writestr('system/share/cr3/' + entry, data)
with zipfile.ZipFile(out / 'ota-ui-success.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
    for entry, data in add_shared_identity({'release-version.txt': '1.0.2\n', 'bin/cr3-pb.app': 'binary', 'cr3-defaults.ini': 'defaults', 'skins/pb631fw5.cr3skin': 'skin'}, api=True).items():
        archive.writestr('system/share/cr3/' + entry, data)
# A small EPUB exercises the new parser, CSS and image/page-painting path.
def png_chunk(kind, data):
    return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data) & 0xffffffff)
pixel = b'\x89PNG\r\n\x1a\n' + png_chunk(b'IHDR', struct.pack('!2I5B', 1, 1, 8, 2, 0, 0, 0)) + png_chunk(b'IDAT', zlib.compress(b'\x00\xff\x00\x00')) + png_chunk(b'IEND', b'')
epub = out / 'regression-章节.epub'
with zipfile.ZipFile(epub, 'w', zipfile.ZIP_DEFLATED) as book:
    book.writestr('mimetype', 'application/epub+zip', compress_type=zipfile.ZIP_STORED)
    book.writestr('META-INF/container.xml', '<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
    book.writestr('OPS/content.opf', '<package version="2.0" unique-identifier="id" xmlns="http://www.idpf.org/2007/opf"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>EPUB regression</dc:title><dc:language>ru</dc:language><dc:identifier id="id">regression</dc:identifier></metadata><manifest><item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/><item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/><item id="image" href="pixel.png" media-type="image/png"/></manifest><spine toc="ncx"><itemref idref="chapter"/></spine></package>')
    book.writestr('OPS/toc.ncx', '<ncx version="2005-1" xmlns="http://www.daisy.org/z3986/2005/ncx/"><head/><docTitle><text>EPUB regression</text></docTitle><navMap><navPoint id="chapter" playOrder="1"><navLabel><text>Chapter</text></navLabel><content src="chapter.xhtml"/></navPoint></navMap></ncx>')
    book.writestr('OPS/chapter.xhtml', '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Chapter</title><style>p { text-indent: 1em; color: #111; }</style></head><body><h1>Проверка EPUB</h1><p>Привет! Unicode 😀.</p><img src="pixel.png" width="16" height="16"/>' + '<p>Paragraph layout and page rendering.</p>' * 40 + '</body></html>')
    book.writestr('OPS/pixel.png', pixel)
# Keep a legacy DOM fixture in both ordinary and ZIP form.
migration_xml = '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><book-title>Migration</book-title></title-info></description><body><section><p>First paragraph.</p><p>Saved reading position with <strong>inline text</strong> after it.</p></section></body></FictionBook>'
(out / 'migration.fb2').write_text(migration_xml)
with zipfile.ZipFile(out / 'migration.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
    archive.writestr('migration.fb2', migration_xml)
# A private local CA makes HTTPS checks independent of the public network.
class TLSHandler(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    def handle(self):
        try:
            super().handle()
        except (ConnectionResetError, ssl.SSLEOFError):
            # Negative certificate tests deliberately abort the TLS connection.
            pass
    def do_GET(self):
        if self.path == '/missing':
            self.send_response(404)
            self.send_header('Content-Length', '0')
            self.end_headers()
        elif self.path == '/slow':
            import time
            payload = b'x' * (128 * 1024)
            self.send_response(200)
            self.send_header('Content-Length', str(len(payload)))
            self.end_headers()
            try:
                for offset in range(0, len(payload), 4096):
                    self.wfile.write(payload[offset:offset+4096]); self.wfile.flush(); time.sleep(0.005)
            except (BrokenPipeError, ConnectionResetError): pass
        elif self.path == '/redirect-file':
            self.send_response(302)
            self.send_header('Location', 'file:///etc/passwd')
            self.send_header('Content-Length', '0')
            self.end_headers()
        else:
            self.send_response(200)
            self.send_header('Content-Length', str(len(b'TLS verified\n')))
            self.end_headers()
            self.wfile.write(b'TLS verified\n')
    def log_message(self, *args): pass
for name in ('trusted', 'untrusted'):
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                    '-days', '1', '-subj', '/CN=Dependency regression ' + name,
                    '-addext', 'subjectAltName=IP:127.0.0.1',
                    '-keyout', str(out / (name + '.key')),
                    '-out', str(out / (name + '.crt'))],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), TLSHandler)
context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
context.load_cert_chain(out / 'trusted.crt', out / 'trusted.key')
server.socket = context.wrap_socket(server.socket, server_side=True)
threading.Thread(target=server.serve_forever, daemon=True).start()
tls_url = 'https://127.0.0.1:' + str(server.server_port)
readelf = tool(profile, 'readelf')
arm_attributes = subprocess.check_output([str(readelf), '-A', str(application)], text=True)
if profile['build']['cmake'].get('CR3_SHARED_PACKAGE') == 'ON':
    if not re.search(r'Tag_FP_arch: VFPv3(?:\s|$)', arm_attributes) or 'Fused-MAC' in arm_attributes:
        raise SystemExit('Shared binary exceeds the intended NEON/VFPv3 baseline')
runtime_check = check_runtime(application, runtime, readelf, profile['firmware'])
startup = None
if profile.get('device_registry'):
    startup = probe_startup(application, runtime, qemu, out, root / profile['package']['assets'] / 'Welcome.fb2',
                            root / profile['package']['assets'] / 'runtime-entry.app')
    (out / 'startup.log').write_text(startup['stdout'] + startup['stderr'])
    if not startup['control_transferred']:
        raise SystemExit('Production runtime entry point did not reach the application; see startup.log')
    startup['stdout'] = startup['stdout'][-2000:]
    startup['stderr'] = startup['stderr'][-3000:]
symbols = subprocess.check_output([str(readelf), '--dyn-syms', '--wide', str(application)], text=True)
if re.search(r'\b(?:FT_|jpeg_|png_|curl_|deflate|inflate|zlibVersion)\w*\b', symbols):
    raise SystemExit('Application must not import/export firmware copies of private library APIs')
needed = subprocess.check_output([str(readelf), '-d', str(application)], text=True)
if re.search(r'NEEDED.*\[lib(?:png|z\.|curl)', needed):
    raise SystemExit('Application must link private image/compression/network libraries')
passed = []
for name, sources, engine, arguments in [
    ('shared-device-profiles', ['tests/shared-device-profiles.cpp'], True, [firmware / 'ebrmain/cramfs/fonts/DejaVuSans.ttf', root / 'data/pocketbook-armv7/cr3-defaults.ini']),
    ('reference-initialization', ['tests/lvref-static-global.cpp', 'tests/lvref-static-sentinel.cpp'], False, []),
    ('frontlight-status', ['tests/frontlight-status.cpp'], True, [firmware / 'ebrmain/cramfs/fonts/DejaVuSans.ttf', out]),
    ('standby-frontlight', ['tests/standby-frontlight.cpp'], False, []),
    ('css-null-ancestor', ['tests/css-selector-null-ancestor.cpp'], True, []),
    ('skin-language-symbol', ['tests/skin-language-symbol.cpp'], True, [firmware / 'ebrmain/cramfs/fonts/DejaVuSans.ttf', firmware / 'ebrmain/cramfs/fonts/DejaVuSans-Bold.ttf', build / 'cr3gui/pb631fw5.cr3skin', out]),
    ('document-render', ['tests/document-render-startup.cpp'], True, [firmware / 'ebrmain/cramfs/fonts/DejaVuSans.ttf', firmware / 'ebrmain/cramfs/fonts/DejaVuSans-Bold.ttf', firmware / 'ebrmain/cramfs/fonts/DroidSans.ttf', root / profile['package']['assets'] / 'cr3-defaults.ini', root / 'cr3gui/data/hyph/', epub, root / 'cr3gui/data/fb2.css']),
    ('reading-state-migration', ['tests/reading-state-migration.cpp'], True, [firmware / 'ebrmain/cramfs/fonts/DejaVuSans.ttf', out / 'migration.fb2', out / 'migration.zip']),
    ('dependency-compatibility', ['tests/dependency-compatibility.cpp'], True, [out]),
    ('web-tls', ['tests/web-tls.cpp', 'cr3gui/src/web.cpp', 'cr3gui/src/web_transfer.cpp'], True, [tls_url, out / 'trusted.crt', out / 'untrusted.crt', out]),
    ('dictionary-safety', ['tests/dictionary-safety.cpp'], True, []),
    ('history-persistence', ['tests/history-persistence.cpp'], True, [out]),
    ('ota-ui', ['tests/ota-ui.cpp'], True, [out]),
    ('ota-package', ['tests/ota-package.cpp'], True, [out]),
    ('archive-boundaries', ['tests/archive-boundaries.cpp'], True, [out]),
    ('atomic-faults', ['tests/atomic-faults.cpp'], True, [out]),
]:
    binary = out / name
    command = [compiler] + options + sources
    if engine:
        command += ['-Wl,--exclude-libs,ALL', '-Wl,--start-group'] + libraries + ['-Wl,--end-group', '-linkview'] + ['-l' + name for name in dependencies['runtime']]
    subprocess.run([str(x) for x in command + ['-o', binary]], check=True)
    subprocess.run([str(x) for x in [qemu, '-L', runtime, '-E', 'LD_LIBRARY_PATH=' + str(runtime / 'lib'), binary] + arguments], check=True, timeout=30)
    if name == 'atomic-faults':
        preload = out / 'atomic-fsync-failure.so'
        subprocess.run([str(compiler).replace('-g++', '-gcc'), '-shared', '-fPIC',
                        'tests/atomic-fsync-failure.c', '-o', str(preload)], check=True)
        subprocess.run([str(x) for x in [qemu, '-L', runtime, '-E',
                       'LD_LIBRARY_PATH=' + str(runtime / 'lib'), '-E',
                       'LD_PRELOAD=' + str(preload), binary, out, 'sync-failure']],
                       check=True, timeout=30)
    passed.append(name)
    print('PASS:', name, flush=True)
server.shutdown()
server.server_close()
if source_metadata(root) != source_before:
    raise SystemExit('Sources changed while tests were running')
report = {
    'schema': 1, 'status': 'passed', 'passed': passed,
    'finished_at': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'binary_sha256': sha256(application), 'source': source_before,
    'target': target_evidence(profile), 'runtime_compatibility': runtime_check,
    'startup_probe': startup,
    'arm_attributes': arm_attributes,
    'configuration_sha256': sha256(build / 'crengine/CMakeFiles/crengine.dir/flags.make'),
    'environment': 'ARM/QEMU with extracted PocketBook firmware runtime; no physical device test',
}
report_path.write_text(json.dumps(report, indent=2) + '\n')
