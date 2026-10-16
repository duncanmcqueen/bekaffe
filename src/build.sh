#!/bin/bash
# Build the Haiku AWT peer overlay (classes + libhaikupeer.so) on the VM.
set -e
B=/boot/home/Projects/bekaffe
P=$B/haiku-peer
GLIBJ=$B/cp-098-jni/lib/glibj.zip
# jni.h: any VM or Classpath copy works; Classpath installs one.
JNI_INC=${JNI_INC:-$B/cp-ingelabs/install/include}
JAVAC=/boot/system/lib/openjdk8/bin/javac
cd $P
rm -rf classes && mkdir -p classes lib
$JAVAC -source 1.6 -target 1.6 -Xlint:none -nowarn -bootclasspath $GLIBJ \
  -d classes gnu/java/awt/peer/haiku/*.java gnu/javax/sound/sampled/haiku/*.java 2>&1 | grep -v "bootstrap class path" || true
ls classes/gnu/java/awt/peer/haiku/HaikuToolkit.class >/dev/null
cp -r resource/gnu resource/META-INF classes/
# JNI headers for the native code.
${JAVAC%javac}javah -jni -d native/haiku-peer -bootclasspath classes:$GLIBJ \
  gnu.java.awt.peer.haiku.HaikuNative gnu.java.awt.peer.haiku.HaikuRaster \
  gnu.javax.sound.sampled.haiku.HaikuAudio
g++ -shared -fPIC -O2 -Wall -Wno-unused-parameter -I$JNI_INC \
  -o lib/libhaikupeer.so native/haiku-peer/gnu_java_awt_peer_haiku_HaikuNative.cpp \
  native/haiku-peer/gnu_java_awt_peer_haiku_HaikuRaster.cpp -lbe -ltracker -lmedia -ltranslation
mkdir -p testclasses
$JAVAC -source 1.6 -target 1.6 -Xlint:none -nowarn -bootclasspath $GLIBJ \
  -d testclasses test/*.java 2>&1 | grep -v "bootstrap class path" || true
$JAVAC -source 1.6 -target 1.6 -Xlint:none -nowarn -bootclasspath $GLIBJ \
  -d testclasses applet/bekaffe/applet/*.java 2>&1 | grep -v "bootstrap class path" || true
# Demo applets are compiled next to their HTML pages and images.
for d in demos/*/; do
  $JAVAC -source 1.6 -target 1.6 -Xlint:none -nowarn -bootclasspath $GLIBJ \
    -d $d $d*.java 2>&1 | grep -v "bootstrap class path" || true
done
echo BUILD OK
