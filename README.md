# PGAS N-Body Problem (UPC++)

This program simulates the motion of `N` bodies interacting through gravity (Velocity Verlet method) using **UPC++** and a **Partitioned Global Address Space (PGAS)** model.

Target environment: **Linux compute cluster** with UPC++ installed (module or `UPCXX_INSTALL`).

## Requirements

- C++14 compiler (via `upcxx` wrapper)
- [UPC++](https://upcxx.lbl.gov/) (`upcxx`, `upcxx-run`)
- `make`, `libm`
- Optional: OpenMP support in the compiler (for `make threaded`)

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

OpenMP build (multi-threading inside each UPC++ process):

```bash
make threaded
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

## Parallelism: UPC++ and OpenMP

Two independent levels:

| Level | Mechanism | Controlled by |
|-------|-----------|---------------|
| Between processes | UPC++ / PGAS | `upcxx-run -n P` (`RUN_PROCS`) |
| Inside one process | OpenMP (optional) | `make threaded` |

- **`make`** (default): one CPU thread per process runs all loops sequentially. PGAS still uses `P` processes.
- **`make threaded`**: compiles with `-fopenmp` and links UPC++ in `UPCXX_THREADMODE=par`. Loops over `local_count` in the Verlet step use `#pragma omp parallel for` (acceleration and velocity/position updates).

OpenMP splits iterations `li = 0 … local_count-1` across CPU threads **on the same rank**. It does not replace UPC++ processes: with `RUN_PROCS=4` and 8 OpenMP threads you get 4 processes × up to 8 threads each.

**Note:** `compute_accel` calls `global_get` / `global_put` (PGAS). With OpenMP, multiple threads on one rank may access the shared heap concurrently; this requires UPC++ built with `-threadmode=par` (`make threaded` sets that). Use `make` (sequential) if your site recommends `threadmode=seq` only.

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

## Cluster notes

- Load UPC++ module before building (site-specific).
- Set `-shared-heap` large enough for `10 * N * sizeof(double)` global arrays plus runtime overhead.
- Use `make opt` for production runs on the cluster.
