#include "internal.h"

#ifdef __linux__
#include <linux/ublk_cmd.h>
#include <stddef.h>
#include <unistd.h>
#include "compat.h"
#endif

VALUE ublk_module;
VALUE ublk_native_module;

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
  ublk_module = rb_define_module("UBLK");
  ublk_native_module = rb_define_module_under(ublk_module, "Native");
  rb_define_singleton_method(ublk_native_module, "supported?", ublk_native_supported, 0);
  rb_define_singleton_method(ublk_native_module, "layout", native_layout, 0);
  ublk_init_constants();
  ublk_init_control();
  ublk_init_server();
}
