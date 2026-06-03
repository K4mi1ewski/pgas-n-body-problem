# PGAS N-Body Problem (UPC++)

This program simulates the motion of `N` bodies interacting through gravity (Velocity Verlet method) using **UPC++** and a **Partitioned Global Address Space (PGAS)** model.

Target environment: **Linux compute cluster** with UPC++ installed (module or `UPCXX_INSTALL`).

## Requirements

- C++14 compiler (via `upcxx` wrapper)
- [UPC++](https://upcxx.lbl.gov/) (`upcxx`, `upcxx-run`)
- `make`, `libm`
- Optional: OpenMP (for `make threaded`)
- Optional: CUDA Toolkit (for `make cuda`)

## Build

```bash
cd src
export UPCXX_INSTALL=/path/to/upcxx   # if not in PATH
make
```

Optimized build:

```bash
make opt
```

## Running

```bash
cd src
make run
```

Default `make run` parameters:

- `RUN_PROCS=4` — number of UPC++ processes (`upcxx-run -n`)
- `INPUT=input.txt`
- `OUTPUT=output.txt`
- `SHARED_HEAP=512M` — shared heap size (increase for large `N`)

Examples:

```bash
make run RUN_PROCS=8 INPUT=input.txt OUTPUT=result.txt
make run RUN_PROCS=16 SHARED_HEAP=1G
```

Direct launch:

```bash
OUTPUT_FILE=result.txt upcxx-run -shared-heap 512M -n 4 ./nbody_upcxx input.txt
```

## PGAS vs MPI

The reference MPI version uses explicit collectives (`MPI_Allgatherv`) to replicate positions and velocities each step. This PGAS version stores state in the **UPC++ shared heap** (`upcxx::new_array`). Each process updates its body slice in place; a `upcxx::barrier()` provides global visibility before the next acceleration pass. Remote reads use one-sided `rget` where needed.

## Input File Format

1. First line:

   ```text
   N dt steps
   ```

2. Next `N` lines (one per body):

   ```text
   M x y z vx vy vz
   ```

Field meanings:

- `N` — number of bodies
- `dt` — time step (seconds)
- `steps` — number of time steps
- `M` — mass (kg)
- `x y z` — position (m)
- `vx vy vz` — velocity (m/s)

Gravitational constant: `G = 6.67430e-11 m^3/(kg*s^2)` (SI units).

## Program Output

- Prints to `stdout`: header line `N dt steps`, then `N` lines `M x y z vx vy vz`
- Writes `OUTPUT` file (default `output.txt`):

  ```text
  N={N} bodies
  i - M x y z vx vy vz
  ...
  ```

## Optional CUDA

The `cuda/` directory is optional. Default `make` does not use the GPU.

```bash
make cuda
make run RUN_PROCS=4
```

Requires `nvcc` and CUDA runtime. You can remove `cuda/` entirely; the CPU build is unchanged.

## Cluster notes

- Load UPC++ module before building (site-specific).
- Set `-shared-heap` large enough for `10 * N * sizeof(double)` global arrays plus runtime overhead.
- Use `make opt` for production runs on the cluster.
