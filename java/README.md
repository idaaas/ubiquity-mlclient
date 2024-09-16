# Java ML Client

```bash
earthly +java-bindings --os=ubuntu:22.04
ls local-ouput/java
# ubiquity-client.jar: the jar to add to the classpath
```

You can select your Java release with the `--release` option, that will be passed to javac as is:
```bash
earthly +java-bindings --os=ubuntu:22.04 --release=8
ls local-ouput/java
# ubiquity-client.jar: the jar for Java 8 to add to the classpath
```

## SBT

Add the jar as a dependency in your `build.sbt`:
```scala
Compile / unmanagedJars += baseDirectory.value / "ubiquity-client.jar"
```

## Running Examples

Java examples are provided in a docker to quickly run scenarios:
```bash
earthly +java-examples --os=ubuntu:22.04
docker run java-examples ai.ubiquity.samples.lineitem.Example1 API_TOKEN CLUSTER_ID
```
