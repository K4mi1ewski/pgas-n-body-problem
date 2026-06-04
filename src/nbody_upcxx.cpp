/*
 * UPC++ (PGAS) N-body simulation.
 *
 * Usage:
 *   upcxx-run -n <P> ./nbody_upcxx <input_file> [output_file]
 *
 * Or using make:
 *   make run RUN_PROCS=8 INPUT=input.txt OUTPUT=result.txt
 *
 * Input file format:
 *   Line 1: N dt steps
 *   Next N lines: M x y z vx vy vz
 *
 * Output file format:
 *   Line 1: N=<N> bodies
 *   Line 2: i - M x y z vx vy vz
 *   Next N lines: i - M x y z vx vy vz
 *   (i is the body index from 1 to N)
 */

#include <cstdio>
#include <cstdlib>
#include <upcxx/upcxx.hpp>
#include "nbody_common.hpp"

// Kolektywne zwolnienie tablic w shared heap.
static void free_global_arrays(upcxx::global_ptr<double> gm,
                               upcxx::global_ptr<double> gx,
                               upcxx::global_ptr<double> gy,
                               upcxx::global_ptr<double> gz,
                               upcxx::global_ptr<double> gvx,
                               upcxx::global_ptr<double> gvy,
                               upcxx::global_ptr<double> gvz,
                               upcxx::global_ptr<double> gvxh,
                               upcxx::global_ptr<double> gvyh,
                               upcxx::global_ptr<double> gvzh) {
    upcxx::delete_(gm);
    upcxx::delete_(gx);
    upcxx::delete_(gy);
    upcxx::delete_(gz);
    upcxx::delete_(gvx);
    upcxx::delete_(gvy);
    upcxx::delete_(gvz);
    upcxx::delete_(gvxh);
    upcxx::delete_(gvyh);
    upcxx::delete_(gvzh);
}

