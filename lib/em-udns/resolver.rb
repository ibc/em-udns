module EventMachine::Udns

  class Resolver

    def initialize(nameserver = nil)
      raise UdnsError, @alloc_error if @alloc_error

      @queries = {}

      if nameserver
        ENV.delete("NAMESERVERS")
        ENV["NAMESERVERS"] = case nameserver
          # A single nameserver.
          when String
            nameserver
          # Multiple nameservers.
          when Array
            nameserver.join(" ")
          else
            raise Error, "`nameserver' argument must be a String or Array of addresses"
          end
      end

      dns_open
    end

    def set_timer(timeout)
      @timer = EM::Timer.new(timeout) do
        timeouts
      end
    end
    private :set_timer

  end

end