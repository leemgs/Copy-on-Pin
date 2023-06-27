import $file.Utils
import $file.RaftKVSMagic

os.proc(os.pwd / "pgo" / "systems" / "raftkvs" / "server",
  "-srvId", Utils.serverIdx + 1,
  "-c", RaftKVSMagic.configFile)
  .call(
    stdin = os.Inherit,
    stderr = os.Inherit,
    stdout = os.Inherit,
  )
