<h1 align="center">UBLK</h1>

<p align="center">
  <strong>Write Linux block-device targets in Ruby</strong>
</p>

<p align="center">
  <a href="https://github.com/ydah/ublk/actions/workflows/main.yml"><img src="https://github.com/ydah/ublk/actions/workflows/main.yml/badge.svg" alt="Ruby CI"></a>
  <img src="https://img.shields.io/badge/ruby-%3E%3D%203.2-ruby.svg" alt="Ruby 3.2 or newer">
  <img src="https://img.shields.io/badge/linux-%3E%3D%206.6-FCC624.svg" alt="Linux 6.6 or newer">
  <a href="LICENSE.txt"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="MIT License"></a>
</p>

<p align="center">
  <a href="#features">Features</a> ·
  <a href="#installation">Installation</a> ·
  <a href="#quick-start">Quick Start</a> ·
  <a href="#target-api">Target API</a> ·
  <a href="#development">Development</a> ·
  <a href="#benchmark">Benchmark</a>
</p>

---

UBLK provides a Ruby API for implementing Linux userspace block devices. A C
extension, liburing, and the kernel's `UBLK_F_USER_COPY` interface handle the
data plane while target implementations exchange ordinary Ruby strings.

> [!WARNING]
> This project is experimental. Never use it for a root filesystem, swap, or
> data you cannot recreate. A Ruby GC pause stops device I/O, and a crashed
> server makes outstanding I/O fail unless recovery was enabled.

## Features

- Ruby `Target` API for reads, writes, flushes, discard, and write zeroes
- C and io_uring data plane with the GVL released while waiting for the kernel
- Multiple hardware queues with one Ruby worker thread per queue
- Kernel errno propagation from Ruby exceptions
- Optional memory locking and user recovery
- Automatic device and native-resource cleanup
- RAM, loop, HTTP, encrypted, tracing, and fault-injection examples
- Destructive system tests on Linux 6.6 and 6.12

## Installation

Install liburing headers first:

```sh
# Debian or Ubuntu
sudo apt-get install liburing-dev

# Fedora
sudo dnf install liburing-devel
```

Until the gem is published, add the GitHub repository to your `Gemfile`:

```ruby
gem "ublk", github: "ydah/ublk"
```

Then install:

```sh
bundle install
```

The native extension deliberately fails to build without liburing.

### Requirements

- Linux 6.6 or newer with `CONFIG_BLK_DEV_UBLK`
- `/dev/ublk-control` and normally `CAP_SYS_ADMIN`
- Ruby 3.2 or newer
- liburing headers

Once installed, `require "ublk"` remains safe on unsupported hosts.
`UBLK.supported?` reports whether the running kernel exposes USER_COPY.

## Quick Start

Create a 256 MiB RAM-backed block device:

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
interrupt. For a background server:

```ruby
device.start
puts device.path # /dev/ublkbN
# Use the block device from another process.
device.delete
```

Created devices are also deleted by an `at_exit` hook.

## Target API

Subclass `UBLK::Target`, pass the device size to `super`, and implement the
operations your target supports.

| Method | Required | Result |
|---|---:|---|
| `read(offset, length)` | yes | A binary string exactly `length` bytes long |
| `write(offset, data)` | writable targets | Number of bytes written |
| `flush` | no | `0`; succeeds by default |
| `discard(offset, length)` | no | `0`; defaults to `EOPNOTSUPP` |
| `write_zeroes(offset, length)` | no | `0`; defaults to `EOPNOTSUPP` |
| `read_only?` | no | `false` by default |
| `rotational?` | no | `false` by default |

Raising an `Errno::*` exception returns that errno to the kernel. Other
exceptions become `EIO`.

`Device.create` locks current and future memory by default to avoid a paging
deadlock. Pass `mlock: false` only for disposable development devices.
Multiple queues use separate Ruby threads, but callbacks remain serialized by
the GVL.

## Recovery

Create a recoverable device:

```ruby
device = UBLK::Device.create(target, recovery: true)
device.run
```

Reconnect it from a replacement server using the same device ID:

```ruby
device = UBLK::Device.recover(target, id: 0)
device.run
```

