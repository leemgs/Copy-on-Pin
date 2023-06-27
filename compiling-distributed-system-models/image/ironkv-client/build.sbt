ThisBuild / version := "0.1.0-SNAPSHOT"

ThisBuild / scalaVersion := "2.13.8"

lazy val root = (project in file("."))
  .settings(
    name := "ironkv-client",
    organization := "com.github.fhackett",

    libraryDependencies += "com.lihaoyi" %% "os-lib" % "0.8.1",
    libraryDependencies += "com.lihaoyi" %% "upickle" % "2.0.0",
  )
