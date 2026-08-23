# frozen_string_literal: true

require "ublk"

class LoopTarget < UBLK::Target
  def initialize(path, size: File.size(path), read_only: false)
    @file = File.open(path, read_only ? "rb" : "r+b")
    @read_only = read_only
    super(size:)
  end

  def read(offset, length) = @file.pread(length, offset)
  def write(offset, data) = @file.pwrite(data, offset)
  def flush = @file.fsync
  def read_only? = @read_only
end

abort "usage: #{$PROGRAM_NAME} IMAGE" unless ARGV.one?
UBLK::Device.create(LoopTarget.new(ARGV.fetch(0))).run
