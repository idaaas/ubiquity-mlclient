VERSION 0.8
FROM busybox

ARG --global --required os
ARG --global type=Release
ARG --global disableIdChecks=OFF

# to publish artefacts
ARG --global imagePrefix
ARG --global imageVersion=latest
ARG --global latest=false
ARG --global private=true


all:
  BUILD +cpp-bindings
  BUILD +java-bindings
  BUILD +go-bindings
  BUILD +java-examples-build
  BUILD +go-examples-build

build-examples:
  BUILD +java-examples
  BUILD +go-examples
  BUILD +python-examples
  BUILD +cpp-examples

build:
  FROM +cmake-image
  CACHE ./build
  COPY --keep-ts cpp .git .
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

cpp-examples:
  FROM +cmake-image
  COPY --keep-ts cpp .git .
  COPY datasets datasets
  RUN cmake -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF -DBUILD_AERON_ARCHIVE_API=OFF -DBUILD_SAMPLES=ON -DCMAKE_BUILD_TYPE=$type -DDISABLE_ID_CHECKS=$disableIdChecks -S . -B build
  RUN cmake --build build --parallel $(nproc) --target line-item probability-estimator
  RUN cp -r ./build/samples/* .
  SAVE IMAGE --push ${imagePrefix}cpp-examples:${imageVersion}
  IF [ "$latest" = "true" ]
    SAVE IMAGE --push ${imagePrefix}cpp-examples:latest
  END

java-bindings:
  ARG release
  FROM +java-swig-image
  COPY +build/libubiquity_client_static.a +build/Ubiquity.hpp .
  COPY java/swig .
  RUN cmake -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DCMAKE_BUILD_TYPE=Release -S . -B build
  RUN cmake --build build --parallel $(nproc) --target jniUbiquityClient
  RUN mkdir -p jar-root/NATIVE && cp build/libjniUbiquityClient.so jar-root/NATIVE
  RUN javac -Werror -Xlint -d jar-root build/output/*.java $([ -z "$release" ] || echo --release $release)
  RUN jar cf ubiquity-mlclient.jar -C jar-root .
  SAVE ARTIFACT ubiquity-mlclient.jar AS LOCAL local-output/java/

java-publish:
  FROM +sbt-image
  ARG token
  ARG release
  ARG version
  ARG repo
  ARG snap
  ENV OS_NAME=$(echo $os | sed 's/ubuntu:\([0-9][0-9]\).[0-9][0-9]/ubuntu-\1/g')
  COPY +java-bindings/ubiquity-mlclient.jar .
  COPY java/publish/settings.xml .
  RUN wget https://dlcdn.apache.org/maven/maven-3/3.9.9/binaries/apache-maven-3.9.9-bin.tar.gz \
      && tar -xzvf apache-maven-3.9.9-bin.tar.gz \
      && mv apache-maven-3.9.9 /opt/
  ENV PATH=/opt/apache-maven-3.9.9/bin:$PATH
  RUN mkdir ~/.m2 && mv settings.xml ~/.m2/
  #TODO : refacto this to make it clearer
  RUN SNAP=$(if [ "$snap" = "true" ]; then echo "-SNAPSHOT"; else echo ""; fi) \
      PRIVATE_GROUP=$(if [ "$private" = "true" ]; then echo "private."; else echo ""; fi) \
      && echo $PRIVATE_GROUP \
      && mvn deploy:deploy-file \
      -DgroupId=com.idaaas.${PRIVATE_GROUP}${OS_NAME}-java-${release} \
      -DartifactId=ubiquity-mlclient \
      -Dversion=${version}${SNAP} \
      -Dpackaging=jar \
      -Dfile=./ubiquity-mlclient.jar \
      -DrepositoryId=github \
      -Durl=https://maven.pkg.github.com/idaaas/${repo} \
      -Dsettings.token=${token}

java-examples-build:
  FROM +sbt-image
  COPY +java-bindings/ubiquity-mlclient.jar .
  COPY java/sbt .
  RUN sbt assembly
  SAVE ARTIFACT target/main.jar

java-examples:
  FROM +base-image
  DO +JAVA_INSTALL
  COPY +java-examples-build/main.jar .
  COPY datasets datasets
  ENTRYPOINT ["java","-cp","main.jar"]
  SAVE IMAGE --push ${imagePrefix}java-examples:${imageVersion}
  IF [ "$latest" = "true" ]
    SAVE IMAGE --push ${imagePrefix}java-examples:latest
  END

go-publish:
  FROM +go-bindings
  ARG version
  ARG token
  ENV OS_NAME=$(echo $os | sed 's/ubuntu:\([0-9][0-9]\).[0-9][0-9]/ubuntu-\1/g')
  COPY +go-bindings/libubiquity_client_static.a package/Ubiquity/
  COPY +go-bindings/libubiquityclientwrapper.a package/Ubiquity/
  COPY +go-bindings/Ubiquity.go package/Ubiquity/
  COPY go/publish/pushToPackagePublicRepo.sh .
  COPY go/publish/README.md ./package
  RUN apt-get -y update && apt-get -y install git \
      && cd package/Ubiquity && go mod init github.com/idaaas/ubiquity-go-mlclient/Ubiquity && go mod tidy && cd ../.. \
      && chmod a+x pushToPackagePublicRepo.sh && ./pushToPackagePublicRepo.sh ${token} package/ ${OS_NAME}-${version} ${OS_NAME}

go-bindings:
  FROM +go-swig-image
  COPY +build/libubiquity_client_static.a +build/Ubiquity.hpp .
  COPY go/swig .
  RUN cmake -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DCMAKE_BUILD_TYPE=Release -S . -B build \
      && cmake --build build --parallel $(nproc) --target libubiquityclientwrapper
  SAVE ARTIFACT build/output/Ubiquity.go AS LOCAL local-output/go/Ubiquity/
  SAVE ARTIFACT build/libubiquityclientwrapper.a AS LOCAL local-output/go/Ubiquity/
  SAVE ARTIFACT libubiquity_client_static.a AS LOCAL local-output/go/Ubiquity/

go-examples-build:
  FROM +go-image
  COPY +go-bindings/Ubiquity.go Ubiquity/
  COPY +go-bindings/libubiquityclientwrapper.a Ubiquity/
  COPY +go-bindings/libubiquity_client_static.a Ubiquity/
  COPY go/samples .
  COPY datasets datasets
  RUN apt-get install --no-install-recommends -qq g++
  RUN SUBDIRS=$(find ./src -maxdepth 2 -mindepth 2 -type d -printf '%P\n') && \
      for dir in $SUBDIRS; \
      do \
        echo 'building '$dir' ...'; \
        go build -o bin/$dir src/$dir/Main.go; \
      done
  SAVE ARTIFACT bin
  SAVE ARTIFACT ./datasets

go-examples:
  FROM +base-image
  COPY +go-examples-build/bin/ .
  COPY +go-examples-build/datasets/* ./datasets/
  SAVE IMAGE --push ${imagePrefix}go-examples:${imageVersion}
  IF [ "$latest" = "true" ]
    SAVE IMAGE --push ${imagePrefix}go-examples:latest
  END

python-bindings:
  FROM +python-swig-image
  COPY +build/libubiquity_client_static.a +build/Ubiquity.hpp .
  COPY python/swig .
  RUN cmake -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DCMAKE_BUILD_TYPE=Release -S . -B build \
      && cmake --build build --parallel $(nproc) --target libUbiquityClientWrapper
  SAVE ARTIFACT build/output/Ubiquity.py AS LOCAL local-output/python/Ubiquity/
  SAVE ARTIFACT build/_libUbiquityClientWrapper.so AS LOCAL local-output/python/Ubiquity/

python-examples:
  FROM +python-image
  IF [ "$os" = "ubuntu:24.04" ]
    RUN apt-get update && apt-get install -qq --no-install-recommends pipx \
      && pipx install --include-deps requests
  ELSE
    RUN apt-get update && apt-get install -qq --no-install-recommends python3-pip \
      && pip3 install requests
  END
  COPY +python-bindings/Ubiquity.py Ubiquity/
  COPY +python-bindings/_libUbiquityClientWrapper.so Ubiquity/
  COPY python/samples .
  COPY datasets datasets
  ENTRYPOINT ["python3","-m"]
  SAVE IMAGE --push ${imagePrefix}python-examples:${imageVersion}
  IF [ "$latest" = "true" ]
    SAVE IMAGE --push ${imagePrefix}python-examples:latest
  END

python-publish:
  FROM +python-bindings
  ARG version
  ARG token
  ENV OS_NAME=$(echo $os | sed 's/ubuntu:\([0-9][0-9]\).[0-9][0-9]/ubuntu-\1/g')
  COPY +python-bindings/_libUbiquityClientWrapper.so +python-bindings/Ubiquity.py package/Ubiquity/
  COPY python/publish/__init__.py package/Ubiquity/
  COPY python/publish/setup.cfg python/publish/pyproject.toml python/publish/README.md package/
  COPY python/publish/pushToPackagePublicRepo.sh .
  RUN sed -i "s/%%VERSION%%/${version}/g" package/setup.cfg
  RUN apt-get -y update && apt-get -y install git \
      && chmod a+x pushToPackagePublicRepo.sh && ./pushToPackagePublicRepo.sh ${token} package/ ${OS_NAME}-${version} ${OS_NAME}

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
    RUN sbt update --allow-empty
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

python-swig-image:
  FROM +cmake-image
  DO +PYTHON_INSTALL
  DO +SWIG_INSTALL

PYTHON_INSTALL:
  FUNCTION
  IF echo $os | grep -q ubuntu
      RUN apt-get install --no-install-recommends -qq python3-dev python3 && apt-get clean
  END

go-swig-image:
  FROM +cmake-image
  DO +GO_INSTALL
  DO +SWIG_INSTALL

python-image:
  FROM +base-image
  DO +PYTHON_INSTALL

go-image:
  FROM +base-image
  DO +GO_INSTALL

GO_INSTALL:
  FUNCTION
  IF echo $os | grep -q ubuntu
    RUN apt-get install --no-install-recommends -qq gcc && apt-get clean
    RUN wget https://go.dev/dl/go1.16.linux-amd64.tar.gz \
        && tar -C /usr/local -xzf go1.16.linux-amd64.tar.gz && rm go1.16.linux-amd64.tar.gz
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
