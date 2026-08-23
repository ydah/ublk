# frozen_string_literal: true

require_relative "../examples/ramdisk"

abort "set UBLK_BENCH=1 inside a privileged Linux VM" unless ENV["UBLK_BENCH"] == "1"

device = nil
at_exit { device&.delete }
duration = Integer(ENV.fetch("UBLK_BENCH_SECONDS", "60"), 10)
path = ENV["UBLK_BENCH_DEVICE"]
if path
  abort "UBLK_BENCH_DEVICE must be a block device" unless File.blockdev?(path)
else
  GC.stress = true if ENV["UBLK_BENCH_GC_STRESS"] == "1"
  device = UBLK::Device.create(RamDisk.new(256 * 1024 * 1024), depth: 128, mlock: false)
  device.start
  path = device.path
end

%w[randread randwrite read].each do |workload|
  success = system(
    "fio", "--name=#{workload}", "--filename=#{path}", "--direct=1", "--rw=#{workload}",
    "--bs=4k", "--iodepth=32", "--runtime=#{duration}", "--time_based=1",
    "--output-format=json"
  )
  abort "fio #{workload} failed" unless success
end
