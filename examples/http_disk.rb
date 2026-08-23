# frozen_string_literal: true

require "net/http"
require "ublk"

class HTTPDisk < UBLK::Target
  def initialize(url)
    @uri = URI(url)
    response = Net::HTTP.start(@uri.host, @uri.port, use_ssl: @uri.scheme == "https") { |http| http.head(@uri.request_uri) }
    super(size: Integer(response.fetch("content-length"), 10))
  end

  def read(offset, length)
    request = Net::HTTP::Get.new(@uri)
    request["Range"] = "bytes=#{offset}-#{offset + length - 1}"
    response = Net::HTTP.start(@uri.host, @uri.port, use_ssl: @uri.scheme == "https") { |http| http.request(request) }
    raise Errno::EIO, "HTTP #{response.code}" unless response.is_a?(Net::HTTPSuccess)

    response.body
  end

  def read_only? = true
end

abort "usage: #{$PROGRAM_NAME} URL" unless ARGV.one?
UBLK::Device.create(HTTPDisk.new(ARGV.fetch(0))).run