int main(int argc, char **argv) {
    upcxx::init();

    const int rank = upcxx::rank_me();
    const int nprocs = upcxx::rank_n();

    if (argc < 2 || argc > 3) {
        if (rank == 0) {
            std::fprintf(stderr,
                         "Usage: upcxx-run -n <P> %s <input_file> [output_file]\n"
                         "  default output_file: output.txt\n",
                         argv[0]);
        }
        upcxx::finalize();
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = (argc == 3) ? argv[2] : "output.txt";

    int n = 0, steps = 0;
    double dt = 0.0;
    Body *all_bodies = nullptr;

    int read_ok = 0;
    if (rank == 0) {
        read_ok = (read_input(input_path, &n, &dt, &steps, &all_bodies) == 0) ? 1
                                                                            : 0;
    }
    read_ok = upcxx::broadcast(read_ok, 0).wait();
    if (!read_ok) {
        if (rank == 0) {
            std::fprintf(stderr, "Failed to read input file\n");
        }
        std::free(all_bodies);
        upcxx::finalize();
        return 1;
    }

    n = upcxx::broadcast(n, 0).wait();
    dt = upcxx::broadcast(dt, 0).wait();
    steps = upcxx::broadcast(steps, 0).wait();

    int *counts =
        static_cast<int *>(std::malloc(static_cast<size_t>(nprocs) * sizeof(int)));
    int *displs =
        static_cast<int *>(std::malloc(static_cast<size_t>(nprocs) * sizeof(int)));
    if (!counts || !displs) {
        if (rank == 0) {
            std::fprintf(stderr, "Allocation failed\n");
        }
        std::free(all_bodies);
        std::free(counts);
        std::free(displs);
        upcxx::finalize();
        return 1;
    }
    partition_counts(n, nprocs, counts, displs);

    const int local_count = counts[rank];
    const int lo = displs[rank];
    const size_t local_d_alloc =
        local_count > 0 ? static_cast<size_t>(local_count) : 1u;

    // Tablice stanu w shared heap (PGAS)
    upcxx::global_ptr<double> gm = upcxx::new_array<double>(n);
    upcxx::global_ptr<double> gx = upcxx::new_array<double>(n);
    upcxx::global_ptr<double> gy = upcxx::new_array<double>(n);
    upcxx::global_ptr<double> gz = upcxx::new_array<double>(n);
    upcxx::global_ptr<double> gvx = upcxx::new_array<double>(n);
    upcxx::global_ptr<double> gvy = upcxx::new_array<double>(n);
    upcxx::global_ptr<double> gvz = upcxx::new_array<double>(n);
    upcxx::global_ptr<double> gvxh = upcxx::new_array<double>(n);
    upcxx::global_ptr<double> gvyh = upcxx::new_array<double>(n);
    upcxx::global_ptr<double> gvzh = upcxx::new_array<double>(n);

    if (rank == 0) {
        fill_global_state(n, all_bodies, gm, gx, gy, gz, gvx, gvy, gvz);
        std::free(all_bodies);
        all_bodies = nullptr;
    }
    upcxx::barrier();

    double *ax0 = static_cast<double *>(std::malloc(local_d_alloc * sizeof(double)));
    double *ay0 = static_cast<double *>(std::malloc(local_d_alloc * sizeof(double)));
    double *az0 = static_cast<double *>(std::malloc(local_d_alloc * sizeof(double)));
    double *ax1 = static_cast<double *>(std::malloc(local_d_alloc * sizeof(double)));
    double *ay1 = static_cast<double *>(std::malloc(local_d_alloc * sizeof(double)));
    double *az1 = static_cast<double *>(std::malloc(local_d_alloc * sizeof(double)));

    if (!ax0 || !ay0 || !az0 || !ax1 || !ay1 || !az1) {
        if (rank == 0) {
            std::fprintf(stderr, "Allocation failed\n");
        }
        std::free(ax0);
        std::free(ay0);
        std::free(az0);
        std::free(ax1);
        std::free(ay1);
        std::free(az1);
        std::free(counts);
        std::free(displs);
        free_global_arrays(gm, gx, gy, gz, gvx, gvy, gvz, gvxh, gvyh, gvzh);
        upcxx::finalize();
        return 1;
    }

    // Petla Verlet: a(t) -> polowa predkosc i pozycja -> barrier ->
    //               a(t+dt) -> pelna predkosc -> barrier
    for (int s = 0; s < steps; s++) {
        // 1) przyspieszenie a(t) dla lokalnych cial
#ifdef _OPENMP
#pragma omp parallel for
#endif
        for (int li = 0; li < local_count; li++) {
            const int gi = lo + li;
            compute_accel(n, gm, gx, gy, gz, gi, &ax0[li], &ay0[li], &az0[li]);
        }

        // 2) polowkowy krok predkosci i aktualizacja pozycji
#ifdef _OPENMP
#pragma omp parallel for
#endif
        for (int li = 0; li < local_count; li++) {
            const int gi = lo + li;
            const double vx_i = global_get(gvx, gi);
            const double vy_i = global_get(gvy, gi);
            const double vz_i = global_get(gvz, gi);
            const double vxh_i = vx_i + 0.5 * ax0[li] * dt;
            const double vyh_i = vy_i + 0.5 * ay0[li] * dt;
            const double vzh_i = vz_i + 0.5 * az0[li] * dt;
            const double x_i = global_get(gx, gi);
            const double y_i = global_get(gy, gi);
            const double z_i = global_get(gz, gi);
            global_put(gvxh, gi, vxh_i);
            global_put(gvyh, gi, vyh_i);
            global_put(gvzh, gi, vzh_i);
            global_put(gx, gi, x_i + vxh_i * dt);
            global_put(gy, gi, y_i + vyh_i * dt);
            global_put(gz, gi, z_i + vzh_i * dt);
        }
        upcxx::barrier();

        // 3) przyspieszenie a(t+dt) z nowych pozycji
#ifdef _OPENMP
#pragma omp parallel for
#endif
        for (int li = 0; li < local_count; li++) {
            const int gi = lo + li;
            compute_accel(n, gm, gx, gy, gz, gi, &ax1[li], &ay1[li], &az1[li]);
        }

        // 4) pelny krok predkosci v(t+dt)
#ifdef _OPENMP
#pragma omp parallel for
#endif
        for (int li = 0; li < local_count; li++) {
            const int gi = lo + li;
            global_put(gvx, gi, global_get(gvxh, gi) + 0.5 * ax1[li] * dt);
            global_put(gvy, gi, global_get(gvyh, gi) + 0.5 * ay1[li] * dt);
            global_put(gvz, gi, global_get(gvzh, gi) + 0.5 * az1[li] * dt);
        }
        upcxx::barrier();
    }

    // Rank 0: wynik na stdout i do pliku wyjsciowego
    if (rank == 0) {
        FILE *fout = std::fopen(output_path, "w");
        if (!fout) {
            std::perror(output_path);
        }

        std::printf("%d %.17g %d\n", n, dt, steps);
        for (int i = 0; i < n; i++) {
            const double bm = global_get(gm, i);
            const double bx = global_get(gx, i);
            const double by = global_get(gy, i);
            const double bz = global_get(gz, i);
            const double bvx = global_get(gvx, i);
            const double bvy = global_get(gvy, i);
            const double bvz = global_get(gvz, i);
            std::printf("%.17g %.17g %.17g %.17g %.17g %.17g %.17g\n", bm, bx,
                        by, bz, bvx, bvy, bvz);
            if (fout) {
                if (i == 0) {
                    std::fprintf(fout, "N=%d bodies\n", n);
                    std::fprintf(fout, "i - M x y z vx vy vz\n");
                }
                std::fprintf(fout,
                             "%d - %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n",
                             i + 1, bm, bx, by, bz, bvx, bvy, bvz);
            }
        }
        if (fout) {
            std::fclose(fout);
        }
    }

    std::free(ax0);
    std::free(ay0);
    std::free(az0);
    std::free(ax1);
    std::free(ay1);
    std::free(az1);
    std::free(counts);
    std::free(displs);
    free_global_arrays(gm, gx, gy, gz, gvx, gvy, gvz, gvxh, gvyh, gvzh);
    upcxx::finalize();
    return 0;
}
