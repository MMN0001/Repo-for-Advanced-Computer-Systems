#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <time.h>

#include <cblas.h>
#include <immintrin.h>
#include <omp.h>

// timing
static inline double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static inline uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}
static inline float rand01(uint32_t *st) {
    return (xorshift32(st) >> 8) * (1.0f / 16777216.0f);
}
static inline float randm11(uint32_t *st) {
    return 2.0f * rand01(st) - 1.0f;
}

// Dense
typedef struct {
    int rows, cols;
    float *data;
} DenseMatrix;

static DenseMatrix dense_create(int r, int c) {
    DenseMatrix M;
    M.rows = r; M.cols = c;
    M.data = (float*)calloc((size_t)r * (size_t)c, sizeof(float));
    if (!M.data) { fprintf(stderr, "calloc dense failed\n"); exit(1); }
    return M;
}
static void dense_free(DenseMatrix *M) {
    free(M->data); M->data = NULL;
    M->rows = M->cols = 0;
}
static inline float dense_get(const DenseMatrix *M, int i, int j) {
    return M->data[(size_t)i * (size_t)M->cols + (size_t)j];
}
static inline void dense_add(DenseMatrix *M, int i, int j, float v) {
    M->data[(size_t)i * (size_t)M->cols + (size_t)j] += v;
}
static inline void dense_set(DenseMatrix *M, int i, int j, float v) {
    M->data[(size_t)i * (size_t)M->cols + (size_t)j] = v;
}
static DenseMatrix random_dense(int r, int c, uint32_t seed) {
    DenseMatrix M = dense_create(r, c);
    uint32_t st = seed ? seed : 1u;
    for (size_t i = 0; i < (size_t)r * (size_t)c; ++i) M.data[i] = randm11(&st);
    return M;
}
static void dense_zero(DenseMatrix *M) {
    size_t n = (size_t)M->rows * (size_t)M->cols;
    for (size_t i = 0; i < n; ++i) M->data[i] = 0.0f;
}

// CSR sparse
typedef struct {
    int rows, cols;
    int *row_ptr;
    int *col_idx;
    float *values;
    int nnz;
} CSRMatrix;

static CSRMatrix csr_create_empty(int r, int c) {
    CSRMatrix A;
    A.rows = r; A.cols = c; A.nnz = 0;
    A.row_ptr = (int*)calloc((size_t)r + 1u, sizeof(int));
    A.col_idx = NULL; A.values = NULL;
    if (!A.row_ptr) { fprintf(stderr, "calloc csr row_ptr failed\n"); exit(1); }
    return A;
}
static void csr_free(CSRMatrix *A) {
    free(A->row_ptr); free(A->col_idx); free(A->values);
    A->row_ptr = NULL; A->col_idx = NULL; A->values = NULL;
    A->rows = A->cols = A->nnz = 0;
}

static CSRMatrix random_csr_uniform(int r, int c, float density, uint32_t seed) {
    assert(density > 0.0f && density <= 1.0f);
    CSRMatrix A = csr_create_empty(r, c);

    uint32_t st_struct = seed ? seed : 1u;
    uint32_t st_val    = (seed ? seed : 1u) ^ 0x9e3779b9u;

    int *row_nnz = (int*)calloc((size_t)r, sizeof(int));
    if (!row_nnz) { fprintf(stderr, "calloc row_nnz failed\n"); exit(1); }

    for (int i = 0; i < r; ++i) {
        int cnt = 0;
        for (int j = 0; j < c; ++j) if (rand01(&st_struct) < density) cnt++;
        row_nnz[i] = cnt;
    }

    A.row_ptr[0] = 0;
    for (int i = 0; i < r; ++i) A.row_ptr[i + 1] = A.row_ptr[i] + row_nnz[i];
    A.nnz = A.row_ptr[r];

    A.col_idx = (int*)malloc((size_t)A.nnz * sizeof(int));
    A.values  = (float*)malloc((size_t)A.nnz * sizeof(float));
    if (A.nnz > 0 && (!A.col_idx || !A.values)) {
        fprintf(stderr, "malloc csr arrays failed\n"); exit(1);
    }

    st_struct = seed ? seed : 1u;

    for (int i = 0; i < r; ++i) {
        int base = A.row_ptr[i];
        int off = 0;
        for (int j = 0; j < c; ++j) {
            if (rand01(&st_struct) < density) {
                A.col_idx[base + off] = j;
                A.values[base + off]  = randm11(&st_val);
                off++;
            }
        }
        assert(off == row_nnz[i]);
    }

    free(row_nnz);
    return A;
}


