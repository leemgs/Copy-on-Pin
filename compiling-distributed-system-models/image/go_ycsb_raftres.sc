import $file.Utils
import $file.RaftResMagic

val ycsb = Utils.GoYCSB(binding = "raftres")(
  "-p", s"raftres.config=${RaftResMagic.configFile}")

println("loading YCSB")
ycsb.load()

Thread.sleep(5000)

println("running YCSB")
ycsb.run()
