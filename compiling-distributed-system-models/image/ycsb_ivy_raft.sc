import $file.Utils

val endpoints = Utils.hosts.iterator
  .map(host => s"$host:4000")
  .mkString(",")

val ycsb = Utils.YCSB(binding = "vard")(
  "-p", s"vard.endpoints=$endpoints",
  "-p", "vard.ivy_mode=true")

println("load YCSB")
ycsb.load()

println("run YCSB")
ycsb.run(timeout = 60 * 1000 * 10) // 10 minute timeout
