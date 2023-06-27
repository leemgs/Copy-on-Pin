import scala.collection.View
import upickle.default._

def linesMatching[T](dir: os.Path)(fn: PartialFunction[String,T]): View[T] =
  os.read.lines(dir / "results.txt")
    .view
    .collect(fn)

val GoYCSBReadThroughputPattern = raw"READ .*OPS: (\d+(?:\.\d+)?).*".r
val GoYCSBUpdateThroughputPattern = raw"UPDATE .*OPS: (\d+(?:\.\d+)?).*".r

def tryReadGoYCSBThroughput(dir: os.Path): Option[String] = {
  List(GoYCSBReadThroughputPattern, GoYCSBUpdateThroughputPattern).flatMap { Pattern =>
    linesMatching(dir) {
      case Pattern(throughput) => throughput
    }
      .lastOption
      .map(_.toFloat)
  }
    .reduceOption(_ + _)
    .map(_.toString)
}

val JavaYCSBThroughputPattern = raw"\[OVERALL\], Throughput\(ops\/sec\), (\d+(?:\.\d+)?)".r

def tryReadJavaYCSBThroughput(dir: os.Path): Option[String] =
  linesMatching(dir) {
    case JavaYCSBThroughputPattern(throughput) => throughput
  }.lastOption

def tryReadThroughput(dir: os.Path): Option[String] =
  tryReadJavaYCSBThroughput(dir) match {
    case Some(value) => Some(value)
    case None => tryReadGoYCSBThroughput(dir)
  }

def calcLatency(latencies: List[Float], opCounts: List[Float]): Option[Float] = {
  val opSum = opCounts.reduceOption(_ + _)
  opSum match {
    case None => None
    case Some(s) =>
      val weights = opCounts.map(_ / s)
      Some(latencies.zip(weights).foldLeft(0.0f) {
        case (cum, (x, w)) => cum + x*w
      })
  }
}

val JavaYCSBReadOpCntPattern = raw"\[READ\], Operations, (\d+(?:\.\d+)?)".r
val JavaYCSBUpdateOpCntPattern = raw"\[UPDATE\], Operations, (\d+(?:\.\d+)?)".r

val JavaYCSBReadLatencyPattern = raw"\[READ\], AverageLatency\(us\), (\d+(?:\.\d+)?)".r
val JavaYCSBUpdateLatencyPattern = raw"\[UPDATE\], AverageLatency\(us\), (\d+(?:\.\d+)?)".r

def tryReadJavaYCSBLatency(dir: os.Path): Option[String] = {
  val opCounts = List(JavaYCSBReadOpCntPattern, JavaYCSBUpdateOpCntPattern).flatMap { Pattern =>
    linesMatching(dir) {
      case Pattern(latency) => latency
    }
      .lastOption
      .map(_.toFloat)
  }
  val latencies = List(JavaYCSBReadLatencyPattern, JavaYCSBUpdateLatencyPattern).flatMap { Pattern =>
    linesMatching(dir) {
      case Pattern(latency) => latency
    }
      .lastOption
      .map(_.toFloat)
  }
  calcLatency(latencies, opCounts).map(_.toString)
}

val GoYCSBReadOpCntPattern = raw"READ .*Count: (\d+(?:\.\d+)?).*".r
val GoYCSBUpdateOpCntPattern = raw"UPDATE .*Count: (\d+(?:\.\d+)?).*".r

val GoYCSBReadLatencyPattern = raw"READ .*Avg\(us\): (\d+(?:\.\d+)?).*".r
val GoYCSBUpdateLatencyPattern = raw"UPDATE .*Avg\(us\): (\d+(?:\.\d+)?).*".r

def tryReadGoYCSBLatency(dir: os.Path): Option[String] = {
  val opCounts = List(GoYCSBReadOpCntPattern, GoYCSBUpdateOpCntPattern).flatMap { Pattern =>
    linesMatching(dir) {
      case Pattern(latency) => latency
    }
      .lastOption
      .map(_.toFloat)
  }
  val latencies = List(GoYCSBReadLatencyPattern, GoYCSBUpdateLatencyPattern).flatMap { Pattern =>
    linesMatching(dir) {
      case Pattern(latency) => latency
    }
      .lastOption
      .map(_.toFloat)
  }
  calcLatency(latencies, opCounts).map(_.toString)
}

def tryReadLatency(dir: os.Path): Option[String] =
  tryReadJavaYCSBLatency(dir) match {
    case Some(value) => Some(value)
    case None => tryReadGoYCSBLatency(dir)
  }

val FailedPattern = raw"\[\w+-FAILED\], Operations, (\d+)".r

def readErrors(dir: os.Path): Int =
  linesMatching(dir) {
    case FailedPattern(count) => count.toInt
  }
    .sum

val resultDirs = os.list(os.pwd / "results").filter(dir => os.isFile(dir / "results.txt")).toList
val configs: List[Map[String,String]] = resultDirs.map { dir =>
  val data = ujson.read(os.read.stream(dir / "config.json"), trace = true)
  val name = data("name").str
  val repeatIdx = data("repeatIdx").num
  val serverCount = data("serverCount").num
  val config = read[Map[String,String]](data("config"), trace = true)
  val throughput = tryReadThroughput(dir)
  val latency = tryReadLatency(dir)
  val errors = readErrors(dir)

  (Iterator(
    "name" -> name,
    "repeatIdx" -> repeatIdx.toInt.toString,
    "serverCount" -> serverCount.toInt.toString,
  ) ++
    config.iterator ++
    throughput.iterator.map("throughput" -> _) ++
    latency.iterator.map("latency" -> _) ++
    Iterator.single("errors" -> errors.toString))
    .toMap
}

val resultKeys = configs.iterator
  .map(_.keysIterator)
  .foldLeft(Set.empty[String])(_ ++ _)
  .toList
  .sorted

def escapeField(field: String): String =
  s"\"${field.flatMap {
    case '"' => "\"\""
    case ch => ch.toString
  }}\""

// print header
print(resultKeys.iterator
  .map(escapeField)
  .mkString("", ",", "\r\n"))

configs.foreach { config =>
  print(resultKeys.iterator.map(config.get)
    .map(_.getOrElse(""))
    .map(escapeField)
    .mkString("", ",", "\r\n"))
}
