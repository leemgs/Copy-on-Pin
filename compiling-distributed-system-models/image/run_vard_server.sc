import $file.Utils

Utils.killPort(5000, "udp")

val dataDir = os.temp.dir(prefix = "vard")

val nodes = Utils.hosts.iterator
  .zipWithIndex
  .flatMap {
    case (host, idx) =>
      List("-node", s"$idx,$host:5000")
  }
  .toList

os.proc(os.pwd / "verdi-raft" / "extraction" / "vard" / "vard.native",
  nodes,
  "-me", Utils.serverIdx,
  "-port", 4000,
  "-dbpath", dataDir)
  .call(
    stdin = os.Inherit,
    stderr = os.Inherit,
    stdout = os.Inherit)
