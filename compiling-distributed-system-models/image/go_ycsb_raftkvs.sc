import $file.Utils
import $file.RaftKVSMagic

val ycsb = Utils.GoYCSB(binding = "raftkvs")(
  "-p", s"raftkvs.config=${RaftKVSMagic.configFile}",
  "-p", "ycsb.useints=true")

println("loading YCSB")
ycsb.load()

Thread.sleep(5000)

println("running YCSB")
ycsb.run()
