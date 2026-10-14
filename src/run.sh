#!/bin/bash
# Run a test class with Kaffe and the Haiku AWT peer overlay.
# Usage: run.sh <Class> [args...]
B=/boot/home/Projects/bekaffe
P=$B/haiku-peer
cd $B/checkout/kaffe-latest
VMJ=libraries/javalib/vmspecific/kaffe-vminterface.jar
GLIBJ=$B/cp-098-jni/lib/glibj.zip
N=$B/cp-098-jni/native/jni
export KAFFEHOME=$B/cp-install
export KAFFELIBRARYPATH="$P/lib:$N/java-nio/.libs:$N/java-io/.libs:$N/java-lang/.libs:$N/java-util/.libs:$N/java-net/.libs:$N/native-lib/.libs:$PWD/kaffe/kaffevm/.libs"
export BOOTCLASSPATH="$P/classes:$VMJ:$GLIBJ"
exec ./kaffe/kaffe/kaffe-bin $KAFFE_OPTS -Dawt.toolkit=gnu.java.awt.peer.haiku.HaikuToolkit \
  -classpath "$VMJ:$GLIBJ:$P/testclasses" "$@"
