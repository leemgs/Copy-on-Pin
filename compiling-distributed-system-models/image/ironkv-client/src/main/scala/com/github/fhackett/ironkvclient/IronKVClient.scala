package com.github.fhackett.ironkvclient

import upickle.default._

import java.net.{InetSocketAddress, SocketException}
import java.nio.ByteBuffer
import java.nio.channels.{AsynchronousCloseException, SocketChannel}
import java.util.concurrent.{SynchronousQueue, TimeUnit}
import java.util.{Base64, UUID}
import scala.annotation.tailrec
import scala.collection.mutable
import scala.util.{Failure, Random, Success, Try, Using}

final class IronKVClient private (configFile: os.Path, timeout: Int, debug: Boolean) extends AutoCloseable {
  @inline
  private def println(line: =>String): Unit =
    if(debug) Predef.println(line)

  private val config =
    read[IronKVClient.RootConfig](os.read.stream(configFile), trace = true)
  require(!config.UseSsl)

  private val clientId: UUID = UUID.randomUUID()

  private val endpointAddrs = config.Servers.map { server =>
    new InetSocketAddress(server.HostNameOrAddress, server.Port)
  }

  private def asHex(bytes: Iterable[Byte]): String = {
    val builder = new mutable.StringBuilder
    bytes.foreach { b =>
      builder ++= String.format("%02x", b)
    }
    builder.result().grouped(16).map(_.grouped(4).mkString(" ")).mkString(" | ")
  }

  private def bufferBytes(buf: ByteBuffer): Array[Byte] = {
    buf.mark()
    val result = Array.fill(buf.remaining())(buf.get())
    buf.reset()
    result
  }

  private object withTimeout {
    sealed abstract class Update
    final case object DoneOp extends Update
    final case class BeginOp(channel: SocketChannel) extends Update
    final case object EndOp extends Update

    private val queue = new SynchronousQueue[Update]()
    private val thread = new Thread(() => {
      @tailrec
      def impl(): Unit =
        queue.take() match {
          case DoneOp => ()
          case BeginOp(channel) =>
            queue.poll(timeout, TimeUnit.MILLISECONDS) match {
              case null =>
                channel.close()
                // drain the end op
                queue.take() match {
                  case EndOp => impl()
                  case _ => ???
                }
              case EndOp => impl()
              case _ => ???
            }
          case _ => ???
        }

      impl()
    })
    thread.start()

    private var taskRunning: Boolean = false
    def apply[T](channel: SocketChannel)(fn: =>T): T = {
      taskRunning = true
      queue.put(BeginOp(channel))
      try {
        fn
      } finally {
        queue.put(EndOp)
        taskRunning = false
      }
    }

    def close(): Unit = {
      assert(!taskRunning)
      queue.put(DoneOp)
      thread.join()
    }
  }

  private implicit final class SocketChannelHelper(channel: SocketChannel) {
    def readAll(buffer: ByteBuffer): Unit = {
      val bytesExpected = buffer.remaining()
      val bytesRead = Iterator.continually {
        withTimeout(channel) {
          channel.read(buffer)
        }
      }
        .scanLeft(0)(_ + _)
        .find(_ == bytesExpected)
        .get

      assert(bytesRead == bytesExpected, s"read $bytesRead, but expected $bytesExpected")
      assert(buffer.remaining() == 0, s"buffer had non-zero bytes remaining ${buffer.remaining()}")
    }

    def writeAll(buffer: ByteBuffer): Unit = {
      val bytesExpected = buffer.remaining()
      val bytesWritten = Iterator.continually {
        withTimeout(channel) {
          channel.write(buffer)
        }
      }
        .scanLeft(0)(_ + _)
        .find(_ == bytesExpected)
        .get

      assert(bytesWritten == bytesExpected, s"wrote $bytesWritten, but expected $bytesExpected")
      assert(buffer.remaining() == 0, s"buffer had non-zero bytes remaining ${buffer.remaining()}")
    }
  }

  private object socket {
    private val random = new Random()
    private var channel: Option[SocketChannel] = None
    private var channelRemote: Option[String] = None

    private val lenBuffer = ByteBuffer
      .allocate(8)

    private def sendImpl(channel: SocketChannel, bufs: Array[ByteBuffer], lenSize: Int = 8): Try[SocketChannel] =
      Try {
        val totalRemaining = bufs.iterator.map(_.remaining()).sum
        lenBuffer
          .clear()
          .limit(lenSize)
        if(lenSize == 4) {
          lenBuffer.putInt(totalRemaining)
        } else {
          lenBuffer.putLong(totalRemaining)
        }
        lenBuffer.flip()

        val expectedBytesWritten = lenBuffer.remaining() + totalRemaining
        val sentBytes = (Iterator.single(lenBuffer) ++ bufs.iterator).map(bufferBytes).reduce(_ ++ _)
        println(s"send ${asHex(sentBytes)}")
        channel.writeAll(lenBuffer)
        bufs.foreach(channel.writeAll)
        println("sent")

        channel
      }

