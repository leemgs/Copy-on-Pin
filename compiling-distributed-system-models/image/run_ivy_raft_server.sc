import $file.Utils

Utils.killPort(5000, "udp")
Utils.killPort(4000, "tcp")

os.proc(os.pwd / "tausigplan-pldi18-impl-6cee11b50570" / "evaluation" / "build" / "server",
  "--node-id", Utils.serverIdx,
  "--cluster", Utils.hosts.iterator.map(host => s"$host:5000").mkString(","),
  "--client-port", 4000,
  "--log",
)
  .call(
    stdin = os.Inherit,
    stderr = os.Inherit,
    stdout = os.Inherit,
  )
