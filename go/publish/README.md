# Ubiquity Go MLClient

The `ubiquity-go-mlclient` library provides a Go client using our ML Clusters. This module simplifies interactions with Ubiquity's machine learning services, making it easy to integrate into your Go applications.

## Installation

Ensure you have Go version 1.16 or higher installed on your system.

Ensure you have installed bsd, curl and uuid

```bash
sudo apt-get update
sudo apt-get install --no-install-recommends -qq libbsd-dev uuid-dev libcurl4-openssl-dev
```

To include the `ubiquity-go-mlclient` module in your project, run:

```bash
go get github.com/idaaas/ubiquity-go-mlclient
```

## Requirements

* ubuntu 16.04, 20.04, 22.04 or 24.04 (if you need an other os, please contact our support)
* go 1.16 or higher
* linux libraries **bsd, curl, uuid**

## Usage

You can get examples from our private repository.  
If you do not have access to it yet, please contact our support.

In your go package (for ubuntu 22.04 with driver version v0.7)
```go
import (
    "github.com/idaaas/ubiquity-go-mlclient/Ubiquity@ubuntu-22-v0.7"
)
```

## Support

Contact at **support@ubiquity.ai**