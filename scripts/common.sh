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

# \brief Run Series of Tests on Client-Intermediary-Contractor Model
#
# Runs a series of tests with varying parameters on the client-intermediary-
# contractor model.  The output is written to the file given as the only
# parameter.
#
# \param -a If present, run in asynchronous mode
# \param $1 Name of the logfile
# \param $2-$n Name of the programs to start/stop.  These are assumed to reside
# in the same directory as the script.

progdir=`dirname $0`

if [ $# -lt 2 -o $# -gt 4 ]; then
    echo "Usage: common [-a] logfile [middle-program] server-program"
    exit 1;
fi

if [ $1 = "-a" ]; then
    async_flags=" --margin 2 --asynchronous"
    shift
else
    async_flags=""
fi

# set the remaining parameters

logfile=$1;
if [ $# = 2 ]; then
    middle=""
    server=$2
else
    middle=$2
    server=$3
fi

for burst in 1 2 4 8 16 32 64 96 128 160 192 224 256
do
    echo "Setting burst to $burst"

    # Start the server programs.  First make sure that the message
    # queues has been deleted before any run starts.
    $progdir/queue_clear

    # If an intermediary/receptionist program was specified, start
    # it.
    if [ "$middle" != "" ]; then
        $progdir/$middle --burst $burst &
    fi;

    # Start the server.  If running asynchronously, we want the server
    # to process packets as soon as they arrive.
    if [ "$async_flags" = "" ]; then
        $progdir/$server --burst $burst &
    else
        $progdir/$server --burst 1 &
    fi

    # Allow server programs to start.
    sleep 3

    for rpt in {1..64}
    do
        cmd="$progdir/client --count 4096 --burst $burst --size 256 --logfile $logfile $async_flags"
        echo "$rpt) $cmd"
        $cmd
    done

    # Kill the server programs and wait to stop before doing the next loop.
    kill -9 %1
    if [ "$middle" != "" ]; then
        kill -9 %2
    fi;

    sleep 3
done
