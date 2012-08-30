#!/bin/sh

${PYTHON} "$1" 2>&1 | cut -d\  -f3- | diff -b - "$2" 1>&2
