#!/usr/bin/env python3
"""Prepare private build inputs inside a Linux Docker volume. No reader is modified."""
from pathlib import Path
import gzip,json,os,shutil,struct,subprocess,sys,zipfile
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from targets import DEFAULT_TARGET, load_target, target_evidence
from download_inputs import sha256

root=Path('/src');work=root/'work'
profile=load_target(os.environ.get('CR3_TARGET',DEFAULT_TARGET))
inputs=json.loads((root/profile['inputs']).read_text())
marker=work/'docker-inputs.json'
expected={'schema':2,'target':target_evidence(profile),'inputs':inputs}
# Legacy PB632 caches contain these same pinned inputs and may be reused.
if marker.exists():
    previous=json.loads(marker.read_text())
    legacy=previous=={'schema':1,'inputs':inputs} and profile['id']=='pb632-fw6'
    if previous!=expected and not legacy:
        raise SystemExit('Docker input cache belongs to another target/profile; choose a new CR3_DOCKER_VOLUME')
    if legacy:marker.write_text(json.dumps(expected,indent=2)+'\n')
if marker.exists() and json.loads(marker.read_text())==expected:
    print('SDK and firmware runtime cache ready',flush=True)
    raise SystemExit(0)
for entry in inputs.values():
    assert sha256(Path('/downloads')/entry['filename'])==entry['sha256'],entry['filename']

def run(args,log=None):
    if log:
        with log.open('w') as stream:result=subprocess.run([str(x) for x in args],stdout=stream,stderr=subprocess.STDOUT)
        if result.returncode:raise RuntimeError(str(args)+' failed; see '+str(log))
    else:subprocess.run([str(x) for x in args],check=True)

sdkbase=work/'sdk';sdkbase.mkdir(exist_ok=True);archive=Path('/downloads')/inputs['sdk']['filename']
sdk=sdkbase/profile['sdk']['directory']
if not sdk.is_dir():
    print('Extracting SDK',flush=True)
    # 7z refuses some valid relative symlinks. Repair only links contained in
    # the SDK below; do not disable its link-path checks globally.
    with (work/'sdk-extract.log').open('w') as log:result=subprocess.run(['7z','x','-y',str(archive),'-o'+str(sdkbase)],stdout=log,stderr=subprocess.STDOUT)
    if result.returncode not in (0,2):raise RuntimeError('SDK extraction failed')
listing=subprocess.check_output(['7z','l','-slt',str(archive)],text=True)
entries=[]
for block in listing.split('\n\n'):
    d=dict(line.split(' = ',1) for line in block.splitlines() if ' = ' in line)
    if d.get('Attributes','').startswith('A l'):
        p=sdkbase/d['Path']
        assert p.is_relative_to(sdkbase)
        if not p.is_symlink() and (not p.exists() or p.is_file() and p.stat().st_size==0):entries.append(d)
if entries:
    names=work/'sdk-link-files.txt';names.write_text('\n'.join(d['Path'] for d in entries)+'\n')
    data=subprocess.check_output(['7z','x','-so',str(archive),'-i@'+str(names)])
    offset=0
    for d in entries:
        size=int(d['Size']);target=data[offset:offset+size].decode();offset+=size;p=sdkbase/d['Path']
        if (p.parent/target).resolve().is_relative_to(sdkbase.resolve()):
            p.unlink(missing_ok=True);p.symlink_to(target)
    assert offset==len(data)
run([sdk/('usr/bin/'+profile['sdk']['prefix']+'-g++'),'--version'])

firmware=root/profile['firmware']['directory'];firmware.mkdir(exist_ok=True)
raw=firmware/'SWUPDATE.BIN'
with zipfile.ZipFile(Path('/downloads')/inputs['firmware']['filename']) as archive:
    if not raw.exists():
        with archive.open('SWUPDATE.BIN') as src,raw.open('wb') as dst:shutil.copyfileobj(src,dst)
with raw.open('rb') as stream:
    header=stream.read(1024);assert header[:16]==b'PocketBookUpdate' and profile['firmware']['header_model'].encode() in header[:64]
    for kind,unused,offset,size in struct.iter_unpack('<4I',header[256:]):
        if kind not in (0x72,0x65):continue
        name='rootfs' if kind==0x72 else 'ebrmain';image=firmware/(name+'.img')
        if image.exists():continue
        assert offset+1024+size<=raw.stat().st_size
        stream.seek(offset+1024);compressed=firmware/(name+'.gz')
        with compressed.open('wb') as dst:
            remaining=size
            while remaining:
                chunk=stream.read(min(remaining,1024*1024));assert chunk;dst.write(chunk);remaining-=len(chunk)
        with gzip.open(compressed,'rb') as src,image.open('wb') as dst:shutil.copyfileobj(src,dst)
# Firmware stores its userland in ext2 plus a SquashFS file named cramfs.img.
# Use userspace extractors rather than privileged mounts or device access.
runtime=firmware/'runtime';runtime.mkdir(exist_ok=True);ebrmain=firmware/'ebrmain';ebrmain.mkdir(exist_ok=True)
run(['debugfs','-R','rdump /lib '+str(runtime),firmware/'rootfs.img'],work/'runtime-extract.log')
run(['debugfs','-R','dump /cramfs.img '+str(ebrmain/'cramfs.img'),firmware/'ebrmain.img'],work/'image-extract.log')
if not (ebrmain/'cramfs').exists():
    run(['unsquashfs','-no-progress','-processors','2','-d',ebrmain/'cramfs',ebrmain/'cramfs.img'],work/'squashfs-extract.log')
# Add userland libraries to the root filesystem loader. Their SONAMEs are
# symlinks in /ebrmain/lib; recreate only aliases whose targets were extracted.
shutil.copytree(ebrmain/'cramfs/lib',runtime/'lib',dirs_exist_ok=True,symlinks=True)
# debugfs cannot read a few of the firmware's long symlinks. Parse its symlink
# metadata explicitly instead of requiring a filesystem mount.
listing=subprocess.check_output(['debugfs','-R','ls -l /lib',str(firmware/'ebrmain.img')],stderr=subprocess.DEVNULL,text=True)
for line in listing.splitlines():
    fields=line.split()
    if len(fields)<9 or fields[1]!='120777':continue
    name=fields[-1]
    details=subprocess.check_output(['debugfs','-R','stat /lib/'+name,str(firmware/'ebrmain.img')],stderr=subprocess.DEVNULL,text=True)
    import re
    target=re.search(r'Fast link dest: "([^"]+)"',details)
    if target:
        candidate=runtime/'lib'/Path(target.group(1)).name
        alias=runtime/'lib'/name
        if candidate.is_file() and not alias.exists():
            alias.unlink(missing_ok=True);alias.symlink_to(candidate.name)
for filename in ['DejaVuSans.ttf','DejaVuSans-Bold.ttf','DroidSans.ttf']:assert (ebrmain/'cramfs/fonts'/filename).is_file(),filename
assert (runtime/profile['firmware']['loader']).is_file()
run(['qemu-arm','-L',runtime,runtime/profile['firmware']['loader'],'--version'])
marker.write_text(json.dumps(expected,indent=2)+'\n')
print('SDK and extracted firmware runtime ready',flush=True)
