#!/usr/bin/env python3
"""misra_run.py <file.dump> - run cppcheck's MISRA C:2012 addon on one dump file
and print only findings located in the project (src/, include/) as
    path:line: <rule>
System-header findings are dropped."""
import glob, os, re, subprocess, sys

CANDIDATES = [os.environ.get("MISRA_PY", "")] + [
    "/usr/lib/x86_64-linux-gnu/cppcheck/addons/misra.py",
    "/usr/share/cppcheck/addons/misra.py",
    "/usr/local/share/cppcheck/addons/misra.py",
] + glob.glob("/usr/local/lib/python3*/dist-packages/cppcheck/Cppcheck/addons/misra.py")

misra = next((c for c in CANDIDATES if c and os.path.exists(c)), None)
if misra is None:
    sys.exit("misra.py not found (set MISRA_PY=/path/to/misra.py)")

res = subprocess.run([sys.executable, misra, sys.argv[1]],
                     capture_output=True, text=True)
pat = re.compile(r"^\[((?:src|include)/[^:\]]+):(\d+)\].*\[misra-c2012-([\d.]+)\]")
for line in (res.stderr + res.stdout).splitlines():
    m = pat.match(line)
    if m:
        print(f"{m.group(1)}:{m.group(2)}: {m.group(3)}")
