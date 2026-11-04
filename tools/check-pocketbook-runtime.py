#!/usr/bin/env python3
"""Check an application against an extracted target firmware runtime."""
import argparse
import json
from pathlib import Path
from runtime_compatibility import check_runtime
from targets import DEFAULT_TARGET, firmware_path, load_target, tool

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('application', type=Path)
parser.add_argument('--target', default=DEFAULT_TARGET)
parser.add_argument('--runtime', type=Path)
parser.add_argument('--readelf', type=Path)
args = parser.parse_args()
try:
    target = load_target(args.target)
    result = check_runtime(args.application, args.runtime or firmware_path(target) / 'runtime',
                           args.readelf or tool(target, 'readelf'), target['firmware'])
except ValueError as error:
    parser.error(str(error))
print(json.dumps(result, indent=2))
