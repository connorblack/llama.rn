#pragma once

#include "ggml-cpu-impl.h"

#ifdef __ARM_FEATURE_SVE
#include <arm_sve.h>
#endif // __ARM_FEATURE_SVE

#if defined(__ARM_NEON) && !defined(__CUDACC__) && !defined(__MUSACC__)
// if YCM cannot find <arm_neon.h>, make a symbolic link to it, for example:
//
//   $ ln -sfn /Library/Developer/CommandLineTools/usr/lib/clang/13.1.6/include/arm_neon.h ./src/
//
#include <arm_neon.h>
#endif

#if defined(__F16C__)
#include <immintrin.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

//
// simd mappings
//

// FP16 to FP32 conversion

// 16-bit float
// on Arm, we use __fp16
// on x86, we use uint16_t
//
// for old CUDA compilers (<= 11), we use uint16_t: ref https://github.com/ggml-org/llama.cpp/pull/10616
// for     MUSA compilers        , we use uint16_t: ref https://github.com/ggml-org/llama.cpp/pull/11843
//
#if defined(__ARM_NEON) && !(defined(__CUDACC__) && __CUDACC_VER_MAJOR__ <= 11) && !defined(__MUSACC__)
    #define LM_GGML_CPU_COMPUTE_FP16_TO_FP32(x) neon_compute_fp16_to_fp32(x)
    #define LM_GGML_CPU_COMPUTE_FP32_TO_FP16(x) neon_compute_fp32_to_fp16(x)

    #define LM_GGML_CPU_FP16_TO_FP32(x) LM_GGML_CPU_COMPUTE_FP16_TO_FP32(x)

    static inline float neon_compute_fp16_to_fp32(lm_ggml_fp16_t h) {
        __fp16 tmp;
        memcpy(&tmp, &h, sizeof(lm_ggml_fp16_t));
        return (float)tmp;
    }

    static inline lm_ggml_fp16_t neon_compute_fp32_to_fp16(float f) {
        lm_ggml_fp16_t res;
        __fp16 tmp = f;
        memcpy(&res, &tmp, sizeof(lm_ggml_fp16_t));
        return res;
    }
#elif defined(__F16C__)
    #ifdef _MSC_VER
        #define LM_GGML_CPU_COMPUTE_FP16_TO_FP32(x) _mm_cvtss_f32(_mm_cvtph_ps(_mm_cvtsi32_si128(x)))
        #define LM_GGML_CPU_COMPUTE_FP32_TO_FP16(x) _mm_extract_epi16(_mm_cvtps_ph(_mm_set_ss(x), 0), 0)
    #else
        #define LM_GGML_CPU_COMPUTE_FP16_TO_FP32(x) _cvtsh_ss(x)
        #define LM_GGML_CPU_COMPUTE_FP32_TO_FP16(x) _cvtss_sh(x, 0)
    #endif
#elif defined(__POWER9_VECTOR__)
    #define LM_GGML_CPU_COMPUTE_FP16_TO_FP32(x) power_compute_fp16_to_fp32(x)
    #define LM_GGML_CPU_COMPUTE_FP32_TO_FP16(x) power_compute_fp32_to_fp16(x)
    /* the inline asm below is about 12% faster than the lookup method */
    #define LM_GGML_CPU_FP16_TO_FP32(x) LM_GGML_CPU_COMPUTE_FP16_TO_FP32(x)
    #define LM_GGML_CPU_FP32_TO_FP16(x) LM_GGML_CPU_COMPUTE_FP32_TO_FP16(x)

    static inline float power_compute_fp16_to_fp32(lm_ggml_fp16_t h) {
        float f;
        double d;
        __asm__(
            "mtfprd %0,%2\n"
            "xscvhpdp %0,%0\n"
            "frsp %1,%0\n" :
            /* temp */ "=d"(d),
            /* out */  "=f"(f):
            /* in */   "r"(h));
        return f;
    }

    static inline lm_ggml_fp16_t power_compute_fp32_to_fp16(float f) {
        double d;
        lm_ggml_fp16_t r;
        __asm__( /* xscvdphp can work on double or single precision */
            "xscvdphp %0,%2\n"
            "mffprd %1,%0\n" :
            /* temp */ "=d"(d),
            /* out */  "=r"(r):
            /* in */   "f"(f));
        return r;
    }
#elif defined(__riscv) && defined(__riscv_zfhmin)
    static inline float riscv_compute_fp16_to_fp32(lm_ggml_fp16_t h) {
        float f;
        __asm__(
            "fmv.h.x %[f], %[h]\n\t"
            "fcvt.s.h %[f], %[f]"
            : [f] "=&f" (f)
            : [h] "r" (h)
        );
        return f;
    }

    static inline lm_ggml_fp16_t riscv_compute_fp32_to_fp16(float f) {
        lm_ggml_fp16_t res;
        __asm__(
            "fcvt.h.s %[f], %[f]\n\t"
            "fmv.x.h %[h], %[f]"
            : [h] "=&r" (res)
            : [f] "f" (f)
        );
        return res;
    }

    #define LM_GGML_CPU_COMPUTE_FP16_TO_FP32(x) riscv_compute_fp16_to_fp32(x)
    #define LM_GGML_CPU_COMPUTE_FP32_TO_FP16(x) riscv_compute_fp32_to_fp16(x)
    #define LM_GGML_CPU_FP16_TO_FP32(x) LM_GGML_CPU_COMPUTE_FP16_TO_FP32(x)
    #define LM_GGML_CPU_FP32_TO_FP16(x) LM_GGML_CPU_COMPUTE_FP32_TO_FP16(x)
#elif defined(__NNPA__)
    #define LM_GGML_CPU_COMPUTE_FP16_TO_FP32(x) nnpa_compute_fp16_to_fp32(x)
    #define LM_GGML_CPU_COMPUTE_FP32_TO_FP16(x) nnpa_compute_fp32_to_fp16(x)

    #define LM_GGML_CPU_FP16_TO_FP32(x) LM_GGML_CPU_COMPUTE_FP16_TO_FP32(x)
    #define LM_GGML_CPU_FP32_TO_FP16(x) LM_GGML_CPU_COMPUTE_FP32_TO_FP16(x)

    static inline float nnpa_compute_fp16_to_fp32(lm_ggml_fp16_t h) {
        uint16x8_t v_h = vec_splats(h);
        uint16x8_t v_hd = vec_convert_from_fp16(v_h, 0);
        return vec_extend_to_fp32_hi(v_hd, 0)[0];
    }

    static inline lm_ggml_fp16_t nnpa_compute_fp32_to_fp16(float f) {
        float32x4_t v_f = vec_splats(f);
        float32x4_t v_zero = vec_splats(0.0f);
        uint16x8_t v_hd = vec_round_from_fp32(v_f, v_zero, 0);
        uint16x8_t v_h = vec_convert_to_fp16(v_hd, 0);
        return vec_extract(v_h, 0);
    }
#endif

// precomputed f32 table for f16 (256 KB)
// defined in ggml-cpu.c, initialized in lm_ggml_cpu_init()
extern float lm_ggml_table_f32_f16[1 << 16];

// On ARM NEON, it's quicker to directly convert x -> x instead of calling into lm_ggml_lookup_fp16_to_fp32,
// so we define LM_GGML_CPU_FP16_TO_FP32 and LM_GGML_CPU_FP32_TO_FP16 elsewhere for NEON.
// This is also true for POWER9.
#if !defined(LM_GGML_CPU_FP16_TO_FP32)
inline static float lm_ggml_lookup_fp16_to_fp32(lm_ggml_fp16_t f) {
    uint16_t s;
    memcpy(&s, &f, sizeof(uint16_t));
    return lm_ggml_table_f32_f16[s];
}

#define LM_GGML_CPU_FP16_TO_FP32(x) lm_ggml_lookup_fp16_to_fp32(x)
#endif

#if !defined(LM_GGML_CPU_FP32_TO_FP16)
#define LM_GGML_CPU_FP32_TO_FP16(x) LM_GGML_COMPUTE_FP32_TO_FP16(x)
#endif


// we define a common set of C macros which map to specific intrinsics based on the current architecture
// we then implement the fundamental computation operations below using only these macros
// adding support for new architectures requires to define the corresponding SIMD macros
//
// LM_GGML_F32_STEP / LM_GGML_F16_STEP
//   number of elements to process in a single step
//
// LM_GGML_F32_EPR / LM_GGML_F16_EPR
//   number of elements to fit in a single register
//

#if defined(__ARM_FEATURE_SVE) && defined(__ARM_FEATURE_FMA)

