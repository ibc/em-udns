require "mkmf"
require "rbconfig"
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

    case host_os = RbConfig::CONFIG["host_os"]
    
    # Linux.
    # Under Linux 64 bits udns must be compiled dynamically (also works for 32 bits):
    #   https://github.com/ibc/em-udns/issues#issue/1
    when /linux/i
      sys("make sharedlib")
      sys("ar r libudns.a *.lo")

    # BSD.
    # TODO: Not tested. Let's try same as under Linux.
    when /bsd/i
      sys("make sharedlib")
      sys("ar r libudns.a *.lo")

    # Solaris.
    # TODO: Not tested. Let's try same as under Linux.
    when /solaris/i
      sys("make sharedlib")
      sys("ar r libudns.a *.lo")
      
    # Mac OSX.
    # TODO: https://github.com/ibc/em-udns/issues#issue/1
    when /darwin|mac os/
      sys("make dylib")
      sys("ar r libudns.a *.lo")

    # Windows.
    # NOTE: udns doesn't work on Windows, but there is a port:
    #   http://network-research.org/udns.html
    when /mswin|msys|mingw32|windows/i
      raise "udns doesn't compile on Windows, you can try a Windows port: http://network-research.org/udns.html," \
            "report to https://github.com/ibc/em-udns/issues"
    
    # Other platforms. Error.
    else
      raise "unknown operating system: #{host_os.inspect}"
      
    end
      
    FileUtils.mv "libudns.a", "../"
    FileUtils.mv "udns.h", "../"
  end

  FileUtils.remove_dir(udns_path, force = true)
end

have_library("udns")  # == -ludns
create_makefile("em-udns/em_udns_ext")
