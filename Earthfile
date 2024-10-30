VERSION 0.8
FROM busybox

ARG --global --required os


all:
  BUILD +cpp-bindings
  BUILD +java-bindings

build:
  FROM +cmake-image
  CACHE ./build
  ARG type=Release
  ARG disableIdChecks=OFF
  COPY --keep-ts cpp .
  RUN cmake -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF -DBUILD_AERON_ARCHIVE_API=OFF -DCMAKE_BUILD_TYPE=$type -DDISABLE_ID_CHECKS=$disableIdChecks -S . -B build
  RUN cmake --build build --parallel $(nproc) --target ubiquity_client_static docs
  #RUN ctest
  RUN mkdir -p tmp \ # https://github.com/earthly/earthly/issues/788
    && cp build/src/libubiquity_client_static.a include/ubiquity_client/Ubiquity.hpp tmp \
    && cp -r build/docs/html tmp
  SAVE ARTIFACT tmp/*

cpp-bindings:
  COPY --dir +build/* local-output/
  SAVE ARTIFACT local-output AS LOCAL local-output/cpp

java-bindings:
  ARG release
  FROM +java-swig-image
  COPY +build/libubiquity_client_static.a +build/Ubiquity.hpp .
  COPY java/swig .
  RUN cmake -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DCMAKE_BUILD_TYPE=Release -S . -B build
  RUN cmake --build build --parallel $(nproc) --target jniUbiquityClient
  RUN mkdir -p jar-root/NATIVE && cp build/libjniUbiquityClient.so jar-root/NATIVE
  RUN javac -Werror -Xlint -d jar-root build/output/*.java $([ -z "$release" ] || echo --release $release)
  RUN jar cf ubiquity-client.jar -C jar-root .
  SAVE ARTIFACT ubiquity-client.jar AS LOCAL local-output/java/

java-examples-build:
  FROM +sbt-image
  COPY +java-bindings/ubiquity-client.jar .
  COPY java/sbt .
  RUN sbt assembly
  SAVE ARTIFACT target/main.jar

java-examples:
  FROM +base-image
  DO +JAVA_INSTALL
  COPY +java-examples-build/main.jar ./target
  ENTRYPOINT ["java","-cp","target/main.jar"]
  SAVE IMAGE java-examples:latest

go-bindings:
  ARG release
  FROM +go-swig-image
  COPY +build/libubiquity_client_static.a +build/Ubiquity.hpp .
  COPY go/swig .
  RUN cmake -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DCMAKE_BUILD_TYPE=Release -S . -B build \
      && cmake --build build --parallel $(nproc) --target ubiquityclient
  SAVE ARTIFACT build/output/Ubiquity.go AS LOCAL local-output/go/Ubiquity/
  SAVE ARTIFACT build/ubiquityclient.so AS LOCAL local-output/go/Ubiquity/

go-examples-build:
  FROM +go-image
  COPY +go-bindings/Ubiquity.go samples/lib/Ubiquity/
  COPY +go-bindings/ubiquityclient.so samples/lib/Ubiquity/
  COPY go/samples samples
  COPY datasets samples/rsc/datasets
  RUN apt-get install --no-install-recommends -qq gzip
  RUN cd samples/rsc/datasets && gzip -d *.gz
  RUN cd samples && SUBDIRS=$(find ./src -maxdepth 2 -mindepth 2 -type d -printf '%P\n') && \
      for dir in $SUBDIRS; \
      do \
        echo 'building '$dir' ...'; \
        go build -o bin/$dir src/$dir/Main.go; \
      done
  SAVE ARTIFACT ./samples

go-examples:
  FROM +base-image
  COPY +go-examples-build/samples/bin/ .
  COPY +go-examples-build/samples/rsc/ ./rsc
  COPY +go-examples-build/samples/lib/ ./lib
  SAVE IMAGE go-examples:latest

base-image:
  LET supportedOs=ubuntu:16.04 ubuntu:20.04 ubuntu:22.04 ubuntu:24.04
  IF echo $supportedOs | grep -q $os
    FROM $os
  ELSE
    RUN printf "%s is not supported. Valide choices are: %s\n" $os "$supportedOs"; false
  END
  IF echo $os | grep -q ubuntu
    ENV DEBIAN_FRONTEND noninteractive
    ENV DEBCONF_NONINTERACTIVE_SEEN true
    RUN apt-get update -qq
    IF [ "$os" = "ubuntu:16.04" ]
      RUN apt-get install --no-install-recommends -qq software-properties-common && add-apt-repository -y ppa:ubuntu-toolchain-r/test && apt-get update -qq
      RUN apt-get install --no-install-recommends -qq libstdc++-9-dev
    END
    # tools
    RUN apt-get install --no-install-recommends -qq wget ca-certificates
    # project deps
    RUN apt-get install --no-install-recommends -qq libbsd-dev uuid-dev libcurl4-openssl-dev
    RUN apt-get clean
  END
  WORKDIR /client

cmake-image:
  FROM +base-image
  IF echo $os | grep -q ubuntu
    IF [ "$os" = "ubuntu:16.04" ]
      RUN apt-get install --no-install-recommends -qq software-properties-common && add-apt-repository -y ppa:ubuntu-toolchain-r/test && apt-get update -qq
      RUN apt-get install --no-install-recommends -qq build-essential g++-9 libssl-dev git doxygen
      RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 10
      RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 10
      RUN update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 30
      RUN update-alternatives --set cc /usr/bin/gcc
      RUN update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++ 30
      RUN update-alternatives --set c++ /usr/bin/g++
    ELSE
      RUN apt-get install --no-install-recommends -qq build-essential g++ libssl-dev git doxygen && apt-get clean
    END
    RUN wget --quiet https://github.com/Kitware/CMake/releases/download/v3.30.2/cmake-3.30.2-linux-x86_64.sh
    RUN sh cmake-3.30.2-linux-x86_64.sh --skip-license --prefix=/usr/local/ && rm cmake-3.30.2-linux-x86_64.sh
  END

java-swig-image:
  FROM +cmake-image
  DO +JAVA_INSTALL
  DO +SWIG_INSTALL

sbt-image:
  FROM +base-image
  DO +JAVA_INSTALL
  IF echo $os | grep -q ubuntu
    RUN apt-get install --no-install-recommends -qq apt-transport-https gnupg curl
    RUN echo "deb https://repo.scala-sbt.org/scalasbt/debian all main" >/etc/apt/sources.list.d/sbt.list
    RUN echo "deb https://repo.scala-sbt.org/scalasbt/debian /" >/etc/apt/sources.list.d/sbt_old.list
    RUN curl -sL "https://keyserver.ubuntu.com/pks/lookup?op=get&search=0x2EE0EA64E40A89B84B2DF73499E82A75642AC823" | gpg --no-default-keyring --keyring gnupg-ring:/etc/apt/trusted.gpg.d/scalasbt-release.gpg --import
    RUN chmod 644 /etc/apt/trusted.gpg.d/scalasbt-release.gpg
    RUN apt-get update -qq && apt-get install --no-install-recommends -qq sbt
    RUN sbt update
  END

JAVA_INSTALL:
  FUNCTION
  IF echo $os | grep -q ubuntu
    IF [ "$os" = "ubuntu:16.04" ]
      RUN apt-get install --no-install-recommends -qq software-properties-common && add-apt-repository -y ppa:openjdk-r/ppa && apt-get update -qq
      RUN apt-get install --no-install-recommends -qq openjdk-17-jdk
      RUN dpkg --purge --force-depends ca-certificates-java && apt-get install -qq ca-certificates-java
      RUN keytool -importkeystore -srcstorepass changeit -deststorepass changeit -srckeystore /etc/ssl/certs/java/cacerts -srcstoretype pkcs12 -destkeystore key.jks -deststoretype jks
      RUN mv -f key.jks /etc/ssl/certs/java/cacerts
    ELSE
      RUN apt-get install --no-install-recommends -qq openjdk-17-jdk
    END
    ENV JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
  END

go-swig-image:
  FROM +cmake-image
  DO +GO_INSTALL
  DO +SWIG_INSTALL

go-image:
  FROM +base-image
  DO +GO_INSTALL

GO_INSTALL:
  FUNCTION
  IF echo $os | grep -q ubuntu
    RUN apt-get install --no-install-recommends -qq gcc && apt-get clean \
        && wget --quiet https://go.dev/dl/go1.23.2.linux-amd64.tar.gz \
        && echo "542d3c1705f1c6a1c5a80d5dc62e2e45171af291e755d591c5e6531ef63b454e go1.23.2.linux-amd64.tar.gz" | sha256sum --check --status \
        && tar -C /usr/local -xzf go1.23.2.linux-amd64.tar.gz && rm go1.23.2.linux-amd64.tar.gz
    ENV PATH=$PATH:/usr/local/go/bin
  END

SWIG_INSTALL:
  FUNCTION
  IF echo $os | grep -q ubuntu
    IF [ "$os" = "ubuntu:16.04" ]
      RUN wget --quiet https://ftp.gnu.org/gnu/bison/bison-3.8.2.tar.gz && tar xzf bison-3.8.2.tar.gz
      RUN apt-get install --no-install-recommends -qq m4 automake libz-dev libpcre2-dev
      RUN cd bison-3.8.2 && ./configure && make && make install && cd .. && rm -rf bison-3.8.2*
    ELSE
      RUN apt-get install --no-install-recommends -qq automake bison libz-dev libpcre2-dev
    END
    GIT CLONE --branch v4.3.0 https://github.com/swig/swig /swig
    RUN cd /swig && ./autogen.sh && ./configure && make -j $(nproc) && make install && rm -rf /swig
  END
