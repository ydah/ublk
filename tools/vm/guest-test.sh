#!/bin/sh
set -eu

modprobe ublk_drv
export UBLK_SYSTEM_TEST=1
exec bundle exec rake test:system
