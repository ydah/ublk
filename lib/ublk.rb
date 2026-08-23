# frozen_string_literal: true

require_relative "ublk/version"
require_relative "ublk/target"
require_relative "ublk/device_info"
require_relative "ublk/params"

module UBLK
  class Error < StandardError; end
  class UnsupportedError < Error; end
  class DeviceError < Error; end

  begin
    require "ublk/ublk"
  rescue LoadError => error
    @native_load_error = error
  end

  class << self
    attr_reader :native_load_error

    def supported?
      !!(defined?(Native) && Native.supported?)
    end

    def ensure_supported!
      return if supported?

      detail = native_load_error&.message || "/dev/ublk-control is unavailable"
      raise UnsupportedError, "ublk requires Linux 6.6+, ublk_drv, and liburing (#{detail})"
    end
  end
end

require_relative "ublk/control"
require_relative "ublk/device"
