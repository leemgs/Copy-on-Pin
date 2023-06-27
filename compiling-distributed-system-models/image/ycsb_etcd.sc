import $file.Utils

val endpoints = Utils.hosts.iterator
  .map(host => s"tcp://$host:2379")
  .mkString(",")

val ycsb = Utils.YCSB(binding = "etcd")(
    "-p", s"etcd.endpoints=$endpoints")

println("load YCSB")
ycsb.load()

println("run YCSB")
ycsb.run()
