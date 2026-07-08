# C# ML Client

## Supported Platforms

- Linux x64

## Requirements

- .NET Standard 2.0 or higher

## Built module

Comming soon.

## Build

In the client's root directory:
```bash
earthly +build --lang=csharp --os=ubuntu:22.04
tree local-ouput/csharp/ubuntu:22.04
#local-output/csharp/ubuntu:22.04
#└── UbiquityMlClient.X.Y.Z.nupkg  // this is the nuget package, where X.Y.Z is the version of the driver
```

## Installation

```bash
dotnet add package UbiquityMlClient --source /path/to/nupkgs
```

## Usage

In your dotnet program's source code, import the package as follows:
```csharp
import "ai.ubiquity"
```

See the general documentation for more information about using the client.

## Running Examples

C# examples are provided in a docker to quickly run scenarios: 
```bash
earthly +examples --lang=csharp --os=ubuntu:22.04
docker run csharp-examples ./bid-shading/example1 API_TOKEN CLUSTER_ID
```
