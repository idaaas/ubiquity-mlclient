# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [v1.0.0] - 2026-06-29

### Fixed

- **Connection issues** between ML client and ML cluster were fixed. The MlClient now relies on a new robust communication protocol based on hash lists.
    - Improved reliability and fault tolerance
    - Optimized message delivery guarantees
    - Better error recovery and retry logic

### Added

- **Bid Shading** application is now available with two operational modes: (1) Expected surplus maximization, (2) Win Rate Preservation. 

- **C# support** added for the ML client integration.

- **Backoff idle strategy**: Added an exponential backoff sleeping strategy that can be used when waiting for predictions from the cluster.

- **Comprehensive logging framework** with configurable levels and outputs.

- Status codes `OUT_OF_MEMORY` and `CONNECTION_TIMEOUT`.

### Changed

- **[Breaking]** `ClientHandle.startLineItem()` method signature has been changed. The fields `modelId` and `queryId` were removed from the query setup. The field `bidFloor` is added with a default value of `0.01` and the field `exposureCount` is added but remains optional for now (Used for **frequency capping**, a feature to be released later).
    - **Before**: `startLineItem(modelId, queryId, maxCPM)`
    - **After**: `startLineItem(maxCPM, bidFloor=0.01f, exposureCount=0)`

- **[Breaking]** `ClientHandle.startActionrate()` method signature has been changed. The fields `modelId` and `queryId` were removed from the query setup. The field `bidFloor` is added with a default value of `0.01` and the field `exposureCount` is added but remains optional for now (Used for **frequency capping**, a feature to be released later).
    - **Before**: `startActionRate(modelId, queryId, bidPrice)`
    - **After**: `startActionRate(bidPrice, bidFloor=0.01F, exposureCount=0)`

- **[Breaking]** `ClientHandle.predict()` method signature has been changed. The fields `modelId` and `queryId`were move from query setup to here.
    - **Before**: `predict()`
    - **After**: `predict(modelId, queryId)`

- **[Breaking]** `Ubiquity.setCredential()` method signature has been fixed. 
    - **Before**: `setCredential(appToken)`
    - **After**: `setCredentials(appToken)`

## [v0.9.2] - 2025-10-06

### Fixed

  - Issue when build is not done from a git repository
  - Deprecation of earthly satellites on CI

## [v0.9.1] - 2025-04-24

### Fixed

- Issue with high latencies

### Added

- Action rate sample code

### Changed

- Arguments name of certain functions

## [v0.9.0] - 2025-03-19

### Added

- Probability estimator samples.
- Integration tests.
- Action rate model.
- Build instructions for M1 architecture.
- Method getDiagnosticString

### Fixed

- GetStatusString parse status code properly.
- More consistent client handle internal states.

### Changed

- earthly build pipeline and documentation

## [v0.8.1] - 2025-03-03

### Changed

- examples feedback delays to match new cluster spec

## [v0.8.0] - 2025-01-30

### Added

- Query cancel function

### Fixed

- Reconnexion when full cluster is shutdown gracefully or not
- Feedback delays in samples to match new API requirements

## [v0.7.5] - 2025-01-09

### Added

- Cpp examples.
- Status codes UNAUTHORIZED and INTERNAL_API_ERROR.

### Changed

- Documentation updated.
- FORBIDDEN status code now refers to 403 Http error.

### Fixed

- Not sending Disconnection request when client Id is not set.
- Try to reconnect in case of a graceful shutdown.
- CI deployement of Go and Python. 

## [v0.7.4] - 2024-12-17

### Added

- Changelog file.

### Fixed

- Race condition between node clients when a modelAdd message is received before client is ready.

## [v0.7.3] - 2024-12-17

### Fixed

 - Refactor load balancer to avoid race condition.

[v1.0.0]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.9.2...v1.0.0
[v0.9.2]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.9.1...v0.9.2
[v0.9.1]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.9.0...v0.9.1
[v0.9.0]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.8.1...v0.9.0
[v0.8.1]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.8.0...v0.8.1
[v0.8.0]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.7.5...v0.8.0
[v0.7.5]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.7.4...v0.7.5
[v0.7.4]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.7.3...v0.7.4
[v0.7.3]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.7.2...v0.7.3
