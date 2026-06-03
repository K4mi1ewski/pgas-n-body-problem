#ifndef NBODY_ACCEL_HPP
#define NBODY_ACCEL_HPP

#ifdef __cplusplus
extern "C" {
#endif

int cuda_accel_init(int n, int lo, int local_count);
void cuda_accel_finalize(void);
void cuda_compute_accel_batch(const double *h_m, const double *h_x,
                              const double *h_y, const double *h_z,
                              double *ax, double *ay, double *az);

#ifdef __cplusplus
}
#endif

#endif /* NBODY_ACCEL_HPP */
