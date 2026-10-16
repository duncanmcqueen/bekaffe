#!/bin/bash
# Configure and build JamVM on Haiku against a GNU Classpath install.
# Put the patched, autogen-ed JamVM tree in $B/jamvm first (see the README).
# B: the work directory (default /boot/home/Projects/bekaffe);
# CP_INSTALL: the Classpath install directory; JAMVM_CONFIGURE: more
# configure options.
# Status lines go to $B/jamvm-status; the last line is DONE or FAILED.
B=${B:-/boot/home/Projects/bekaffe}
CP=${CP_INSTALL:-$B/cp-ingelabs/install}
export PATH=/boot/system/lib/openjdk8/bin:$PATH
export LIBRARY_PATH=${LIBRARY_PATH:-%A/lib:/boot/home/config/non-packaged/lib:/boot/home/config/lib:/boot/system/non-packaged/lib:/boot/system/lib}
S=$B/jamvm-status
: > $S
# Inputs first, then generated files, so make does not rerun autotools
# (the Haiku clock can be behind the machine that ran autogen.sh).
cd $B/jamvm
find . -exec touch -h {} + ; sleep 2
touch aclocal.m4; sleep 2
touch configure src/config.h.in; sleep 2
find . -name Makefile.in -exec touch {} +
cd $B && rm -rf jamvm-build jamvm-install && mkdir jamvm-build && cd jamvm-build
echo "CONFIGURE start $(date)" >> $S
../jamvm/configure --prefix=$B/jamvm-install --with-classpath-install-dir=$CP \
  $JAMVM_CONFIGURE > configure.log 2>&1
rc=$?; echo "CONFIGURE rc=$rc $(date)" >> $S
[ $rc = 0 ] || { echo FAILED >> $S; exit 1; }
make -j4 > make.log 2>&1
rc=$?; echo "MAKE rc=$rc $(date)" >> $S
[ $rc = 0 ] || { echo FAILED >> $S; exit 1; }
make install > install.log 2>&1
rc=$?; echo "INSTALL rc=$rc $(date)" >> $S
[ $rc = 0 ] || { echo FAILED >> $S; exit 1; }
echo DONE >> $S
