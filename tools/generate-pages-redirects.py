#!/usr/bin/env python3
"""Maintain the former GitHub Pages URLs as permanent moves to ReaderTools."""
from html import escape
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ORIGIN = 'https://readertools.eu'
PAGES = {
    'index.html': {
        'path': '/',
        'title': 'ReaderTools — CoolReader, FTP & SSH apps for PocketBook',
        'description': 'The PocketBook tools website has moved to ReaderTools. Find CoolReader and Easy Connect, the PocketBook FTP and SSH server, with installation guides and manuals.',
        'heading': 'PocketBook tools are now on ReaderTools',
        'sections': {'coolreader': '/coolreader/', 'easy-connect': '/easy-connect/'},
    },
    'installation.html': {
        'path': '/coolreader/installation/',
        'title': 'Install CoolReader on PocketBook — guide moved to ReaderTools',
        'description': 'The PocketBook CoolReader installation guide has moved. Check supported models, install over USB and open EPUB and FB2 books from Library.',
        'heading': 'The CoolReader installation guide has moved',
        'sections': {'wireless': '/coolreader/manual/#wireless', 'project': '/coolreader/#project'},
    },
    'easy-connect.html': {
        'path': '/easy-connect/',
        'title': 'PocketBook FTP & SSH server — Easy Connect guide moved',
        'description': 'Easy Connect for PocketBook has moved to ReaderTools. Find FTP transfers, SSH connections, installation, Library refresh and troubleshooting.',
        'heading': 'The Easy Connect FTP and SSH guide has moved',
        'sections': {
            'installation': '/easy-connect/installation/#installation',
            'remove': '/easy-connect/installation/#remove',
            **{key: '/easy-connect/manual/#' + key for key in ('ftp', 'ssh', 'library', 'security', 'troubleshooting')},
            **{key: '/easy-connect/downloads/#' + key for key in ('changes', 'source')},
        },
    },
}


def render(page):
    target = ORIGIN + page['path']
    # The static refresh works without JavaScript. Only bookmarked sections and
    # query strings need the early script, before the refresh can discard them.
    script = '''(() => {
  if (!location.search && !location.hash) return;
  const destination = new URL(TARGET);
  const sections = SECTIONS;
  const section = location.hash.slice(1);
  const movedSection = sections[section];
  const url = movedSection ? new URL(movedSection, destination.origin) : destination;
  url.search = location.search;
  if (!movedSection) url.hash = location.hash;
  location.replace(url.href);
})();'''.replace('TARGET', json.dumps(target)).replace('SECTIONS', json.dumps(page['sections']))
    links = ''.join(
        '<li><a href="' + ORIGIN + path + '">' + label + '</a></li>'
        for path, label in (
            ('/coolreader/', 'CoolReader for PocketBook'),
            ('/coolreader/installation/', 'Install CoolReader'),
            ('/coolreader/manual/', 'CoolReader manual'),
            ('/coolreader/downloads/', 'CoolReader downloads'),
            ('/easy-connect/', 'Easy Connect: PocketBook FTP &amp; SSH server'),
            ('/easy-connect/installation/', 'Install Easy Connect'),
            ('/easy-connect/manual/', 'FTP &amp; SSH manual'),
            ('/easy-connect/downloads/', 'Easy Connect downloads'),
        )
    )
    return f'''<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{escape(page['title'])}</title>
  <meta name="description" content="{escape(page['description'])}">
  <meta name="robots" content="index,follow">
  <link rel="canonical" href="{target}">
  <script>{script}</script>
  <meta http-equiv="refresh" content="0; url={target}">
</head>
<body>
  <main>
    <h1>{page['heading']}</h1>
    <p>Continue to <a href="{target}">the current page on ReaderTools</a>.</p>
    <nav aria-label="Current PocketBook tools and guides"><ul>{links}</ul></nav>
  </main>
</body>
</html>
'''


if __name__ == '__main__':
    for name, page in PAGES.items():
        (ROOT / 'docs' / name).write_text(render(page), encoding='utf-8')
    (ROOT / 'docs/.nojekyll').touch()
    print('Generated three permanent GitHub Pages moves with canonical targets and guide links.')
