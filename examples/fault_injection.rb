# frozen_string_literal: true

require_relative "ramdisk"

class FaultInjectionDisk < RamDisk
  def initialize(size, failing_range)
    @failing_range = failing_range
    super(size)
  end

  def read(offset, length)
    fail_if_needed(offset, length)
    super
  end

  def write(offset, data)
    fail_if_needed(offset, data.bytesize)
    super
  end

  private

  def fail_if_needed(offset, length)
    raise Errno::EIO if @failing_range.cover?(offset) || @failing_range.cover?(offset + length - 1)
  end
end

disk = FaultInjectionDisk.new(256 * 1024 * 1024, (64 * 1024 * 1024)...(65 * 1024 * 1024))
UBLK::Device.create(disk).run
