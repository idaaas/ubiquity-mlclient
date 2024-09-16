# Documentation for the C++ client for Ubiquity.ai® {#mainpage}

This library is a thin abstraction towards Machine Learning on top of the low-latency RPC library [Aeron](https://github.com/real-logic/aeron).

It is designed for low latency and minimal overhead, allowing to continuously learn from up to one million trainings per second, across a cluster of hundred of nodes.

## Features

* The C++ Driver is **Shard Aware** and will use a data token to consistently select a specific node with the necessary model. As a result, latency is significantly reduced because there is no need to pass data between nodes.
* Async API
* Auto node discovery
* Configurable to trade-off between latency and thread counts
* Garbage free in steady state running
* Low-memory footprint
* Non-blocking IO in the message path
* Avoid unnecessary data copies
* No exceptions in the hot-code path
* Parameter validation at the edge

## Provided methods

* `Ubiquity::connect`
