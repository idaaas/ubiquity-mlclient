# Ubiquity Python MLClient

The `ubiquity-python-mlclient` library provides a Python client using our ML Clusters. This module simplifies interactions with Ubiquity's machine learning services, making it easy to integrate into your Python applications.

## Installation

Ensure you have **Python 3.5** or higher installed on your system.

Ensure you have installed bsd, curl and uuid

```bash
sudo apt-get update
sudo apt-get install --no-install-recommends -qq libbsd-dev uuid-dev libcurl4-openssl-dev
```

To include the `ubiquity-python-mlclient` module in your project, run:

```bash
pip install git+https://<PAT_token>@github.com/idaaas/ubiquity-python-mlclient.git@ubuntu-22-v0.7.2
```
where the PAT_token is generated on github with repository access

## Requirements

* ubuntu 16.04, 20.04, 22.04 or 24.04 (if you need an other os, please contact our support)
* python 3.5 or higher
* linux libraries **bsd, curl, uuid**

## Usage

You can get examples from our private repository.  
If you do not have access to it yet, please contact our support.

In your python package (for ubuntu 22.04 with driver version v0.7)
```python
from Ubiquity import Ubiquity
```

## Support

Contact at **support@ubiquity.ai**