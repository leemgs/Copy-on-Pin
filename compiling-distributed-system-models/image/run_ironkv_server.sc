import $file.IronKVMagic
import $file.Utils

Utils.killPort(4000, "tcp")

println("running ironkv server")
os.proc("dotnet", os.pwd / "Ironclad" / "ironfleet" / "bin" / "IronRSLKVServer.dll", IronKVMagic.serviceInfo, IronKVMagic.serverPrivateInfo)
  .call(stdin = os.Inherit, stdout = os.Inherit, stderr = os.Inherit)
