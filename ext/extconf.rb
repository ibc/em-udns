require "mkmf"
require "fileutils"


here = File.expand_path(File.dirname(__FILE__))
udns_tarball = Dir.glob("#{here}/udns-*.tar.gz").first
udns_path = udns_tarball.gsub(".tar.gz", "")

Dir.chdir(here) do
  puts(cmd = "tar xzf #{udns_tarball} 2>&1")
  raise "'#{cmd}' failed" unless system(cmd)

  Dir.chdir(udns_path) do
    puts(cmd = "./configure 2>&1")
    raise "'#{cmd}' failed" unless system(cmd)

    puts(cmd = "make sharedlib 2>&1")
    raise "'#{cmd}' failed" unless system(cmd)

    puts(cmd = "ar r libudns.a *.lo 2>&1")
    raise "'#{cmd}' failed" unless system(cmd)

    FileUtils.mv "libudns.a", "../"
    FileUtils.mv "udns.h", "../"
  end

  FileUtils.remove_dir(udns_path, force = true)
end


have_library("udns")  # == -ludns
create_makefile("em_udns_ext")
