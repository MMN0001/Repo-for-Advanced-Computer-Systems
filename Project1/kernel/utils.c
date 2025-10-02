#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>

void saxpy_f32(float  a, const float*  x, float*  y, size_t M, size_t stride);
void saxpy_f64(double a, const double* x, double* y, size_t M, size_t stride);
double dot_f32(const float*  x, const float*  y, size_t M, size_t stride);
double dot_f64(const double* x, const double* y, size_t M, size_t stride);
void elemul_f32(const float*  x, const float*  y, float*  z, size_t M, size_t stride);
void elemul_f64(const double* x, const double* y, double* z, size_t M, size_t stride);

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int aligned_alloc_wrap(void** p, size_t align, size_t bytes) {
    return posix_memalign(p, align, bytes);
}

static double max_relative_error_f32_at_stride(const float* y, const float* yref,
                                               size_t M, size_t stride, double eps) {
    double m = 0.0;
    for (size_t i = 0; i < M; ++i) {
        size_t j = i * stride;
        double num = fabs((double)y[j] - (double)yref[j]);
        double den = fabs((double)yref[j]) + eps;
        double r = num / den;
        if (r > m) m = r;
    }
    return m;
}
static double max_relative_error_f64_at_stride(const double* y, const double* yref,
                                               size_t M, size_t stride, double eps) {
    double m = 0.0;
    for (size_t i = 0; i < M; ++i) {
        size_t j = i * stride;
        double num = fabs(y[j] - yref[j]);
        double den = fabs(yref[j]) + eps;
        double r = num / den;
        if (r > m) m = r;
    }
    return m;
}

static void saxpy_ref_f32(float a, const float* x, const float* y0, float* yref, size_t N) {
    double ad = (double)a;
    for (size_t i = 0; i < N; ++i) {
        yref[i] = (float)(ad * (double)x[i] + (double)y0[i]);
    }
}
static void saxpy_ref_f64(double a, const double* x, const double* y0, double* yref, size_t N) {
    for (size_t i = 0; i < N; ++i) {
        yref[i] = a * x[i] + y0[i];
    }
}
static void elemul_ref_f32(const float* x, const float* y, float* zref, size_t N) {
    for (size_t i = 0; i < N; ++i) zref[i] = x[i] * y[i];
}
static void elemul_ref_f64(const double* x, const double* y, double* zref, size_t N) {
    for (size_t i = 0; i < N; ++i) zref[i] = x[i] * y[i];
}
static double dot_ref_f32(const float* x, const float* y, size_t N) {
    double acc = 0.0;
    for (size_t i = 0; i < N; ++i) acc += (double)x[i] * (double)y[i];
    return acc;
}
static double dot_ref_f64(const double* x, const double* y, size_t N) {
    double acc = 0.0;
    for (size_t i = 0; i < N; ++i) acc += x[i] * y[i];
    return acc;
}

// ---- args ----
typedef struct {
    const char* kernel;       
    size_t  M;                 
    size_t  stride;            
    const char* dtype;         
    const char* align_mode;    
    size_t  misalign_bytes;    
    int     repeats;           
    double  rtol;             
    const char* csv;           
    unsigned seed;            
    double  a_val;            
} Args;

static void parse_args(int argc, char** argv, Args* A) {
    A->kernel = "saxpy";
    A->M = (size_t)1 << 20;
    A->stride = 1;
    A->dtype = "f32";
    A->align_mode = "aligned";
    A->misalign_bytes = 0;
    A->repeats = 10;
    A->rtol = 1e-6;
    A->csv = NULL;
    A->seed = 30;
    A->a_val = 3.1415;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--kernel") && i+1 < argc) A->kernel = argv[++i];
        else if (!strcmp(argv[i], "--M") && i+1 < argc) A->M = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--stride") && i+1 < argc) A->stride = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--dtype") && i+1 < argc) A->dtype = argv[++i];
        else if (!strcmp(argv[i], "--align") && i+1 < argc) A->align_mode = argv[++i];
        else if (!strcmp(argv[i], "--misalign_bytes") && i+1 < argc) A->misalign_bytes = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--repeats") && i+1 < argc) A->repeats = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rtol") && i+1 < argc) A->rtol = atof(argv[++i]);
        else if (!strcmp(argv[i], "--csv") && i+1 < argc) A->csv = argv[++i];
        else if (!strcmp(argv[i], "--seed") && i+1 < argc) A->seed = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--a") && i+1 < argc) A->a_val = atof(argv[++i]);
        else if (!strcmp(argv[i], "--help")) {
            fprintf(stderr,
                "Usage: %s [--kernel saxpy|dot|elemul] [--M iters] [--stride s]\n"
                "          [--dtype f32|f64] [--align aligned|misaligned]\n"
                "          [--misalign_bytes B] [--repeats k] [--rtol tol]\n"
                "          [--csv path] [--seed N] [--a val]\n", argv[0]);
            exit(0);
        }
    }
}

