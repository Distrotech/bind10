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
    echo "Usage: common [-a] memsize logfile first-program [second-program]"
    exit 1;
fi

if [ $1 = "-a" ]; then
    async_flags=" --margin 4 --asynchronous"
    shift
else
    async_flags=""
fi

# set the remaining parameters

memsize=$1
logfile=$2
first=$3
if [ $# = 2 ]; then
    second=""
else
    second=$4
fi

for burst in 1 2 4 8 16 32 64 128 256
do
    echo "Setting burst to $burst"

    # Start the server programs.  First make sure that the message
    # queues has been deleted before any run starts.
    $progdir/queue_clear

    # Start the first of the server program(s) (the client or intermediary
    # if two programs are specified, or the server if just one is being used)
    # with the specified burst level.
    $progdir/$first --burst $burst &

    # If this is a chain of three programs, start the second program.  We want
    # this program to process packets as soon as they arrive.
    if [ "$second" != "" ]; then
        $progdir/$second --burst 1 &
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
    if [ "$second" != "" ]; then
        kill -9 %2
    fi;

    sleep 3
done
