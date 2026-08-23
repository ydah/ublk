# frozen_string_literal: true

module UBLK
  class Device
    attr_reader :target, :info, :params

    def self.create(target, id: nil, queues: target.queues, depth: target.depth,
                    max_io_bytes: 512 * 1024, recovery: false, mlock: true)
      control = Control.new
      info = control.add_dev(id:, queues:, depth:, max_io_bytes:, recovery:)
      params = Params.from_target(target, max_io_bytes:)
      control.set_params(info.id, params)
      new(target, control, info, params, mlock:)
    rescue Exception
      control&.del_dev(info.id) if info
      raise
    end

    def self.list = Control.new.list

    def self.delete_all!
      control = Control.new
      control.list.each { |item| control.del_dev(item.id) }
    end

    def initialize(target, control, info, params, mlock: true)
      @target = target
      @control = control
      @info = info
      @params = params
      @mlock = mlock
      @started = false
      @deleted = false
    end

    def id = info.id
    def path = info.path
    def char_path = info.char_path

    def start
      raise DeviceError, "device has been deleted" if @deleted

      @control.start_dev(id)
      @started = true
      self
    end

    def stop
      return self unless @started

      @control.stop_dev(id)
      @started = false
      self
    end

    def delete
      return self if @deleted

      stop
      @control.del_dev(id)
      @deleted = true
      self
    end

    def run(threads: :auto)
      raise UnsupportedError, "data plane is unavailable in this build" unless Native.const_defined?(:Server)

      count = threads == :auto ? info.queues : Integer(threads)
      raise ArgumentError, "threads must equal the hardware queue count" unless count == info.queues

      Native.lock_memory! if @mlock
      server = Native::Server.new(id, info.queues, info.depth)
      workers = count.times.map { |queue| Thread.new { server.run(queue, target) } }
      start
      workers.each(&:join)
      self
    ensure
      stop if @started
      server&.close
      workers&.each { |worker| worker.join unless worker == Thread.current }
    end
  end
end
