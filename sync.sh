#!/usr/bin/env bash
# Push the Haiku AWT peer sources to the VM, stop old test runs, and rebuild.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
vm=$here/../vm
cd "$here/src"
timeout 20 "$vm/vmssh.sh" 'for p in $(ps | grep "kaffe-latest/kaffe/kaffe/" | grep -v grep | awk "{print \$2}"); do kill -9 $p; done; true'
tar czf - gnu native resource test applet demos build.sh run.sh 2>/dev/null \
  | timeout 30 "$vm/vmssh.sh" 'cd /boot/home/Projects/bekaffe/haiku-peer && tar xzmf - 2>/dev/null'
timeout 200 "$vm/vmssh.sh" 'cd /boot/home/Projects/bekaffe/haiku-peer && ./build.sh 2>&1 | grep -E "error|warning:|BUILD" '
