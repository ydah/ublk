#ifndef UBLK_COMPAT_H
#define UBLK_COMPAT_H

#include <linux/ioctl.h>
#include <linux/types.h>

#ifndef UBLK_F_CMD_IOCTL_ENCODE
#define UBLK_F_CMD_IOCTL_ENCODE (1ULL << 6)
#endif
#ifndef UBLK_F_USER_COPY
#define UBLK_F_USER_COPY (1ULL << 7)
#endif
#ifndef UBLK_MAX_QUEUE_DEPTH
#define UBLK_MAX_QUEUE_DEPTH 4096
#endif
#ifndef UBLK_MAX_NR_QUEUES
#define UBLK_MAX_NR_QUEUES 4096
#endif
#ifndef UBLKSRV_CMD_BUF_OFFSET
#define UBLKSRV_CMD_BUF_OFFSET 0
#endif
#ifndef UBLKSRV_IO_BUF_OFFSET
#define UBLKSRV_IO_BUF_OFFSET 0x80000000ULL
#endif
#ifndef UBLK_CMD_GET_FEATURES
#define UBLK_CMD_GET_FEATURES 0x13
#endif
#ifndef UBLK_IO_BUF_BITS
#define UBLK_IO_BUF_BITS 25
#endif
#ifndef UBLK_TAG_OFF
#define UBLK_TAG_OFF UBLK_IO_BUF_BITS
#endif
#ifndef UBLK_QID_OFF
#define UBLK_QID_OFF (UBLK_TAG_OFF + 16)
#endif

struct ublk_rb_ctrl_cmd {
  __u32 dev_id;
  __u16 queue_id;
  __u16 len;
  __u64 addr;
  __u64 data[1];
  __u16 dev_path_len;
  __u16 pad;
  __u32 reserved;
};

struct ublk_rb_dev_info {
  __u16 nr_hw_queues;
  __u16 queue_depth;
  __u16 state;
  __u16 pad0;
  __u32 max_io_buf_bytes;
  __u32 dev_id;
  __s32 ublksrv_pid;
  __u32 pad1;
  __u64 flags;
  __u64 ublksrv_flags;
  __u32 owner_uid;
  __u32 owner_gid;
  __u64 reserved1;
  __u64 reserved2;
};

struct ublk_rb_params {
  __u32 len;
  __u32 types;
  struct ublk_param_basic basic;
  struct ublk_param_discard discard;
};

#define UBLK_RB_U_CMD(op) _IOWR('u', (op), struct ublk_rb_ctrl_cmd)
#define UBLK_RB_U_IO_CMD(op) _IOWR('u', (op), struct ublksrv_io_cmd)

#endif
