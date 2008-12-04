#!/bin/sh

set -e
rm -rf autom4te.cache
autoconf
./configure "$@"
