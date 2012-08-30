#!/bin/sh

${PYTHON} "$1" 2>&1 | egrep -v '^\[[0-9]+ refs\]$' |\
    cut -d\  -f3- | diff -b - "$2" 1>&2
