# frozen_string_literal: true

require_relative "../../examples/fault_injection"

RSpec.describe FaultInjectionDisk do
  subject(:disk) { described_class.new(4096, 1024...2048) }

  it "rejects every request overlapping the failing range" do
    expect { disk.read(0, 4096) }.to raise_error(Errno::EIO)
    expect { disk.read(0, 1024) }.not_to raise_error
    expect { disk.write(2048, "x" * 512) }.not_to raise_error
  end
end