// SIMD+THREAD 
static void dense_gemm_baseline(const DenseMatrix *A,
                               const DenseMatrix *B,
                               DenseMatrix *C) {
    assert(A->cols == B->rows);
    assert(C->rows == A->rows && C->cols == B->cols);

    const int M = A->rows;
    const int K = A->cols;
    const int N = B->cols;

    #pragma omp parallel for schedule(static) 
    for (int i = 0; i < M; ++i) {
        float *c_row = &C->data[(size_t)i * (size_t)N];
        for (int k = 0; k < K; ++k) {
            const float a = A->data[(size_t)i * (size_t)K + (size_t)k];
            const __m256 a8 = _mm256_set1_ps(a);

            const float *b_row = &B->data[(size_t)k * (size_t)N];

            int j = 0;
            for (; j + 7 < N; j += 8) {
                __m256 c8 = _mm256_loadu_ps(c_row + j);
                __m256 b8 = _mm256_loadu_ps(b_row + j);
#if defined(__FMA__)
                c8 = _mm256_fmadd_ps(a8, b8, c8);
#else
                c8 = _mm256_add_ps(c8, _mm256_mul_ps(a8, b8));
#endif
                _mm256_storeu_ps(c_row + j, c8);
            }
            for (; j < N; ++j) {
                c_row[j] += a * b_row[j];
            }
        }
    }
}

static void csr_spmm_baseline(const CSRMatrix *A,
                             const DenseMatrix *B,
                             DenseMatrix *C) {
    assert(A->cols == B->rows);
    assert(C->rows == A->rows && C->cols == B->cols);

    const int M = A->rows;
    const int N = B->cols;

    #pragma omp parallel for schedule(static) 
    for (int i = 0; i < M; ++i) {
        float *c_row = &C->data[(size_t)i * (size_t)N];

        for (int idx = A->row_ptr[i]; idx < A->row_ptr[i + 1]; ++idx) {
            const int jrow = A->col_idx[idx];
            const float a  = A->values[idx];
            const __m256 a8 = _mm256_set1_ps(a);

            const float *b_row = &B->data[(size_t)jrow * (size_t)N];

            int col = 0;
            for (; col + 7 < N; col += 8) {
                __m256 c8 = _mm256_loadu_ps(c_row + col);
                __m256 b8 = _mm256_loadu_ps(b_row + col);
#if defined(__FMA__)
                c8 = _mm256_fmadd_ps(a8, b8, c8);
#else
                c8 = _mm256_add_ps(c8, _mm256_mul_ps(a8, b8));
#endif
                _mm256_storeu_ps(c_row + col, c8);
            }
            for (; col < N; ++col) {
                c_row[col] += a * b_row[col];
            }
        }
    }
}

static DenseMatrix csr_to_dense(const CSRMatrix *A) {
    DenseMatrix D = dense_create(A->rows, A->cols);
    for (int i = 0; i < A->rows; ++i) {
        for (int idx = A->row_ptr[i]; idx < A->row_ptr[i + 1]; ++idx) {
            dense_set(&D, i, A->col_idx[idx], A->values[idx]);
        }
    }
    return D;
}

// error check
static float max_absolute_error(const DenseMatrix *X, const DenseMatrix *Y) {
    assert(X->rows == Y->rows && X->cols == Y->cols);
    float maxerr = 0.0f;
    for (int i = 0; i < X->rows; ++i) {
        for (int j = 0; j < X->cols; ++j) {
            float diff = fabsf(dense_get(X, i, j) - dense_get(Y, i, j));
            if (diff > maxerr) maxerr = diff;
        }
    }
    return maxerr;
}

