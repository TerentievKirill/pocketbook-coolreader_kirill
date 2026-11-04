#!/usr/bin/env python3
"""Compatibility entry point for the PB632 package."""
from pathlib import Path
import runpy
import sys
# Append last so this historical command always selects its original target.
sys.argv.extend(['--target', 'pb632-fw6'])
runpy.run_path(str(Path(__file__).with_name('package-pocketbook.py')), run_name='__main__')
