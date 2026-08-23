# frozen_string_literal: true

require_relative "ramdisk"

class TracingTarget < UBLK::Target
  def initialize(target, output: $stderr)
    @target = target
    @output = output
    super(size: target.size, queues: target.queues, depth: target.depth)
  end

  %i[read write discard write_zeroes].each do |operation|
    define_method(operation) do |offset, value|
      @output.puts("#{operation} offset=#{offset} length=#{value.respond_to?(:bytesize) ? value.bytesize : value}")
      @target.public_send(operation, offset, value)
    end
  end

  def flush
    @output.puts("flush")
    @target.flush
  end
end

UBLK::Device.create(TracingTarget.new(RamDisk.new(256 * 1024 * 1024))).run
