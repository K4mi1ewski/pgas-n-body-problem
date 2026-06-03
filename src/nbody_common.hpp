#ifndef NBODY_COMMON_HPP
#define NBODY_COMMON_HPP

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <upcxx/upcxx.hpp>

constexpr double G_CONST = 6.67430e-11;
constexpr double SOFTENING = 1e-9;

struct Body {
    double m;
    double x, y, z;
    double vx, vy, vz;
};

inline double global_get(upcxx::global_ptr<const double> base, int idx) {
    upcxx::global_ptr<const double> p = base + idx;
    if (p.is_local()) {
        return *p.local();
    }
    return upcxx::rget(p).wait();
}

inline void global_put(upcxx::global_ptr<double> base, int idx, double value) {
    upcxx::global_ptr<double> p = base + idx;
    if (p.is_local()) {
        *p.local() = value;
    } else {
        upcxx::rput(value, p).wait();
    }
}

inline void compute_accel(int n, upcxx::global_ptr<const double> gm,
                          upcxx::global_ptr<const double> gx,
                          upcxx::global_ptr<const double> gy,
                          upcxx::global_ptr<const double> gz, int gi,
                          double *ax, double *ay, double *az) {
    double sumx = 0.0, sumy = 0.0, sumz = 0.0;
    const double xi = global_get(gx, gi);
    const double yi = global_get(gy, gi);
    const double zi = global_get(gz, gi);

    for (int j = 0; j < n; j++) {
        if (j == gi) {
            continue;
        }
        const double dx = global_get(gx, j) - xi;
        const double dy = global_get(gy, j) - yi;
        const double dz = global_get(gz, j) - zi;
        const double dist2 =
            dx * dx + dy * dy + dz * dz + SOFTENING * SOFTENING;
        const double invr = 1.0 / std::sqrt(dist2);
        const double invr3 = invr * invr * invr;
        const double s = G_CONST * global_get(gm, j) * invr3;
        sumx += s * dx;
        sumy += s * dy;
        sumz += s * dz;
    }
    *ax = sumx;
    *ay = sumy;
    *az = sumz;
}

inline int read_input(const char *path, int *n_out, double *dt_out,
                      int *steps_out, Body **bodies_out) {
    FILE *f = std::fopen(path, "r");
    if (!f) {
        std::perror(path);
        return -1;
    }
    int n, steps;
    double dt;
    if (std::fscanf(f, "%d %lf %d", &n, &dt, &steps) != 3) {
        std::fprintf(stderr, "Invalid header: expected \"N dt steps\"\n");
        std::fclose(f);
        return -1;
    }
    if (n <= 0 || steps < 0 || dt <= 0.0) {
        std::fprintf(stderr, "Invalid N, dt, or steps\n");
        std::fclose(f);
        return -1;
    }

    Body *b = static_cast<Body *>(std::malloc(static_cast<size_t>(n) *
                                              sizeof(Body)));
    if (!b) {
        std::fclose(f);
        return -1;
    }

    for (int i = 0; i < n; i++) {
        Body *bi = &b[i];
        if (std::fscanf(f, "%lf %lf %lf %lf %lf %lf %lf", &bi->m, &bi->x,
                        &bi->y, &bi->z, &bi->vx, &bi->vy, &bi->vz) != 7) {
            std::fprintf(stderr, "Invalid body line %d\n", i + 1);
            std::free(b);
            std::fclose(f);
            return -1;
        }
    }
    std::fclose(f);

    *n_out = n;
    *dt_out = dt;
    *steps_out = steps;
    *bodies_out = b;
    return 0;
}

inline void partition_counts(int n, int size, int *counts, int *displs) {
    const int base = n / size;
    const int rem = n % size;
    int off = 0;
    for (int r = 0; r < size; r++) {
        counts[r] = base + (r < rem ? 1 : 0);
        displs[r] = off;
        off += counts[r];
    }
}

inline void fill_global_state(int n, const Body *bodies,
                              upcxx::global_ptr<double> gm,
                              upcxx::global_ptr<double> gx,
                              upcxx::global_ptr<double> gy,
                              upcxx::global_ptr<double> gz,
                              upcxx::global_ptr<double> gvx,
                              upcxx::global_ptr<double> gvy,
                              upcxx::global_ptr<double> gvz) {
    for (int i = 0; i < n; i++) {
        global_put(gm, i, bodies[i].m);
        global_put(gx, i, bodies[i].x);
        global_put(gy, i, bodies[i].y);
        global_put(gz, i, bodies[i].z);
        global_put(gvx, i, bodies[i].vx);
        global_put(gvy, i, bodies[i].vy);
        global_put(gvz, i, bodies[i].vz);
    }
}

#endif /* NBODY_COMMON_HPP */
