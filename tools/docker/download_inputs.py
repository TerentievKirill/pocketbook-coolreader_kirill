#!/usr/bin/env python3
"""Download the pinned SDK and firmware; resume partial transfers and verify SHA-256."""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from targets import DEFAULT_TARGET, ROOT, load_target

def sha256(path):
    value=hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda:stream.read(1024*1024),b''):value.update(chunk)
    return value.hexdigest()

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cache',type=Path)
    parser.add_argument('--target',default=DEFAULT_TARGET)
    args=parser.parse_args()
    profile=load_target(args.target)
    cache=args.cache;cache.mkdir(parents=True,exist_ok=True)
    inputs=list(json.loads((ROOT/profile['inputs']).read_text()).values())
    candidates={item['id']:item for item in json.loads((ROOT/'config/firmware-candidates.json').read_text())['devices']}
    inputs += [candidates[name]['archive'] for name in profile.get('qualification_candidates',[])]
    for entry in inputs:
        target=cache/entry['filename']
        if target.is_file():
            if sha256(target)!=entry['sha256']:raise SystemExit('Checksum mismatch: '+str(target))
            print('Verified cached '+target.name,flush=True);continue
        partial=target.with_name(target.name+'.part')
        subprocess.run(['curl','--fail','--location','--retry','3','--continue-at','-',entry['url'],'--output',str(partial)],check=True)
        if sha256(partial)!=entry['sha256']:raise SystemExit('Checksum mismatch: '+str(partial))
        partial.replace(target)
