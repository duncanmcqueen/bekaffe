#!/bin/bash
# Configure and build GNU Classpath with the Haiku peer on Haiku.
# Put the patched, autoreconf-ed Classpath tree in $B/src first.
# CP_DIR: the build directory (default .../cp-haiku);
# CP_CONFIGURE: more configure options.
# Status lines go to $B/status; the last line is DONE or FAILED.
B=${CP_DIR:-/boot/home/Projects/bekaffe/cp-haiku}
export PATH=/boot/system/lib/openjdk8/bin:$PATH
export LIBRARY_PATH=${LIBRARY_PATH:-%A/lib:/boot/home/config/non-packaged/lib:/boot/home/config/lib:/boot/system/non-packaged/lib:/boot/system/lib}
cd $B
: > status
# Inputs first, then generated files, so make does not rerun autotools
# (the Haiku clock can be behind the machine that ran autoreconf).
touchall() { for f in "$@"; do [ -e "$f" ] && touch "$f"; done; }
find src -exec touch -h {} + ; sleep 2
touchall src/include/*.h; sleep 2
touch src/aclocal.m4; sleep 2
touch src/configure src/include/config.h.in; sleep 2
find src -name Makefile.in -exec touch {} +
# The 0.98 release ships these; makeinfo of today fails on them.
sleep 2; touchall src/doc/*.info
rm -rf build install && mkdir build && cd build
echo "CONFIGURE start $(date)" >> ../status
../src/configure --prefix=$B/install --enable-jni --disable-examples \
  --disable-plugin --disable-gjdoc --disable-gtk-peer --disable-gconf-peer \
  $CP_CONFIGURE > ../configure.log 2>&1
rc=$?; echo "CONFIGURE rc=$rc $(date)" >> ../status
[ $rc = 0 ] || { echo FAILED >> ../status; exit 1; }
make -j3 > ../make.log 2>&1
rc=$?; echo "MAKE rc=$rc $(date)" >> ../status
[ $rc = 0 ] || { echo FAILED >> ../status; exit 1; }
make install > ../install.log 2>&1
rc=$?; echo "INSTALL rc=$rc $(date)" >> ../status
[ $rc = 0 ] || { echo FAILED >> ../status; exit 1; }
echo DONE >> ../status
