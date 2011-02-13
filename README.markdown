# EM-Udns

EM-Udns is an async DNS resolver for [EventMachine](http://rubyeventmachine.com) based on [udns](http://www.corpit.ru/mjt/udns.html) C library. Having most of the code written in C, EM-Udns becomes very fast. It can resolve DNS A, AAAA, PTR, MX, TXT, SRV and NAPTR records, and can handle every kind of errors (domain/record not found, request timeout, malformed response...).

C udns is a stub resolver, so also EM-Udns. This means that it must rely on a recursive name server, usually co-located in local host or local network. A very good choice is [Unbound](http://unbound.net), a validating, recursive and caching DNS resolver.


## Usage Example

    require "em-udns"

    EM.run do
      resolver = EM::Udns::Resolver.new "127.0.0.1"
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


## Initializing a Resolver

    resolver = EM::Udns::Resolver.new(nameserver = nil)

Returns a EM::Udns::Resolver instance. `nameserver` parameter determines the nameserver(s) to use:

 * `nil` - List of nameservers are taken from `/etc/resolv.conf` (default behavior).
 * `String` - The IP of a single nameserver.
 * `Array` -  IP's of multiple nameservers.


## Runnig a Resolver

    EM::Udns.run resolver

Attaches the UDP socket of the `resolver` to EventMachine. This method must be called after EventMachine is running.


## Async DNS Queries

    resolver.submit_XXX(parameters)

DNS queries are performed by invoking `EM::Udns::Resolver#submit_XXX(parameters)` methods on the resolver. The complete list of `submit_XXX` methods are shown below. These methods return a `EM::Udns::Query` instance which includes `EM::Deferrable` module. Callback and errback can then be assigned to the `Query` object as usual.

In case of success, callback is invoked on the  `EM::Udns::Query` object passing the DNS result object as single argument. Definition of those objects are shown below.

In case of error, errback is invoked with the exact error as single argument, which is a Ruby Symbol:

 * `:dns_error_nxdomain` - The domain name does not exist.
 * `:dns_error_nodata` - There is no data of requested type found.
 * `:dns_error_tempfail` - Temporary error, the resolver nameserver was not able to process our query or timed out.
 * `:dns_error_protocol` - Protocol error, a nameserver returned malformed reply.

In same cases `submit_XXX` method could raise an exception when it's invoked:

 * `EM::Udns::UdnsBadQuery` - Bad query, name of dn is invalid.
 * `EM::Udns::UdnsNoMem` - No memory available to allocate query structure.
 * `EM::Udns::UdnsTempFail` - Internal error occured.

All these exceptions inherit from `EM::Udns::UdnsError`.


## Type Specific Queries


### A Record

    resolver.submit_A(domain)

In case of success the callback is invoked passing as argument an array of `String` objects. Each `String` represents an IPv4 address.

Example:

    resolver.submit_A "google.com"

Callback is called with argument:

    ["209.85.227.105", "209.85.227.103", "209.85.227.104", "209.85.227.106", "209.85.227.99", "209.85.227.147"]


### AAAA Record

    resolver.submit_AAAA(domain)

In case of success the callback is invoked passing as argument an array of `String` objects. Each `String` represents an IPv6 address.

Example:

    resolver.submit_AAAA "google.com"

Callback is called with argument:

    ["2001:838:2:1::30:67", "2001:838:2:1:2a0:24ff:feab:3b53", "2001:960:800::2", "2001:1af8:4050::2"]


### MX Record

    resolver.submit_MX(domain)

In case of success the callback is invoked passing as argument an array of `EM::Udns::RR_MX` objects. Such object contains the following attribute readers:

 * `domain` - `String` representing the domain of the MX record.
 * `priority` - `Fixnum` representing the priority of the MX record.

Example:

    resolver.submit_MX "linux.org"

Callback is called with argument:

    [#<EventMachine::Udns::RR_MX:0x000000018008f8 @domain="mail.linux.org", @priority=10>]


### PTR Record

    resolver.submit_PTR(ip)

Argument `ip` must be a String representing a IPv4 or IPv6 (if not, `ArgumentError` exception would be raised). In case of success the callback is invoked passing as argument an array of `String` objects. Each `String` represents a domain.

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

    
### SRV Record

    resolver.submit_SRV(domain)
    resolver.submit_SRV(domain, service, protocol)

There are two ways to perform a SRV query:

 * By passing as argument a single String (`domain`) with the format `_service._protocol.domain`.
 * By passing three String arguments (`domain`, `service` and `protocol`).
 
In case of success the callback is invoked passing as argument an array of `EM::Udns::RR_SRV` objects. Such object contains the following attribute readers:

 * `domain` - `String` representing the domain of the SRV record.
 * `priority` - `Fixnum` representing the priority of the SRV record.
 * `weight` - `Fixnum` representing the weight of the SRV record.

Example:

    resolver.submit_SRV "_sip._udp.oversip.net"

or:

    resolver.submit_SRV "oversip.net", "sip", "udp"

Callback is called with argument:

    [#<EventMachine::Udns::RR_SRV:0x00000000b9ea88 @domain="sip.oversip.net", @priority=0, @weight=0, @port=5062>]


### NAPTR Record

    resolver.submit_NAPTR(domain)

In case of success the callback is invoked passing as argument an array of `EM::Udns::RR_NAPTR` objects. Such object contains the following attribute readers:

 * `order` - `Fixnum` representing the order of the NAPTR record.
 * `preference` - `Fixnum` representing the preference of the NAPTR record.
 * `regexp` - `String` representing the regular expression field of the NAPTR record (`nil` in case `replacement` has value).
 * `replacement` - `String` representing the replacement string field of the NAPTR record (`nil` in case `regexp` has value).

Example:

    resolver.submit_NAPTR "opensips.org"

Callback is called with argument:

    [#<EventMachine::Udns::RR_NAPTR:0x000000024e12c0 @order=10, @preference=10, @flags="s", @service="SIP+D2U", @regexp=nil, @replacement="_sip._udp.opensips.org">]


## Other Features

### Number of Active Queries

    resolver.active

`EM::Udns::Resolver#active` returns the number of pending queries of `resolver` as a `Fixnum`.


### Cancelling a Pending Query

    resolver.cancel query

`EM::Udns::Resolver#cancel(query)` cancels `EM::Udns::Query` given as argument so no callback/errback would be called upon query completion.


## Acknowledgement

Many thanks to Michael Tokarev (the author of [udns](http://www.corpit.ru/mjt/udns.html) C library) for all the help provided in udns maillist.