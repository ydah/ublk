#include "internal.h"

#include <stdatomic.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/mman.h>

#define UBLK_STOP_DATA UINT64_MAX

typedef struct {
  struct io_uring ring;
  struct ublksrv_io_desc *descriptors;
  size_t map_size;
  int event_fd;
  int ready;
} ublk_queue;

typedef struct {
  int fd;
  pid_t pid;
  unsigned queues;
  unsigned depth;
  atomic_int closed;
  ublk_queue *queue;
} ublk_server;

static VALUE cServer;
static ID id_read, id_write, id_flush, id_discard, id_write_zeroes, id_push, id_errno;
static int submit_ring(struct io_uring *ring);

static void queue_close(ublk_queue *queue)
{
  if (!queue->ready) return;
  io_uring_queue_exit(&queue->ring);
  munmap(queue->descriptors, queue->map_size);
  close(queue->event_fd);
  memset(queue, 0, sizeof(*queue));
  queue->event_fd = -1;
}

static void server_release_resources(ublk_server *server)
{
  unsigned index;
  for (index = 0; index < server->queues; index++) queue_close(&server->queue[index]);
  if (server->fd >= 0) close(server->fd);
  server->fd = -1;
}

static void server_free(void *pointer)
{
  ublk_server *server = pointer;
  atomic_store(&server->closed, 1);
  server_release_resources(server);
  xfree(server->queue);
  xfree(server);
}

static size_t server_size(const void *pointer)
{
  const ublk_server *server = pointer;
  return sizeof(*server) + server->queues * sizeof(ublk_queue);
}

