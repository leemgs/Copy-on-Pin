import $file.Utils

lazy val configFile: os.Path = {
  val contents =
    s"""
       |numRounds: 100
       |
       |broadcastInterval: 50ms
       |broadcastSize: 2
       |sendTimeout: 200ms
       |dialTimeout: 50ms
       |
       |peers:${
      Utils.hosts.iterator
        .zipWithIndex
        .map { case (host, idx) =>
          s"""
             |  ${idx + 1}: "$host:8000"
             |""".stripMargin
        }
        .mkString
    }
       |""".stripMargin

  os.temp(contents = contents, suffix = ".yaml")
}