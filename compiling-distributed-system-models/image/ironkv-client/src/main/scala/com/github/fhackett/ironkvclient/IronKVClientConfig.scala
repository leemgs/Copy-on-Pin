package com.github.fhackett.ironkvclient

final case class IronKVClientConfig(configFile: os.Path, timeout: Int = 1000, debug: Boolean = false) {
  def withTimeout(timeout: Int): IronKVClientConfig =
    copy(timeout = timeout)

  def withDebug(debug: Boolean): IronKVClientConfig =
    copy(debug = debug)

  def build(): IronKVClient =
    IronKVClient.fromConfig(this)
}

object IronKVClientConfig {
  def fromFile(fileName: String): IronKVClientConfig =
    IronKVClientConfig(configFile = os.Path(fileName, os.pwd))
}
