# Python ML Client

## Built package

You can download python package with **pip** for a direct use [here](https://github.com/idaaas/ubiquity-python-mlclient/tree/ubuntu-22-v0.7.4).  
Select the appropriate **tag** corresponding to your **os version** and read the repo **README** for installation instructions.

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
docker run python-examples src.line-item.example1 API_TOKEN CLUSTER_ID NUM_QUERIES
```
NUM_QUERIES is optional and 500000 by default.
