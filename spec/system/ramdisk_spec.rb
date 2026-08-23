# frozen_string_literal: true

require "fileutils"
require "open3"
require "tempfile"
require "timeout"
require "tmpdir"
require_relative "../../examples/fault_injection"
require_relative "../../examples/loop"

class ObservedRamDisk < RamDisk
  attr_reader :flushes, :discards

  def initialize(size)
    @flushes = @discards = 0
    super
  end

  def flush
    @flushes += 1
    super
  end

  def discard(offset, length)
    @discards += 1
    super
  end
end

RSpec.describe "UBLK system behavior" do
  before do
    skip "set UBLK_SYSTEM_TEST=1 inside a privileged Linux VM" unless ENV["UBLK_SYSTEM_TEST"] == "1"
    skip "ublk USER_COPY is unavailable" unless UBLK.supported?
    skip "system test requires root" unless Process.uid.zero?
  end

  after { UBLK::Device.delete_all! if ENV["UBLK_SYSTEM_TEST"] == "1" && UBLK.supported? }

  def run!(*command)
    output, error, status = Open3.capture3(*command)
    raise "#{command.join(" ")} failed:\n#{output}#{error}" unless status.success?

    output
  end

  def wait_until(timeout: 10)
    Timeout.timeout(timeout) do
      loop do
        return if yield

        sleep 0.05
      end
    end
  end

  it "handles raw, boundary, filesystem, flush, discard, and deletion lifecycles" do
    Timeout.timeout(180) do
      target = ObservedRamDisk.new(96 * 1024 * 1024)
      device = UBLK::Device.create(target, depth: 32, max_io_bytes: 512 * 1024, mlock: false)
      mountpoint = Dir.mktmpdir("ublk-mount")
      path = device.path
      device.start

      expect(File.blockdev?(path)).to be(true)
      run!("dd", "if=#{path}", "of=/dev/null", "bs=1M", "count=64", "iflag=direct")

      payload = Random.bytes(1024 * 1024)
      offset = 512 * 1024 - 4096
      File.open(path, "r+b") { |disk| disk.pwrite(payload, offset) }
      run!("blockdev", "--flushbufs", path)
      expect(File.open(path, "rb") { |disk| disk.pread(payload.bytesize, offset) }).to eq(payload)

      run!("blkdiscard", "--offset", (8 * 1024 * 1024).to_s, "--length", (1024 * 1024).to_s, path)
      wait_until { target.flushes.positive? && target.discards.positive? }

      run!("mkfs.ext4", "-F", path)
      run!("mount", path, mountpoint)
      File.binwrite(File.join(mountpoint, "proof"), payload)
      expect(File.binread(File.join(mountpoint, "proof"))).to eq(payload)
      run!("umount", mountpoint)
      run!("fsck.ext4", "-fn", path)

      device.delete
      wait_until { !File.exist?(path) }
    ensure
      system("umount", mountpoint, out: File::NULL, err: File::NULL) if mountpoint && File.directory?(mountpoint)
      FileUtils.remove_entry(mountpoint) if mountpoint && File.exist?(mountpoint)
      device&.delete
    end
  end

  it "turns target EIO exceptions into failed block I/O" do
    device = UBLK::Device.create(FaultInjectionDisk.new(8 * 1024 * 1024, 0...4096), depth: 8, mlock: false)
    device.start

    _output, error, status = Open3.capture3("dd", "if=#{device.path}", "of=/dev/null", "bs=4096", "count=1", "iflag=direct")
    expect(status).not_to be_success
    expect(error).to match(/error/i)
  ensure
    device&.delete
  end

  it "recovers a file-backed device after its server is killed" do
    image = Tempfile.new("ublk-recovery")
    image.truncate(64 * 1024 * 1024)
    reader, writer = IO.pipe
    pid = fork do
      reader.close
      device = UBLK::Device.create(LoopTarget.new(image.path), depth: 8, recovery: true, mlock: false)
      device.start
      writer.puts(device.id)
      writer.close
      sleep
    end
    writer.close
    id = Integer(Timeout.timeout(30) { reader.gets }, 10)
    Process.kill("KILL", pid)
    Process.wait(pid)

    wait_until { UBLK::Device.list.any? { |info| info.id == id } }
    recovered = UBLK::Device.recover(LoopTarget.new(image.path), id:, mlock: false)
    recovered.start
    payload = Random.bytes(4096)
    File.open(recovered.path, "r+b") { |disk| disk.pwrite(payload, 4096) }
    expect(File.open(recovered.path, "rb") { |disk| disk.pread(4096, 4096) }).to eq(payload)
  ensure
    reader&.close unless reader&.closed?
    writer&.close unless writer&.closed?
    if pid
      begin
        unless Process.waitpid(pid, Process::WNOHANG)
          Process.kill("KILL", pid)
          Process.wait(pid)
        end
      rescue Errno::ECHILD, Errno::ESRCH
        nil
      end
    end
    recovered&.delete
    image&.close!
  end

  it "survives four-way verified I/O and a one-minute leak check" do
    device = UBLK::Device.create(RamDisk.new(256 * 1024 * 1024), queues: 4, depth: 32, mlock: false)
    device.start
    common = ["--filename=#{device.path}", "--direct=1", "--bs=4k", "--iodepth=16", "--numjobs=4", "--group_reporting=1"]
    run!("fio", "--name=verify", "--rw=randwrite", "--size=16m", "--offset_increment=16m",
         "--verify=crc32c", "--do_verify=1", "--verify_fatal=1", *common)

    GC.start
    before = File.read("/proc/self/status")[/^VmRSS:\s+(\d+)/, 1].to_i
    run!("fio", "--name=leak", "--rw=randrw", "--runtime=60", "--time_based=1", *common)
    GC.start
    after = File.read("/proc/self/status")[/^VmRSS:\s+(\d+)/, 1].to_i
    expect(after - before).to be < 32 * 1024
  ensure
    device&.delete
  end

  it "does not leak control file descriptors" do
    before = Dir.children("/proc/self/fd").size
    1_000.times { UBLK::Control.new.close }
    GC.start
    expect(Dir.children("/proc/self/fd").size).to be <= before + 2
  end
end
