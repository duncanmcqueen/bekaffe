#!/usr/bin/env bash
# Start Kaffe's AppletViewer on a demo page on the VM, detached.
# Usage: runav.sh <Demo/page.html> [viewer args...]
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
vm=$here/../vm
page=$1; shift
P=/boot/home/Projects/bekaffe/haiku-peer
log=$(echo "$page" | tr '/.' '__').log
timeout 15 "$vm/vmssh.sh" "cd $P && KAFFE_OPTS=\"${KAFFE_OPTS:-}\" nohup ./run.sh bekaffe.applet.AppletViewer $* $P/demos/$page </dev/null >$log 2>&1 & echo started $log" || true
