Gem::Specification.new do |spec|
  spec.name = "em-udns"
  spec.version = "0.1.3"
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
    ext/em-udns.c
    ext/em-udns.h
    ext/extconf.rb
    ext/udns-0.1.tar.gz
    test/test-em-udns.rb
  }
  spec.require_paths = ["lib"]
end
