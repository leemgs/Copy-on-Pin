import $file.Utils

lazy val configFile: os.Path = {
  val contents =
    s"""
       |numServers: ${Utils.hosts.size}
       |numClients: ${Utils.threadCount}
       |
       |debug: false
       |
       |clientRequestTimeout: 1s
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
       |leaderElection:
       |  timeout: 150ms
       |  timeoutOffset: 150ms
       |
       |appendEntriesSendInterval: 2ms
       |
       |sharedResourceTimeout: 1ms
       |
       |inputChanReadTimeout: 5ms
       |
       |servers:${
      Utils.hosts.iterator
        .zipWithIndex
        .map { case (host, idx) =>
          s"""
             |  ${idx + 1}:
             |    raftAddr: "$host:8000"
             |    kvAddr: "$host:9000"
             |    monitorAddr: "$host:10000"
             |""".stripMargin
        }
        .mkString
    }
       |clients:${
      (1 to Utils.threadCount).iterator
        .map { num =>
          s"""
             |  $num:
             |    mailboxAddr: "${Utils.clientHost}:${8000 + num}"
             |""".stripMargin
        }
        .mkString
    }
       |""".stripMargin

  os.temp(contents = contents, suffix = ".yaml")
}