#define LM_GGML_SIMD

// F32 SVE
#define LM_GGML_F32_EPR 8
#define DEFAULT_PG svptrue_b32()

#define LM_GGML_F32xt                        svfloat32_t
#define LM_GGML_F32xt_ZERO                   svdup_n_f32(0.0f)
#define LM_GGML_F32xt_SET1(x)                svdup_n_f32(x)
#define LM_GGML_F32xt_LOAD_IMPL(pg, a, ...)  svld1_f32(pg, a)
#define LM_GGML_F32xt_LOAD(...)              LM_GGML_F32xt_LOAD_IMPL(DEFAULT_PG, __VA_ARGS__)
#define LM_GGML_F32xt_STORE_IMPL(pg,a,b)     svst1_f32(pg, a, b)
#define LM_GGML_F32xt_STORE(...)             LM_GGML_F32xt_STORE_IMPL(DEFAULT_PG, __VA_ARGS__)
#define LM_GGML_F32xt_FMA_IMPL(pg, a, b, c)  svmad_f32_m(pg, b, c, a)
#define LM_GGML_F32xt_FMA(...)               LM_GGML_F32xt_FMA_IMPL(DEFAULT_PG, __VA_ARGS__)
#define LM_GGML_F32xt_ADD_IMPL(pg, a, b)     svadd_f32_m(pg, a, b)
#define LM_GGML_F32xt_ADD(...)               LM_GGML_F32xt_ADD_IMPL(DEFAULT_PG, __VA_ARGS__)
#define LM_GGML_F32xt_MUL_IMPL(pg, a, b)     svmul_f32_m(pg, a, b)
#define LM_GGML_F32xt_MUL(...)               LM_GGML_F32xt_MUL_IMPL(DEFAULT_PG, __VA_ARGS__)
#define LM_GGML_F32xt_REDUCE_ONE_IMPL(pg, a) svaddv(pg, a)
#define LM_GGML_F32xt_REDUCE_ONE(...)        LM_GGML_F32xt_REDUCE_ONE_IMPL(DEFAULT_PG, __VA_ARGS__)
#define LM_GGML_F32xt_REDUCE_IMPL(pg, res, sum1, sum2, sum3, sum4, sum5, sum6, sum7, sum8)  \
{                                                      \
    sum1 = svadd_f32_m(DEFAULT_PG, sum1, sum2);        \
    sum3 = svadd_f32_m(DEFAULT_PG, sum3, sum4);        \
    sum5 = svadd_f32_m(DEFAULT_PG, sum5, sum6);        \
    sum7 = svadd_f32_m(DEFAULT_PG, sum7, sum8);        \
    sum1 = svadd_f32_m(DEFAULT_PG, sum1, sum3);        \
    sum5 = svadd_f32_m(DEFAULT_PG, sum5, sum7);        \
    sum1 = svadd_f32_m(DEFAULT_PG, sum1, sum5);        \
    (res) = (lm_ggml_float) LM_GGML_F32xt_REDUCE_ONE(sum1);  \
}
#define LM_GGML_F32xt_REDUCE(...) LM_GGML_F32xt_REDUCE_IMPL(DEFAULT_PG, __VA_ARGS__)

#define LM_GGML_F32_VEC        LM_GGML_F32xt
#define LM_GGML_F32_VEC_ZERO   LM_GGML_F32xt_ZERO
#define LM_GGML_F32_VEC_SET1   LM_GGML_F32xt_SET1
#define LM_GGML_F32_VEC_LOAD   LM_GGML_F32xt_LOAD
#define LM_GGML_F32_VEC_STORE  LM_GGML_F32xt_STORE
#define LM_GGML_F32_VEC_FMA    LM_GGML_F32xt_FMA
#define LM_GGML_F32_VEC_ADD    LM_GGML_F32xt_ADD
#define LM_GGML_F32_VEC_MUL    LM_GGML_F32xt_MUL
#define LM_GGML_F32_VEC_REDUCE LM_GGML_F32xt_REDUCE

// F16 NEON

#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    #define LM_GGML_F16_STEP 32
    #define LM_GGML_F16_EPR  8

    #define LM_GGML_F16x8              float16x8_t
    #define LM_GGML_F16x8_ZERO         vdupq_n_f16(0.0f)
    #define LM_GGML_F16x8_SET1(x)      vdupq_n_f16(x)
    #define LM_GGML_F16x8_LOAD(x)      vld1q_f16((const __fp16 *)(x))
    #define LM_GGML_F16x8_STORE        vst1q_f16
    #define LM_GGML_F16x8_FMA(a, b, c) vfmaq_f16(a, b, c)
    #define LM_GGML_F16x8_ADD          vaddq_f16
    #define LM_GGML_F16x8_MUL          vmulq_f16
    #define LM_GGML_F16x8_REDUCE(res, x)                               \
    do {                                                            \
        int offset = LM_GGML_F16_ARR >> 1;                             \
        for (int i = 0; i < offset; ++i) {                          \
            (x)[i] = vaddq_f16((x)[i], (x)[offset+i]);              \
        }                                                           \
        offset >>= 1;                                               \
        for (int i = 0; i < offset; ++i) {                          \
            (x)[i] = vaddq_f16((x)[i], (x)[offset+i]);              \
        }                                                           \
        offset >>= 1;                                               \
        for (int i = 0; i < offset; ++i) {                          \
            (x)[i] = vaddq_f16((x)[i], (x)[offset+i]);              \
        }                                                           \
        const float32x4_t t0 = vcvt_f32_f16(vget_low_f16 ((x)[0])); \
        const float32x4_t t1 = vcvt_f32_f16(vget_high_f16((x)[0])); \
        (res) = (lm_ggml_float) vaddvq_f32(vaddq_f32(t0, t1));         \
    } while (0)

    #define LM_GGML_F16_VEC                LM_GGML_F16x8
    #define LM_GGML_F16_VEC_ZERO           LM_GGML_F16x8_ZERO
    #define LM_GGML_F16_VEC_SET1           LM_GGML_F16x8_SET1
    #define LM_GGML_F16_VEC_LOAD(p, i)     LM_GGML_F16x8_LOAD(p)
    #define LM_GGML_F16_VEC_STORE(p, r, i) LM_GGML_F16x8_STORE((__fp16 *)(p), (r)[i])
    #define LM_GGML_F16_VEC_FMA            LM_GGML_F16x8_FMA
    #define LM_GGML_F16_VEC_ADD            LM_GGML_F16x8_ADD
    #define LM_GGML_F16_VEC_MUL            LM_GGML_F16x8_MUL
    #define LM_GGML_F16_VEC_REDUCE         LM_GGML_F16x8_REDUCE
#else
    // if FP16 vector arithmetic is not supported, we use FP32 instead
    // and take advantage of the vcvt_ functions to convert to/from FP16

    #define LM_GGML_F16_STEP 16
    #define LM_GGML_F16_EPR  4

    #define LM_GGML_F32Cx4              float32x4_t
    #define LM_GGML_F32Cx4_ZERO         vdupq_n_f32(0.0f)
    #define LM_GGML_F32Cx4_SET1(x)      vdupq_n_f32(x)
    #define LM_GGML_F32Cx4_LOAD(x)      vcvt_f32_f16(vld1_f16((const __fp16 *)(x)))
    #define LM_GGML_F32Cx4_STORE(x, y)  vst1_f16(x, vcvt_f16_f32(y))
    #define LM_GGML_F32Cx4_FMA(a, b, c) vfmaq_f32(a, b, c)
    #define LM_GGML_F32Cx4_ADD          vaddq_f32
    #define LM_GGML_F32Cx4_MUL          vmulq_f32
    #define LM_GGML_F32Cx4_REDUCE       LM_GGML_F32x4_REDUCE

    #define LM_GGML_F16_VEC                LM_GGML_F32Cx4
    #define LM_GGML_F16_VEC_ZERO           LM_GGML_F32Cx4_ZERO
    #define LM_GGML_F16_VEC_SET1           LM_GGML_F32Cx4_SET1
    #define LM_GGML_F16_VEC_LOAD(p, i)     LM_GGML_F32Cx4_LOAD(p)
    #define LM_GGML_F16_VEC_STORE(p, r, i) LM_GGML_F32Cx4_STORE((__fp16 *)(p), r[i])
    #define LM_GGML_F16_VEC_FMA            LM_GGML_F32Cx4_FMA
    #define LM_GGML_F16_VEC_ADD            LM_GGML_F32Cx4_ADD
    #define LM_GGML_F16_VEC_MUL            LM_GGML_F32Cx4_MUL
    #define LM_GGML_F16_VEC_REDUCE         LM_GGML_F32Cx4_REDUCE
