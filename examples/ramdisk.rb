# frozen_string_literal: true

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

  def discard(offset, length)
    @data[offset, length] = "\0".b * length
    0
  end

  alias write_zeroes discard
end

UBLK::Device.create(RamDisk.new(256 * 1024 * 1024)).run if $PROGRAM_NAME == __FILE__
