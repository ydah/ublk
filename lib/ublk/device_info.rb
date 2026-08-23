# frozen_string_literal: true

module UBLK
  DeviceInfo = Data.define(
    :id, :queues, :depth, :state, :max_io_bytes, :pid, :flags, :owner_uid, :owner_gid
  ) do
    def path = "/dev/ublkb#{id}"
    def char_path = "/dev/ublkc#{id}"

    def self.from_native(values)
      new(*values)
    end
  end
end
