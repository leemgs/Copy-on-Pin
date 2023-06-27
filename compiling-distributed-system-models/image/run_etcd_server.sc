import $file.Utils

Utils.killPort(2380, "tcp")
Utils.killPort(2379, "tcp")

val dataDir = os.temp.dir(prefix = "etcd")

val cluster: String = Utils.hosts.iterator
  .zipWithIndex
  .map {
      case (host, idx) =>
      s"machine-${idx + 1}=http://$host:2380"
  }
  .mkString(",")

os.proc(os.pwd / "etcd-v3.5.4-linux-amd64" / "etcd",
    "--data-dir", s"$dataDir",
    "--name", s"machine-${Utils.serverNum}",
    "--initial-advertise-peer-urls", s"http://${Utils.ownHost}:2380",
    "--listen-peer-urls", s"http://${Utils.ownHost}:2380",
    "--advertise-client-urls", s"http://${Utils.ownHost}:2379",
    "--listen-client-urls", s"http://${Utils.ownHost}:2379",
    "--initial-cluster", cluster,
    "--initial-cluster-state", "new",
    "--initial-cluster-token", "token-01")
  .call(stdin = os.Inherit,
    stderr = os.Inherit,
    stdout = os.Inherit)