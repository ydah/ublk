# frozen_string_literal: true

require_relative "../examples/ramdisk"

abort "set UBLK_BENCH=1 inside a privileged Linux VM" unless ENV["UBLK_BENCH"] == "1"

device = UBLK::Device.create(RamDisk.new(1024 * 1024 * 1024), depth: 128, mlock: false)
device.start
system("fio", "--name=ublk", "--filename=#{device.path}", "--direct=1", "--rw=randrw",
       "--bs=4k", "--iodepth=32", "--runtime=60", "--time_based=1") || abort("fio failed")
device.delete
