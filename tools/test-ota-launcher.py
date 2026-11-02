#!/usr/bin/env python3
"""Exercise the production launcher against staged updates in isolated storage."""
from pathlib import Path
import subprocess,tempfile
source=Path(__file__).resolve().parents[1]/'data/touch-hd3/cr3-pb.app'
with tempfile.TemporaryDirectory() as tmp:
 root=Path(tmp);share=root/'system/share/cr3';share.mkdir(parents=True)
 (root/'system').mkdir(exist_ok=True)
 (share/'bin').mkdir();(share/'bin/cr3-pb.app').write_text('#!/bin/sh\nexit 0\n');(share/'bin/cr3-pb.app').chmod(0o755)
 (share/'custom-font.ttf').write_text('user font')
 (share/'Welcome.fb2').write_text('book')
 (share/'last-book.txt').write_text(str(share/'Welcome.fb2')+'\n')
 pending=share.parent/'cr3-ota-new';(pending/'bin').mkdir(parents=True)
 (pending/'bin/cr3-pb.app').write_text('#!/bin/sh\nprintf updated > "'+tmp+'/ran"\nexit 0\n');(pending/'bin/cr3-pb.app').chmod(0o755)
 script=root/'launcher';script.write_text(source.read_text().replace('/mnt/ext1',tmp).replace('    '+tmp+'/system/share/cr3/bin/cr3-pb.app', '    sh '+tmp+'/system/share/cr3/bin/cr3-pb.app'))
 # An incomplete stage must never be installed.
 subprocess.run(['sh',str(script)],check=True)
 assert not (root/'ran').exists()
 (pending/'.ready').write_text('1.0.1\n')
 subprocess.run(['sh',str(script)],check=True)
 assert (root/'ran').read_text()=='updated'
 assert (share/'custom-font.ttf').read_text()=='user font'
 assert (share/'last-book.txt').read_text()==str(share/'Welcome.fb2')+'\n'
 assert not pending.exists() and not (share/'.ready').exists()
 # Recover a power interruption between the two directory renames.
 import shutil
 shutil.rmtree(share)
 subprocess.run(['sh',str(script)],check=True)
 assert (share/'bin/cr3-pb.app').exists()
print('PASS: incomplete stage, installation, custom assets, last book and interrupted-switch recovery')
