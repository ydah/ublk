#ifndef UBLK_INTERNAL_H
#define UBLK_INTERNAL_H

#include "ruby.h"
#include "ruby/thread.h"
#include "extconf.h"

#include <errno.h>
#include <fcntl.h>
#include <liburing.h>
#include <linux/ublk_cmd.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "compat.h"

extern VALUE ublk_module;
extern VALUE ublk_native_module;

void ublk_init_control(void);
void ublk_init_server(void);
void ublk_init_constants(void);
VALUE ublk_native_supported(VALUE self);

static inline void ublk_check_pid(pid_t owner)
{
  if (owner != getpid()) rb_raise(rb_eRuntimeError, "ublk handle cannot be used after fork");
}

static inline void ublk_raise_result(int result, const char *operation)
{
  if (result < 0) rb_syserr_fail(-result, operation);
}

static inline void ublk_prep_cmd(struct io_uring_sqe *sqe, int fd, unsigned op)
{
  memset(sqe, 0, sizeof(*sqe) * 2);
  sqe->opcode = IORING_OP_URING_CMD;
  sqe->fd = fd;
  sqe->cmd_op = op;
}

#endif
