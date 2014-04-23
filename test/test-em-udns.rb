#!/usr/bin/ruby

$0 = "test-em-udns.rb"

require "rubygems"
require "em-udns"


def show_usage
  puts <<-END_USAGE
USAGE:

  #{$0} A domain [times]
  #{$0} AAAA domain [times]
  #{$0} PTR ip [times]
  #{$0} MX domain [times]
  #{$0} TXT domain [times]
  #{$0} SRV _service._protocol.domain [times]
  #{$0} SRV domain [service] [protocol] [times]
  #{$0} NAPTR domain [times]
  #{$0} NS domain [times]
END_USAGE
end


if ARGV.size < 2
  show_usage
  exit false
end

type = ARGV[0].upcase
name = ARGV[1]
case type
when "A", "AAAA", "MX", "TXT", "PTR", "NAPTR", "NS"
  times = (ARGV[2].to_i > 0) ? ARGV[2].to_i : 1
when "SRV"
  if ARGV[3]
    service = ARGV[2]
    protocol = ARGV[3]
    times = (ARGV[4].to_i > 0) ? ARGV[4].to_i : 1
  else
    service = nil
    protocol = nil
    times = (ARGV[2].to_i > 0) ? ARGV[2].to_i : 1
  end
else
  show_usage
  exit false
end


def print_info(times, time_start)
  puts "\n\nINFO: all the #{times} queries terminated"
  printf "INFO: time elapsed: %.4f seconds (%.4f queries/second)\n", (time_elapsed = Time.now - time_start), (times / time_elapsed)
end


EM.set_max_timers 100000
EM.run do

  # Set the nameserver rather than using /etc/resolv.conf.
  #EM::Udns.nameservers = "127.0.0.1"
  
  resolver = EM::Udns::Resolver.new
  EM::Udns.run resolver

  second = 0
  EM.add_periodic_timer(1) { puts "[#{second += 1}] - active queries: #{resolver.active}" }

  time_start = Time.now
  sent = 0
  recv = 0

  timer = EM::PeriodicTimer.new(0) do
    if sent == times
      timer.cancel

    else
      sent += 1
    
      query = case type
        when "A"
          resolver.submit_A name
        when "AAAA"
          resolver.submit_AAAA name
        when "PTR"
          resolver.submit_PTR name
        when "MX"
          resolver.submit_MX name
        when "TXT"
          resolver.submit_TXT name
        when "SRV"
          resolver.submit_SRV name, service, protocol
        when "NAPTR"
          resolver.submit_NAPTR name
        when "NS"
          resolver.submit_NS name
      end

      query.callback do |result|
        recv += 1
        puts "#{Time.now} INFO: callback: result =>"
        result.each do |rr|
          puts "- #{rr.inspect}"
        end
        puts "(active queries: #{resolver.active} / sent: #{sent} / recv: #{recv})"
        if recv == times
          print_info(times, time_start)
          exit
        end
      end

      query.errback do |error|
        recv += 1
        puts "#{Time.now} INFO: errback: error => #{error.inspect}  (active queries: #{resolver.active} / sent: #{sent} / recv: #{recv})"
        if recv == times
          print_info(times, time_start)
          exit
        end
      end
    end

    # Uncomment to cancel the query (so no callback/errback will be called).
    #resolver.cancel(query)
  end
  
end

