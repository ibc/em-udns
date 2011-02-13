require "mkmf"

# TODO: In case udns is included within em-udns sources, this code must be removed.
#   see: https://github.com/ibc/em-udns/issues#issue/1
unless have_library("udns")  # == -ludns
  raise "libudns-dev is required"
end

create_makefile("em_udns_ext")