#endif

#elif defined(__ARM_NEON) && defined(__ARM_FEATURE_FMA)

#define LM_GGML_SIMD

// F32 NEON

#define LM_GGML_F32_STEP 16
#define LM_GGML_F32_EPR  4

#define LM_GGML_F32x4              float32x4_t
#define LM_GGML_F32x4_ZERO         vdupq_n_f32(0.0f)
#define LM_GGML_F32x4_SET1(x)      vdupq_n_f32(x)
#define LM_GGML_F32x4_LOAD         vld1q_f32
#define LM_GGML_F32x4_STORE        vst1q_f32
#define LM_GGML_F32x4_FMA(a, b, c) vfmaq_f32(a, b, c)
#define LM_GGML_F32x4_ADD          vaddq_f32
#define LM_GGML_F32x4_MUL          vmulq_f32
#define LM_GGML_F32x4_REDUCE_ONE(x) vaddvq_f32(x)
#define LM_GGML_F32x4_REDUCE(res, x)                       \
{                                                       \
    int offset = LM_GGML_F32_ARR >> 1;                     \
    for (int i = 0; i < offset; ++i) {                  \
        (x)[i] = vaddq_f32((x)[i], (x)[offset+i]);      \
    }                                                   \
    offset >>= 1;                                       \
    for (int i = 0; i < offset; ++i) {                  \
        (x)[i] = vaddq_f32((x)[i], (x)[offset+i]);      \
    }                                                   \
    offset >>= 1;                                       \
    for (int i = 0; i < offset; ++i) {                  \
        (x)[i] = vaddq_f32((x)[i], (x)[offset+i]);      \
    }                                                   \
    (res) = (lm_ggml_float) LM_GGML_F32x4_REDUCE_ONE((x)[0]); \
}

#define LM_GGML_F32_VEC        LM_GGML_F32x4
#define LM_GGML_F32_VEC_ZERO   LM_GGML_F32x4_ZERO
#define LM_GGML_F32_VEC_SET1   LM_GGML_F32x4_SET1
#define LM_GGML_F32_VEC_LOAD   LM_GGML_F32x4_LOAD
#define LM_GGML_F32_VEC_STORE  LM_GGML_F32x4_STORE
#define LM_GGML_F32_VEC_FMA    LM_GGML_F32x4_FMA
#define LM_GGML_F32_VEC_ADD    LM_GGML_F32x4_ADD
#define LM_GGML_F32_VEC_MUL    LM_GGML_F32x4_MUL
#define LM_GGML_F32_VEC_REDUCE LM_GGML_F32x4_REDUCE

// F16 NEON

#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
    #define LM_GGML_F16_STEP 32
    #define LM_GGML_F16_EPR  8

    #define LM_GGML_F16x8              float16x8_t
    #define LM_GGML_F16x8_ZERO         vdupq_n_f16(0.0f)
    #define LM_GGML_F16x8_SET1(x)      vdupq_n_f16(x)
    #define LM_GGML_F16x8_LOAD(x)      vld1q_f16((const __fp16 *)(x))
    #define LM_GGML_F16x8_STORE        vst1q_f16
    #define LM_GGML_F16x8_FMA(a, b, c) vfmaq_f16(a, b, c)
    #define LM_GGML_F16x8_ADD          vaddq_f16
    #define LM_GGML_F16x8_MUL          vmulq_f16
    #define LM_GGML_F16x8_REDUCE(res, x)                               \
    do {                                                            \
        int offset = LM_GGML_F16_ARR >> 1;                             \
        for (int i = 0; i < offset; ++i) {                          \
            (x)[i] = vaddq_f16((x)[i], (x)[offset+i]);              \
        }                                                           \
        offset >>= 1;                                               \
        for (int i = 0; i < offset; ++i) {                          \
            (x)[i] = vaddq_f16((x)[i], (x)[offset+i]);              \
        }                                                           \
        offset >>= 1;                                               \
        for (int i = 0; i < offset; ++i) {                          \
            (x)[i] = vaddq_f16((x)[i], (x)[offset+i]);              \
        }                                                           \
        const float32x4_t t0 = vcvt_f32_f16(vget_low_f16 ((x)[0])); \
        const float32x4_t t1 = vcvt_f32_f16(vget_high_f16((x)[0])); \
        (res) = (lm_ggml_float) vaddvq_f32(vaddq_f32(t0, t1));         \
    } while (0)

    #define LM_GGML_F16_VEC                LM_GGML_F16x8
    #define LM_GGML_F16_VEC_ZERO           LM_GGML_F16x8_ZERO
    #define LM_GGML_F16_VEC_SET1           LM_GGML_F16x8_SET1
    #define LM_GGML_F16_VEC_LOAD(p, i)     LM_GGML_F16x8_LOAD(p)
    #define LM_GGML_F16_VEC_STORE(p, r, i) LM_GGML_F16x8_STORE((__fp16 *)(p), (r)[i])
    #define LM_GGML_F16_VEC_FMA            LM_GGML_F16x8_FMA
    #define LM_GGML_F16_VEC_ADD            LM_GGML_F16x8_ADD
    #define LM_GGML_F16_VEC_MUL            LM_GGML_F16x8_MUL
    #define LM_GGML_F16_VEC_REDUCE         LM_GGML_F16x8_REDUCE
#else
    // if FP16 vector arithmetic is not supported, we use FP32 instead
    // and take advantage of the vcvt_ functions to convert to/from FP16

    #define LM_GGML_F16_STEP 16
    #define LM_GGML_F16_EPR  4

    #define LM_GGML_F32Cx4              float32x4_t
    #define LM_GGML_F32Cx4_ZERO         vdupq_n_f32(0.0f)
    #define LM_GGML_F32Cx4_SET1(x)      vdupq_n_f32(x)
    #define LM_GGML_F32Cx4_LOAD(x)      vcvt_f32_f16(vld1_f16((const __fp16 *)(x)))
    #define LM_GGML_F32Cx4_STORE(x, y)  vst1_f16(x, vcvt_f16_f32(y))
    #define LM_GGML_F32Cx4_FMA(a, b, c) vfmaq_f32(a, b, c)
    #define LM_GGML_F32Cx4_ADD          vaddq_f32
    #define LM_GGML_F32Cx4_MUL          vmulq_f32
    #define LM_GGML_F32Cx4_REDUCE       LM_GGML_F32x4_REDUCE

    #define LM_GGML_F16_VEC                LM_GGML_F32Cx4
    #define LM_GGML_F16_VEC_ZERO           LM_GGML_F32Cx4_ZERO
    #define LM_GGML_F16_VEC_SET1           LM_GGML_F32Cx4_SET1
    #define LM_GGML_F16_VEC_LOAD(p, i)     LM_GGML_F32Cx4_LOAD(p)
    #define LM_GGML_F16_VEC_STORE(p, r, i) LM_GGML_F32Cx4_STORE((__fp16 *)(p), r[i])
    #define LM_GGML_F16_VEC_FMA            LM_GGML_F32Cx4_FMA
    #define LM_GGML_F16_VEC_ADD            LM_GGML_F32Cx4_ADD
    #define LM_GGML_F16_VEC_MUL            LM_GGML_F32Cx4_MUL
    #define LM_GGML_F16_VEC_REDUCE         LM_GGML_F32Cx4_REDUCE
#endif

#elif defined(__AVX512F__)

#define LM_GGML_SIMD

// F32 AVX512

#define LM_GGML_F32_STEP 64
#define LM_GGML_F32_EPR  16

#define LM_GGML_F32x16         __m512
#define LM_GGML_F32x16_ZERO    _mm512_setzero_ps()
#define LM_GGML_F32x16_SET1(x) _mm512_set1_ps(x)
#define LM_GGML_F32x16_LOAD    _mm512_loadu_ps
#define LM_GGML_F32x16_STORE   _mm512_storeu_ps
// _mm512_fmadd_ps is defined in AVX512F so no guard is required
#define LM_GGML_F32x16_FMA(a, b, c) _mm512_fmadd_ps(b, c, a)
#define LM_GGML_F32x16_ADD     _mm512_add_ps
#define LM_GGML_F32x16_MUL     _mm512_mul_ps
#define LM_GGML_F32x16_REDUCE(res, x)                                    \
do {                                                                  \
    int offset = LM_GGML_F32_ARR >> 1;                                   \
    for (int i = 0; i < offset; ++i) {                                \
        x[i] = _mm512_add_ps(x[i], x[offset+i]);                      \
    }                                                                 \
    offset >>= 1;                                                     \
    for (int i = 0; i < offset; ++i) {                                \
        x[i] = _mm512_add_ps(x[i], x[offset+i]);                      \
    }                                                                 \
    offset >>= 1;                                                     \
    for (int i = 0; i < offset; ++i) {                                \
        x[i] = _mm512_add_ps(x[i], x[offset+i]);                      \
    }                                                                 \
    res = (lm_ggml_float) _mm512_reduce_add_ps(x[0]);                    \
} while (0)

