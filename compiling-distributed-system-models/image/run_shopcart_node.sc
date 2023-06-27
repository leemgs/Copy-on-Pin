import $file.Utils
import $file.ShopcartMagic

os.proc(os.pwd / "pgo" / "systems" / "shopcart" / "node",
  "-nid", Utils.serverIdx + 1,
  "-c", ShopcartMagic.configFile)
  .call(
    stdin = os.Inherit,
    stderr = os.Inherit,
    stdout = os.Inherit,
  )