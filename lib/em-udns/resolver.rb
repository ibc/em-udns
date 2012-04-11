module EventMachine::Udns

  class Resolver
    def initialize
      raise UdnsError, @alloc_error if @alloc_error
      @queries = {}
      dns_open
    end


    private

    def set_timer(timeout)
      @timer = EM::Timer.new(timeout) { timeouts }
    end
  end

end