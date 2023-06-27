import $file.Utils

val endpoints = Utils.hosts.iterator
  .map(host => s"$host:4000")
  .mkString(",")

val ycsb = Utils.YCSB(binding = "vard")(
  "-p", s"vard.endpoints=$endpoints",
  "-p", "vard.raw_utf8=true",
  "-s")

println("load YCSB")
ycsb.load()

println("run YCSB")
ycsb.run(timeout = 60 * 1000 * 20) // 20 minute timeout
