#!/bin/sh
set -eu

kernel=${1:-v6.12}
exec vng -r "$kernel" --user root -- tools/vm/guest-test.sh
