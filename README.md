# PortScope

PortScope is a Linux-only TCP connect port scanner written in C++17. It scans IPv4 hosts with bounded concurrency and per-port connect timeouts, then reports deterministic per-port results and a summary.

## Features

- IPv4 hostname/address resolution and TCP connect scanning
- Port specification parsing for single ports, ranges, and mixed lists (`22,80-82,443`)
- Input validation for CLI options and port specs
- Bounded concurrency via a fixed worker pool (no unbounded thread creation)
- Nonblocking `connect()` with `poll()` timeout handling
- Result states:
  - `open` (connect succeeded)
  - `closed` (`ECONNREFUSED`)
  - `filtered` (timeout or indeterminate/socket/network errors)
- Lightweight no-dependency automated tests with CTest

## Requirements

- Linux (POSIX sockets + `poll`)
- CMake 3.16+
- C++17 compiler (GCC or Clang)

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Binary output:

- `build/portscope`

## Usage

```bash
./build/portscope --host 127.0.0.1 --ports 22,80-82,443
```

### Options

- `--host HOST` (required): IPv4 address or hostname
- `--ports SPEC` (required): port specification
- `--timeout MS` (optional, default `1000`): per-port connect timeout in milliseconds
- `--concurrency N` (optional, default `100`): max simultaneous connection attempts
- `--help`: show help
- `--version`: show version

### Port specification format

- Single port: `443`
- Inclusive range: `1-1024`
- Comma-separated list: `22,80,443`
- Mixed format: `22,80-82,443`

Ports are deduplicated and sorted before scanning.

## Example commands

```bash
# Basic scan
./build/portscope --host 127.0.0.1 --ports 1-1024

# Custom timeout and concurrency
./build/portscope --host scanme.local --ports 22,80,443 --timeout 750 --concurrency 32

# Help/version
./build/portscope --help
./build/portscope --version
```

## TCP connect semantics

PortScope uses TCP `connect()` only. It does not send raw SYN packets and does not include stealth, evasion, or exploitation behavior.

Result interpretation:

- `open`: TCP handshake completed.
- `closed`: target actively refused (`ECONNREFUSED`).
- `filtered`: timed out or otherwise indeterminate from connect-level behavior.

## Limitations

- Linux-only implementation.
- IPv4 scanning only.
- `filtered` can include multiple causes (timeouts, unreachable network, permission, and other indeterminate socket errors).
- This is a TCP connect scanner, not a full service fingerprinting tool.

## Testing

From a clean build directory:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests cover:

- Port parsing
- CLI validation
- Invalid ranges/out-of-range ports
- Timeout/concurrency validation paths
- Error classification
- Loopback integration with a local TCP listener

## Responsible use

Only scan systems you own or are explicitly authorized to test.