static void blas_sgemm_ref(const DenseMatrix *A,
                           const DenseMatrix *B,
                           DenseMatrix *C) {
    assert(A->cols == B->rows);
    assert(C->rows == A->rows && C->cols == B->cols);
    dense_zero(C);

    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                A->rows, B->cols, A->cols,
                1.0f,
                A->data, A->cols,
                B->data, B->cols,
                0.0f,
                C->data, C->cols);
}

int main(int argc, char **argv) {
    int m = 512, k = 512, n = 512;
    float density = 0.01f;
    int repeats = 3;

    int threads = 0;

    if (argc >= 4) { m = atoi(argv[1]); k = atoi(argv[2]); n = atoi(argv[3]); }
    if (argc >= 5) density = (float)atof(argv[4]);
    if (argc >= 6) repeats = atoi(argv[5]);
    if (argc >= 7) threads = atoi(argv[6]);  

    if (threads > 0) omp_set_num_threads(threads);  

    printf("Exp1 SIMD + multithreading (AVX2 + OpenMP)\n");  
    printf("m=%d k=%d n=%d, density=%.4f, repeats=%d, threads=%d\n",
           m, k, n, density, repeats,
           (threads > 0 ? threads : omp_get_max_threads()));  

    DenseMatrix B = random_dense(k, n, 42u);
    CSRMatrix A_csr = random_csr_uniform(m, k, density, 43u);

    DenseMatrix A_dense_from_csr = csr_to_dense(&A_csr);

    {
        DenseMatrix A_test = random_dense(m, k, 100u);
        DenseMatrix C_my   = dense_create(m, n);
        DenseMatrix C_blas = dense_create(m, n);

        dense_zero(&C_my);
        dense_gemm_baseline(&A_test, &B, &C_my);

        blas_sgemm_ref(&A_test, &B, &C_blas);

        float err = max_absolute_error(&C_my, &C_blas);
        printf("CHECK dense_err=%.8g\n", err);

        dense_free(&A_test);
        dense_free(&C_my);
        dense_free(&C_blas);
    }

    {
        DenseMatrix C_spmm = dense_create(m, n);
        DenseMatrix C_blas = dense_create(m, n);

        dense_zero(&C_spmm);
        csr_spmm_baseline(&A_csr, &B, &C_spmm);

        blas_sgemm_ref(&A_dense_from_csr, &B, &C_blas);

        float err = max_absolute_error(&C_spmm, &C_blas);
        printf("CHECK spmm_err=%.8g\n", err);

        dense_free(&C_spmm);
        dense_free(&C_blas);
    }

    double sum_gemm = 0.0;
    for (int t = 0; t < repeats; ++t) {
        DenseMatrix A = random_dense(m, k, (uint32_t)(200 + t));
        DenseMatrix C = dense_create(m, n);
        dense_zero(&C);

        double t0 = now_sec();
        dense_gemm_baseline(&A, &B, &C);
        double t1 = now_sec();

        sum_gemm += (t1 - t0);

        dense_free(&A);
        dense_free(&C);
    }
    double avg_gemm = sum_gemm / (double)repeats;
    double flops = 2.0 * (double)m * (double)n * (double)k;
    double gflops = (flops / avg_gemm) / 1e9;

    printf("\n[Dense GEMM SIMD+threads]\n");
    printf("GEMM avg runtime = %.6f s\n", avg_gemm);
    printf("GFLOP/s     = %.3f\n", gflops);

    double sum_spmm = 0.0;
    for (int t = 0; t < repeats; ++t) {
        DenseMatrix C = dense_create(m, n);
        dense_zero(&C);

        double t0 = now_sec();
        csr_spmm_baseline(&A_csr, &B, &C);
        double t1 = now_sec();

        sum_spmm += (t1 - t0);
        dense_free(&C);
    }

    double avg_spmm = sum_spmm / (double)repeats;
    printf("\n[CSR SpMM SIMD+threads]\n");
    printf("SpMM avg runtime = %.6f s\n", avg_spmm);
    printf("nnz         = %d\n", A_csr.nnz);

    dense_free(&A_dense_from_csr);
    dense_free(&B);
    csr_free(&A_csr);

    return 0;
}
