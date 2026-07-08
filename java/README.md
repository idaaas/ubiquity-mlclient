# Java ML Client

## Use prebuilt jar

You can use prebuilt jar available on github package manager.

### Using maven

#### pom.xml

In your pom.XML, add the following in repositories section:
<details>
    <summary>Expand to see</summary>

```xml
<repositories>
  <repository>
    <id>github-repo</id>
    <url>https://maven.pkg.github.com/idaaas/ubiquity-mlclient</url>
  </repository>

  <!-- other repositories -->
</repositories>
```
and the following in repositories section:
```xml
<dependencies>
  <dependency>
    <groupId>com.idaaas.ubuntu-22-java-17</groupId>
    <artifactId>ubiquity-mlclient</artifactId>
    <version>v1.0.1</version>
  </dependency>

  <!-- other dependencies -->
</dependencies>
```
Choose groupId com.idaaas.*my_os_and_java_version* according to your os and your java install.  
Select last version in field *\<version>*
</details>

#### settings.xml

Setup your **github PAT** in settings.xml as follow:
<details>
    <summary>Expand to see</summary>

```xml
<settings xmlns="http://maven.apache.org/SETTINGS/1.0.0"
  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
  xsi:schemaLocation="http://maven.apache.org/SETTINGS/1.0.0
                      https://maven.apache.org/xsd/settings-1.0.0.xsd">

  <servers>
    <server>
      <id>github-repo</id>
      <configuration>
        <httpHeaders>
          <property>
            <name>Authorization</name>
            <value>Bearer ${env.TOKEN}</value>
          </property>
        </httpHeaders>
      </configuration>
    </server>
  </servers>
</settings>
```
You can replace *${env.TOKEN}* by your github PAT (Private Access Token) with registry access.


You can either place settings.xml in **~/.m2** or add **-s=my/path/to/settings.xml** in your install command line
</details>

#### build your project

build your project as usual with maven and ommit -s param if your settings.xml is in ~/.m2

```bash
TOKEN=<yourPAT> mvn install -s=my/path/to/settings.xml
```

## Running Examples

Java examples are provided in a docker to quickly run scenarios:
```bash
earthly +examples --lang=java --os=ubuntu:22.04
docker run java-examples ai.ubiquity.samples.lineitem.Example1 API_TOKEN CLUSTER_ID NUM_QUERIES
```
NUM_QUERIES is optional and 500000 by default.

## Sample Maven Project

You can find a sample maven project in ubiquity-mlclient/java/maven.  
To build it:
```bash
cd java/maven
TOKEN=githubPAT mvn install -s=settings.xml
mvn exec:java
```

## Build from sources

```bash
earthly +build --lang=java --os=ubuntu:22.04
ls local-ouput/java/ubuntu:22.04/java-17
# ubiquity-mlclient.jar: the jar to add to the classpath
```

You can select your Java release with the `--release` option, that will be passed to javac as is:
```bash
earthly +build --lang=java --os=ubuntu:22.04 --release=8
ls local-ouput/java/ubuntu:22.04/java-8
# ubiquity-mlclient.jar: the jar for Java 8 to add to the classpath
```

### SBT

Add the jar as a dependency in your `build.sbt`:
```scala
Compile / unmanagedJars += baseDirectory.value / "ubiquity-mlclient.jar"
```


