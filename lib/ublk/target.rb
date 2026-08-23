# frozen_string_literal: true

module UBLK
  class Target
    attr_reader :size, :queues, :depth

    def initialize(size:, queues: 1, depth: 128)
      raise ArgumentError, "size must be a positive multiple of 512" unless size.is_a?(Integer) && size.positive? && (size % 512).zero?
      raise ArgumentError, "queues must be between 1 and 4096" unless queues.is_a?(Integer) && queues.between?(1, 4096)
      raise ArgumentError, "depth must be between 1 and 4096" unless depth.is_a?(Integer) && depth.between?(1, 4096)

      @size = size
      @queues = queues
      @depth = depth
    end

    def read(_offset, _length)
      raise NotImplementedError, "targets must implement #read"
    end

    def write(_offset, _data)
      raise Errno::EROFS if read_only?

      raise NotImplementedError, "targets must implement #write"
    end

    def flush = 0

    def discard(_offset, _length)
      raise Errno::EOPNOTSUPP
    end

    def write_zeroes(_offset, _length)
      raise Errno::EOPNOTSUPP
    end

    def logical_block_size = 512
    def physical_block_size = 4096
    def read_only? = false
    def rotational? = false
  end
end
