import $file.Utils

val primary = Utils.hosts.head

val ycsb = Utils.GoYCSB(binding = "redis")(
  "-p", s"redis.addr=${primary}:6379",
  "-p", s"redis.numreplicas=${Utils.hosts.length - 1}",
)

println("loading YCSB")
ycsb.load()

Thread.sleep(5000)

println("running YCSB")
ycsb.run()
