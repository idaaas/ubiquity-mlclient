# Python ML Client

## Build

In the client's root directory :
```bash
earthly +python-bindings --os=ubuntu:22.04
tree local-ouput/python
#local-output/python
#└── Ubiquity  // this is the package to import
#    ├── Ubiquity.py
#    └── _libubiquityclientwrapper.so
```

## Usage

In your Python package source code, copy the Ubiquity folder like so:
```bash
#my-package/
#└── __init__.py
#└── src/my-code
#└── Ubiquity  // this is the package to import
#    ├── Ubiquity.py
#    └── _libubiquityclientwrapper.so
```

import the module as follows :
```python
import "Ubiquity"
```

See the general documentation for more information about using the client.

## Running Examples

Python examples are provided in a docker to quickly run scenarios:
```bash
earthly +python-examples --os=ubuntu:22.04
docker run python-examples src.line-item.main API_TOKEN CLUSTER_ID
