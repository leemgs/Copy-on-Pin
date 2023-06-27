import $file.Utils

println("numServers", Utils.hosts.size, "serverIdx", Utils.serverIdx)

Utils.killPort(6379, "tcp")

val primary = Utils.hosts.head

if (Utils.serverIdx == 0) {
  os.proc("redis-server",
    "--bind", Utils.ownHost,
    "--save", "\"\"",
    "--appendonly", "no")
    .call(
      stdin = os.Inherit,
      stderr = os.Inherit,
      stdout = os.Inherit,
    )
} else {
  os.proc("redis-server",
    "--bind", Utils.ownHost,
    "--save", "\"\"",
    "--appendonly", "no",
    "--replicaof", primary, "6379")
    .call(
      stdin = os.Inherit,
      stderr = os.Inherit,
      stdout = os.Inherit,
    )
}
