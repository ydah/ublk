# frozen_string_literal: true

RSpec.describe UBLK::Control do
  subject(:control) { described_class.allocate }

  it "rejects invalid device geometry before entering native code" do
    expect { control.add_dev(queues: 0) }.to raise_error(ArgumentError)
    expect { control.add_dev(depth: 4097) }.to raise_error(ArgumentError)
    expect { control.add_dev(max_io_bytes: 513) }.to raise_error(ArgumentError)
  end
end
