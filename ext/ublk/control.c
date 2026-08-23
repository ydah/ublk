#include "internal.h"

#include <limits.h>

typedef struct {
  int fd;
  pid_t pid;
  int ring_ready;
  struct io_uring ring;
} ublk_control;

static VALUE cControl;

static void control_free(void *pointer)
{
  ublk_control *control = pointer;
  if (control->ring_ready) io_uring_queue_exit(&control->ring);
  if (control->fd >= 0) close(control->fd);
  xfree(control);
}

static size_t control_size(const void *pointer)
{
  (void)pointer;
  return sizeof(ublk_control);
}

static const rb_data_type_t control_type = {
  "UBLK::Native::Control",
  {NULL, control_free, control_size, NULL},
  NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE control_alloc(VALUE klass)
{
  ublk_control *control;
  VALUE object = TypedData_Make_Struct(klass, ublk_control, &control_type, control);
  memset(control, 0, sizeof(*control));
  control->fd = -1;
  return object;
}

static VALUE control_initialize(VALUE self)
{
  ublk_control *control;
  struct io_uring_params params;
  int result;

  TypedData_Get_Struct(self, ublk_control, &control_type, control);
  control->fd = open("/dev/ublk-control", O_RDWR | O_CLOEXEC);
  if (control->fd < 0) rb_sys_fail("open(/dev/ublk-control)");

  memset(&params, 0, sizeof(params));
  params.flags = IORING_SETUP_SQE128;
  result = io_uring_queue_init_params(8, &control->ring, &params);
  if (result < 0) {
    close(control->fd);
    control->fd = -1;
    rb_syserr_fail(-result, "io_uring_queue_init_params");
  }

  control->ring_ready = 1;
  control->pid = getpid();
  return self;
}

struct control_submit_args {
  struct io_uring *ring;
  int result;
};

static void *control_submit_without_gvl(void *pointer)
{
  struct control_submit_args *args = pointer;
  struct io_uring_cqe *cqe;

  do {
    args->result = io_uring_submit_and_wait(args->ring, 1);
  } while (args->result == -EINTR);
  if (args->result < 0) return NULL;

  args->result = io_uring_peek_cqe(args->ring, &cqe);
  if (args->result == 0) {
    args->result = cqe->res;
    io_uring_cqe_seen(args->ring, cqe);
  }
  return NULL;
}

static int control_submit(ublk_control *control, unsigned op, uint32_t id,
                          void *buffer, uint16_t length, uint64_t data)
{
  struct io_uring_sqe *sqe = io_uring_get_sqe(&control->ring);
  struct ublk_rb_ctrl_cmd *command;
  struct control_submit_args args = {&control->ring, 0};

  if (!sqe) rb_raise(rb_eRuntimeError, "control io_uring is full");
  ublk_prep_cmd(sqe, control->fd, op);
  command = (struct ublk_rb_ctrl_cmd *)&sqe->addr3;
  command->dev_id = id;
  command->queue_id = UINT16_MAX;
  command->addr = (uint64_t)(uintptr_t)buffer;
  command->len = length;
  command->data[0] = data;
  io_uring_sqe_set_data64(sqe, 1);

  rb_thread_call_without_gvl(control_submit_without_gvl, &args, RUBY_UBF_IO, NULL);
  return args.result;
}

VALUE ublk_native_supported(VALUE self)
{
  ublk_control control;
  struct io_uring_params params;
  uint64_t features = 0;
  int result;
  (void)self;

  memset(&control, 0, sizeof(control));
  control.fd = open("/dev/ublk-control", O_RDWR | O_CLOEXEC);
  if (control.fd < 0) return Qfalse;
  memset(&params, 0, sizeof(params));
  params.flags = IORING_SETUP_SQE128;
  result = io_uring_queue_init_params(2, &control.ring, &params);
  if (result < 0) {
    close(control.fd);
    return Qfalse;
  }

  result = control_submit(&control, UBLK_RB_U_CMD(UBLK_CMD_GET_FEATURES), 0,
                          &features, sizeof(features), 0);
  io_uring_queue_exit(&control.ring);
  close(control.fd);
  return result >= 0 && (features & UBLK_F_USER_COPY) ? Qtrue : Qfalse;
}

static ublk_control *get_control(VALUE self)
{
  ublk_control *control;
  TypedData_Get_Struct(self, ublk_control, &control_type, control);
  ublk_check_pid(control->pid);
  if (control->fd < 0) rb_raise(rb_eRuntimeError, "closed ublk control");
  return control;
}

static VALUE control_close(VALUE self)
{
  ublk_control *control;
  TypedData_Get_Struct(self, ublk_control, &control_type, control);
  ublk_check_pid(control->pid);
  if (control->ring_ready) {
    io_uring_queue_exit(&control->ring);
    control->ring_ready = 0;
  }
  if (control->fd >= 0) {
    close(control->fd);
    control->fd = -1;
  }
  return Qnil;
}

static VALUE info_to_array(const struct ublk_rb_dev_info *info)
{
  return rb_ary_new_from_args(9,
    UINT2NUM(info->dev_id), UINT2NUM(info->nr_hw_queues), UINT2NUM(info->queue_depth),
    UINT2NUM(info->state), UINT2NUM(info->max_io_buf_bytes), INT2NUM(info->ublksrv_pid),
    ULL2NUM(info->flags), UINT2NUM(info->owner_uid), UINT2NUM(info->owner_gid));
}

static VALUE control_add_dev(VALUE self, VALUE id, VALUE queues, VALUE depth,
                             VALUE max_io_bytes, VALUE recovery)
{
  ublk_control *control = get_control(self);
  struct ublk_rb_dev_info info;
  int result;

  memset(&info, 0, sizeof(info));
  info.dev_id = NIL_P(id) ? UINT32_MAX : NUM2UINT(id);
  info.nr_hw_queues = NUM2UINT(queues);
  info.queue_depth = NUM2UINT(depth);
  info.max_io_buf_bytes = NUM2UINT(max_io_bytes);
  info.flags = UBLK_F_USER_COPY | UBLK_F_CMD_IOCTL_ENCODE;
  if (RTEST(recovery)) info.flags |= UBLK_F_USER_RECOVERY;

  result = control_submit(control, UBLK_RB_U_CMD(UBLK_CMD_ADD_DEV), info.dev_id,
                          &info, sizeof(info), 0);
  ublk_raise_result(result, "UBLK_U_CMD_ADD_DEV");
  return info_to_array(&info);
}

static unsigned block_shift(VALUE value)
{
  unsigned size = NUM2UINT(value);
  unsigned shift = 0;
  while ((1U << shift) < size) shift++;
  return shift;
}

static VALUE control_set_params(VALUE self, VALUE id, VALUE size,
                                VALUE logical, VALUE physical, VALUE max_io,
                                VALUE read_only, VALUE rotational, VALUE discard)
{
  ublk_control *control = get_control(self);
  struct ublk_rb_params params;
  int result;

  memset(&params, 0, sizeof(params));
  params.len = sizeof(params);
  params.types = UBLK_PARAM_TYPE_BASIC;
  params.basic.logical_bs_shift = block_shift(logical);
  params.basic.physical_bs_shift = block_shift(physical);
  params.basic.io_min_shift = params.basic.logical_bs_shift;
  params.basic.max_sectors = NUM2ULL(max_io) >> 9;
  params.basic.dev_sectors = NUM2ULL(size) >> 9;
  if (RTEST(read_only)) params.basic.attrs |= UBLK_ATTR_READ_ONLY;
  if (RTEST(rotational)) params.basic.attrs |= UBLK_ATTR_ROTATIONAL;
  if (RTEST(discard)) {
    params.types |= UBLK_PARAM_TYPE_DISCARD;
    params.discard.discard_granularity = NUM2UINT(logical);
    params.discard.max_discard_sectors = params.basic.max_sectors;
    params.discard.max_write_zeroes_sectors = params.basic.max_sectors;
    params.discard.max_discard_segments = 1;
  }

  result = control_submit(control, UBLK_RB_U_CMD(UBLK_CMD_SET_PARAMS), NUM2UINT(id),
                          &params, sizeof(params), 0);
  ublk_raise_result(result, "UBLK_U_CMD_SET_PARAMS");
  return Qtrue;
}

static VALUE control_simple(VALUE self, VALUE id, unsigned op, const char *name, uint64_t data)
{
  int result = control_submit(get_control(self), UBLK_RB_U_CMD(op), NUM2UINT(id), NULL, 0, data);
  ublk_raise_result(result, name);
  return Qtrue;
}

static VALUE control_start_dev(VALUE self, VALUE id, VALUE pid)
{
  return control_simple(self, id, UBLK_CMD_START_DEV, "UBLK_U_CMD_START_DEV", NUM2UINT(pid));
}

static VALUE control_stop_dev(VALUE self, VALUE id)
{
  return control_simple(self, id, UBLK_CMD_STOP_DEV, "UBLK_U_CMD_STOP_DEV", 0);
}

static VALUE control_del_dev(VALUE self, VALUE id)
{
  return control_simple(self, id, UBLK_CMD_DEL_DEV, "UBLK_U_CMD_DEL_DEV", 0);
}

static VALUE control_start_recovery(VALUE self, VALUE id)
{
  return control_simple(self, id, UBLK_CMD_START_USER_RECOVERY, "UBLK_U_CMD_START_USER_RECOVERY", 0);
}

static VALUE control_end_recovery(VALUE self, VALUE id, VALUE pid)
{
  return control_simple(self, id, UBLK_CMD_END_USER_RECOVERY, "UBLK_U_CMD_END_USER_RECOVERY", NUM2UINT(pid));
}

static VALUE control_get_info(VALUE self, VALUE id)
{
  ublk_control *control = get_control(self);
  struct ublk_rb_dev_info info;
  int result;

  memset(&info, 0, sizeof(info));
  info.dev_id = NUM2UINT(id);
  result = control_submit(control, UBLK_RB_U_CMD(UBLK_CMD_GET_DEV_INFO), info.dev_id,
                          &info, sizeof(info), 0);
  ublk_raise_result(result, "UBLK_U_CMD_GET_DEV_INFO");
  return info_to_array(&info);
}

void ublk_init_control(void)
{
  cControl = rb_define_class_under(ublk_native_module, "Control", rb_cObject);
  rb_define_alloc_func(cControl, control_alloc);
  rb_define_method(cControl, "initialize", control_initialize, 0);
  rb_define_method(cControl, "close", control_close, 0);
  rb_define_method(cControl, "add_dev", control_add_dev, 5);
  rb_define_method(cControl, "set_params", control_set_params, 8);
  rb_define_method(cControl, "start_dev", control_start_dev, 2);
  rb_define_method(cControl, "stop_dev", control_stop_dev, 1);
  rb_define_method(cControl, "del_dev", control_del_dev, 1);
  rb_define_method(cControl, "start_user_recovery", control_start_recovery, 1);
  rb_define_method(cControl, "end_user_recovery", control_end_recovery, 2);
  rb_define_method(cControl, "get_dev_info", control_get_info, 1);
}
