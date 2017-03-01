# EM-Udns

* Home page: http://ibc.github.com/em-udns/


## Overview

EM-Udns is an async DNS resolver for [EventMachine](http://rubyeventmachine.com) based on [udns](http://www.corpit.ru/mjt/udns.html) C library. Having most of the code written in C, EM-Udns becomes very fast. It can resolve DNS A, AAAA, PTR, MX, TXT, NS, SRV and NAPTR records, and can handle every kind of errors (domain/record not found, request timeout, malformed response...).

C udns is a stub resolver, so also EM-Udns. This means that it must rely on a recursive name server, usually co-located in local host or local network. A very good choice is [Unbound](http://unbound.net), a validating, recursive and caching DNS resolver.

**IMPORTANT:** Please read this again: EM-Udns is a stub resolver so you need a recursive nameserver. Probably the DNS nameserver offered via DHCP by your Internet provider is not a recursive nameserver so EM-Udns will **NOT** work. Please don't attempt to use EM-Udns if you don't properly understand what this note means.


## Usage Example

    require "em-udns"

    EM.run do
      # Set the nameserver rather than using /etc/resolv.conf.
      EM::Udns.nameservers = "127.0.0.1"
      
      resolver = EM::Udns::Resolver.new

      # alternate method of setting nameserver, including non-standard port
      # resolver = EM::Udns::Resolver.new(nameserver: '127.0.0.1:5353')
      # resolver = EM::Udns::Resolver.new(nameserver: ['192.168.0.1', '192.168.0.2:5353'])

      EM::Udns.run resolver

      query = resolver.submit_A "google.com"

      query.callback do |result|
        puts "result => #{result.inspect}"
      end

      query.errback do |error|
        puts "error => #{error.inspect}"
      end
    end

It would produce following output:

    result => ["209.85.227.105", "209.85.227.103", "209.85.227.104", "209.85.227.106", "209.85.227.99", "209.85.227.147"]


## Setting the Nameservers

    EM::Udns.nameservers = nameservers

This class method set the nameservers list to use for all the `EM::Udns::Resolver` instances. If not used, nameservers are taken from `/etc/resolv.conf` (default behavior). nameserver parameter can be:

 * `String` - The IP of a single nameserver.
 * Array of `String` -  IP's of multiple nameservers.

IMPORTANT: This class method must be used before initializing any `EM::Udns::Resolver` instance.

NOTE: Nameservers must be IPv4 addresses since [udns](http://www.corpit.ru/mjt/udns.html) does not listen in IPv6.

Example 1:

    EM::Udns.nameservers = "127.0.0.1"
    
Example 2:

    EM::Udns.nameservers = ["192.168.100.1", "192.168.100.2"]
    

## Initializing a Resolver

    resolver = EM::Udns::Resolver.new

Returns a `EM::Udns::Resolver` instance. If there is an error an exception `EM::Udns::UdnsError` is raised.

    nameserver(s) may also be passed to `new` as a hash argument:

    resolver = EM::Udns::Resolver.new(nameserver: '127.0.0.1:5353')
    resolver = EM::Udns::Resolver.new(nameserver: ['192.168.0.1', '192.168.0.2:5353'])

## Running a Resolver

    EM::Udns.run resolver

Attaches the UDP socket of the resolver to EventMachine. This method must be called after EventMachine is running.


## Async DNS Queries

    resolver.submit_XXX(parameters)

DNS queries are performed by invoking `EM::Udns::Resolver#submit_XXX(parameters)` methods on the resolver. The complete list of `submit_XXX` methods are shown below. These methods return a `EM::Udns::Query` instance. Callback and errback can then be assigned to the `Query` object via the `callback` and `errback` methods which accept a code block as single argument.

In case of success, the callback code block is invoked on the `EM::Udns::Query` object passing the DNS result object as single argument. Definition of those objects are shown below.

In case of error, the errback code block is invoked with the exact error as single argument, which is a Ruby Symbol:

 * `:dns_error_nxdomain` - The domain name does not exist.
 * `:dns_error_nodata` - The domain exists, but has no data of requested type.
 * `:dns_error_tempfail` - Temporary error, the resolver nameserver was not able to process our query or timed out.
 * `:dns_error_protocol` - Protocol error, a nameserver returned malformed reply.
 * `:dns_error_badquery` - Bad query, name of dn is invalid.
 * `:dns_error_nomem` - No memory available to allocate query structure.
 * `:dns_error_unknown` - An unknown error has occurred.


## Type Specific Queries


### A Record

    resolver.submit_A(domain)

In case of success the callback is invoked passing as argument an array of `String` objects. Each `String` represents an IPv4 address.

Example:

    resolver.submit_A "google.com"

Callback is called with argument:

    ["209.85.227.105",
     "209.85.227.103",
     "209.85.227.104",
     "209.85.227.106",
     "209.85.227.99",
     "209.85.227.147"]


### AAAA Record

    resolver.submit_AAAA(domain)

In case of success the callback is invoked passing as argument an array of `String` objects. Each `String` represents an IPv6 address.

Example:

    resolver.submit_AAAA "sixxs.net"

Callback is called with argument:

    ["2001:838:2:1::30:67",
     "2001:838:2:1:2a0:24ff:feab:3b53",
     "2001:960:800::2",
     "2001:1af8:4050::2"]


### MX Record

    resolver.submit_MX(domain)

In case of success the callback is invoked passing as argument an array of `EM::Udns::RR_MX` objects. Such object contains the following attribute readers:

 * `domain` - `String` representing the domain of the MX record.
 * `priority` - `Fixnum` representing the priority of the MX record.

Example:

    resolver.submit_MX "gmail.com"

Callback is called with argument:

    [#<EventMachine::Udns::RR_MX:0x00000002289090 @domain="alt1.gmail-smtp-in.l.google.com", @priority=10>,
     #<EventMachine::Udns::RR_MX:0x00000002288e60 @domain="alt3.gmail-smtp-in.l.google.com", @priority=30>,
     #<EventMachine::Udns::RR_MX:0x000000022886e0 @domain="gmail-smtp-in.l.google.com", @priority=5>,
     #<EventMachine::Udns::RR_MX:0x00000002288618 @domain="alt2.gmail-smtp-in.l.google.com", @priority=20>,
     #<EventMachine::Udns::RR_MX:0x000000022883c0 @domain="alt4.gmail-smtp-in.l.google.com", @priority=40>]


### PTR Record

    resolver.submit_PTR(ip)

Argument ip must be a `String` representing a IPv4 or IPv6. In case of success the callback is invoked passing as argument an array of `String` objects. Each `String` represents a domain.

Example 1:

    resolver.submit_PTR "8.8.8.8"

Callback is called with argument:

    ["google-public-dns-a.google.com"]

Example 2:

    resolver.submit_PTR "2001:838:2:1:2a0:24ff:feab:3b53"

Callback is called with argument:

    ["tunnelserver.concepts-ict.net"]


### TXT Record

    resolver.submit_TXT(domain)

In case of success the callback is invoked passing as argument an array of `String` objects. Each `String` represents a text entry in the TXT result.

Example:

    resolver.submit_TXT "gmail.com"

Callback is called with argument:

    ["v=spf1 redirect=_spf.google.com"]


### NS Record

    resolver.submit_NS(domain)

In case of success the callback is invoked passing as argument an array of `String` objects.  Each `String`  represents a nameserver entry in the NS result.

Example:

    resolver.submit_NS "gmail.com"

Callback is called with argument:

    ["ns1.google.com", "ns3.google.com", "ns4.google.com", "ns2.google.com"]

    
### SRV Record

    resolver.submit_SRV(domain)
    resolver.submit_SRV(domain, service, protocol)

There are two ways to perform a SRV query:

 * By passing as argument a single `String` (domain) with the format "_service._protocol.domain".
 * By passing three `String` arguments (domain, service and protocol).
 
In case of success the callback is invoked passing as argument an array of `EM::Udns::RR_SRV` objects. Such object contains the following attribute readers:

 * `domain` - `String` representing the A domain of the SRV record.
 * `port` - `Fixnum` representing the port of the SRV record.
 * `priority` - `Fixnum` representing the priority of the SRV record.
 * `weight` - `Fixnum` representing the weight of the SRV record.

For more information about these fields check [RFC 2782](http://tools.ietf.org/html/rfc2782).

Example:

    resolver.submit_SRV "_sip._tcp.oversip.net"

or:

    resolver.submit_SRV "oversip.net", "sip", "tcp"

Callback is called with argument:

    [#<EventMachine::Udns::RR_SRV:0x00000001ea4970 @domain="sip1.oversip.net", @priority=1, @weight=50, @port=5062>,
     #<EventMachine::Udns::RR_SRV:0x00000001ea4b50 @domain="sip2.oversip.net", @priority=2, @weight=50, @port=5060>]
     

### NAPTR Record

    resolver.submit_NAPTR(domain)

In case of success the callback is invoked passing as argument an array of `EM::Udns::RR_NAPTR` objects. Such object contains the following attribute readers:

 * `order` - `Fixnum` representing the order of the NAPTR record.
 * `preference` - `Fixnum` representing the preference of the NAPTR record.
 * `flags` - `String` representing the flags of the NAPTR record.
 * `service` - `String` representing the service of the NAPTR record.
 * `regexp` - `String` representing the regular expression field of the NAPTR record (`nil` in case `replacement` has value).
 * `replacement` - `String` representing the replacement string field of the NAPTR record (`nil` in case `regexp` has value).

For more information about these fields check [RFC 2915](http://tools.ietf.org/html/rfc2915).

Example:

    resolver.submit_NAPTR "oversip.net"

Callback is called with argument:

    [#<EventMachine::Udns::RR_NAPTR:0x00000002472aa0 @order=30, @preference=50, @flags="S", @service="SIPS+D2T", @regexp=nil, @replacement="_sips._tcp.oversip.net">,
     #<EventMachine::Udns::RR_NAPTR:0x00000002472848 @order=40, @preference=50, @flags="S", @service="SIP+D2S", @regexp=nil, @replacement="_sip._sctp.oversip.net">,
     #<EventMachine::Udns::RR_NAPTR:0x000000024723e8 @order=10, @preference=50, @flags="S", @service="SIP+D2T", @regexp=nil, @replacement="_sip._tcp.oversip.net">,
     #<EventMachine::Udns::RR_NAPTR:0x00000002471d80 @order=20, @preference=50, @flags="S", @service="SIP+D2U", @regexp=nil, @replacement="_sip._udp.oversip.net">]


## Other Features

### Number of Active Queries

    resolver.active

`EM::Udns::Resolver#active` returns the number of pending queries of resolver as a `Fixnum`.


### Cancelling a Pending Query

    resolver.cancel query

`EM::Udns::Resolver#cancel(query)` cancels the `EM::Udns::Query` given as argument so no callback/errback would be called upon query completion.


## Installation

EM-Udns is provided as a Ruby Gem:

    ~$ gem install em-udns


## Supported Platforms

EM-Udns is tested under the following platforms:

 * Linux 32 and 64 bits
 * Mac OSX 32 and 64 bits

See also the [extconf.rb](https://github.com/ibc/em-udns/blob/master/ext/extconf.rb) file which compiles udns C library according to current platform.


## TODO

 * Testing on other platforms.


## Acknowledgement

Many thanks to Michael Tokarev (the author of [udns](http://www.corpit.ru/mjt/udns.html) C library) for all the help provided in udns mailing list.
