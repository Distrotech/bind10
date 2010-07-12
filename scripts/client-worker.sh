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

# \brief Run Series of Tests on Client-Receptionist-Worker Model
#
# Runs a series of tests with varying parameters on the client-receptionist-
# worker model.  The output is written to the file given as the only parameter.

progdir=`dirname $0`

if [ $# != 1 ]; then
    echo "Usage: client-worker logfile"
    exit 1;
fi

for burst in 1 2 4 8 16 32 64 96 128 160 192 224 256
do
    echo "Setting burst to $burst"
    $progdir/worker --burst $burst &
    $progdir/receptionist --burst $burst &
    sleep 3     # Allow worker to start

    for rpt in {1..64}
    do
        a="$progdir/client --count 4096 --burst $burst --pktsize 256 --logfile $1"
        echo $a
        $a
    done

    kill %?receptionist
    kill %?worker
    sleep 3
done
