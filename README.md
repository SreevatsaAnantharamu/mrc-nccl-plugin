# mrc-nccl-plugin

An external NCCL network plugin using MRC (Multi-Path Reliable Connection) as transport. This rebased C++ branch builds the NCCL NET v12 ABI and reports the network name `MRC`. It requires a NCCL runtime that supports v12; the legacy C sources are not part of this build.


## Requirements

* MRC
* CUDA
* OFED
* NCCL with NET v12 support (runtime)
* A C++17 compiler, Make, and Python 3 (build)

## Build

```bash
make MRC_HOME=/path/to/mrc CUDA_HOME=/path/to/cuda
```
Debug build
```bash
make MRC_HOME=/path/to/mrc CUDA_HOME=/path/to/cuda DEBUG=1
```

libnccl-net-mrc.so will be generated after building, with `mrc` as the suffix of the plugin library.

The plugin links its own Linux support, logger adapter, parameter handling, and CUDA runtime. It does not depend on private symbols exported by libnccl. Only `ncclNetPlugin_v12` is exported; unresolved references are rejected at link time. Set `PYTHON` to a Python 3 executable if needed.

Hardware-free regression tests:

```bash
make test
```

The load test uses `RTLD_NOW` without linking the test executable to libnccl or CUDA. Additional tests cover MRC INIT/RTR/RTS masks, the ordinary verbs GPU-flush path, optional port-speed queries, NCCL logger callback forwarding, and QP/CC hint payloads and failure cleanup. See [MRC_ENV_VARIABLES.md](MRC_ENV_VARIABLES.md) for the hint toggles and per-QP rate semantics.

## Run (NCCL)

NCCL loads external plugins via the `NCCL_NET_PLUGIN` environment variable. It can be set to either
a suffix string or to a library name.

Example:

```bash
export NCCL_NET_PLUGIN=$PWD/libnccl-net-mrc.so
```
or
```bash
export NCCL_NET_PLUGIN=mrc
```

Deploy the same rebuilt library at the selected path on **every MPI node**. With `NCCL_DEBUG=INFO`, verify `Loaded net plugin MRC (v12)` and `Using network MRC`. A `NET/Plugin` load error followed by `Using network IB` means NCCL fell back to its built-in transport; subsequent IB CQE errors do not establish a failure in the MRC data path. Setting `NCCL_NET=MRC` (and forwarding it to every rank) makes such fallback fail explicitly instead.
