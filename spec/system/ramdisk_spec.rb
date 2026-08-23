# frozen_string_literal: true

require "fileutils"
require "timeout"
require "tmpdir"
require_relative "../../examples/ramdisk"

RSpec.describe "UBLK RAM disk" do
  before do
    skip "set UBLK_SYSTEM_TEST=1 inside a privileged Linux VM" unless ENV["UBLK_SYSTEM_TEST"] == "1"
    skip "ublk USER_COPY is unavailable" unless UBLK.supported?
    skip "system test requires root" unless Process.uid.zero?
  end

  it "survives raw I/O and an ext4 lifecycle" do
    Timeout.timeout(90) do
      device = UBLK::Device.create(RamDisk.new(64 * 1024 * 1024), depth: 32, mlock: false)
      mountpoint = Dir.mktmpdir("ublk-mount")
      device.start

      payload = Random.bytes(4096)
      File.open(device.path, "r+b") { |disk| disk.pwrite(payload, 8192) }
      expect(File.open(device.path, "rb") { |disk| disk.pread(payload.bytesize, 8192) }).to eq(payload)

      expect(system("mkfs.ext4", "-F", device.path, out: File::NULL, err: File::NULL)).to be(true)
      expect(system("mount", device.path, mountpoint)).to be(true)
      File.binwrite(File.join(mountpoint, "proof"), payload)
      expect(File.binread(File.join(mountpoint, "proof"))).to eq(payload)
      expect(system("umount", mountpoint)).to be(true)
      expect(system("fsck.ext4", "-fn", device.path, out: File::NULL, err: File::NULL)).to be(true)
    ensure
      system("umount", mountpoint, out: File::NULL, err: File::NULL) if mountpoint && File.directory?(mountpoint)
      FileUtils.remove_entry(mountpoint) if mountpoint && File.exist?(mountpoint)
      device&.delete
    end
  end

  it "does not leak control file descriptors" do
    before = Dir.children("/proc/self/fd").size
    1_000.times { UBLK::Control.new.close }
    GC.start
    expect(Dir.children("/proc/self/fd").size).to be <= before + 2
  end
end
