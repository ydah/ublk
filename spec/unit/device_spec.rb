# frozen_string_literal: true

RSpec.describe UBLK::Device do
  it "starts workers before the device and cleans up in order" do
    server_class = Class.new do
      def initialize(*) = @closed = false

      def run(queue, _target, ready)
        ready << queue
        sleep(0.001) until @closed
      end

      def close = @closed = true
    end
    native = Module.new
    native.const_set(:Server, server_class)
    stub_const("UBLK::Native", native)

    calls = []
    control = Object.new
    control.define_singleton_method(:start_dev) { |id, **| calls << [:start, id] }
    control.define_singleton_method(:stop_dev) { |id| calls << [:stop, id] }
    control.define_singleton_method(:del_dev) { |id| calls << [:delete, id] }
    control.define_singleton_method(:close) { calls << [:close] }
    info = UBLK::DeviceInfo.new(7, 2, 8, 0, 4096, 0, 0, 0, 0)
    target = UBLK::Target.new(size: 4096, queues: 2, depth: 8)
    params = UBLK::Params.from_target(target, max_io_bytes: 4096)
    device = described_class.new(target, control, info, params, mlock: false)

    device.start.delete

    expect(calls).to eq([[:start, 7], [:stop, 7], [:delete, 7], [:close]])
  end

  it "deletes the kernel device even when a worker failed" do
    server_class = Class.new do
      def initialize(*) = nil
      def run(queue, _target, ready)
        ready << queue
        sleep 0.01
        raise "worker failed"
      end
      def close = nil
    end
    native = Module.new
    native.const_set(:Server, server_class)
    stub_const("UBLK::Native", native)

    calls = []
    control = Object.new
    control.define_singleton_method(:start_dev) { |*, **| nil }
    control.define_singleton_method(:stop_dev) { |*| sleep 0.02 }
    control.define_singleton_method(:del_dev) { |id| calls << [:delete, id] }
    control.define_singleton_method(:close) { calls << [:close] }
    info = UBLK::DeviceInfo.new(9, 1, 8, 0, 4096, 0, 0, 0, 0)
    target = UBLK::Target.new(size: 4096, depth: 8)
    params = UBLK::Params.from_target(target, max_io_bytes: 4096)
    device = described_class.new(target, control, info, params, mlock: false)

    device.start
    expect { device.delete }.to raise_error(RuntimeError, "worker failed")
    expect(calls).to eq([[:delete, 9], [:close]])
  end
end
