#include "ruby.h"

#ifdef __linux__
#include <linux/ublk_cmd.h>
#include <stddef.h>
#include <unistd.h>
#include "compat.h"
#endif

static VALUE mUBLK;
static VALUE mNative;

static VALUE native_supported(VALUE self)
{
  (void)self;
#ifdef __linux__
  return access("/dev/ublk-control", R_OK | W_OK) == 0 ? Qtrue : Qfalse;
#else
  return Qfalse;
#endif
}

static VALUE native_layout(VALUE self)
{
  VALUE result = rb_hash_new();
  (void)self;
#ifdef __linux__
  rb_hash_aset(result, ID2SYM(rb_intern("ctrl_cmd_size")), SIZET2NUM(sizeof(struct ublk_rb_ctrl_cmd)));
  rb_hash_aset(result, ID2SYM(rb_intern("dev_info_size")), SIZET2NUM(sizeof(struct ublk_rb_dev_info)));
  rb_hash_aset(result, ID2SYM(rb_intern("params_size")), SIZET2NUM(sizeof(struct ublk_rb_params)));
  rb_hash_aset(result, ID2SYM(rb_intern("io_desc_size")), SIZET2NUM(sizeof(struct ublksrv_io_desc)));
  rb_hash_aset(result, ID2SYM(rb_intern("io_desc_start_sector_offset")), SIZET2NUM(offsetof(struct ublksrv_io_desc, start_sector)));
#endif
  return result;
}

void Init_ublk(void)
{
  mUBLK = rb_define_module("UBLK");
  mNative = rb_define_module_under(mUBLK, "Native");
  rb_define_singleton_method(mNative, "supported?", native_supported, 0);
  rb_define_singleton_method(mNative, "layout", native_layout, 0);
}