    private def endpointsIter: Iterator[InetSocketAddress] = {
      val startIdx = random.between(minInclusive = 0, maxExclusive = endpointAddrs.length)
      endpointAddrs.iterator.drop(startIdx) ++
        endpointAddrs.iterator.take(startIdx)
    }

    private def findChannel(): Try[SocketChannel] =
      channel
        .map(Success(_))
        .getOrElse {
          Try {
            var lastErr: Option[Throwable] = None

            val channelOpt: Option[SocketChannel] = endpointsIter
              .collectFirst(Function.unlift { addr =>
                val result = Try {
                  println(s"try connecting to $addr")
                  val channel = SocketChannel.open()
                  channel.socket().connect(addr, timeout)
                  channel
                }
                  .flatMap(sendImpl(_, Array(ByteBuffer.wrap(clientId.toString.getBytes))))


                result.failed.foreach { err =>
                  println(s"connection failed with ${err.getMessage}")
                  lastErr = Some(err)
                }

                result.toOption
              })

            if (channelOpt.isEmpty) {
              channel = None
              channelRemote = None
              throw new AssertionError("could not connect to any endpoints", lastErr.get)
            }

            channel = channelOpt
            channelRemote = Some(channel.get.getRemoteAddress.toString)
            channelOpt.get
          }
        }

    def send(bufs: Array[ByteBuffer]): Try[Unit] =
      findChannel()
        .flatMap(sendImpl(_, bufs))
        .map(_ => ())

    private var responseBuffer: ByteBuffer = ByteBuffer.allocate(64)

    private def recvImpl(channel: SocketChannel): Try[ByteBuffer] =
      Try {
        lenBuffer.clear()
        println("try recv")
        channel.readAll(lenBuffer)
        val responseLen = lenBuffer.flip().getLong()
        println(s"rlen $responseLen")

        if (responseBuffer.capacity() < responseLen) {
          if (responseBuffer.capacity() * 2 < responseLen) {
            responseBuffer = ByteBuffer.allocate(responseLen.toInt)
          } else {
            responseBuffer = ByteBuffer.allocate(responseBuffer.capacity() * 2)
          }
        }
        responseBuffer
          .clear()
          .limit(responseLen.toInt)

        channel.readAll(responseBuffer)

        responseBuffer.flip()
        assert(responseBuffer.remaining() == responseLen, s"expected $responseLen bytes in buffer, got ${responseBuffer.remaining()}")
        println(s"recv ${asHex(bufferBytes(responseBuffer))}")
        assert(responseBuffer.remaining() == responseLen)
        responseBuffer
      }

    def recv(): Try[ByteBuffer] =
      Try {
        assert(channel.nonEmpty)
        channel.get
      }
        .flatMap(recvImpl)

    def drop(): Unit = {
      channel
        .filter(_.isOpen)
        .foreach { chan =>
          chan.close()
        }
      channelRemote = None
      channel = None
    }

    def currentRemote: String =
      channelRemote.getOrElse("<socket missing>")
  }

  private var requestId: Long = 0

  private val requestMetaBuffer = ByteBuffer.allocate(8 * 3)

  private def performRequest[T](req: IronKVClient.KVRequest)(fn: PartialFunction[IronKVClient.KVReply,Try[T]]): Try[T] = {
    val msg = req.write()
    val currentRequestId = requestId
    requestId += 1
    Iterator.continually {
      requestMetaBuffer
        .clear()
        .putLong(0)
        .putLong(currentRequestId)
        .putLong(msg.length)
        .flip()

      socket.send(Array(requestMetaBuffer, ByteBuffer.wrap(msg)))
        .flatMap { _ =>
          // keep receiving until we see the right sequence number
          // any actual problems and we'll escape with an exception
          Iterator.continually(socket.recv())
            .find {
              case Failure(_) => true
              case Success(replyBuf) =>
                val msgType = replyBuf.getLong
                assert(msgType == 6)
                val seqNumber = replyBuf.getLong
                val replyLength = replyBuf.getLong
                assert(replyLength == replyBuf.remaining())

                seqNumber == currentRequestId
            }
            .get
        }
        .map(IronKVClient.KVReply.parser)
        .flatMap(fn)
    }
      .find {
        case Success(_) => true
        case Failure(err: SocketException) =>
          err.printStackTrace()
          socket.drop()
          false
        case Failure(_: AsynchronousCloseException) =>
          println(s"${socket.currentRemote} timeout, dropping connection")
          socket.drop()
          false
        case Failure(_) => true
      }
      .get
  }

