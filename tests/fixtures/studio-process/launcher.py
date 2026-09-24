#!/usr/bin/env python3
"""An executable CLI stand-in; only local processes and temporary files."""
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

assert sys.argv[1] == "studio"
args = dict(zip(sys.argv[2::2], sys.argv[3::2]))
workspace = Path(args["--workspace"])
assert workspace.resolve() == Path.cwd().resolve()
assert all(Path(args[key]).is_absolute() for key in ("--run", "--runtime", "--workspace"))
(workspace / "arguments.json").write_text(json.dumps(sys.argv[1:]))
mode = Path(args["--run"]).read_text().strip()
if mode == "early":
    sys.stderr.write("fixture startup rejected")
    sys.exit(23)
if mode == "invalid":
    for url in ("https://localhost:5599/", "http://example.com:5599/",
                "http://localhost.evil:5599/", "http://localhost:5599@evil/",
                "http://localhost:5599/path", "http://127.0.0.2:5599/",
                "http://localhost:0/", "http://localhost:65536/"):
        print("  Local: " + url, flush=True)
    sys.exit(0)
if mode == "stderr":
    print("Local: http://localhost:5599/", file=sys.stderr, flush=True)
    sys.exit(0)
if mode == "tail":
    sys.stdout.write("Local: http://127.0.0.1:5612/")
    sys.exit(0)
if mode == "crash":
    os.kill(os.getpid(), signal.SIGKILL)
if mode == "timeout":
    time.sleep(120)
    sys.exit(0)
if mode == "ipv6":
    print("Local: http://[::1]:5613/", flush=True)
else:
    child = subprocess.Popen([sys.executable, "-c",
                              "import signal,time; signal.signal(signal.SIGTERM, signal.SIG_IGN); time.sleep(120)"
                              if mode == "stubborn" else "import time; time.sleep(120)"])
    (workspace / "child.pid").write_text(str(child.pid))
    # Reap the child on TERM; real esbuild also exits when its parent pipe closes.
    def terminate(_signal, _frame):
        child.terminate()
        child.wait(timeout=2)
        sys.exit(0)
    signal.signal(signal.SIGTERM, signal.SIG_IGN if mode == "stubborn" else terminate)
    for part in (b"Port 5599 is in use, trying another one...\n\x1b[3",
                 b"2m  \xe2\x9e\x9c  Lo", b"cal:\x1b[0m   http://local", b"host:5600/\n"):
        os.write(sys.stdout.fileno(), part)
        time.sleep(0.04)
    print("Local: http://localhost:5600/", flush=True)
while True:
    time.sleep(1)
