#!/bin/bash
# Run a class with JamVM and a GNU Classpath build that includes the
# Haiku peer. No overlay and no -Dawt.toolkit: Classpath's defaults apply.
# java.awt.printerjob selects Haiku printing; the user sets it, not the toolkit.
# CP_INSTALL: the Classpath install directory; JAMVM_DIR: the JamVM install.
# Usage: run-jamvm.sh <Class> [args...]
B=/boot/home/Projects/bekaffe
I=${CP_INSTALL:-$B/cp-ingelabs/install}
J=${JAMVM_DIR:-$B/jamvm-install}
# JamVM finds glibj.zip through its configure setting; JNI libraries
# (Classpath's and libhaikupeer.so) are found through LIBRARY_PATH.
export LIBRARY_PATH="$I/lib/classpath:${LIBRARY_PATH:-%A/lib:/boot/home/config/non-packaged/lib:/boot/home/config/lib:/boot/system/non-packaged/lib:/boot/system/lib}"
exec $J/bin/jamvm $JAMVM_OPTS \
  -Djava.awt.printerjob=gnu.java.awt.peer.haiku.HaikuPrinterJob -cp "${CLASSPATH:-$B/haiku-peer/testclasses}" "$@"
