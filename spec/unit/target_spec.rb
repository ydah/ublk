# frozen_string_literal: true

RSpec.describe UBLK::Target do
  subject(:target) { described_class.new(size: 4096) }

  it "validates geometry" do
    expect { described_class.new(size: 513) }.to raise_error(ArgumentError)
    expect { described_class.new(size: 4096, depth: 0) }.to raise_error(ArgumentError)
  end

  it "provides conservative optional operations" do
    expect(target.flush).to eq(0)
    expect { target.discard(0, 512) }.to raise_error(Errno::EOPNOTSUPP)
    expect { target.write_zeroes(0, 512) }.to raise_error(Errno::EOPNOTSUPP)
  end
end
