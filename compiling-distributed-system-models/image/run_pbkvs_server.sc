import $file.Utils
import $file.PBKVSMagic

os.proc(os.pwd / "pgo" / "systems" / "pbkvs" / "server",
  "-srvId", Utils.serverIdx + 1,
  "-c", PBKVSMagic.configFile)
  .call(
    stdin = os.Inherit,
    stderr = os.Inherit,
    stdout = os.Inherit,
  )