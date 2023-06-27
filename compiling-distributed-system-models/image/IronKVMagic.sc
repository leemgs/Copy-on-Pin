import $file.Utils

lazy val hosts: List[(String,Int)] = Utils.hosts.map((_, 4000))

lazy val serviceInfo: os.Path = locally {
  val info = ujson.read(os.read.stream(os.pwd / "ironkv-certs" / "IronKV.IronRSLKV.service.txt"))

  info("Servers") = (info("Servers").arr.iterator zip hosts)
    .map {
      case (server, (host, port)) =>
        server("HostNameOrAddress") = host
        server("Port") = port
        server
    }
    .toList

  println(s"service info: ${info.render(indent = 2)}")
  os.temp(contents = ujson.write(info))
}

lazy val serverPrivateInfo: os.Path = locally {
  val info = ujson.read(os.read.stream(os.pwd / "ironkv-certs" / s"IronKV.IronRSLKV.server${Utils.serverIdx + 1}.private.txt"))

  info("HostNameOrAddress") = hosts(Utils.serverIdx)._1
  info("Port") = hosts(Utils.serverIdx)._2

  println(s"server private info: ${info.render(indent = 2)}")
  os.temp(contents = ujson.write(info))
}
