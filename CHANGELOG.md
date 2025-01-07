# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [v0.7.5] - 2025-01-06

### Added

- Cpp examples.
- Status codes UNAUTHORIZED and INTERNAL_API_ERROR.

### Changed

- Documentation updated.
- FORBIDDEN status code now refers to 403 Http error.

### Fixed

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

[Unreleased]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.7.5...HEAD
[v0.7.5]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.7.4...v0.7.5
[v0.7.4]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.7.3...v0.7.4
[v0.7.3]: https://github.com/idaaas/ubiquity-mlclient/compare/v0.7.2...v0.7.3