// TODO: is this optimal ?

#define LM_GGML_F32_VEC        LM_GGML_F32x16
#define LM_GGML_F32_VEC_ZERO   LM_GGML_F32x16_ZERO
#define LM_GGML_F32_VEC_SET1   LM_GGML_F32x16_SET1
#define LM_GGML_F32_VEC_LOAD   LM_GGML_F32x16_LOAD
#define LM_GGML_F32_VEC_STORE  LM_GGML_F32x16_STORE
#define LM_GGML_F32_VEC_FMA    LM_GGML_F32x16_FMA
#define LM_GGML_F32_VEC_ADD    LM_GGML_F32x16_ADD
#define LM_GGML_F32_VEC_MUL    LM_GGML_F32x16_MUL
#define LM_GGML_F32_VEC_REDUCE LM_GGML_F32x16_REDUCE

// F16 AVX512

// F16 AVX

#define LM_GGML_F16_STEP 64
#define LM_GGML_F16_EPR  16

// AVX512 has FP16 extension (AVX512_FP16) but I don't have it on my machine so I use FP32 instead

#define LM_GGML_F32Cx16             __m512
#define LM_GGML_F32Cx16_ZERO        _mm512_setzero_ps()
#define LM_GGML_F32Cx16_SET1(x)     _mm512_set1_ps(x)

// unlike  _mm256_cvt intrinsics that require F16C, _mm512_cvt is defined in AVX512F
// so F16C guard isn't required
#define LM_GGML_F32Cx16_LOAD(x)     _mm512_cvtph_ps(_mm256_loadu_si256((const __m256i *)(x)))
#define LM_GGML_F32Cx16_STORE(x, y) _mm256_storeu_si256((__m256i *)(x), _mm512_cvtps_ph(y, 0))

#define LM_GGML_F32Cx16_FMA(a, b, c) _mm512_fmadd_ps(b, c, a)
#define LM_GGML_F32Cx16_ADD         _mm512_add_ps
#define LM_GGML_F32Cx16_MUL         _mm512_mul_ps
#define LM_GGML_F32Cx16_REDUCE(res, x)                               \
do {                                                              \
    int offset = LM_GGML_F32_ARR >> 1;                               \
    for (int i = 0; i < offset; ++i) {                            \
        x[i] = _mm512_add_ps(x[i], x[offset+i]);                  \
    }                                                             \
    offset >>= 1;                                                 \
    for (int i = 0; i < offset; ++i) {                            \
        x[i] = _mm512_add_ps(x[i], x[offset+i]);                  \
    }                                                             \
    offset >>= 1;                                                 \
    for (int i = 0; i < offset; ++i) {                            \
        x[i] = _mm512_add_ps(x[i], x[offset+i]);                  \
    }                                                             \
    res = (lm_ggml_float) _mm512_reduce_add_ps(x[0]);                \
} while (0)

#define LM_GGML_F16_VEC                LM_GGML_F32Cx16
#define LM_GGML_F16_VEC_ZERO           LM_GGML_F32Cx16_ZERO
#define LM_GGML_F16_VEC_SET1           LM_GGML_F32Cx16_SET1
#define LM_GGML_F16_VEC_LOAD(p, i)     LM_GGML_F32Cx16_LOAD(p)
#define LM_GGML_F16_VEC_STORE(p, r, i) LM_GGML_F32Cx16_STORE(p, r[i])
#define LM_GGML_F16_VEC_FMA            LM_GGML_F32Cx16_FMA
#define LM_GGML_F16_VEC_ADD            LM_GGML_F32Cx16_ADD
#define LM_GGML_F16_VEC_MUL            LM_GGML_F32Cx16_MUL

#define LM_GGML_F16_VEC_REDUCE         LM_GGML_F32Cx16_REDUCE
#elif defined(__AVX__)

#define LM_GGML_SIMD

// F32 AVX

#define LM_GGML_F32_STEP 32
#define LM_GGML_F32_EPR  8

#define LM_GGML_F32x8         __m256
#define LM_GGML_F32x8_ZERO    _mm256_setzero_ps()
#define LM_GGML_F32x8_SET1(x) _mm256_set1_ps(x)
#define LM_GGML_F32x8_LOAD    _mm256_loadu_ps
#define LM_GGML_F32x8_STORE   _mm256_storeu_ps
#if defined(__FMA__)
    #define LM_GGML_F32x8_FMA(a, b, c) _mm256_fmadd_ps(b, c, a)
#else
    #define LM_GGML_F32x8_FMA(a, b, c) _mm256_add_ps(_mm256_mul_ps(b, c), a)
#endif
#define LM_GGML_F32x8_ADD     _mm256_add_ps
#define LM_GGML_F32x8_MUL     _mm256_mul_ps
#define LM_GGML_F32x8_REDUCE(res, x)                                 \
do {                                                              \
    int offset = LM_GGML_F32_ARR >> 1;                               \
    for (int i = 0; i < offset; ++i) {                            \
        x[i] = _mm256_add_ps(x[i], x[offset+i]);                  \
    }                                                             \
    offset >>= 1;                                                 \
    for (int i = 0; i < offset; ++i) {                            \
        x[i] = _mm256_add_ps(x[i], x[offset+i]);                  \
    }                                                             \
    offset >>= 1;                                                 \
    for (int i = 0; i < offset; ++i) {                            \
        x[i] = _mm256_add_ps(x[i], x[offset+i]);                  \
    }                                                             \
    const __m128 t0 = _mm_add_ps(_mm256_castps256_ps128(x[0]),    \
                                 _mm256_extractf128_ps(x[0], 1)); \
    const __m128 t1 = _mm_hadd_ps(t0, t0);                        \
    res = (lm_ggml_float) _mm_cvtss_f32(_mm_hadd_ps(t1, t1));        \
} while (0)
// TODO: is this optimal ?

#define LM_GGML_F32_VEC        LM_GGML_F32x8
#define LM_GGML_F32_VEC_ZERO   LM_GGML_F32x8_ZERO
#define LM_GGML_F32_VEC_SET1   LM_GGML_F32x8_SET1
#define LM_GGML_F32_VEC_LOAD   LM_GGML_F32x8_LOAD
#define LM_GGML_F32_VEC_STORE  LM_GGML_F32x8_STORE
#define LM_GGML_F32_VEC_FMA    LM_GGML_F32x8_FMA
#define LM_GGML_F32_VEC_ADD    LM_GGML_F32x8_ADD
#define LM_GGML_F32_VEC_MUL    LM_GGML_F32x8_MUL
#define LM_GGML_F32_VEC_REDUCE LM_GGML_F32x8_REDUCE

// F16 AVX

#define LM_GGML_F16_STEP 32
#define LM_GGML_F16_EPR  8

// F16 arithmetic is not supported by AVX, so we use F32 instead

#define LM_GGML_F32Cx8             __m256
#define LM_GGML_F32Cx8_ZERO        _mm256_setzero_ps()
#define LM_GGML_F32Cx8_SET1(x)     _mm256_set1_ps(x)

#if defined(__F16C__)
// the  _mm256_cvt intrinsics require F16C
#define LM_GGML_F32Cx8_LOAD(x)     _mm256_cvtph_ps(_mm_loadu_si128((const __m128i *)(x)))
#define LM_GGML_F32Cx8_STORE(x, y) _mm_storeu_si128((__m128i *)(x), _mm256_cvtps_ph(y, 0))
#else
static inline __m256 __avx_f32cx8_load(const lm_ggml_fp16_t * x) {
    float tmp[8];

    for (int i = 0; i < 8; i++) {
        tmp[i] = LM_GGML_CPU_FP16_TO_FP32(x[i]);
    }

    return _mm256_loadu_ps(tmp);
}
static inline void __avx_f32cx8_store(lm_ggml_fp16_t *x, __m256 y) {
    float arr[8];

    _mm256_storeu_ps(arr, y);

    for (int i = 0; i < 8; i++)
        x[i] = LM_GGML_CPU_FP32_TO_FP16(arr[i]);
}
#define LM_GGML_F32Cx8_LOAD(x)     __avx_f32cx8_load(x)
#define LM_GGML_F32Cx8_STORE(x, y) __avx_f32cx8_store(x, y)
#endif

