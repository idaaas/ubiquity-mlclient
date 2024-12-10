# Go ML Client

## Build

In the client's root directory :
```bash
earthly +go-bindings --os=ubuntu:22.04
tree local-ouput/go
#local-output/go
#└── Ubiquity  // this is the package to import
#    ├── Ubiquity.go
#    └── libubiquityclientwrapper.a
#    └── libubiquity_client_static.a
```

## Usage

In your Go program's source code, import the package as follows :
```go
import "Ubiquity"
```

See the general documentation for more information about using the client.

## Running Examples

Go examples are provided in a docker to quickly run scenarios:
```bash
earthly +go-examples --os=ubuntu:22.04
docker run go-examples ./line-item/example1 API_TOKEN CLUSTER_ID NUM_QUERIES
```
NUM_QUERIES is optional and 500000 by default.