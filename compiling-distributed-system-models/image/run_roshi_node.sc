import $file.Utils

println("numServers", Utils.hosts.size, "serverIdx", Utils.serverIdx)

val redisInstances = Utils.hosts.iterator.map(host => host + ":6379")
  .mkString(";")

println("starting redis")
val redisProc = os.proc("redis-server",
  "--bind", Utils.ownHost,
  "--save", "\"\"",
  "--appendonly", "no")
  .spawn(
    stdin = os.Inherit,
    stderr = os.Inherit,
    stdout = os.Inherit,
  )

Thread.sleep(3000)

println("starting roshi server")
val roshiServer = os.proc(os.pwd / "roshi" / "roshi-server" / "roshi-server",
  "-redis.instances", Utils.ownHost + ":6379")
  .spawn(
    stdin = os.Inherit,
    stderr = os.Inherit,
    stdout = os.Inherit,
  )

var roshiWalker: os.SubProcess = _

if (Utils.serverIdx == 0) {
  println("starting roshi walker")
  roshiWalker = os.proc(os.pwd / "roshi" / "roshi-walker" / "roshi-walker",
    "-redis.instances", redisInstances,
    "-walk.interval", "50ms")
    .spawn(
      stdin = os.Inherit,
      stderr = os.Inherit,
      stdout = os.Inherit,
    )
}

Thread.sleep(5000)

println("starting roshi app")
val roshiApp = os.proc(os.pwd / "roshiapp" / "main",
  "-serverIdx", Utils.serverIdx,
  "-numNodes", Utils.hosts.size,
  "-roshiServer", "http://" + Utils.ownHost + ":6302",
  "-numRounds", "100")
  .spawn(
    stdin = os.Inherit,
    stderr = os.Inherit,
    stdout = os.Inherit,
  )

roshiApp.join()

roshiServer.destroy()

if (Utils.serverIdx == 0) {
  roshiWalker.destroy()
}

redisProc.destroy()
