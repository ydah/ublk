#ifndef UBLK_COMPAT_H
#define UBLK_COMPAT_H

#include <linux/ioctl.h>
#include <linux/types.h>
#include <stddef.h>

#if !defined(UBLK_FORCE_COMPAT) && defined(__has_include)
#if __has_include(<linux/ublk_cmd.h>)
#include <linux/ublk_cmd.h>
#define UBLK_HAVE_UAPI_HEADER 1
#endif
#endif

#ifndef UBLK_HAVE_UAPI_HEADER
#define UBLK_CMD_GET_QUEUE_AFFINITY 0x01
#define UBLK_CMD_GET_DEV_INFO 0x02
#define UBLK_CMD_ADD_DEV 0x04
#define UBLK_CMD_DEL_DEV 0x05
#define UBLK_CMD_START_DEV 0x06
#define UBLK_CMD_STOP_DEV 0x07
#define UBLK_CMD_SET_PARAMS 0x08
#define UBLK_CMD_GET_PARAMS 0x09
#define UBLK_CMD_START_USER_RECOVERY 0x10
#define UBLK_CMD_END_USER_RECOVERY 0x11
#define UBLK_CMD_GET_DEV_INFO2 0x12

#define UBLK_IO_FETCH_REQ 0x20
#define UBLK_IO_COMMIT_AND_FETCH_REQ 0x21
#define UBLK_IO_NEED_GET_DATA 0x22

#define UBLK_F_USER_RECOVERY (1ULL << 3)

#define UBLK_IO_OP_READ 0
#define UBLK_IO_OP_WRITE 1
#define UBLK_IO_OP_FLUSH 2
#define UBLK_IO_OP_DISCARD 3
#define UBLK_IO_OP_WRITE_SAME 4
#define UBLK_IO_OP_WRITE_ZEROES 5

#define UBLK_ATTR_READ_ONLY (1U << 0)
#define UBLK_ATTR_ROTATIONAL (1U << 1)
#define UBLK_ATTR_VOLATILE_CACHE (1U << 2)
#define UBLK_ATTR_FUA (1U << 3)
#define UBLK_PARAM_TYPE_BASIC (1U << 0)
#define UBLK_PARAM_TYPE_DISCARD (1U << 1)

struct ublksrv_io_desc {
  __u32 op_flags;
  __u32 nr_sectors;
  __u64 start_sector;
  __u64 addr;
};

struct ublksrv_io_cmd {
  __u16 q_id;
  __u16 tag;
  __s32 result;
  __u64 addr;
};

struct ublk_param_basic {
  __u32 attrs;
  __u8 logical_bs_shift;
  __u8 physical_bs_shift;
  __u8 io_opt_shift;
  __u8 io_min_shift;
  __u32 max_sectors;
  __u32 chunk_sectors;
  __u64 dev_sectors;
  __u64 virt_boundary_mask;
};

struct ublk_param_discard {
  __u32 discard_alignment;
  __u32 discard_granularity;
  __u32 max_discard_sectors;
  __u32 max_write_zeroes_sectors;
  __u16 max_discard_segments;
  __u16 reserved0;
};
#endif

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

#define UBLK_RB_U_READ_CMD(op) _IOR('u', (op), struct ublk_rb_ctrl_cmd)
#define UBLK_RB_U_RDWR_CMD(op) _IOWR('u', (op), struct ublk_rb_ctrl_cmd)
#define UBLK_RB_U_IO_CMD(op) _IOWR('u', (op), struct ublksrv_io_cmd)

_Static_assert(sizeof(struct ublk_rb_ctrl_cmd) == 32, "unexpected ublk control command layout");
_Static_assert(sizeof(struct ublk_rb_dev_info) == 64, "unexpected ublk device info layout");
_Static_assert(sizeof(struct ublk_rb_params) == 64, "unexpected ublk parameter layout");
_Static_assert(sizeof(struct ublksrv_io_desc) == 24, "unexpected ublk I/O descriptor layout");
_Static_assert(sizeof(struct ublksrv_io_cmd) == 16, "unexpected ublk I/O command layout");

#ifdef UBLK_U_CMD_GET_FEATURES
_Static_assert(UBLK_RB_U_READ_CMD(UBLK_CMD_GET_FEATURES) == UBLK_U_CMD_GET_FEATURES,
               "GET_FEATURES command encoding mismatch");
#endif
#ifdef UBLK_U_CMD_GET_DEV_INFO
_Static_assert(UBLK_RB_U_READ_CMD(UBLK_CMD_GET_DEV_INFO) == UBLK_U_CMD_GET_DEV_INFO,
               "GET_DEV_INFO command encoding mismatch");
#endif
#ifdef UBLK_U_CMD_ADD_DEV
_Static_assert(UBLK_RB_U_RDWR_CMD(UBLK_CMD_ADD_DEV) == UBLK_U_CMD_ADD_DEV,
               "ADD_DEV command encoding mismatch");
#endif

#ifdef UBLK_HAVE_UAPI_HEADER
_Static_assert(sizeof(struct ublk_rb_ctrl_cmd) == sizeof(struct ublksrv_ctrl_cmd),
               "control command size differs from Linux UAPI");
_Static_assert(offsetof(struct ublk_rb_ctrl_cmd, addr) == offsetof(struct ublksrv_ctrl_cmd, addr),
               "control command address offset differs from Linux UAPI");
_Static_assert(sizeof(struct ublk_rb_dev_info) == sizeof(struct ublksrv_ctrl_dev_info),
               "device info size differs from Linux UAPI");
_Static_assert(offsetof(struct ublk_rb_dev_info, flags) == offsetof(struct ublksrv_ctrl_dev_info, flags),
               "device info flags offset differs from Linux UAPI");
_Static_assert(offsetof(struct ublk_rb_params, basic) == offsetof(struct ublk_params, basic),
               "basic parameter offset differs from Linux UAPI");
_Static_assert(offsetof(struct ublk_rb_params, discard) == offsetof(struct ublk_params, discard),
               "discard parameter offset differs from Linux UAPI");
#endif

#endif
