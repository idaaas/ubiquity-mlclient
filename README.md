<p align="center">
  <a href="https://ubiquity.ai">
    <img alt="Ubiquity for Ad Networks" src="banners/UbiquityAI.png" width="80%">
  </a>
</p>

<p align="center">
  <a href="https://ubiquity-api.readme.io" target="_blank">Documentation</a>  •
  <a href="https://ubiquity-api.readme.io/reference/v1_cluster_list" target="_blank">Management API</a>  •
  <a href="https://ubiquity-api.readme.io/reference/classes_and_constants" target="_blank">ML Client API</a>  •
  <a href="https://ubiquity-api.readme.io/discuss" target="_blank">Community Forum</a>  •
  <a href="https://github.com/idaaas/ubiquity-mlclient/issues/new" target="_blank">Report an issue</a>
</p>

The ML client connects to your Machine Learning cluster and let you query predictions and send training data to the models that you deployed on the cluster using the [Management API](ubiquity-api.readme.io/docs/getting-your-api-keys).

The ML client is designed for low latency and minimal overhead, allowing to continuously learn from up to one million trainings per second, across a cluster of hundred of nodes.

## :sparkles: Features

- Async API
- Auto node discovery
- Garbage free in steady state running
- Low-memory footprint
- Non-blocking IO in the message path
- Avoid unnecessary data copies
- No exceptions in the hot-code path
- Parameter validation at the edge
- Configurable to trade-off between latency and thread counts
- Low-latency UDP protocol based on the RPC library [Aeron](https://aeron.io)

## :bulb: Getting the ML Client

To build the ML client, you need to install the build automation tool, Earthly v0.8+, by following [its installation guide](https://earthly.dev/get-earthly).

Once the `earthly` command installed, build clients for all supported languages and output the binaries in the `local-output` directory:
```bash
git clone git@github.com:idaaas/ubiquity-mlclient.git
cd ubiquity-mlclient
earthly +all --os=ubuntu:22.04
ls local-output # all binaries for all languages for ubuntu:22.04
```

Alternatively, you can limit the build to your language:
```bash
earthly +cpp-bindings --os=ubuntu:22.04
ls local-output # all binaries for cpp and ubuntu:22.04
```

Note that you are required to specify the OS you intend to use the client for.

Specific instructions for programming languages are available:
- [C++ 11 and above](cpp/README.md)
- [Java 8 and above](java/README.md)
- [Go 1.2 and above](go/README.md)
- [Python 3.5 and above](python/README.md)

If your programming language is not in the list, please fill in a [request](https://github.com/idaaas/ubiquity-mlclient/issues/new).

### Earthfile Options

The `--os` option let you specify the target OS, however changing the OS after having built the client for another OS is not detected by Earthly' cache and leads to incompatibilty issues. To overcome this, when selecting a new OS, you need to empty Earthly's cache with `--no-cache`.

These commands will lead to issues:
```bash
earthly +cpp-bindings --os=ubuntu:16.04
earthly +cpp-bindings --os=ubuntu:22.04 # Earthly has 16.04 artifacts in its cache
```

These commands will get you the correct result:
```bash
earthly +cpp-bindings --os=ubuntu:16.04
earthly --no-cache +cpp-bindings --os=ubuntu:22.04 # OK
earthly +java-bindings --os=ubuntu:22.04 # no need for `--no-cache` as OS did not change
```

You can use `--type=(Debug|Release)` with `Release` for production usage (the default), and `Debug` to log debug messages on stdout, add debugging symbols and disable most compiler optimizations.
As for the `--os` option, you have to reset Earthly's cache (`--no-cache`) when switching between `Debug` and `Release`.

Use `--disableIdChecks=ON` to enable faster cluster and model IDs serialization if IDs are well-formed and are provided by the API. However, with this option, inputing a ill-formed ID will probably lead to a crash. The default is `--disableIdChecks=OFF`.

### Supported Operating Systems and Architectures

- Ubuntu: ubuntu:16.04, ubuntu:20.04, ubuntu:22.04, ubuntu:24.04
- Architectures: 64-bit (x64)
- Processor architecture: Little Endian

If your OS or architecture is not in the list, please fill in a [request](https://github.com/idaaas/ubiquity-mlclient/issues/new).

### Dependencies

Dependencies per OS and language can be retrieved from the [Earthfile](Earthfile)

For instance, for Ubuntu, you need to install these libraries:
```bash
sudo apt-get install --no-install-recommends -qq libbsd-dev uuid-dev libcurl4-openssl-dev
```

## :link: Useful Links

- [Online documentation](https://ubiquity-api.readme.io)
- [ML client API](https://ubiquity-api.readme.io/reference/classes_and_constants)
- [Community Forum](https://ubiquity-api.readme.io/discuss)
- [Reporting an issue](https://github.com/idaaas/ubiquity-mlclient/issues/new)

## :page_facing_up: License

Copyright IDAaaS S.A.S

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
