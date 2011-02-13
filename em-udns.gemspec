Gem::Specification.new do |spec|
  spec.name = "em-udns"
  spec.version = "0.1.0"
  spec.date = Time.now
  spec.authors = ["Iñaki Baz Castillo"]
  spec.email = "ibc@aliax.net"
  spec.summary = "Async DNS resolver for EventMachine based on udns C library"
  spec.homepage = "https://github.com/ibc/em-udns"
  spec.description =
    "em-udns is an async DNS resolver for EventMachine based on udns C library. Having most of the core written in C, em-udns becomes very fast. It can resolve DNS A, AAAA, PTR, MX, TXT, SRV and NAPTR records, and can handle every kind of errors (domain/record not found, request timeout, malformed response...)."
  spec.extensions = ["ext/extconf.rb"]
  spec.required_ruby_version = ">= 1.8.7"
  spec.add_dependency "eventmachine"
  spec.files = %w{
    lib/em-udns.rb
    lib/em-udns/resolver.rb
    lib/em-udns/version.rb
    ext/em-udns.c
    ext/em-udns.h
    ext/extconf.rb
  }
# TODO: In case udns is included within em-udns sources, this files must be un-commented.
#   see: https://github.com/ibc/em-udns/issues#issue/1
#     ext/udns-0.1/config.h
#     ext/udns-0.1/getopt.c
#     ext/udns-0.1/inet_XtoX.c
#     ext/udns-0.1/udns_bl.c
#     ext/udns-0.1/udns_codes.c
#     ext/udns-0.1/udns_dn.c
#     ext/udns-0.1/udns_dntosp.c
#     ext/udns-0.1/udns.h
#     ext/udns-0.1/udns_init.c
#     ext/udns-0.1/udns_jran.c
#     ext/udns-0.1/udns_misc.c
#     ext/udns-0.1/udns_parse.c
#     ext/udns-0.1/udns_resolver.c
#     ext/udns-0.1/udns_rr_a.c
#     ext/udns-0.1/udns_rr_mx.c
#     ext/udns-0.1/udns_rr_naptr.c
#     ext/udns-0.1/udns_rr_ptr.c
#     ext/udns-0.1/udns_rr_srv.c
#     ext/udns-0.1/udns_rr_txt.c
#     ext/udns-0.1/udns_XtoX.c
  spec.require_paths = ["lib"]
end
