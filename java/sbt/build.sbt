ThisBuild / version := "0.1"
ThisBuild / organization := "ai.ubiquity"

// deps
libraryDependencies += "com.squareup.okhttp3" % "okhttp" % "4.12.0"
libraryDependencies += "org.json" % "json" % "20231013"
libraryDependencies += "com.opencsv" % "opencsv" % "5.9"
Compile / unmanagedJars += baseDirectory.value / "ubiquity-mlclient.jar"

// assembly settings
// no cross-build for the fat jar. If you disable this it will be set in target/scala_something
assembly / target := target.value

assembly / assemblyJarName := "main.jar"

// no scala libs in final jar
assembly / assemblyOption ~= {
      _.withIncludeScala(false)
}

// shading spurious dependencies
ThisBuild / assemblyShadeRules := Seq(
  ShadeRule
    .keep("ai.ubiquity.samples.*", "ai.ubiquity.samples.utils.*", "ai.ubiquity.samples.lineitem.*", "ai.ubiquity.samples.probabilityestimator.*")
    .inProject
)

// needed but cannot work see https://github.com/sbt/sbt-assembly/issues/391
assembly / assemblyMergeStrategy := {
  case PathList("module-info.class") => MergeStrategy.discard
  case PathList("META-INF", "versions", "9", "module-info.class") => MergeStrategy.discard
  case PathList("META-INF", xs @ _*) => MergeStrategy.discard
  case x =>
    val oldStrategy = (ThisBuild / assemblyMergeStrategy).value
    oldStrategy(x)
}
