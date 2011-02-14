require "mkmf"
require "fileutils"


def sys(cmd)
  puts "system command:  #{cmd}"
  unless ret = xsystem(cmd)
    raise "system command `#{cmd}' failed, please report to https://github.com/ibc/em-udns/issues"
  end
  ret
end


here = File.expand_path(File.dirname(__FILE__))
udns_tarball = Dir.glob("#{here}/udns-*.tar.gz").first
udns_path = File.basename(udns_tarball, ".tar.gz")

Dir.chdir(here) do
  sys("tar zxf #{udns_tarball}")

  Dir.chdir(udns_path) do
    sys("./configure")
    # udns must be compiled dynamically or it would fail under Linux 64 bits.
    # See https://github.com/ibc/em-udns/issues#issue/1
    sys("make sharedlib")
    sys("ar r libudns.a *.lo")

    FileUtils.mv "libudns.a", "../"
    FileUtils.mv "udns.h", "../"
  end

  FileUtils.remove_dir(udns_path, force = true)
end

have_library("udns")  # == -ludns
create_makefile("em_udns_ext")
