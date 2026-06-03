#include <cuda_runtime.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "nbody_accel.hpp"

static constexpr double G_CONST = 6.67430e-11;
static constexpr double SOFTENING = 1e-9;

static int g_n = 0;
static int g_lo = 0;
static int g_local_count = 0;

static double *d_m = nullptr;
static double *d_x = nullptr;
static double *d_y = nullptr;
static double *d_z = nullptr;
static double *d_ax = nullptr;
static double *d_ay = nullptr;
static double *d_az = nullptr;

__global__ void accel_kernel(const double *m, const double *x, const double *y,
                             const double *z, int n, int lo, int local_count,
                             double *ax, double *ay, double *az) {
    const int li = blockIdx.x * blockDim.x + threadIdx.x;
    if (li >= local_count) {
        return;
    }
    const int gi = lo + li;
    const double xi = x[gi];
    const double yi = y[gi];
    const double zi = z[gi];
    double sumx = 0.0, sumy = 0.0, sumz = 0.0;

    for (int j = 0; j < n; j++) {
        if (j == gi) {
            continue;
        }
        const double dx = x[j] - xi;
        const double dy = y[j] - yi;
        const double dz = z[j] - zi;
        const double dist2 =
            dx * dx + dy * dy + dz * dz + SOFTENING * SOFTENING;
        const double invr = 1.0 / sqrt(dist2);
        const double invr3 = invr * invr * invr;
        const double s = G_CONST * m[j] * invr3;
        sumx += s * dx;
        sumy += s * dy;
        sumz += s * dz;
    }
    ax[li] = sumx;
    ay[li] = sumy;
    az[li] = sumz;
}

extern "C" int cuda_accel_init(int n, int lo, int local_count) {
    g_n = n;
    g_lo = lo;
    g_local_count = local_count;
    if (local_count <= 0) {
        return 0;
    }

    const size_t nbytes = static_cast<size_t>(n) * sizeof(double);
    const size_t lb = static_cast<size_t>(local_count) * sizeof(double);

    auto check = [](cudaError_t e, const char *msg) -> int {
        if (e != cudaSuccess) {
            std::fprintf(stderr, "%s: %s\n", msg, cudaGetErrorString(e));
            return -1;
        }
        return 0;
    };

    if (check(cudaMalloc(&d_m, nbytes), "cudaMalloc d_m") != 0) return -1;
    if (check(cudaMalloc(&d_x, nbytes), "cudaMalloc d_x") != 0) return -1;
    if (check(cudaMalloc(&d_y, nbytes), "cudaMalloc d_y") != 0) return -1;
    if (check(cudaMalloc(&d_z, nbytes), "cudaMalloc d_z") != 0) return -1;
    if (check(cudaMalloc(&d_ax, lb), "cudaMalloc d_ax") != 0) return -1;
    if (check(cudaMalloc(&d_ay, lb), "cudaMalloc d_ay") != 0) return -1;
    if (check(cudaMalloc(&d_az, lb), "cudaMalloc d_az") != 0) return -1;
    return 0;
}

extern "C" void cuda_accel_finalize(void) {
    cudaFree(d_m);
    cudaFree(d_x);
    cudaFree(d_y);
    cudaFree(d_z);
    cudaFree(d_ax);
    cudaFree(d_ay);
    cudaFree(d_az);
    d_m = d_x = d_y = d_z = nullptr;
    d_ax = d_ay = d_az = nullptr;
}

extern "C" void cuda_compute_accel_batch(const double *h_m, const double *h_x,
                                         const double *h_y, const double *h_z,
                                         double *ax, double *ay, double *az) {
    if (g_local_count <= 0) {
        return;
    }

    const size_t nbytes = static_cast<size_t>(g_n) * sizeof(double);
    const size_t lb = static_cast<size_t>(g_local_count) * sizeof(double);

    cudaMemcpy(d_m, h_m, nbytes, cudaMemcpyHostToDevice);
    cudaMemcpy(d_x, h_x, nbytes, cudaMemcpyHostToDevice);
    cudaMemcpy(d_y, h_y, nbytes, cudaMemcpyHostToDevice);
    cudaMemcpy(d_z, h_z, nbytes, cudaMemcpyHostToDevice);

    const int block = 256;
    const int grid = (g_local_count + block - 1) / block;
    accel_kernel<<<grid, block>>>(d_m, d_x, d_y, d_z, g_n, g_lo, g_local_count,
                                  d_ax, d_ay, d_az);
    cudaDeviceSynchronize();

    cudaMemcpy(ax, d_ax, lb, cudaMemcpyDeviceToHost);
    cudaMemcpy(ay, d_ay, lb, cudaMemcpyDeviceToHost);
    cudaMemcpy(az, d_az, lb, cudaMemcpyDeviceToHost);
}
