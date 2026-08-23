# frozen_string_literal: true

require "mkmf"

abort "ublk requires Linux" unless RbConfig::CONFIG["host_os"].include?("linux")
abort "install liburing-dev (Debian/Ubuntu) or liburing-devel (Fedora)" unless have_header("liburing.h") && have_library("uring", "io_uring_queue_init_params")

have_header("linux/ublk_cmd.h")
have_header("sys/eventfd.h")
have_func("mlockall", "sys/mman.h")
have_const("UBLK_F_USER_COPY", "linux/ublk_cmd.h")
have_const("UBLK_F_CMD_IOCTL_ENCODE", "linux/ublk_cmd.h")

$CFLAGS << " -std=c11 -Wall -Wextra -Werror=implicit-function-declaration"
create_header
create_makefile("ublk/ublk")
