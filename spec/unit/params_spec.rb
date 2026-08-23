# frozen_string_literal: true

RSpec.describe UBLK::Params do
  it "validates block sizes and alignment" do
    expect { described_class.new(size: 4096, logical_block_size: 1000) }.to raise_error(ArgumentError)
    expect { described_class.new(size: 4608, logical_block_size: 4096) }.to raise_error(ArgumentError)
  end

  it "copies target geometry" do
    target = UBLK::Target.new(size: 8192)
    expect(described_class.from_target(target).logical_block_size).to eq(512)
    expect(described_class.from_target(target).discard).to be(false)

    target.define_singleton_method(:discard) { |*, **| 0 }
    expect(described_class.from_target(target).discard).to be(true)
  end
end
