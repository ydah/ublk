# frozen_string_literal: true

module UBLK
  Params = Data.define(
    :size, :logical_block_size, :physical_block_size, :max_io_bytes,
    :read_only, :rotational, :discard
  ) do
    def initialize(size:, logical_block_size: 512, physical_block_size: 4096,
                   max_io_bytes: 512 * 1024, read_only: false, rotational: false,
                   discard: true)
      [logical_block_size, physical_block_size].each do |block_size|
        valid = block_size.is_a?(Integer) && block_size.between?(512, 4096) && (block_size & (block_size - 1)).zero?
        raise ArgumentError, "block sizes must be powers of two between 512 and 4096" unless valid
      end
      raise ArgumentError, "physical block size must not be smaller than logical block size" if physical_block_size < logical_block_size
      raise ArgumentError, "size must be aligned to the logical block size" unless (size % logical_block_size).zero?
      raise ArgumentError, "max_io_bytes must be a positive multiple of 512" unless max_io_bytes.is_a?(Integer) && max_io_bytes.positive? && (max_io_bytes % 512).zero?
      raise ArgumentError, "max_io_bytes must not exceed 32 MiB" if max_io_bytes > 32 * 1024 * 1024

      super
    end

    def self.from_target(target, max_io_bytes: 512 * 1024)
      new(
        size: target.size,
        logical_block_size: target.logical_block_size,
        physical_block_size: target.physical_block_size,
        max_io_bytes:,
        read_only: target.read_only?,
        rotational: target.rotational?,
        discard: target.method(:discard).owner != Target || target.method(:write_zeroes).owner != Target
      )
    end
  end
end
