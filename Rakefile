# frozen_string_literal: true

require "bundler/gem_tasks"
require "rake/extensiontask"
require "rspec/core/rake_task"

Rake::ExtensionTask.new("ublk") do |ext|
  ext.lib_dir = "lib/ublk"
end

namespace :test do
  RSpec::Core::RakeTask.new(:unit) { |task| task.pattern = "spec/unit/**/*_spec.rb" }
  RSpec::Core::RakeTask.new(:system) { |task| task.pattern = "spec/system/**/*_spec.rb" }
end

task default: "test:unit"
