# Go ML Client

## Build

In the client's root directory :
```bash
earthly +go-bindings --os=ubuntu:22.04
tree local-ouput/go
#local-output/go
#└── Ubiquity  // this is the package to import
#    ├── Ubiquity.go
#    └── ubiquityclient.so
```

## Usage

In your Go program's source code, import the package as follows :
```go
//#cgo LDFLAGS: COMPILE_PATH/ubiquityclient.so -Wl,-rpath=RUNTIME_PATH/
import "C"
import "Ubiquity"
```

The #cgo comment followed by `import "C"` tells the go compiler to pass the commented line to the linker.
`COMPILE_PATH` represents the path in which you want to put the Ubiquity package for compilation. 
`RUNTIME_PATH` represents the path in which you want to put `ubiquityclient.so` at runtime for your compiled program to use it.
Those path can be the same path if you compile and run the program on the same system.

See the general documentation for more information about using the client.

## Running Examples

Go examples are provided in a docker to quickly run scenarios:
```bash
earthly +go-examples --os=ubuntu:22.04
docker run go-examples ./line-item/example1 API_TOKEN CLUSTER_ID
```
