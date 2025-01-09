name := "integration"
scalaVersion := "2.13.10"
javaOptions ++= Seq("--add-opens", "java.base/java.nio=ALL-UNNAMED", "--add-opens", "java.base/sun.nio.ch=ALL-UNNAMED")

Compile / unmanagedJars += baseDirectory.value / "../build/bin/ubiquity-mlclient.jar"
assembly / assemblyJarName := "assembly.jar"
assembly / target := file("target/")

testFrameworks += new TestFramework("munit.Framework")
testOptions += Tests.Argument(TestFrameworks.JUnit, "-v")

