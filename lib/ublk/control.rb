# frozen_string_literal: true

module UBLK
  class Control
    SYSFS = "/sys/class/ublk-char"

    def initialize
      UBLK.ensure_supported!
      @native = Native::Control.new
    end

    def add_dev(id: nil, queues: 1, depth: 128, max_io_bytes: 512 * 1024, recovery: false)
      raise ArgumentError, "id must be nil or a non-negative Integer" unless id.nil? || (id.is_a?(Integer) && id >= 0)
      raise ArgumentError, "queues must be between 1 and 4096" unless queues.is_a?(Integer) && queues.between?(1, 4096)
      raise ArgumentError, "depth must be between 1 and 4096" unless depth.is_a?(Integer) && depth.between?(1, 4096)
      raise ArgumentError, "max_io_bytes must be a positive multiple of 512" unless max_io_bytes.is_a?(Integer) && max_io_bytes.positive? && (max_io_bytes % 512).zero?
      raise ArgumentError, "max_io_bytes must not exceed 32 MiB" if max_io_bytes > 32 * 1024 * 1024

      DeviceInfo.from_native(@native.add_dev(id, queues, depth, max_io_bytes, recovery))
    end

    def set_params(id, params)
      @native.set_params(id, params.size, params.logical_block_size,
                         params.physical_block_size, params.max_io_bytes,
                         params.read_only, params.rotational, params.discard)
      params
    end

    def start_dev(id, pid: Process.pid) = @native.start_dev(id, pid)
    def stop_dev(id) = @native.stop_dev(id)
    def del_dev(id) = @native.del_dev(id)
    def start_user_recovery(id) = @native.start_user_recovery(id)
    def end_user_recovery(id, pid: Process.pid) = @native.end_user_recovery(id, pid)
    def get_dev_info(id) = DeviceInfo.from_native(@native.get_dev_info(id))
    def close = @native.close

    def list
      Dir.glob("#{SYSFS}/ublkc*").filter_map do |path|
        id = File.basename(path).delete_prefix("ublkc")
        get_dev_info(Integer(id, 10))
      rescue SystemCallError, ArgumentError
        nil
      end
    end
  end
end
