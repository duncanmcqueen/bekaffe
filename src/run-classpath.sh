#!/bin/bash
# Run a class with Kaffe and the GNU Classpath build that includes the
# Haiku peer. No overlay and no -Dawt.toolkit: Classpath's defaults apply.
# Usage: run-classpath.sh <Class> [args...]
B=/boot/home/Projects/bekaffe
I=$B/cp-haiku/install
cd $B/checkout/kaffe-latest
VMJ=libraries/javalib/vmspecific/kaffe-vminterface.jar
export KAFFEHOME=$B/cp-install
export KAFFELIBRARYPATH="$I/lib/classpath:$PWD/kaffe/kaffevm/.libs"
export BOOTCLASSPATH="$VMJ:$I/share/classpath/glibj.zip"
exec ./kaffe/kaffe/kaffe-bin $KAFFE_OPTS \
  -classpath "$VMJ:$I/share/classpath/glibj.zip:$B/haiku-peer/testclasses" "$@"
