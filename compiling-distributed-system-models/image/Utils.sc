lazy val clientHost: String = sys.env("AZ_CLIENT_IP")
lazy val hosts: List[String] =
  sys.env("AZ_SERVER_IPS")
    .split(",")
    .iterator
    .toList

lazy val serverIdx: Int = sys.env("AZ_SERVER_IDX").toInt
lazy val serverNum: Int = serverIdx + 1

lazy val workload: String = sys.env("AZ_CONF_WORKLOAD")
lazy val threadCount: Int = sys.env("AZ_CONF_THREADCOUNT").toInt
lazy val operationCount: Int = sys.env.get("AZ_CONF_OPERATIONCOUNT").map(_.toInt).getOrElse(1000)

lazy val ownHost: String = hosts(serverIdx)

final case class YCSB(binding: String, workload: String = workload, threadCount: Int = threadCount)(config: os.Shellable*) {
  private def exec(cmd: String, timeout: Long = -1): Unit =
    os.proc("./bin/ycsb.sh", cmd, binding,
      "-P", s"workloads/$workload",
      "-p", s"threadcount=$threadCount",
      "-p", s"operationcount=$operationCount",
      config)
      .call(
        cwd = os.pwd / "YCSB",
        timeout = timeout,
        stdin = os.Inherit,
        stderr = os.Inherit,
        stdout = os.Inherit)

  def load(): Unit = exec(cmd = "load")

  def run(timeout: Long = 1000 * 60 * 10): Unit = // default timeout of 10 minutes
    exec(cmd = "run", timeout = timeout)
}

final case class GoYCSB(binding: String, workload: String = workload, threadCount: Int = threadCount)(config: os.Shellable*) {
  private def exec(cmd: String, timeout: Long = -1): Unit =
    os.proc("./bin/go-ycsb", cmd, binding,
      "-P", s"workloads/$workload",
      "-p", s"threadcount=$threadCount",
      "-p", s"operationcount=$operationCount",
      config)
      .call(
        cwd = os.pwd / "go-ycsb",
        timeout = timeout,
        stdin = os.Inherit,
        stderr = os.Inherit,
        stdout = os.Inherit)

  def load(): Unit = exec(cmd = "load")

  def run(timeout: Long = 1000 * 60 * 10): Unit = // default timeout of 10 minutes
    exec(cmd = "run", timeout = timeout)
}

def killPort(port: Int, proto: String): Unit =
  os.proc("fuser", "-k", s"$port/$proto").call(
    stdin = os.Inherit,
    stderr = os.Inherit,
    stdout = os.Inherit,
    check = false,
  )