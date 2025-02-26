# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [v0.8.1] - 2025-03-03

### Changed

- examples feedback delays to match new cluster spec

### Added

- Probability estimator samples.
- Integration tests.
- Action rate model.
- Build instructions for M1 architecture.

### Fixed

- GetStatusString parse status code properly.
- Feedback delays in samples.

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

[Unreleased]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.8.1...HEAD
[v0.8.1]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.8.0...v0.8.1
[v0.8.0]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.7.5...v0.8.0
[v0.7.5]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.7.4...v0.7.5
[v0.7.4]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.7.3...v0.7.4
[v0.7.3]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.7.2...v0.7.3
