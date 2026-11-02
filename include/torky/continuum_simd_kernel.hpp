#ifndef TORKY_CONTINUUM_SIMD_KERNEL_HPP
#define TORKY_CONTINUUM_SIMD_KERNEL_HPP

#include <cmath>
#include <cstdint>
#include <algorithm>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define TORKY_HAS_X86_INTRINSICS 1
#endif

namespace torky::core::continuum {

    inline constexpr double L_A_NM            = 0.36787944117144233; // e^-1 nm
    inline constexpr double EPSILON_NM2       = 0.016;
    inline constexpr double V_FINITE_ZERO_EV  = 11.384;
    inline constexpr double FIM_LAMBDA_MIN    = 7.38905609893065;    // e^2 nm^-2
    inline constexpr double HESSIAN_KAPPA_MAX = 18.50;

    #pragma pack(push, 1)
    struct TorkyCellDtype {
        uint8_t  gov_gate;     // TSG Gate Register (0x01 - 0x04)
        int16_t  phase_q16;    // Phase Q16 Fixed-Point
        int16_t  field_q16;    // Field Q16 Fixed-Point
        uint32_t syndrome_u32; // SHA-256 Merkle Syndrome
    };
    #pragma pack(pop)

    static_assert(sizeof(TorkyCellDtype) == 9, "CRITICAL: TorkyCellDtype must be precisely 9 bytes!");

    class ContinuumSimdKernel {
    public:
        static inline double evaluate_v_finite(double r_nm) {
            return 1.0 / std::sqrt((r_nm * r_nm) + EPSILON_NM2);
        }

        // Dynamic SIMD batch evaluation with AVX-512, AVX2, and Scalar fallback
        static void evaluate_batch_avx512(const double* r_input, double* v_output, int count) {
            int i = 0;
#if defined(TORKY_HAS_X86_INTRINSICS)
            // Runtime CPU check to prevent SIGILL (Illegal instruction) on CPUs without AVX-512F or AVX2
            bool has_avx512 = __builtin_cpu_supports("avx512f");
            bool has_avx2   = __builtin_cpu_supports("avx2");

            if (has_avx512) {
#if defined(__AVX512F__) && defined(__AVX512DQ__)
                __m512d v_eps = _mm512_set1_pd(EPSILON_NM2);
                __m512d v_one = _mm512_set1_pd(1.0);
                for (; i <= count - 8; i += 8) {
                    __m512d r = _mm512_loadu_pd(r_input + i);
                    __m512d r2 = _mm512_mul_pd(r, r);
                    __m512d sum = _mm512_add_pd(r2, v_eps);
                    __m512d sq = _mm512_sqrt_pd(sum);
                    __m512d res = _mm512_div_pd(v_one, sq);
                    _mm512_storeu_pd(v_output + i, res);
                }
#endif
            } else if (has_avx2) {
#if defined(__AVX2__)
                __m256d v_eps = _mm256_set1_pd(EPSILON_NM2);
                __m256d v_one = _mm256_set1_pd(1.0);
                for (; i <= count - 4; i += 4) {
                    __m256d r = _mm256_loadu_pd(r_input + i);
                    __m256d r2 = _mm256_mul_pd(r, r);
                    __m256d sum = _mm256_add_pd(r2, v_eps);
                    __m256d sq = _mm256_sqrt_pd(sum);
                    __m256d res = _mm256_div_pd(v_one, sq);
                    _mm256_storeu_pd(v_output + i, res);
                }
#endif
            }
#endif
            // Scalar fallback loop for remaining elements or non-supported hardware
            for (; i < count; ++i) {
                v_output[i] = evaluate_v_finite(r_input[i]);
            }
        }
    };

    class CardanoVieteSolver {
    public:
        static inline double solve_depressed_cubic(double p, double q) {
            double p_safe = std::min(-0.0001, p);
            double arg = (3.0 * q / (2.0 * p_safe)) * std::sqrt(-3.0 / p_safe);
            arg = std::max(-1.0, std::min(1.0, arg));
            return 2.0 * std::sqrt(-p_safe / 3.0) * std::cos(std::acos(arg) / 3.0);
        }
    };

    class TsgGateController {
    public:
        static inline bool validate_authorization(uint8_t gate_request, bool bit_tampered) {
            if (bit_tampered) return false;
            return (gate_request >= 0x01 && gate_request <= 0x04);
        }
    };

} // namespace torky::core::continuum

#endif // TORKY_CONTINUUM_SIMD_KERNEL_HPP
