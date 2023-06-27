import $file.Utils
import $file.IronKVMagic

val ycsb = Utils.YCSB(binding = "ironkv")(
    "-p", "ironkv.timeout=5000",
    "-p", s"ironkv.config_file=${IronKVMagic.serviceInfo}")

println("load YCSB")
ycsb.load()

println("run YCSB")
ycsb.run()
