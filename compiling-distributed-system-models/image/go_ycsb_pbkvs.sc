import $file.Utils
import $file.PBKVSMagic

val ycsb = Utils.GoYCSB(binding = "pbkvs")(
  "-p", s"pbkvs.config=${PBKVSMagic.configFile}")

println("loading YCSB")
ycsb.load()

Thread.sleep(5000)

println("running YCSB")
ycsb.run()
