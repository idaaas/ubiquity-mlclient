VERSION 0.8
FROM busybox

IMPORT ./cpp AS cpp
IMPORT ./java AS java
IMPORT ./csharp AS csharp
IMPORT ./go AS go
IMPORT ./python AS python

ARG --global os #TODO : remove this when deploys steps are separeted in other files

ARG --global type=Release
ARG --global disableIdChecks=OFF

# to publish artefacts
ARG --global private=true

clean:
  LOCALLY
  RUN rm -rf local-output

all:
  BUILD +build --os=ubuntu:16.04 \
    --os=ubuntu:20.04 \
    --os=ubuntu:22.04 \
    --os=ubuntu:24.04 \
    --release=17 \
    --release=8 \
    --lang=java \
    --lang=cpp \
    --lang=go \
    --lang=python

all-examples:
  ARG latest=false
  ARG imagePrefix
  ARG imageSuffix=latest
  BUILD +examples --os=ubuntu:22.04 \
    --release=17 \
    --lang=java \
    --lang=cpp \
    --lang=go \
    --lang=python \
    --latest=$latest \
    --imagePrefix=$imagePrefix \
    --imageSuffix=$imageSuffix

examples:
  ARG latest=false
  ARG imagePrefix
  ARG imageSuffix=latest
  ARG os=ubuntu:22.04
  ARG --required lang
  ARG release=17
  BUILD $lang+examples --os=$os --release=$release --latest=$latest --imagePrefix=$imagePrefix --imageSuffix=$imageSuffix --type=$type --disableIdChecks=$disableIdChecks

build:
  ARG os=ubuntu:22.04
  ARG --required lang
  IF [ "$lang" = "java" ]
    ARG release=17
    COPY ($lang+bindings/ --os=$os --release=$release --type=$type --disableIdChecks=$disableIdChecks) ./artifact
  ELSE
    COPY ($lang+bindings/ --os=$os --type=$type --disableIdChecks=$disableIdChecks) ./artifact
  END
  WAIT
    SAVE ARTIFACT ./artifact/* AS LOCAL local-output/$lang/$os/
  END

deploy:
  ARG --required os
  ARG --required lang
  ARG release=17
  ARG version
  ARG token
  ARG repo
  ARG snap
  ARG private
  BUILD $lang+deploy --os=$os --release=$release --version=$version --token=$token --repo=$repo --snap=$snap --private=$private

# Deprecated, use +build --lang=java instead
java-bindings:
  ARG --required os
  ARG release=17
  COPY (java+bindings/ --os=$os --release=$release --type=$type --disableIdChecks=$disableIdChecks) ./artifact
  WAIT
    SAVE ARTIFACT ./artifact/java-$release/* AS LOCAL local-output/java/
  END
