# UBLK

Write Linux block-device targets in Ruby. UBLK uses a C extension, liburing,
and the kernel's `UBLK_F_USER_COPY` interface so target implementations only
exchange Ruby strings.

> [!WARNING]
> This is experimental. Never use it for a root filesystem, swap, or data you
> cannot recreate. A Ruby GC pause stops device I/O, and a crashed server makes
> outstanding I/O fail unless recovery was enabled.

## Requirements

- Linux 6.6 or newer with `CONFIG_BLK_DEV_UBLK`
- `/dev/ublk-control` and normally `CAP_SYS_ADMIN`
- Ruby 3.2 or newer
- liburing headers (`liburing-dev` on Debian/Ubuntu, `liburing-devel` on Fedora)

The gem deliberately fails to build without liburing. Once installed,
`require "ublk"` remains safe on unsupported hosts and `UBLK.supported?`
reports whether the running kernel exposes USER_COPY.

```sh
bundle install
bundle exec rake compile
```

## RAM disk

```ruby
require "ublk"

class RamDisk < UBLK::Target
  def initialize(size)
    @data = "\0".b * size
    super(size:)
  end

  def read(offset, length) = @data.byteslice(offset, length)

  def write(offset, data)
    @data[offset, data.bytesize] = data
    data.bytesize
  end
end

device = UBLK::Device.create(RamDisk.new(256 * 1024 * 1024))
puts device.path
device.run
```

`run` blocks until another thread calls `stop` or the process receives an
interrupt. For a background server, call `start`, use `device.path`, then call
`delete`. Created devices are also deleted by an `at_exit` hook.

Targets must implement `read` and `write`. `flush` succeeds by default;
`discard` and `write_zeroes` default to `EOPNOTSUPP`. Raising an `Errno::*`
exception returns that errno to the kernel; other exceptions become `EIO`.

`Device.create` locks current and future memory by default to avoid a paging
deadlock. Pass `mlock: false` only for disposable development devices. Multiple
hardware queues use one Ruby thread each, but callbacks remain serialized by
the GVL.

See `examples/` for RAM, file-backed, HTTP read-only, encrypted, tracing, and
fault-injection targets.

## Recovery

Create with `recovery: true`, then reconnect after a server restart:

```ruby
device = UBLK::Device.recover(target, id: 0)
device.run
```

Outstanding requests fail during this v1 recovery mode; they are not reissued.

## Unsupported environments

- macOS and Windows: no Linux ublk driver
- Docker Desktop: its VM normally does not expose `/dev/ublk-control`
- containers without the host ublk device and the required capability
- WSL2 kernels without `CONFIG_BLK_DEV_UBLK`
- Linux before 6.6: no required USER_COPY interface
- standard GitHub-hosted jobs: no usable privileged ublk device

## Tests

```sh
bundle exec rake compile
bundle exec rake test:unit
bundle exec rbs validate
```

The destructive system suite only runs when explicitly enabled inside a VM.
It covers 64 MiB raw reads, maximum-I/O boundary crossing, ext4 and fsck,
flush/discard callbacks, errno propagation, device deletion, SIGKILL recovery,
four-way verified fio, a one-minute RSS leak check, and 1,000 control opens.

```sh
tools/vm/run.sh v6.6
tools/vm/run.sh v6.12
```

Run 4K random-read, random-write, and sequential-read fio workloads in the
same VM:

```sh
UBLK_BENCH=1 bundle exec ruby tools/bench.rb
UBLK_BENCH=1 UBLK_BENCH_GC_STRESS=1 bundle exec ruby tools/bench.rb
UBLK_BENCH=1 UBLK_BENCH_DEVICE=/dev/ublkb0 bundle exec ruby tools/bench.rb
```

The last form measures an externally started device, such as `ublksrv`'s C
loop target, with the identical fio arguments. fio's JSON includes IOPS and
p99 latency. No hardware-independent number is claimed: CPU, Ruby, GC, and VM
acceleration materially change the result, and Ruby callbacks remain bounded
by the GVL.

## License

MIT