  def put(key: Array[Byte], value: Array[Byte]): Try[Unit] =
    performRequest(IronKVClient.KVSetRequest(key = key, value = value)) {
      case IronKVClient.KVSetSuccessReply() => Success(())
      case IronKVClient.KVSetFailureReply() => Failure(IronKVClient.PutFailedError())
    }

  def get(key: Array[Byte]): Try[Array[Byte]] =
    performRequest(IronKVClient.KVGetRequest(key = key)) {
      case IronKVClient.KVGetFoundReply(value) => Success(value)
      case IronKVClient.KVGetUnfoundReply() => Failure(IronKVClient.GetFailedError())
    }

  def del(key: Array[Byte]): Try[Unit] =
    performRequest(IronKVClient.KVDeleteRequest(key = key)) {
      case IronKVClient.KVDeleteFoundReply() => Success(())
      case IronKVClient.KVDeleteUnfoundReply() => Failure(IronKVClient.DeleteFailedError())
    }

  override def close(): Unit = {
    socket.drop()
    withTimeout.close()
  }
}

object IronKVClient {
  def fromConfig(config: IronKVClientConfig): IronKVClient =
    new IronKVClient(
      configFile = config.configFile,
      timeout = config.timeout,
      debug = config.debug)

  final case class PutFailedError() extends RuntimeException
  final case class GetFailedError() extends RuntimeException
  final case class DeleteFailedError() extends RuntimeException

  final case class RootConfig(FriendlyName: String,
                              ServiceType: String,
                              UseSsl: Boolean,
                              Servers: List[ServersConfig])
  object RootConfig {
    implicit val rw: ReadWriter[RootConfig] = macroRW
  }

  final case class ServersConfig(FriendlyName: String,
                                 PublicKey: String,
                                 HostNameOrAddress: String,
                                 Port: Int)
  object ServersConfig {
    implicit val rw: ReadWriter[ServersConfig] = macroRW
  }

  abstract class KVRecordParser[T <: KVRecord] extends (ByteBuffer => T) {
    def apply(buf: ByteBuffer): T
  }
  object KVRecordParser {
    def apply[T <: KVRecord](fn: ByteBuffer => T): KVRecordParser[T] = fn(_)
  }

  final case class InvalidTagError(tag: Int, validTags: List[Int]) extends RuntimeException(
    s"invalid tag $tag; possible tags: ${validTags.mkString(", ")}")

  def kvTaggedRecordParser[T <: KVRecord](map: Map[Int,KVRecordParser[T]]): KVRecordParser[T] =
    KVRecordParser { buf =>
      val tag = buf.getInt
      map.get(tag) match {
        case Some(parser) => parser(buf)
        case _ => throw InvalidTagError(tag = tag, validTags = map.keys.toList.sorted)
      }
    }

  def kvByteFieldsParser[T <: KVRecord](fieldCount: Int)(instantiate: PartialFunction[List[Array[Byte]],T]): KVRecordParser[T] =
    KVRecordParser { buf =>
      val result = instantiate.applyOrElse({
        Iterator
          .fill(fieldCount) {
            val len = buf.getInt
            val decoded = Base64.getDecoder.decode(buf.slice.limit(len))
            buf.position(buf.position() + len)
            val array = Array.ofDim[Byte](decoded.remaining())
            decoded.get(array)
            array
          }
          .toList
      }, { (_: List[Array[Byte]]) =>
        throw new RuntimeException("programmer error: byte fields parser data rejected by instantiate function")
      })

      assert(buf.remaining() == 0)
      result
    }

  def kvConstantParser[T <: KVRecord](factory: =>T): KVRecordParser[T] =
    KVRecordParser { buf =>
      assert(buf.remaining() == 0)
      factory
    }

  sealed abstract class KVRecord(val tag: Int) {
    def size: Int = 0

    def write(): Array[Byte] = {
      val buf = ByteBuffer.allocate(4 + size)
      buf.putInt(tag)
      writeFields(buf)
      buf.array()
    }

    def writeFields(buf: ByteBuffer): Unit = ()
  }

  sealed abstract class KVRequest(tag: Int) extends KVRecord(tag = tag) {
    val parser: KVRecordParser[KVRequest] = kvTaggedRecordParser(Map(
      1 -> kvByteFieldsParser(fieldCount = 1) { case List(key) => KVGetRequest(key) },
      2 -> kvByteFieldsParser(fieldCount = 2) { case List(key, value) => KVSetRequest(key, value) },
      3 -> kvByteFieldsParser(fieldCount = 3) { case List(key) => KVDeleteRequest(key) },
    ))
  }