static const rb_data_type_t server_type = {
  "UBLK::Native::Server",
  {NULL, server_free, server_size, NULL, {NULL}},
  NULL, NULL, RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE server_alloc(VALUE klass)
{
  ublk_server *server;
  VALUE object = TypedData_Make_Struct(klass, ublk_server, &server_type, server);
  memset(server, 0, sizeof(*server));
  server->fd = -1;
  return object;
}

static VALUE server_initialize(VALUE self, VALUE id, VALUE queues, VALUE depth)
{
  ublk_server *server;
  char path[64];
  unsigned index;

  TypedData_Get_Struct(self, ublk_server, &server_type, server);
  server->queues = NUM2UINT(queues);
  server->depth = NUM2UINT(depth);
  if (server->queues == 0 || server->queues > UBLK_MAX_NR_QUEUES) rb_raise(rb_eArgError, "invalid queue count");
  if (server->depth == 0 || server->depth > UBLK_MAX_QUEUE_DEPTH) rb_raise(rb_eArgError, "invalid queue depth");

  snprintf(path, sizeof(path), "/dev/ublkc%u", NUM2UINT(id));
  server->fd = open(path, O_RDWR | O_CLOEXEC);
  if (server->fd < 0) rb_sys_fail(path);
  server->pid = getpid();
  server->queue = ALLOC_N(ublk_queue, server->queues);
  memset(server->queue, 0, server->queues * sizeof(ublk_queue));
  for (index = 0; index < server->queues; index++) server->queue[index].event_fd = -1;
  return self;
}

static ublk_server *get_server(VALUE self)
{
  ublk_server *server;
  TypedData_Get_Struct(self, ublk_server, &server_type, server);
  ublk_check_pid(server->pid);
  if (server->fd < 0) rb_raise(rb_eRuntimeError, "closed ublk server");
  return server;
}

static size_t round_up(size_t value, size_t alignment)
{
  return (value + alignment - 1) / alignment * alignment;
}

static void prep_io_command(ublk_server *server, ublk_queue *queue, unsigned qid,
                            unsigned tag, unsigned operation, int result)
{
  struct io_uring_sqe *sqe = io_uring_get_sqe(&queue->ring);
  struct ublksrv_io_cmd *command;
  if (!sqe) rb_raise(rb_eRuntimeError, "data io_uring is full");

  ublk_prep_cmd(sqe, server->fd, UBLK_RB_U_IO_CMD(operation));
  command = (struct ublksrv_io_cmd *)&sqe->addr3;
  command->q_id = qid;
  command->tag = tag;
  command->result = result;
  command->addr = 0;
  io_uring_sqe_set_data64(sqe, tag + 1);
}

static void queue_setup(ublk_server *server, unsigned qid)
{
  ublk_queue *queue = &server->queue[qid];
  struct io_uring_params params;
  struct io_uring_sqe *sqe;
  size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
  size_t max_map_size = round_up(UBLK_MAX_QUEUE_DEPTH * sizeof(struct ublksrv_io_desc), page_size);
  off_t offset = UBLKSRV_CMD_BUF_OFFSET + (off_t)qid * max_map_size;
  unsigned tag;
  int result;

  queue->map_size = round_up(server->depth * sizeof(struct ublksrv_io_desc), page_size);
  queue->descriptors = mmap(NULL, queue->map_size, PROT_READ, MAP_SHARED, server->fd, offset);
  if (queue->descriptors == MAP_FAILED) rb_sys_fail("mmap(ublk io descriptors)");

  memset(&params, 0, sizeof(params));
  params.flags = IORING_SETUP_SQE128 | IORING_SETUP_CQSIZE;
  params.cq_entries = server->depth + 1;
  result = io_uring_queue_init_params(server->depth + 1, &queue->ring, &params);
  if (result < 0) {
    munmap(queue->descriptors, queue->map_size);
    rb_syserr_fail(-result, "io_uring_queue_init_params(data)");
  }

  queue->event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (queue->event_fd < 0) {
    io_uring_queue_exit(&queue->ring);
    munmap(queue->descriptors, queue->map_size);
    rb_sys_fail("eventfd");
  }
  queue->ready = 1;

  for (tag = 0; tag < server->depth; tag++)
    prep_io_command(server, queue, qid, tag, UBLK_IO_FETCH_REQ, 0);
  sqe = io_uring_get_sqe(&queue->ring);
  if (!sqe) rb_raise(rb_eRuntimeError, "data io_uring has no stop slot");
  io_uring_prep_poll_add(sqe, queue->event_fd, POLLIN);
  io_uring_sqe_set_data64(sqe, UBLK_STOP_DATA);

  result = submit_ring(&queue->ring);
  if (result < 0) rb_syserr_fail(-result, "io_uring_submit(fetch)");
}

struct wait_args {
  struct io_uring *ring;
  struct io_uring_cqe *cqe;
  int result;
};

struct submit_args {
  struct io_uring *ring;
  int result;
};

static void *submit_without_gvl(void *pointer)
{
  struct submit_args *args = pointer;
  do {
    args->result = io_uring_submit(args->ring);
  } while (args->result == -EINTR);
  return NULL;
}

static int submit_ring(struct io_uring *ring)
{
  struct submit_args args = {ring, 0};
  rb_thread_call_without_gvl(submit_without_gvl, &args, RUBY_UBF_IO, NULL);
  return args.result;
}

static void *wait_without_gvl(void *pointer)
{
  struct wait_args *args = pointer;
  do {
    args->result = io_uring_wait_cqe(args->ring, &args->cqe);
  } while (args->result == -EINTR);
  return NULL;
}

struct rw_args {
  int fd;
  void *buffer;
  size_t length;
  off_t offset;
  int write;
  ssize_t result;
  int error;
};

static void *rw_without_gvl(void *pointer)
{
  struct rw_args *args = pointer;
  do {
    args->result = args->write
      ? pwrite(args->fd, args->buffer, args->length, args->offset)
      : pread(args->fd, args->buffer, args->length, args->offset);
  } while (args->result < 0 && errno == EINTR);
  args->error = args->result < 0 ? errno : 0;
  return NULL;
}

struct callback_args {
  VALUE target;
  ID method;
  VALUE first;
  VALUE second;
  int arguments;
};

static VALUE call_target(VALUE pointer)
{
  struct callback_args *args = (struct callback_args *)pointer;
  VALUE values[2] = {args->first, args->second};
  return rb_funcallv(args->target, args->method, args->arguments, values);
}

static VALUE normalize_integer(VALUE value)
{
  return INT2NUM(NUM2INT(value));
}

static int exception_result(void)
{
  VALUE exception = rb_errinfo();
  int result = -EIO;
  if (rb_obj_is_kind_of(exception, rb_eSystemCallError))
    result = -NUM2INT(rb_funcall(exception, id_errno, 0));
  rb_set_errinfo(Qnil);
  return result;
}

static off_t user_copy_offset(unsigned qid, unsigned tag)
{
  return UBLKSRV_IO_BUF_OFFSET + ((off_t)qid << UBLK_QID_OFF) + ((off_t)tag << UBLK_TAG_OFF);
}

static int process_request(ublk_server *server, unsigned qid, unsigned tag, VALUE target)
{
  const struct ublksrv_io_desc *descriptor = &server->queue[qid].descriptors[tag];
  unsigned operation = descriptor->op_flags & 0xff;
  size_t length = (size_t)descriptor->nr_sectors << 9;
  uint64_t offset = descriptor->start_sector << 9;
  struct callback_args callback = {target, 0, ULL2NUM(offset), Qnil, 0};
  struct rw_args transfer = {server->fd, NULL, length, user_copy_offset(qid, tag), 0, 0, 0};
  VALUE value;
  int state = 0;

  if (operation == UBLK_IO_OP_WRITE) {
    callback.method = id_write;
    callback.second = rb_str_new(NULL, length);
    callback.arguments = 2;
    transfer.buffer = RSTRING_PTR(callback.second);
    rb_thread_call_without_gvl(rw_without_gvl, &transfer, RUBY_UBF_IO, NULL);
    if (transfer.result != (ssize_t)length) return transfer.error ? -transfer.error : -EIO;
  } else if (operation == UBLK_IO_OP_READ) {
    callback.method = id_read;
    callback.second = SIZET2NUM(length);
    callback.arguments = 2;
  } else if (operation == UBLK_IO_OP_FLUSH) {
    callback.method = id_flush;
  } else if (operation == UBLK_IO_OP_DISCARD) {
    callback.method = id_discard;
    callback.second = SIZET2NUM(length);
    callback.arguments = 2;
  } else if (operation == UBLK_IO_OP_WRITE_ZEROES) {
    callback.method = id_write_zeroes;
    callback.second = SIZET2NUM(length);
    callback.arguments = 2;
  } else {
    return -EOPNOTSUPP;
  }

  value = rb_protect(call_target, (VALUE)&callback, &state);
  if (state) return exception_result();

  if (operation == UBLK_IO_OP_READ) {
    if (!RB_TYPE_P(value, T_STRING) || (size_t)RSTRING_LEN(value) != length) return -EIO;
    value = rb_str_new(RSTRING_PTR(value), RSTRING_LEN(value));
    transfer.buffer = RSTRING_PTR(value);
    transfer.write = 1;
    rb_thread_call_without_gvl(rw_without_gvl, &transfer, RUBY_UBF_IO, NULL);
    RB_GC_GUARD(value);
    return transfer.result == (ssize_t)length ? (int)length : (transfer.error ? -transfer.error : -EIO);
  }

  if (!RB_INTEGER_TYPE_P(value)) return -EIO;
  if (operation == UBLK_IO_OP_WRITE && !RTEST(rb_equal(value, SIZET2NUM(length)))) return -EIO;
  if (operation == UBLK_IO_OP_WRITE) return (int)length;
  value = rb_protect(normalize_integer, value, &state);
  if (state) return exception_result();
  return NUM2INT(value);
}

static VALUE server_run(VALUE self, VALUE queue_id, VALUE target, VALUE ready_queue)
{
  ublk_server *server = get_server(self);
  unsigned qid = NUM2UINT(queue_id);
  ublk_queue *queue;

  if (qid >= server->queues) rb_raise(rb_eArgError, "invalid queue id");
  queue = &server->queue[qid];
  if (queue->ready) rb_raise(rb_eRuntimeError, "queue is already running");
  queue_setup(server, qid);
  rb_funcall(ready_queue, id_push, 1, queue_id);

  while (!atomic_load(&server->closed)) {
    struct wait_args wait = {&queue->ring, NULL, 0};
    uint64_t data;
    unsigned tag;
    int result;

    rb_thread_call_without_gvl(wait_without_gvl, &wait, RUBY_UBF_IO, NULL);
    if (wait.result < 0) rb_syserr_fail(-wait.result, "io_uring_wait_cqe");
    data = io_uring_cqe_get_data64(wait.cqe);
    result = wait.cqe->res;
    io_uring_cqe_seen(&queue->ring, wait.cqe);
    if (data == UBLK_STOP_DATA || result == -ENODEV) break;
    if (result < 0) rb_syserr_fail(-result, "ublk fetch request");

    tag = (unsigned)data - 1;
    if (tag >= server->depth) rb_raise(rb_eRuntimeError, "kernel returned an invalid ublk tag");
    result = process_request(server, qid, tag, target);
    prep_io_command(server, queue, qid, tag, UBLK_IO_COMMIT_AND_FETCH_REQ, result);
    result = submit_ring(&queue->ring);
    if (result < 0) rb_syserr_fail(-result, "io_uring_submit(commit)");
  }
  return Qnil;
}

static VALUE server_close(VALUE self)
{
  ublk_server *server;
  unsigned index;
  uint64_t value = 1;
  TypedData_Get_Struct(self, ublk_server, &server_type, server);
  if (atomic_exchange(&server->closed, 1)) return Qnil;
  for (index = 0; index < server->queues; index++) {
    if (server->queue[index].event_fd >= 0 &&
        write(server->queue[index].event_fd, &value, sizeof(value)) < 0 && errno != EAGAIN)
      rb_sys_fail("eventfd write");
  }
  return Qnil;
}

static VALUE server_release(VALUE self)
{
  ublk_server *server;
  TypedData_Get_Struct(self, ublk_server, &server_type, server);
  ublk_check_pid(server->pid);
  if (!atomic_load(&server->closed)) rb_raise(rb_eRuntimeError, "close ublk server before releasing it");
  server_release_resources(server);
  return Qnil;
}

static VALUE native_lock_memory(VALUE self)
{
  (void)self;
#ifdef HAVE_MLOCKALL
  if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) rb_sys_fail("mlockall");
  return Qtrue;
#else
  rb_raise(rb_eNotImpError, "mlockall is unavailable");
#endif
}

void ublk_init_server(void)
{
  id_read = rb_intern("read");
  id_write = rb_intern("write");
  id_flush = rb_intern("flush");
  id_discard = rb_intern("discard");
  id_write_zeroes = rb_intern("write_zeroes");
  id_push = rb_intern("push");
  id_errno = rb_intern("errno");

  cServer = rb_define_class_under(ublk_native_module, "Server", rb_cObject);
  rb_define_alloc_func(cServer, server_alloc);
  rb_define_method(cServer, "initialize", server_initialize, 3);
  rb_define_method(cServer, "run", server_run, 3);
  rb_define_method(cServer, "close", server_close, 0);
  rb_define_method(cServer, "release", server_release, 0);
  rb_define_singleton_method(ublk_native_module, "lock_memory!", native_lock_memory, 0);
}
