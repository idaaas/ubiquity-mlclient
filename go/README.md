# Go ML Client

## Built module

You can download go module for a direct use [here](https://github.com/idaaas/ubiquity-go-mlclient/tree/ubuntu-22-v0.7.4).  
Select the appropriate **tag** corresponding to your **os version** and read the repo **README** for installation instructions.

## Build

In the client's root directory :
```bash
earthly +build --lang=go --os=ubuntu:22.04
tree local-ouput/go/ubuntu:22.04
#local-output/go/ubuntu:22.04
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
earthly +examples --lang=go --os=ubuntu:22.04
docker run go-examples ./line-item/example1 API_TOKEN CLUSTER_ID NUM_QUERIES
```
NUM_QUERIES is optional and 500000 by default.