#include <stddef.h>

void saxpy_f32(float a, const float* x, float* y, size_t M, size_t stride) {
    if (stride == 1) {
        for (size_t i = 0; i < M; ++i) {
            y[i] = a * x[i] + y[i];
        }
    } else {
        for (size_t i = 0; i < M; ++i) {
            size_t j = i * stride;
            y[j] = a * x[j] + y[j];
        }
    }
}

void saxpy_f64(double a, const double* x, double* y, size_t M, size_t stride) {
    if (stride == 1) {
        for (size_t i = 0; i < M; ++i) {
            y[i] = a * x[i] + y[i];
        }
    } else {
        for (size_t i = 0; i < M; ++i) {
            size_t j = i * stride;
            y[j] = a * x[j] + y[j];
        }
    }
}

double dot_f32(const float* x, const float* y, size_t M, size_t stride) {
    double acc = 0.0;
    if (stride == 1) {
        for (size_t i = 0; i < M; ++i) {
            acc += (double)x[i] * (double)y[i];
        }
    } else {
        for (size_t i = 0; i < M; ++i) {
            size_t j = i * stride;
            acc += (double)x[j] * (double)y[j];
        }
    }
    return acc;
}

double dot_f64(const double* x, const double* y, size_t M, size_t stride) {
    double acc = 0.0;
    if (stride == 1) {
        for (size_t i = 0; i < M; ++i) {
            acc += x[i] * y[i];
        }
    } else {
        for (size_t i = 0; i < M; ++i) {
            size_t j = i * stride;
            acc += x[j] * y[j];
        }
    }
    return acc;
}

void elemul_f32(const float* x, const float* y, float* z, size_t M, size_t stride) {
    if (stride == 1) {
        for (size_t i = 0; i < M; ++i) {
            z[i] = x[i] * y[i];
        }
    } else {
        for (size_t i = 0; i < M; ++i) {
            size_t j = i * stride;
            z[j] = x[j] * y[j];
        }
    }
}

void elemul_f64(const double* x, const double* y, double* z, size_t M, size_t stride) {
    if (stride == 1) {
        for (size_t i = 0; i < M; ++i) {
            z[i] = x[i] * y[i];
        }
    } else {
        for (size_t i = 0; i < M; ++i) {
            size_t j = i * stride;
            z[j] = x[j] * y[j];
        }
    }
}
