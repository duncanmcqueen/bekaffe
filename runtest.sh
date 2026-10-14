#!/usr/bin/env bash
# Start a test class detached on the VM, wait, then screenshot.
# Usage: runtest.sh <Class> <wait-seconds> <shot.png> [args...]
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
vm=$here/../vm
cls=$1; wait=$2; shot=$3; shift 3
timeout 15 "$vm/vmssh.sh" "cd /boot/home/Projects/bekaffe/haiku-peer && KAFFE_OPTS=\"${KAFFE_OPTS:-}\" nohup ./run.sh $cls $* </dev/null >$cls.log 2>&1 & echo started" || true
sleep "$wait"
"$vm/mon.sh" 'mouse_move 1 1' >/dev/null; sleep 1
"$vm/shot.sh" "$shot" >/dev/null
timeout 20 "$vm/vmssh.sh" "head -60 /boot/home/Projects/bekaffe/haiku-peer/$cls.log"