#define LM_GGML_F32Cx8_FMA         LM_GGML_F32x8_FMA
#define LM_GGML_F32Cx8_ADD         _mm256_add_ps
#define LM_GGML_F32Cx8_MUL         _mm256_mul_ps
#define LM_GGML_F32Cx8_REDUCE      LM_GGML_F32x8_REDUCE

#define LM_GGML_F16_VEC                LM_GGML_F32Cx8
#define LM_GGML_F16_VEC_ZERO           LM_GGML_F32Cx8_ZERO
#define LM_GGML_F16_VEC_SET1           LM_GGML_F32Cx8_SET1
#define LM_GGML_F16_VEC_LOAD(p, i)     LM_GGML_F32Cx8_LOAD(p)
#define LM_GGML_F16_VEC_STORE(p, r, i) LM_GGML_F32Cx8_STORE(p, r[i])
#define LM_GGML_F16_VEC_FMA            LM_GGML_F32Cx8_FMA
#define LM_GGML_F16_VEC_ADD            LM_GGML_F32Cx8_ADD
#define LM_GGML_F16_VEC_MUL            LM_GGML_F32Cx8_MUL
#define LM_GGML_F16_VEC_REDUCE         LM_GGML_F32Cx8_REDUCE

#elif defined(__POWER9_VECTOR__)

#define LM_GGML_SIMD

// F32 POWER9

#define LM_GGML_F32_STEP 32
#define LM_GGML_F32_EPR  4

#define LM_GGML_F32x4              vector float
#define LM_GGML_F32x4_ZERO         {0.0f}
#define LM_GGML_F32x4_SET1         vec_splats
#define LM_GGML_F32x4_LOAD(p)      vec_xl(0, p)
#define LM_GGML_F32x4_STORE(p, r)  vec_xst(r, 0, p)
#define LM_GGML_F32x4_FMA(a, b, c) vec_madd(b, c, a)
#define LM_GGML_F32x4_ADD          vec_add
#define LM_GGML_F32x4_MUL          vec_mul
#define LM_GGML_F32x4_REDUCE(res, x)              \
{                                              \
    int offset = LM_GGML_F32_ARR >> 1;            \
    for (int i = 0; i < offset; ++i) {         \
        x[i] = vec_add(x[i], x[offset+i]);     \
    }                                          \
    offset >>= 1;                              \
    for (int i = 0; i < offset; ++i) {         \
        x[i] = vec_add(x[i], x[offset+i]);     \
    }                                          \
    offset >>= 1;                              \
    for (int i = 0; i < offset; ++i) {         \
        x[i] = vec_add(x[i], x[offset+i]);     \
    }                                          \
    res = vec_extract(x[0], 0) +               \
          vec_extract(x[0], 1) +               \
          vec_extract(x[0], 2) +               \
          vec_extract(x[0], 3);                \
}

#define LM_GGML_F32_VEC        LM_GGML_F32x4
#define LM_GGML_F32_VEC_ZERO   LM_GGML_F32x4_ZERO
#define LM_GGML_F32_VEC_SET1   LM_GGML_F32x4_SET1
#define LM_GGML_F32_VEC_LOAD   LM_GGML_F32x4_LOAD
#define LM_GGML_F32_VEC_STORE  LM_GGML_F32x4_STORE
#define LM_GGML_F32_VEC_FMA    LM_GGML_F32x4_FMA
#define LM_GGML_F32_VEC_ADD    LM_GGML_F32x4_ADD
#define LM_GGML_F32_VEC_MUL    LM_GGML_F32x4_MUL
#define LM_GGML_F32_VEC_REDUCE LM_GGML_F32x4_REDUCE

// F16 POWER9
#define LM_GGML_F16_STEP       LM_GGML_F32_STEP
#define LM_GGML_F16_EPR        LM_GGML_F32_EPR
#define LM_GGML_F16_VEC        LM_GGML_F32x4
#define LM_GGML_F16_VEC_ZERO   LM_GGML_F32x4_ZERO
#define LM_GGML_F16_VEC_SET1   LM_GGML_F32x4_SET1
#define LM_GGML_F16_VEC_FMA    LM_GGML_F32x4_FMA
#define LM_GGML_F16_VEC_ADD    LM_GGML_F32x4_ADD
#define LM_GGML_F16_VEC_MUL    LM_GGML_F32x4_MUL
#define LM_GGML_F16_VEC_REDUCE LM_GGML_F32x4_REDUCE
// Use vec_xl, not vec_ld, in case the load address is not aligned.
#define LM_GGML_F16_VEC_LOAD(p, i) (i & 0x1) ?                   \
  vec_extract_fp32_from_shorth(vec_xl(0, p - LM_GGML_F16_EPR)) : \
  vec_extract_fp32_from_shortl(vec_xl(0, p))
static inline unsigned char lm_ggml_endian_byte(int i) {
       uint16_t tmp_val = 1;
       return ((unsigned char *)&tmp_val)[i];
}
#define LM_GGML_ENDIAN_BYTE(i) lm_ggml_endian_byte(i)
#define LM_GGML_F16_VEC_STORE(p, r, i)                             \
  if (i & 0x1)                                                  \
    vec_xst(vec_pack_to_short_fp32(r[i - LM_GGML_ENDIAN_BYTE(1)],  \
                                   r[i - LM_GGML_ENDIAN_BYTE(0)]), \
            0, p - LM_GGML_F16_EPR)

#elif defined(__wasm_simd128__)

#define LM_GGML_SIMD

// F32 WASM

#define LM_GGML_F32_STEP 16
#define LM_GGML_F32_EPR  4

#define LM_GGML_F32x4              v128_t
#define LM_GGML_F32x4_ZERO         wasm_f32x4_splat(0.0f)
#define LM_GGML_F32x4_SET1(x)      wasm_f32x4_splat(x)
#define LM_GGML_F32x4_LOAD         wasm_v128_load
#define LM_GGML_F32x4_STORE        wasm_v128_store
#define LM_GGML_F32x4_FMA(a, b, c) wasm_f32x4_add(wasm_f32x4_mul(b, c), a)
#define LM_GGML_F32x4_ADD          wasm_f32x4_add
#define LM_GGML_F32x4_MUL          wasm_f32x4_mul
#define LM_GGML_F32x4_REDUCE(res, x)                  \
{                                                  \
    int offset = LM_GGML_F32_ARR >> 1;                \
    for (int i = 0; i < offset; ++i) {             \
        x[i] = wasm_f32x4_add(x[i], x[offset+i]);  \
    }                                              \
    offset >>= 1;                                  \
    for (int i = 0; i < offset; ++i) {             \
        x[i] = wasm_f32x4_add(x[i], x[offset+i]);  \
    }                                              \
    offset >>= 1;                                  \
    for (int i = 0; i < offset; ++i) {             \
        x[i] = wasm_f32x4_add(x[i], x[offset+i]);  \
    }                                              \
    res = wasm_f32x4_extract_lane(x[0], 0) +       \
          wasm_f32x4_extract_lane(x[0], 1) +       \
          wasm_f32x4_extract_lane(x[0], 2) +       \
          wasm_f32x4_extract_lane(x[0], 3);        \
}

#define LM_GGML_F32_VEC        LM_GGML_F32x4
#define LM_GGML_F32_VEC_ZERO   LM_GGML_F32x4_ZERO
#define LM_GGML_F32_VEC_SET1   LM_GGML_F32x4_SET1
#define LM_GGML_F32_VEC_LOAD   LM_GGML_F32x4_LOAD
#define LM_GGML_F32_VEC_STORE  LM_GGML_F32x4_STORE
#define LM_GGML_F32_VEC_FMA    LM_GGML_F32x4_FMA
#define LM_GGML_F32_VEC_ADD    LM_GGML_F32x4_ADD
#define LM_GGML_F32_VEC_MUL    LM_GGML_F32x4_MUL
#define LM_GGML_F32_VEC_REDUCE LM_GGML_F32x4_REDUCE

// F16 WASM

#define LM_GGML_F16_STEP 16
#define LM_GGML_F16_EPR  4

