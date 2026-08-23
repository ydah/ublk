#include <stdio.h>
#include "../ext/ublk/compat.h"

#define DUMP(value) printf("%s=%llu\n", #value, (unsigned long long)(value))

int main(void)
{
  DUMP(UBLK_F_USER_COPY);
  DUMP(UBLK_F_CMD_IOCTL_ENCODE);
  DUMP(UBLK_F_USER_RECOVERY);
  DUMP(UBLK_MAX_QUEUE_DEPTH);
  DUMP(UBLKSRV_IO_BUF_OFFSET);
  DUMP(UBLK_IO_OP_READ);
  DUMP(UBLK_IO_OP_WRITE);
  DUMP(UBLK_IO_OP_FLUSH);
  DUMP(UBLK_IO_OP_DISCARD);
  DUMP(UBLK_IO_OP_WRITE_ZEROES);
  return 0;
}
