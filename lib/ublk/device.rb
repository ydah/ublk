# frozen_string_literal: true

module UBLK
  class Device
    @owned = []

    at_exit do
      @owned&.dup&.each do |device|
        device.delete
      rescue Exception
        nil
      end
    end

    class << self
      attr_reader :owned
    end

    attr_reader :target, :info, :params

    def self.create(target, id: nil, queues: target.queues, depth: target.depth,
                    max_io_bytes: 512 * 1024, recovery: false, mlock: true)
      control = Control.new
      info = control.add_dev(id:, queues:, depth:, max_io_bytes:, recovery:)
      params = Params.from_target(target, max_io_bytes:)
      control.set_params(info.id, params)
      new(target, control, info, params, mlock:).tap { |device| owned << device }
    rescue Exception
      begin
        control&.del_dev(info.id) if info
      rescue SystemCallError, Error
        nil
      end
      control&.close
      raise
    end

    def self.list
      control = Control.new
      control.list
    ensure
      control&.close
    end

    def self.recover(target, id:, mlock: true)
      control = Control.new
      deadline = Process.clock_gettime(Process::CLOCK_MONOTONIC) + 10
      begin
        control.start_user_recovery(id)
      rescue Errno::EBUSY
        raise if Process.clock_gettime(Process::CLOCK_MONOTONIC) >= deadline

        sleep 0.01
        retry
      end
      info = control.get_dev_info(id)
      params = Params.from_target(target, max_io_bytes: info.max_io_bytes)
      new(target, control, info, params, mlock:, recovering: true).tap { |device| owned << device }
    rescue Exception
      control&.close
      raise
    end

    def self.delete_all!
      control = Control.new
      control.list.each { |item| control.del_dev(item.id) }
    ensure
      control&.close
    end

    def initialize(target, control, info, params, mlock: true, recovering: false)
      @target = target
      @control = control
      @info = info
      @params = params
      @mlock = mlock
      @recovering = recovering
      @started = false
      @deleted = false
    end

    def id = info.id
    def path = info.path
    def char_path = info.char_path

    def start(threads: :auto)
      raise DeviceError, "device has been deleted" if @deleted
      return self if @started

      count = threads == :auto ? info.queues : Integer(threads)
      raise ArgumentError, "threads must equal the hardware queue count" unless count == info.queues

      Native.lock_memory! if @mlock
      @server = Native::Server.new(id, info.queues, info.depth)
      ready = Queue.new
      @workers = count.times.map do |queue|
        Thread.new do
          @server.run(queue, target, ready)
        rescue Exception => error
          ready << error
          raise
        end.tap { |worker| worker.report_on_exception = false }
      end
      count.times do
        result = ready.pop
        raise result if result.is_a?(Exception)
      end
      if @recovering
        @control.end_user_recovery(id)
        @recovering = false
      else
        @control.start_dev(id)
      end
      @started = true
      self
    rescue Exception
      @server&.close
      join_workers(@workers, suppress: true)
      @server&.release
      @server = @workers = nil
      raise
    end

    def stop
      return self unless @started || @server

      @control.stop_dev(id) if @started
      self
    ensure
      @started = false
      server, workers = @server, @workers
      @server = @workers = nil
      server&.close
      begin
        join_workers(workers)
      ensure
        server&.release
      end
    end

    def delete
      return self if @deleted

      worker_error = nil
      begin
        stop
      rescue Exception => error
        worker_error = error
      end
      @control.del_dev(id)
      @deleted = true
      self.class.owned.delete(self)
      raise worker_error if worker_error

      self
    ensure
      @control.close if @deleted
    end

    def run(threads: :auto)
      raise UnsupportedError, "data plane is unavailable in this build" unless Native.const_defined?(:Server)

      start(threads:)
      @workers.each(&:join)
      self
    ensure
      stop if @started
    end

    private

    def join_workers(workers, suppress: false)
      error = nil
      workers&.each do |worker|
        worker.join unless worker == Thread.current
      rescue Exception => caught
        error ||= caught
      end
      raise error if error && !suppress
    end
  end
end