static size_t required_length_from_stride(size_t M, size_t stride) {
    if (M == 0) return 0;
    return (M - 1) * stride + 1;
}

int main(int argc, char** argv) {
    Args A; parse_args(argc, argv, &A);

#ifndef BUILD_LABEL
#  define BUILD_LABEL "scalar"
#endif
    const char* BUILD = BUILD_LABEL;

    FILE* out = stdout;
    int need_header = 0;

    if (A.csv) {
        out = fopen(A.csv, "a");  
        if (!out) { fprintf(stderr, "open CSV fail: %s\n", strerror(errno)); return 1; }

        fseek(out, 0, SEEK_END);
        long sz = ftell(out);
        if (sz == 0) need_header = 1;
    } else {
        need_header = 1;
    }

    if (need_header) {
        fprintf(out, "kernel,build,dtype,M,stride,align,misalign_bytes,time_s,run_id,result\n");
    }


    srand(A.seed);

    int is_f32 = !strcmp(A.dtype, "f32");
    int is_f64 = !strcmp(A.dtype, "f64");
    if (!is_f32 && !is_f64) {
        fprintf(stderr, "Unknown --dtype. Use f32 or f64.\n");
        if (out != stdout) fclose(out);
        return 1;
    }

    size_t N_touch = required_length_from_stride(A.M, A.stride);

    if (is_f32) {
        const size_t B = 4;
        size_t extra = 64 + (A.misalign_bytes + B - 1) / B;
        size_t N_alloc = N_touch + extra;

        void* xb=NULL,* yb=NULL,* zb=NULL;
        if (aligned_alloc_wrap(&xb, 64, N_alloc*B) || aligned_alloc_wrap(&yb, 64, N_alloc*B)) {
            fprintf(stderr, "posix_memalign failed\n");
            if (out != stdout) fclose(out);
            return 1;
        }
        if (aligned_alloc_wrap(&zb, 64, N_alloc*B)) {
            fprintf(stderr, "posix_memalign failed (z)\n");
            if (out != stdout) fclose(out);
            return 1;
        }

        float* x = (float*)xb;
        float* y = (float*)yb;
        float* z = (float*)zb;
        if (!strcmp(A.align_mode, "misaligned")) {
            x = (float*)((char*)xb + A.misalign_bytes);
            y = (float*)((char*)yb + A.misalign_bytes);
            z = (float*)((char*)zb + A.misalign_bytes);
        }

        for (size_t i = 0; i < N_alloc; ++i) {
            float r1 = (float)rand() / (float)RAND_MAX;
            float r2 = (float)rand() / (float)RAND_MAX;
            x[i] = 2.0f * r1 - 1.0f;
            y[i] = 2.0f * r2 - 1.0f;
            z[i] = 0.0f;
        }

        float* y0    = (float*)malloc(N_touch*sizeof(float));
        float* yref  = (float*)malloc(N_touch*sizeof(float));
        float* zref  = (float*)malloc(N_touch*sizeof(float));
        if (!y0 || !yref || !zref) { fprintf(stderr, "alloc ref buffers fail\n"); return 1; }
        for (size_t i = 0; i < N_touch; ++i) {
            y0[i] = y[i];
            zref[i] = 0.0f;
        }
        saxpy_ref_f32((float)A.a_val, x, y0, yref, N_touch);
        elemul_ref_f32(x, y, zref, N_touch);
        double dot_ref = dot_ref_f32(x, y, N_touch);

        if (!strcmp(A.kernel, "saxpy")) {
            saxpy_f32((float)A.a_val, x, y, A.M, A.stride);
            for (size_t i = 0; i < N_touch; ++i) y[i] = y0[i];
        } else if (!strcmp(A.kernel, "elemul")) {
            elemul_f32(x, y, z, A.M, A.stride);
            for (size_t i = 0; i < N_touch; ++i) z[i] = 0.0f;
        } else if (!strcmp(A.kernel, "dot")) {
            volatile double warm = dot_f32(x, y, A.M, A.stride);
            (void)warm;
        } else {
            fprintf(stderr, "Unknown --kernel. Use saxpy|dot|elemul.\n");
            if (out != stdout) fclose(out);
            return 1;
        }

        for (int r = 0; r < A.repeats; ++r) {
            for (size_t i = 0; i < N_touch; ++i) {
                y[i] = y0[i];
                z[i] = 0.0f;
            }

            double t0 = now_seconds();
            double result = 0.0; 
            if (!strcmp(A.kernel, "saxpy")) {
                saxpy_f32((float)A.a_val, x, y, A.M, A.stride);
            } else if (!strcmp(A.kernel, "elemul")) {
                elemul_f32(x, y, z, A.M, A.stride);
            } else { 
                result = dot_f32(x, y, A.M, A.stride);
            }
            double t1 = now_seconds();

            if (!strcmp(A.kernel, "saxpy")) {
                double max_rel = max_relative_error_f32_at_stride(y, yref, A.M, A.stride, 1e-12);
                if (max_rel > A.rtol) {
                    fprintf(stderr, "[WARN] f32 saxpy run %d: max_rel=%.3e > rtol=%.3e\n", r, max_rel, A.rtol);
                }
            } else if (!strcmp(A.kernel, "elemul")) {
                double max_rel = max_relative_error_f32_at_stride(z, zref, A.M, A.stride, 1e-12);
                if (max_rel > A.rtol) {
                    fprintf(stderr, "[WARN] f32 elemul run %d: max_rel=%.3e > rtol=%.3e\n", r, max_rel, A.rtol);
                }
            } else { 
                double rel = fabs(result - dot_ref) / (fabs(dot_ref) + 1e-12);
                if (rel > A.rtol) {
                    fprintf(stderr, "[WARN] f32 dot run %d: rel=%.3e > rtol=%.3e (res=%.6e ref=%.6e)\n",
                            r, rel, A.rtol, result, dot_ref);
                }
            }

            if (!strcmp(A.kernel, "dot")) {
                fprintf(out, "%s,%s,f32,%zu,%zu,%s,%zu,%.9f,%d,%.17g\n",
                        A.kernel, BUILD, A.M, A.stride, A.align_mode, A.misalign_bytes, (t1 - t0), r, result);
            } else {
                fprintf(out, "%s,%s,f32,%zu,%zu,%s,%zu,%.9f,%d,\n",
                        A.kernel, BUILD, A.M, A.stride, A.align_mode, A.misalign_bytes, (t1 - t0), r);
            }
        }

        free(y0); free(yref); free(zref);
        free(xb); free(yb); free(zb);
    }
    else {
        const size_t B = 8;
        size_t extra = 64 + (A.misalign_bytes + B - 1) / B;
        size_t N_alloc = N_touch + extra;

        void* xb=NULL,* yb=NULL,* zb=NULL;
        if (aligned_alloc_wrap(&xb, 64, N_alloc*B) || aligned_alloc_wrap(&yb, 64, N_alloc*B)) {
            fprintf(stderr, "posix_memalign failed\n");
            if (out != stdout) fclose(out);
            return 1;
        }
        if (aligned_alloc_wrap(&zb, 64, N_alloc*B)) {
            fprintf(stderr, "posix_memalign failed (z)\n");
            if (out != stdout) fclose(out);
            return 1;
        }

        double* x = (double*)xb;
        double* y = (double*)yb;
        double* z = (double*)zb;
        if (!strcmp(A.align_mode, "misaligned")) {
            x = (double*)((char*)xb + A.misalign_bytes);
            y = (double*)((char*)yb + A.misalign_bytes);
            z = (double*)((char*)zb + A.misalign_bytes);
        }

        for (size_t i = 0; i < N_alloc; ++i) {
            double r1 = (double)rand() / (double)RAND_MAX;
            double r2 = (double)rand() / (double)RAND_MAX;
            x[i] = 2.0*r1 - 1.0;
            y[i] = 2.0*r2 - 1.0;
            z[i] = 0.0;
        }

        double* y0   = (double*)malloc(N_touch*sizeof(double));
        double* yref = (double*)malloc(N_touch*sizeof(double));
        double* zref = (double*)malloc(N_touch*sizeof(double));
        if (!y0 || !yref || !zref) { fprintf(stderr, "alloc ref buffers fail\n"); return 1; }
        for (size_t i = 0; i < N_touch; ++i) {
            y0[i] = y[i];
            zref[i] = 0.0;
        }
        saxpy_ref_f64(A.a_val, x, y0, yref, N_touch);
        elemul_ref_f64(x, y, zref, N_touch);
        double dot_ref = dot_ref_f64(x, y, N_touch);

        if (!strcmp(A.kernel, "saxpy")) {
            saxpy_f64(A.a_val, x, y, A.M, A.stride);
            for (size_t i = 0; i < N_touch; ++i) y[i] = y0[i];
        } else if (!strcmp(A.kernel, "elemul")) {
            elemul_f64(x, y, z, A.M, A.stride);
            for (size_t i = 0; i < N_touch; ++i) z[i] = 0.0;
        } else if (!strcmp(A.kernel, "dot")) {
            volatile double warm = dot_f64(x, y, A.M, A.stride);
            (void)warm;
        } else {
            fprintf(stderr, "Unknown --kernel. Use saxpy|dot|elemul.\n");
            if (out != stdout) fclose(out);
            return 1;
        }

        for (int r = 0; r < A.repeats; ++r) {
            for (size_t i = 0; i < N_touch; ++i) {
                y[i] = y0[i];
                z[i] = 0.0;
            }

            double t0 = now_seconds();
            double result = 0.0;
            if (!strcmp(A.kernel, "saxpy")) {
                saxpy_f64(A.a_val, x, y, A.M, A.stride);
            } else if (!strcmp(A.kernel, "elemul")) {
                elemul_f64(x, y, z, A.M, A.stride);
            } else { // dot
                result = dot_f64(x, y, A.M, A.stride);
            }
            double t1 = now_seconds();

            if (!strcmp(A.kernel, "saxpy")) {
                double max_rel = max_relative_error_f64_at_stride(y, yref, A.M, A.stride, 1e-12);
                if (max_rel > A.rtol) {
                    fprintf(stderr, "[WARN] f64 saxpy run %d: max_rel=%.3e > rtol=%.3e\n", r, max_rel, A.rtol);
                }
            } else if (!strcmp(A.kernel, "elemul")) {
                double max_rel = max_relative_error_f64_at_stride(z, zref, A.M, A.stride, 1e-12);
                if (max_rel > A.rtol) {
                    fprintf(stderr, "[WARN] f64 elemul run %d: max_rel=%.3e > rtol=%.3e\n", r, max_rel, A.rtol);
                }
            } else {
                double rel = fabs(result - dot_ref) / (fabs(dot_ref) + 1e-12);
                if (rel > A.rtol) {
                    fprintf(stderr, "[WARN] f64 dot run %d: rel=%.3e > rtol=%.3e (res=%.6e ref=%.6e)\n",
                            r, rel, A.rtol, result, dot_ref);
                }
            }

            if (!strcmp(A.kernel, "dot")) {
                fprintf(out, "%s,%s,f64,%zu,%zu,%s,%zu,%.9f,%d,%.17g\n",
                        A.kernel, BUILD, A.M, A.stride, A.align_mode, A.misalign_bytes, (t1 - t0), r, result);
            } else {
                fprintf(out, "%s,%s,f64,%zu,%zu,%s,%zu,%.9f,%d,\n",
                        A.kernel, BUILD, A.M, A.stride, A.align_mode, A.misalign_bytes, (t1 - t0), r);
            }
        }

        free(y0); free(yref); free(zref);
        free(xb); free(yb); free(zb);
    }

    if (out != stdout) fclose(out);
    return 0;
}
