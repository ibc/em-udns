module EventMachine::Udns

  class Resolver
    def initialize(options = {})
      raise UdnsError, @alloc_error if @alloc_error
      @queries = {}
      nameservers = [*options[:nameserver]] + [*options[:nameservers]]
      if nameservers.any?
        add_serv(nil) # clear the list initialized from /etc/resolv.conf
        nameservers.each do |ns|
          host, port = ns.split(':')
          if port
            add_serv_s(host, port.to_i)
          else
            add_serv(host)
          end
        end
      end
      dns_open
    end


    private

    def set_timer(timeout)
      @timer = EM::Timer.new(timeout) { timeouts }
    end
  end

end