Outstanding requests fail during this v1 recovery mode; they are not reissued.

## Examples

| Example | Description |
|---|---|
| [`ramdisk.rb`](examples/ramdisk.rb) | In-memory read/write disk |
| [`loop.rb`](examples/loop.rb) | File-backed disk |
| [`http_disk.rb`](examples/http_disk.rb) | Read-only HTTP Range disk |
| [`encrypted.rb`](examples/encrypted.rb) | AES-256-CTR encrypted backing file |
| [`tracing.rb`](examples/tracing.rb) | Logs I/O before forwarding it |
| [`fault_injection.rb`](examples/fault_injection.rb) | Returns `EIO` for a selected range |

## How It Works

1. `UBLK::Control` creates and configures a kernel ublk device.
2. Each hardware queue starts an io_uring request loop in the C extension.
3. The extension releases the GVL while waiting for kernel requests.
4. USER_COPY transfers request data through `pread` and `pwrite`.
5. The matching Ruby target method handles the request and returns its result.

Zero-copy and zoned-device support are intentionally outside the v1 scope.

## Unsupported Environments

- macOS and Windows: no Linux ublk driver
- Docker Desktop: its VM normally does not expose `/dev/ublk-control`
- Containers without the host ublk device and required capability
- WSL2 kernels without `CONFIG_BLK_DEV_UBLK`
- Linux before 6.6: no required USER_COPY interface
- Standard GitHub-hosted jobs: no usable privileged ublk device

## Development

Run the portable checks:

```sh
bundle install
bundle exec rake compile
bundle exec rake test:unit
bundle exec rbs validate
```

The destructive system suite covers raw and boundary I/O, ext4 and fsck,
flush and discard, errno propagation, deletion, SIGKILL recovery, verified
four-way fio, a one-minute leak check, and 1,000 control opens.

```sh
tools/vm/run.sh v6.6
tools/vm/run.sh v6.12
```

## Benchmark

Run 4 KiB random-read, random-write, and sequential-read fio workloads in the
same VM:

```sh
UBLK_BENCH=1 bundle exec ruby tools/bench.rb
UBLK_BENCH=1 UBLK_BENCH_GC_STRESS=1 bundle exec ruby tools/bench.rb
UBLK_BENCH=1 UBLK_BENCH_DEVICE=/dev/ublkb0 bundle exec ruby tools/bench.rb
```

The last form measures an externally started device, such as the C ublksrv
loop target, with identical fio arguments. fio's JSON includes IOPS and p99
latency.

### Reference Results

This comparison ran on Linux 6.12.0 under QEMU TCG with 2 vCPUs and 1.5 GiB
RAM, using Ruby 3.2, fio 3.36, and upstream ublksrv 1.6.1 (`abbfea2b5918`).
Both loop targets used a 256 MiB sparse file on `/tmp`, one queue of depth 128,
and buffered backing-file I/O. fio used its io_uring engine, 4 KiB direct I/O,
iodepth 32, and one 30-second run per workload.

| Target | Workload | IOPS | Completion p99 |
|---|---:|---:|---:|
| Ruby loop | Random read | 33,774 | 2.900 ms |
| Ruby loop | Random write | 33,896 | 2.376 ms |
| Ruby loop | Sequential read | 26,801 | 3.850 ms |
| C ublksrv loop | Random read | 64,213 | 0.938 ms |
| C ublksrv loop | Random write | 56,456 | 1.090 ms |
| C ublksrv loop | Sequential read | 62,011 | 0.987 ms |
| Ruby loop, `GC.stress` | Random read | 9.72 | 6.610 s |
| Ruby loop, `GC.stress` | Random write | 12.61 | 4.178 s |
| Ruby loop, `GC.stress` | Sequential read | 7.42 | 7.952 s |

These are diagnostic VM results, not production performance claims. In this
run the C loop target delivered roughly 1.7–2.3 times the IOPS of normal Ruby,
while `GC.stress` made latency unacceptable for block storage.

## Contributing

Bug reports and pull requests are welcome at
[github.com/ydah/ublk](https://github.com/ydah/ublk).

## License

Released under the [MIT License](LICENSE.txt).
