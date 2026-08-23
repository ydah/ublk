# frozen_string_literal: true

RSpec.describe UBLK do
  it "has a version number" do
    expect(UBLK::VERSION).not_to be_nil
  end

  it "loads on unsupported hosts" do
    expect(described_class.supported?).to be(false) unless RUBY_PLATFORM.include?("linux")
  end

  it "matches the kernel UAPI layout" do
    skip "native extension not loaded" unless defined?(UBLK::Native)

    expect(UBLK::Native.layout).to include(
      ctrl_cmd_size: 32,
      dev_info_size: 64,
      io_desc_size: 24,
      io_desc_start_sector_offset: 8
    )
    expect(UBLK::Native::UBLK_F_USER_COPY).to be_a(Integer)
  end
end