  final case class KVGetRequest(key: Array[Byte]) extends KVRequest(tag = 1) {
    val encodedKey: Array[Byte] = Base64.getEncoder.encode(key)
    override def size: Int = 4 + encodedKey.length

    override def writeFields(buf: ByteBuffer): Unit = {
      buf.putInt(encodedKey.length)
      buf.put(encodedKey)
    }
  }

  final case class KVSetRequest(key: Array[Byte], value: Array[Byte]) extends KVRequest(tag = 2) {
    val encodedKey: Array[Byte] = Base64.getEncoder.encode(key)
    val encodedValue: Array[Byte] = Base64.getEncoder.encode(value)
    override def size: Int = 4 + encodedKey.length + 4 + encodedValue.length

    override def writeFields(buf: ByteBuffer): Unit = {
      buf.putInt(encodedKey.length)
      buf.put(encodedKey)
      buf.putInt(encodedValue.length)
      buf.put(encodedValue)
    }
  }

  final case class KVDeleteRequest(key: Array[Byte]) extends KVRequest(tag = 3) {
    val encodedKey: Array[Byte] = Base64.getEncoder.encode(key)
    override def size: Int = 4 + encodedKey.length

    override def writeFields(buf: ByteBuffer): Unit = {
      buf.putInt(encodedKey.length)
      buf.put(encodedKey)
    }
  }

  sealed abstract class KVReply(tag: Int) extends KVRecord(tag = tag)
  object KVReply {
    val parser: KVRecordParser[KVReply] = kvTaggedRecordParser(Map(
      1 -> kvByteFieldsParser(fieldCount = 1) { case List(value) => KVGetFoundReply(value) },
      2 -> kvConstantParser(KVGetUnfoundReply()),
      3 -> kvConstantParser(KVSetSuccessReply()),
      4 -> kvConstantParser(KVSetFailureReply()),
      5 -> kvConstantParser(KVDeleteFoundReply()),
      6 -> kvConstantParser(KVDeleteUnfoundReply()),
    ))
  }

  final case class KVGetFoundReply(value: Array[Byte]) extends KVReply(tag = 1) {
    val encodedValue: Array[Byte] = Base64.getEncoder.encode(value)
    override def size: Int = 4 + encodedValue.length

    override def writeFields(buf: ByteBuffer): Unit = {
      buf.putInt(encodedValue.length)
      buf.put(encodedValue)
    }
  }

  final case class KVGetUnfoundReply() extends KVReply(tag = 2)

  final case class KVSetSuccessReply() extends KVReply(tag = 3)

  final case class KVSetFailureReply() extends KVReply(tag = 4)

  final case class KVDeleteFoundReply() extends KVReply(tag = 5)

  final case class KVDeleteUnfoundReply() extends KVReply(tag = 6)

  // --- testing routine lives here ---
  def main(args: Array[String]): Unit = {
    Using.resource(IronKVClientConfig.fromFile("/home/finn/programming/Ironclad/ironfleet/certs/IronKV.IronRSLKV.service.txt").build()) { client =>
      (for {
        _ <- client.put(Array(0xCA, 0xFE, 0xBE, 0xEF).map(_.byteValue), Array(0xBA, 0xBE, 0xDE).map(_.byteValue))
        value <- client.get(Array(0xCA, 0xFE, 0xBE, 0xEF).map(_.byteValue))
        _ = println(s"result ${client.asHex(value)}")
        value <- client.get(Array(0xCA, 0xFE, 0xBE, 0xEF).map(_.byteValue))
        _ = println(s"result ${client.asHex(value)}")
        _ <- client.put(Array(0xCA, 0xFE, 0xBE, 0xEF).map(_.byteValue), Array(0xBA, 0xBE, 0xDE).map(_.byteValue))
        value <- client.get(Array(0xCA, 0xFE, 0xBE, 0xEF).map(_.byteValue))
        _ = println(s"result ${client.asHex(value)}")
        _ <- client.del(Array(0xCA, 0xFE, 0xBE, 0xEF).map(_.byteValue))
        _ <- client.get(Array(0xCA, 0xFE, 0xBE, 0xEF).map(_.byteValue)).recover {
          case GetFailedError() => println("after delete, get fails")
        }
        _ <- client.del(Array(0xCA, 0xFE, 0xBE, 0xEF).map(_.byteValue)).recover {
          case DeleteFailedError() => println("after delete, delete also fails")
        }
        _ <- client.put(Array(0xCA, 0xFE, 0xBE, 0xEF).map(_.byteValue), Array(0xBA, 0xBE, 0xDE).map(_.byteValue))
        value <- client.get(Array(0xCA, 0xFE, 0xBE, 0xEF).map(_.byteValue))
        _ = println(s"result ${client.asHex(value)}")
      } yield ())
        .get
    }
  }
}
