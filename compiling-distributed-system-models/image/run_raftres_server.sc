import $file.Utils
import $file.RaftResMagic

os.proc(os.pwd / "pgo" / "systems" / "raftres" / "server",
  "-srvId", Utils.serverIdx + 1,
  "-c", RaftResMagic.configFile)
  .call(
    stdin = os.Inherit,
    stderr = os.Inherit,
    stdout = os.Inherit,
  )