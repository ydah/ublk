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
    raise Errno::EIO if offset < @failing_range.end && offset + length > @failing_range.begin
  end
end

if $PROGRAM_NAME == __FILE__
  disk = FaultInjectionDisk.new(256 * 1024 * 1024, (64 * 1024 * 1024)...(65 * 1024 * 1024))
  UBLK::Device.create(disk).run
end
