import $file.Utils

lazy val configFile: os.Path = {
  val contents =
    s"""
       |numServers: ${Utils.hosts.size}
       |numClients: ${Utils.threadCount}
       |
       |debug: false
       |
       |persist: true
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
       |appendEntriesSendInterval: 5ms
       |
       |sharedResourceTimeout: 2ms
       |
       |inputChanReadTimeout: 5ms
       |
       |servers:${
      Utils.hosts.iterator
        .zipWithIndex
        .map { case (host, idx) =>
          s"""
             |  ${idx + 1}:
             |    mailboxAddr: "$host:8000"
             |    monitorAddr: "$host:9000"
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
