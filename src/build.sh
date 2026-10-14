#!/bin/bash
# Build the Haiku AWT peer overlay (classes + libhaikupeer.so) on the VM.
set -e
B=/boot/home/Projects/bekaffe
P=$B/haiku-peer
GLIBJ=$B/cp-098-jni/lib/glibj.zip
KINC=$B/checkout/kaffe-latest/include
JAVAC=/boot/system/lib/openjdk8/bin/javac
cd $P
rm -rf classes && mkdir -p classes lib
$JAVAC -source 1.6 -target 1.6 -Xlint:none -nowarn -bootclasspath $GLIBJ \
  -d classes gnu/java/awt/peer/haiku/*.java gnu/javax/sound/sampled/haiku/*.java 2>&1 | grep -v "bootstrap class path" || true
ls classes/gnu/java/awt/peer/haiku/HaikuToolkit.class >/dev/null
cp -r resource/gnu resource/META-INF classes/
g++ -shared -fPIC -O2 -Wall -Wno-unused-parameter -I$KINC \
  -o lib/libhaikupeer.so native/haiku-peer/gnu_java_awt_peer_haiku_HaikuNative.cpp -lbe -ltracker -lmedia
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
