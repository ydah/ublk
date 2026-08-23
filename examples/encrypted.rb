# frozen_string_literal: true

require "openssl"
require "ublk"

class EncryptedDisk < UBLK::Target
  def initialize(path, size:, key:)
    raise ArgumentError, "key must be 32 bytes" unless key.bytesize == 32

    @file = File.open(path, File::RDWR | File::CREAT, 0o600)
    @file.truncate(size)
    @key = key
    super(size:)
  end

  def read(offset, length) = crypt(offset, @file.pread(length, offset))

  def write(offset, data)
    @file.pwrite(crypt(offset, data), offset)
  end

  def flush = @file.fsync

  def discard(offset, length)
    write(offset, "\0".b * length)
    0
  end

  alias write_zeroes discard

  private

  def crypt(offset, data)
    cipher = OpenSSL::Cipher.new("aes-256-ctr")
    cipher.encrypt
    cipher.key = @key
    cipher.iv = [0, offset / 16].pack("Q>Q>")
    cipher.update(data) + cipher.final
  end
end

abort "usage: UBLK_KEY_HEX=<64 hex chars> #{$PROGRAM_NAME} FILE SIZE" unless ARGV.size == 2 && ENV["UBLK_KEY_HEX"]
key = [ENV.fetch("UBLK_KEY_HEX")].pack("H*")
UBLK::Device.create(EncryptedDisk.new(ARGV.fetch(0), size: Integer(ARGV.fetch(1), 10), key:)).run
