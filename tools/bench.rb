# frozen_string_literal: true

require "tempfile"
require_relative "../examples/loop"

abort "set UBLK_BENCH=1 inside a privileged Linux VM" unless ENV["UBLK_BENCH"] == "1"

device = nil
backing = nil
at_exit do
  device&.delete
  backing&.close!
end
duration = Integer(ENV.fetch("UBLK_BENCH_SECONDS", "60"), 10)
path = ENV["UBLK_BENCH_DEVICE"]
if path
  abort "UBLK_BENCH_DEVICE must be a block device" unless File.blockdev?(path)
else
  GC.stress = true if ENV["UBLK_BENCH_GC_STRESS"] == "1"
  backing = Tempfile.new("ublk-bench")
  backing.truncate(256 * 1024 * 1024)
  device = UBLK::Device.create(LoopTarget.new(backing.path), depth: 128, mlock: false)
  device.start
  path = device.path
end

%w[randread randwrite read].each do |workload|
  success = system(
    "fio", "--name=#{workload}", "--filename=#{path}", "--direct=1", "--rw=#{workload}",
    "--ioengine=io_uring", "--bs=4k", "--iodepth=32", "--runtime=#{duration}", "--time_based=1",
    "--output-format=json"
  )
  abort "fio #{workload} failed" unless success
end
