module EventMachine::Udns

  class Resolver

    def initialize
      raise UdnsError, @alloc_error if @alloc_error
      @queries = {}
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