inline static v128_t __wasm_f16x4_load(const lm_ggml_fp16_t * p) {
    float tmp[4];

    tmp[0] = LM_GGML_CPU_FP16_TO_FP32(p[0]);
    tmp[1] = LM_GGML_CPU_FP16_TO_FP32(p[1]);
    tmp[2] = LM_GGML_CPU_FP16_TO_FP32(p[2]);
    tmp[3] = LM_GGML_CPU_FP16_TO_FP32(p[3]);

    return wasm_v128_load(tmp);
}

inline static void __wasm_f16x4_store(lm_ggml_fp16_t * p, v128_t x) {
    float tmp[4];

    wasm_v128_store(tmp, x);

    p[0] = LM_GGML_CPU_FP32_TO_FP16(tmp[0]);
    p[1] = LM_GGML_CPU_FP32_TO_FP16(tmp[1]);
    p[2] = LM_GGML_CPU_FP32_TO_FP16(tmp[2]);
    p[3] = LM_GGML_CPU_FP32_TO_FP16(tmp[3]);
}

#define LM_GGML_F16x4             v128_t
#define LM_GGML_F16x4_ZERO        wasm_f32x4_splat(0.0f)
#define LM_GGML_F16x4_SET1(x)     wasm_f32x4_splat(x)
#define LM_GGML_F16x4_LOAD(x)     __wasm_f16x4_load(x)
#define LM_GGML_F16x4_STORE(x, y) __wasm_f16x4_store(x, y)
#define LM_GGML_F16x4_FMA         LM_GGML_F32x4_FMA
#define LM_GGML_F16x4_ADD         wasm_f32x4_add
#define LM_GGML_F16x4_MUL         wasm_f32x4_mul
#define LM_GGML_F16x4_REDUCE(res, x)                           \
{                                                           \
    int offset = LM_GGML_F16_ARR >> 1;                         \
    for (int i = 0; i < offset; ++i) {                      \
        x[i] = wasm_f32x4_add(x[i], x[offset+i]);           \
    }                                                       \
    offset >>= 1;                                           \
    for (int i = 0; i < offset; ++i) {                      \
        x[i] = wasm_f32x4_add(x[i], x[offset+i]);           \
    }                                                       \
    offset >>= 1;                                           \
    for (int i = 0; i < offset; ++i) {                      \
        x[i] = wasm_f32x4_add(x[i], x[offset+i]);           \
    }                                                       \
    res = (lm_ggml_float) (wasm_f32x4_extract_lane(x[0], 0) +  \
          wasm_f32x4_extract_lane(x[0], 1) +                \
          wasm_f32x4_extract_lane(x[0], 2) +                \
          wasm_f32x4_extract_lane(x[0], 3));                \
}

#define LM_GGML_F16_VEC                LM_GGML_F16x4
#define LM_GGML_F16_VEC_ZERO           LM_GGML_F16x4_ZERO
#define LM_GGML_F16_VEC_SET1           LM_GGML_F16x4_SET1
#define LM_GGML_F16_VEC_LOAD(p, i)     LM_GGML_F16x4_LOAD(p)
#define LM_GGML_F16_VEC_STORE(p, r, i) LM_GGML_F16x4_STORE(p, r[i])
#define LM_GGML_F16_VEC_FMA            LM_GGML_F16x4_FMA
#define LM_GGML_F16_VEC_ADD            LM_GGML_F16x4_ADD
#define LM_GGML_F16_VEC_MUL            LM_GGML_F16x4_MUL
#define LM_GGML_F16_VEC_REDUCE         LM_GGML_F16x4_REDUCE

#elif defined(__SSE3__)

#define LM_GGML_SIMD

// F32 SSE

#define LM_GGML_F32_STEP 32
#define LM_GGML_F32_EPR  4

#define LM_GGML_F32x4         __m128
#define LM_GGML_F32x4_ZERO    _mm_setzero_ps()
#define LM_GGML_F32x4_SET1(x) _mm_set1_ps(x)
#define LM_GGML_F32x4_LOAD    _mm_loadu_ps
#define LM_GGML_F32x4_STORE   _mm_storeu_ps
#if defined(__FMA__)
    // TODO: Does this work?
    #define LM_GGML_F32x4_FMA(a, b, c) _mm_fmadd_ps(b, c, a)
#else
    #define LM_GGML_F32x4_FMA(a, b, c) _mm_add_ps(_mm_mul_ps(b, c), a)
#endif
#define LM_GGML_F32x4_ADD     _mm_add_ps
#define LM_GGML_F32x4_MUL     _mm_mul_ps
#define LM_GGML_F32x4_REDUCE(res, x)                                 \
{                                                                 \
    int offset = LM_GGML_F32_ARR >> 1;                               \
    for (int i = 0; i < offset; ++i) {                            \
        x[i] = _mm_add_ps(x[i], x[offset+i]);                     \
    }                                                             \
    offset >>= 1;                                                 \
    for (int i = 0; i < offset; ++i) {                            \
        x[i] = _mm_add_ps(x[i], x[offset+i]);                     \
    }                                                             \
    offset >>= 1;                                                 \
    for (int i = 0; i < offset; ++i) {                            \
        x[i] = _mm_add_ps(x[i], x[offset+i]);                     \
    }                                                             \
    const __m128 t0 = _mm_hadd_ps(x[0], x[0]);                    \
    res = (lm_ggml_float) _mm_cvtss_f32(_mm_hadd_ps(t0, t0));        \
}
// TODO: is this optimal ?

#define LM_GGML_F32_VEC        LM_GGML_F32x4
#define LM_GGML_F32_VEC_ZERO   LM_GGML_F32x4_ZERO
#define LM_GGML_F32_VEC_SET1   LM_GGML_F32x4_SET1
#define LM_GGML_F32_VEC_LOAD   LM_GGML_F32x4_LOAD
#define LM_GGML_F32_VEC_STORE  LM_GGML_F32x4_STORE
#define LM_GGML_F32_VEC_FMA    LM_GGML_F32x4_FMA
#define LM_GGML_F32_VEC_ADD    LM_GGML_F32x4_ADD
#define LM_GGML_F32_VEC_MUL    LM_GGML_F32x4_MUL
#define LM_GGML_F32_VEC_REDUCE LM_GGML_F32x4_REDUCE

// F16 SSE

#define LM_GGML_F16_STEP 32
#define LM_GGML_F16_EPR  4

static inline __m128 __sse_f16x4_load(const lm_ggml_fp16_t * x) {
    float tmp[4];

    tmp[0] = LM_GGML_CPU_FP16_TO_FP32(x[0]);
    tmp[1] = LM_GGML_CPU_FP16_TO_FP32(x[1]);
    tmp[2] = LM_GGML_CPU_FP16_TO_FP32(x[2]);
    tmp[3] = LM_GGML_CPU_FP16_TO_FP32(x[3]);

    return _mm_loadu_ps(tmp);
}

static inline void __sse_f16x4_store(lm_ggml_fp16_t * x, __m128 y) {
    float arr[4];

    _mm_storeu_ps(arr, y);

    x[0] = LM_GGML_CPU_FP32_TO_FP16(arr[0]);
    x[1] = LM_GGML_CPU_FP32_TO_FP16(arr[1]);
    x[2] = LM_GGML_CPU_FP32_TO_FP16(arr[2]);
    x[3] = LM_GGML_CPU_FP32_TO_FP16(arr[3]);
}

#define LM_GGML_F32Cx4             __m128
#define LM_GGML_F32Cx4_ZERO        _mm_setzero_ps()
#define LM_GGML_F32Cx4_SET1(x)     _mm_set1_ps(x)
#define LM_GGML_F32Cx4_LOAD(x)     __sse_f16x4_load(x)
#define LM_GGML_F32Cx4_STORE(x, y) __sse_f16x4_store(x, y)
#define LM_GGML_F32Cx4_FMA         LM_GGML_F32x4_FMA
#define LM_GGML_F32Cx4_ADD         _mm_add_ps
#define LM_GGML_F32Cx4_MUL         _mm_mul_ps
#define LM_GGML_F32Cx4_REDUCE      LM_GGML_F32x4_REDUCE

#define LM_GGML_F16_VEC                 LM_GGML_F32Cx4
#define LM_GGML_F16_VEC_ZERO            LM_GGML_F32Cx4_ZERO
#define LM_GGML_F16_VEC_SET1            LM_GGML_F32Cx4_SET1
#define LM_GGML_F16_VEC_LOAD(p, i)      LM_GGML_F32Cx4_LOAD(p)
#define LM_GGML_F16_VEC_STORE(p, r, i)  LM_GGML_F32Cx4_STORE(p, r[i])
#define LM_GGML_F16_VEC_FMA             LM_GGML_F32Cx4_FMA
#define LM_GGML_F16_VEC_ADD             LM_GGML_F32Cx4_ADD
#define LM_GGML_F16_VEC_MUL             LM_GGML_F32Cx4_MUL
#define LM_GGML_F16_VEC_REDUCE          LM_GGML_F32Cx4_REDUCE

