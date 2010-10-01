#!/bin/bash
# Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
# OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

# $Id$

# This is an alternative way of building the performance testing software.  Use
# of "configure" is preferred, but this script allows a quick build without the
# need to set up the correct version of the autotools suite.  However, the
# script is likely to require editing if moved between platforms.

pushd `dirname $0`
export ROOT=`pwd`
popd
export COMMON=$ROOT/common

export BOOST=/opt/local         # Location of Boost
export BOOSTINC=$BOOST/include  # Include directory
export BOOSTLIB=$BOOST/lib      # Library directory
export BOOSTSUFFIX=-mt          # Suffix added on to the library names

# Build the main library.

build_library()
{
    echo "Building libcommon"
    pushd $COMMON
    for file in *.cc
    do
        if [ "$file" != "queue_clear.cc" ]
        then
            cmd="g++ -c -I$COMMON -I$BOOSTINC -O2 $file"
            echo $cmd
            $cmd
        fi
    done
    rm -f libcommon.a
    ar -crv libcommon.a *.o
    ranlib libcommon.a
    popd
}

# Build a program.  Assumed that all .cc files in the directory together
# with libcommon produce the program.

build_program()
{
    echo "Building $1"
    pushd $ROOT/$1
    cmd="g++ -o $1 -O2 -I$COMMON -I$BOOSTINC -L$BOOSTLIB -lboost_system$BOOSTSUFFIX -lboost_thread$BOOSTSUFFIX -lboost_program_options$BOOSTSUFFIX *.cc $COMMON/libcommon.a"
    echo $cmd
    $cmd
}

# Actually perform the build.

build_library
build_program client
build_program server
build_program receptionist
build_program worker
build_program intermediary
build_program contractor
