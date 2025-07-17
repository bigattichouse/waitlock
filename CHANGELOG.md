# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- New `--done` flag to signal lock holders to release locks
- Signal-based lock release using SIGTERM instead of manual process killing
- Clean alternative to `kill` commands for lock management
- Support for both mutex and semaphore lock release with `--done`
- Comprehensive test suite in `test/` directory
- Build system improvements with separate `build/bin/` and `build/obj/` directories
- Updated documentation with `--done` usage examples

### Changed
- Build system now uses separate build directories for better organization
- Updated examples in documentation to use `--done` instead of `kill` commands
- Improved Makefile structure with proper clean targets

### Fixed
- Build artifacts are now properly separated from source code
- Clean build directory management

## [1.0.0] - 2024-07-16

### Added
- Initial release of waitlock
- Mutex and semaphore functionality
- Cross-platform POSIX compatibility
- Lock inspection and monitoring
- Command execution with locks
- Environment variable support
- Comprehensive documentation