import $file.Utils

lazy val configFile: os.Path = {
  val contents =
    s"""
      |numReplicas: ${Utils.hosts.size}
      |numClients: ${Utils.threadCount}
      |
      |debug: false
      |
      |clientRequestTimeout: 2s
      |
      |fd:
      |  pullInterval: 200ms
      |  timeout: 100ms
      |
      |mailboxes:
      |  receiveChanSize: 10000
      |  dialTimeout: 50ms
      |  readTimeout: 50ms
      |  writeTimeout: 50ms
      |
      |inputChanReadTimeout: 5ms
      |
      |replicas:${
      Utils.hosts.iterator
        .zipWithIndex
        .map { case (host, idx) =>
          s"""
             |  ${idx + 1}:
             |    reqMailboxAddr: "$host:8001"
             |    respMailboxAddr: "$host:8002"
             |    monitorAddr: "$host:8003"
             |""".stripMargin
        }
        .mkString
    }
       |clients:${
      (1 to Utils.threadCount).iterator
        .map { num =>
          s"""
             |  $num:
             |    reqMailboxAddr: "${Utils.clientHost}:${8000 + 2*num}"
             |    respMailboxAddr: "${Utils.clientHost}:${8000 + 2*num+1}"
             |""".stripMargin
        }
        .mkString
    }
       |""".stripMargin

  os.temp(contents = contents, suffix = ".yaml")
}