#elif defined(__loongarch_asx)

#define LM_GGML_SIMD

// F32 LASX
#define LM_GGML_F32_STEP 32
#define LM_GGML_F32_EPR  8

#define LM_GGML_F32x8         __m256
#define LM_GGML_F32x8_ZERO    (__m256)__lasx_xvldi(0)
#define LM_GGML_F32x8_SET1(x) (__m256)__lasx_xvreplfr2vr_s((x))
#define LM_GGML_F32x8_LOAD(x) (__m256)__lasx_xvld((x), 0)
#define LM_GGML_F32x8_STORE(x,y)   __lasx_xvst((y), (x), 0)
#define LM_GGML_F32x8_FMA(a, b, c) __lasx_xvfmadd_s(b, c, a)
#define LM_GGML_F32x8_ADD     __lasx_xvfadd_s
#define LM_GGML_F32x8_MUL     __lasx_xvfmul_s
#define LM_GGML_F32x8_REDUCE(res, x)                                 \
do {                                                              \
    int offset = LM_GGML_F32_ARR >> 1;                               \
    for (int i = 0; i < offset; ++i) {                            \
        x[i] = __lasx_xvfadd_s(x[i], x[offset+i]);                  \
    }                                                             \
    offset >>= 1;                                                 \
    for (int i = 0; i < offset; ++i) {                            \
        x[i] = __lasx_xvfadd_s(x[i], x[offset+i]);                  \
    }                                                             \
    offset >>= 1;                                                 \
    for (int i = 0; i < offset; ++i) {                            \
        x[i] = __lasx_xvfadd_s(x[i], x[offset+i]);                  \
    }                                                             \
    float *tmp_p = (float *)&x[0]; \
    res = tmp_p[0] + tmp_p[1] + tmp_p[2] + tmp_p[3] + tmp_p[4] + tmp_p[5] + tmp_p[6] + tmp_p[7];  \
} while (0)
// TODO: is this optimal ?

#define LM_GGML_F32_VEC        LM_GGML_F32x8
#define LM_GGML_F32_VEC_ZERO   LM_GGML_F32x8_ZERO
#define LM_GGML_F32_VEC_SET1   LM_GGML_F32x8_SET1
#define LM_GGML_F32_VEC_LOAD   LM_GGML_F32x8_LOAD
#define LM_GGML_F32_VEC_STORE  LM_GGML_F32x8_STORE
#define LM_GGML_F32_VEC_FMA    LM_GGML_F32x8_FMA
#define LM_GGML_F32_VEC_ADD    LM_GGML_F32x8_ADD
#define LM_GGML_F32_VEC_MUL    LM_GGML_F32x8_MUL
#define LM_GGML_F32_VEC_REDUCE LM_GGML_F32x8_REDUCE

// F16 LASX

#define LM_GGML_F16_STEP 32
#define LM_GGML_F16_EPR  8

// F16 arithmetic is not supported by LASX, so we use F32 instead

#define LM_GGML_F32Cx8          __m256
#define LM_GGML_F32Cx8_ZERO    (__m256)__lasx_xvldi(0)
#define LM_GGML_F32Cx8_SET1(x) (__m256)__lasx_xvreplgr2vr_w((x))

static inline __m256 __lasx_f32cx8_load(const lm_ggml_fp16_t * x) {
    __m256i a;
    memcpy(&a, x, sizeof(lm_ggml_fp16_t) * 8);
    a = __lasx_xvpermi_d(a, 0 | (1 << 4));
    return __lasx_xvfcvtl_s_h(a);
}

static inline void __lasx_f32cx8_store(lm_ggml_fp16_t * x, __m256 y) {
    __m256i a = __lasx_xvfcvt_h_s(y, y);
    a = __lasx_xvpermi_d(a, 0 | (2 << 2));
    memcpy(x, &a, sizeof(lm_ggml_fp16_t) * 8);
}
#define LM_GGML_F32Cx8_LOAD(x)     __lasx_f32cx8_load(x)
#define LM_GGML_F32Cx8_STORE(x, y) __lasx_f32cx8_store(x, y)

#define LM_GGML_F32Cx8_FMA         LM_GGML_F32x8_FMA
#define LM_GGML_F32Cx8_ADD         __lasx_xvfadd_s
#define LM_GGML_F32Cx8_MUL         __lasx_xvfmul_s
#define LM_GGML_F32Cx8_REDUCE      LM_GGML_F32x8_REDUCE

#define LM_GGML_F16_VEC                LM_GGML_F32Cx8
#define LM_GGML_F16_VEC_ZERO           LM_GGML_F32Cx8_ZERO
#define LM_GGML_F16_VEC_SET1           LM_GGML_F32Cx8_SET1
#define LM_GGML_F16_VEC_LOAD(p, i)     LM_GGML_F32Cx8_LOAD(p)
#define LM_GGML_F16_VEC_STORE(p, r, i) LM_GGML_F32Cx8_STORE(p, r[i])
#define LM_GGML_F16_VEC_FMA            LM_GGML_F32Cx8_FMA
#define LM_GGML_F16_VEC_ADD            LM_GGML_F32Cx8_ADD
#define LM_GGML_F16_VEC_MUL            LM_GGML_F32Cx8_MUL
#define LM_GGML_F16_VEC_REDUCE         LM_GGML_F32Cx8_REDUCE

#elif defined(__loongarch_sx)

#define LM_GGML_SIMD

// F32 LSX

#define LM_GGML_F32_STEP 32
#define LM_GGML_F32_EPR  4

#define LM_GGML_F32x4         __m128
#define LM_GGML_F32x4_ZERO    __lsx_vldi(0)
#define LM_GGML_F32x4_SET1(x) __lsx_vinsgr2vr_w(__lsx_vldi(0),(x), 0)
#define LM_GGML_F32x4_LOAD(x) __lsx_vld((x), 0)
#define LM_GGML_F32x4_STORE(x, y)   __lsx_vst(y, x, 0)
#define LM_GGML_F32x4_FMA(a, b, c) __lsx_vfmadd_s(b, c, a)
#define LM_GGML_F32x4_ADD     __lsx_vfadd_s
#define LM_GGML_F32x4_MUL     __lsx_vfmul_s
#define LM_GGML_F32x4_REDUCE(res, x)                                                     \
{                                                                                     \
    int offset = LM_GGML_F32_ARR >> 1;                                                   \
    for (int i = 0; i < offset; ++i) {                                                \
        x[i] = __lsx_vfadd_s(x[i], x[offset + i]);                                    \
    }                                                                                 \
    offset >>= 1;                                                                     \
    for (int i = 0; i < offset; ++i) {                                                \
        x[i] = __lsx_vfadd_s(x[i], x[offset + i]);                                    \
    }                                                                                 \
    offset >>= 1;                                                                     \
    for (int i = 0; i < offset; ++i) {                                                \
        x[i] = __lsx_vfadd_s(x[i], x[offset + i]);                                    \
    }                                                                                 \
    __m128i tmp     = __lsx_vsrli_d((__m128i) x[0], 32);                              \
    tmp             = (__m128i) __lsx_vfadd_s((__m128) tmp, x[0]);                    \
    tmp             = __lsx_vpickev_w(__lsx_vldi(0), tmp);                            \
    const __m128 t0 = __lsx_vshuf4i_w(tmp, 0x88);                                     \
    tmp             = __lsx_vsrli_d((__m128i) t0, 32);                                \
    tmp             = (__m128i) __lsx_vfadd_s((__m128) tmp, t0);                      \
    tmp             = __lsx_vpickev_w(__lsx_vldi(0), tmp);                            \
    res             = (lm_ggml_float) __lsx_vpickve2gr_w(__lsx_vshuf4i_w(tmp, 0x88), 0); \
}

#define LM_GGML_F32_VEC        LM_GGML_F32x4
#define LM_GGML_F32_VEC_ZERO   LM_GGML_F32x4_ZERO
#define LM_GGML_F32_VEC_SET1   LM_GGML_F32x4_SET1
#define LM_GGML_F32_VEC_LOAD   LM_GGML_F32x4_LOAD
#define LM_GGML_F32_VEC_STORE  LM_GGML_F32x4_STORE
#define LM_GGML_F32_VEC_FMA    LM_GGML_F32x4_FMA
#define LM_GGML_F32_VEC_ADD    LM_GGML_F32x4_ADD
#define LM_GGML_F32_VEC_MUL    LM_GGML_F32x4_MUL
#define LM_GGML_F32_VEC_REDUCE LM_GGML_F32x4_REDUCE

