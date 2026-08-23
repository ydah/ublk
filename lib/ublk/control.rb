# frozen_string_literal: true

module UBLK
  class Control
    SYSFS = "/sys/class/ublk-char"

    def initialize
      UBLK.ensure_supported!
      @native = Native::Control.new
    end

    def add_dev(id: nil, queues: 1, depth: 128, max_io_bytes: 512 * 1024, recovery: false)
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
