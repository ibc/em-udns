require "eventmachine"

require "em_udns_ext"
require "em-udns/resolver"
require "em-udns/version"


module EventMachine::Udns

  module Watcher
    def initialize(resolver)
      @resolver = resolver
    end

    def notify_readable
      @resolver.ioevent
    end
  end

  def self.run(resolver)
    raise Error, "EventMachine is not running" unless EM.reactor_running?

    raise Error, "`resolver' argument must be a EM::Udns::Resolver instance" unless
      resolver.is_a? EM::Udns::Resolver

    EM.watch resolver.fd, Watcher, resolver do |dns_client|
      dns_client.notify_readable = true
    end

    self
  end

end