// F16 LSX

#define LM_GGML_F16_STEP 32
#define LM_GGML_F16_EPR  4

static inline __m128 __lsx_f16x4_load(const lm_ggml_fp16_t * x) {
    float tmp[4];

    tmp[0] = LM_GGML_CPU_FP16_TO_FP32(x[0]);
    tmp[1] = LM_GGML_CPU_FP16_TO_FP32(x[1]);
    tmp[2] = LM_GGML_CPU_FP16_TO_FP32(x[2]);
    tmp[3] = LM_GGML_CPU_FP16_TO_FP32(x[3]);

    return __lsx_vld(tmp, 0);
}

static inline void __lsx_f16x4_store(lm_ggml_fp16_t * x, __m128 y) {
    float arr[4];

    __lsx_vst(y, arr, 0);

    x[0] = LM_GGML_CPU_FP32_TO_FP16(arr[0]);
    x[1] = LM_GGML_CPU_FP32_TO_FP16(arr[1]);
    x[2] = LM_GGML_CPU_FP32_TO_FP16(arr[2]);
    x[3] = LM_GGML_CPU_FP32_TO_FP16(arr[3]);
}

#define LM_GGML_F32Cx4             __m128
#define LM_GGML_F32Cx4_ZERO        __lsx_vldi(0)
#define LM_GGML_F32Cx4_SET1(x)     __lsx_vinsgr2vr_w(__lsx_vldi(0),(x), 0)
#define LM_GGML_F32Cx4_LOAD(x)     __lsx_f16x4_load(x)
#define LM_GGML_F32Cx4_STORE(x, y) __lsx_f16x4_store(x, y)
#define LM_GGML_F32Cx4_FMA         LM_GGML_F32x4_FMA
#define LM_GGML_F32Cx4_ADD         __lsx_vfadd_s
#define LM_GGML_F32Cx4_MUL         __lsx_vfmul_s
#define LM_GGML_F32Cx4_REDUCE      LM_GGML_F32x4_REDUCE

#define LM_GGML_F16_VEC                 LM_GGML_F32Cx4
#define LM_GGML_F16_VEC_ZERO            LM_GGML_F32Cx4_ZERO
#define LM_GGML_F16_VEC_SET1            LM_GGML_F32Cx4_SET1
#define LM_GGML_F16_VEC_LOAD(p, i)      LM_GGML_F32Cx4_LOAD(p)
#define LM_GGML_F16_VEC_STORE(p, r, i)  LM_GGML_F32Cx4_STORE(p, r[i])
#define LM_GGML_F16_VEC_FMA             LM_GGML_F32Cx4_FMA
#define LM_GGML_F16_VEC_ADD             LM_GGML_F32Cx4_ADD
#define LM_GGML_F16_VEC_MUL             LM_GGML_F32Cx4_MUL
#define LM_GGML_F16_VEC_REDUCE          LM_GGML_F32Cx4_REDUCE

#elif defined(__VXE__) || defined(__VXE2__)

#define LM_GGML_SIMD

// F32 s390x

#define LM_GGML_F32_STEP 32
#define LM_GGML_F32_EPR  4

#define LM_GGML_F32x4              float32x4_t
#define LM_GGML_F32x4_ZERO         vec_splats(0.0f)
#define LM_GGML_F32x4_SET1         vec_splats
#define LM_GGML_F32x4_LOAD(p)      vec_xl(0, p)
#define LM_GGML_F32x4_STORE(p, r)  vec_xst(r, 0, p)
#define LM_GGML_F32x4_FMA(a, b, c) vec_madd(b, c, a)
#define LM_GGML_F32x4_ADD          vec_add
#define LM_GGML_F32x4_MUL          vec_mul
#define LM_GGML_F32x4_REDUCE(res, x)                   \
{                                                   \
    int offset = LM_GGML_F32_ARR >> 1;                 \
    for (int i = 0; i < offset; ++i) {              \
        x[i] = vec_add(x[i], x[offset + i]);        \
    }                                               \
    offset >>= 1;                                   \
    for (int i = 0; i < offset; ++i) {              \
        x[i] = vec_add(x[i], x[offset + i]);        \
    }                                               \
    offset >>= 1;                                   \
    for (int i = 0; i < offset; ++i) {              \
        x[i] = vec_add(x[i], x[offset + i]);        \
    }                                               \
    float32x4_t tmp = x[0] + vec_reve(x[0]);        \
    res = tmp[0] + tmp[1];                          \
}

#define LM_GGML_F32_VEC        LM_GGML_F32x4
#define LM_GGML_F32_VEC_ZERO   LM_GGML_F32x4_ZERO
#define LM_GGML_F32_VEC_SET1   LM_GGML_F32x4_SET1
#define LM_GGML_F32_VEC_LOAD   LM_GGML_F32x4_LOAD
#define LM_GGML_F32_VEC_STORE  LM_GGML_F32x4_STORE
#define LM_GGML_F32_VEC_FMA    LM_GGML_F32x4_FMA
#define LM_GGML_F32_VEC_ADD    LM_GGML_F32x4_ADD
#define LM_GGML_F32_VEC_MUL    LM_GGML_F32x4_MUL
#define LM_GGML_F32_VEC_REDUCE LM_GGML_F32x4_REDUCE

// F16 s390x
#define LM_GGML_F16_STEP LM_GGML_F32_STEP
#define LM_GGML_F16_EPR  LM_GGML_F32_EPR

static inline float32x4_t __lzs_f16cx4_load(const lm_ggml_fp16_t * x) {
#if defined(__NNPA__)
    uint16x8_t v_x = vec_xl(0, (const lm_ggml_fp16_t *)x);
    uint16x8_t v_xd = vec_convert_from_fp16(v_x, 0);
    return vec_extend_to_fp32_hi(v_xd, 0);
#else
    float tmp[4];

    for (int i = 0; i < 4; i++) {
        tmp[i] = LM_GGML_CPU_FP16_TO_FP32(x[i]);
    }

    // note: keep type-cast here to prevent compiler bugs
    // see: https://github.com/ggml-org/llama.cpp/issues/12846
    return vec_xl(0, (const float *)(tmp));
#endif
}

static inline void __lzs_f16cx4_store(lm_ggml_fp16_t * x, float32x4_t v_y) {
#if defined(__NNPA__)
    float32x4_t v_zero = vec_splats(0.0f);
    uint16x8_t v_xd = vec_round_from_fp32(v_y, v_zero, 0);
    uint16x8_t v_x = vec_convert_to_fp16(v_xd, 0);

    x[0] = vec_extract(v_x, 0);
    x[1] = vec_extract(v_x, 1);
    x[2] = vec_extract(v_x, 2);
    x[3] = vec_extract(v_x, 3);
#else
    float arr[4];

    // note: keep type-cast here to prevent compiler bugs
    // see: https://github.com/ggml-org/llama.cpp/issues/12846
    vec_xst(v_y, 0, (float *)(arr));

    for (int i = 0; i < 4; i++) {
        x[i] = LM_GGML_CPU_FP32_TO_FP16(arr[i]);
    }
#endif
}

#define LM_GGML_F16_VEC                LM_GGML_F32x4
#define LM_GGML_F16_VEC_ZERO           LM_GGML_F32x4_ZERO
#define LM_GGML_F16_VEC_SET1           LM_GGML_F32x4_SET1
#define LM_GGML_F16_VEC_LOAD(p, i)     __lzs_f16cx4_load(p)
#define LM_GGML_F16_VEC_STORE(p, r, i) __lzs_f16cx4_store(p, r[i])
#define LM_GGML_F16_VEC_FMA            LM_GGML_F32x4_FMA
#define LM_GGML_F16_VEC_ADD            LM_GGML_F32x4_ADD
#define LM_GGML_F16_VEC_MUL            LM_GGML_F32x4_MUL
#define LM_GGML_F16_VEC_REDUCE         LM_GGML_F32x4_REDUCE

#endif

// LM_GGML_F32_ARR / LM_GGML_F16_ARR
//   number of registers to use per step
#ifdef LM_GGML_SIMD
#define LM_GGML_F32_ARR (LM_GGML_F32_STEP/LM_GGML_F32_EPR)
#define LM_GGML_F16_ARR (LM_GGML_F16_STEP/LM_GGML_F16_EPR)
#endif

#ifdef __cplusplus
}
#endif
