// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "layernorm_x86.h"
#include "x86_usability.h"
#include <math.h>
#include <cpu.h>

#if __SSE2__
#include <emmintrin.h>
#if __AVX__
#include <immintrin.h>
#endif // __AVX__
#endif // __SSE2__

static NCNN_FORCEINLINE void fast_mean(float* ptr, float* mean, int elempack, int elemcount, int size)
{
    int i = 0;
    float sum = 0.0f;
#if __SSE2__
#if __AVX__
#if __AVX512F__
    __m512 _sum_512 = _mm512_setzero_ps();
    for (; i + 16 <= size; i += 16, ptr += 16)
    {
        __m512 _cur = _mm512_loadu_ps(ptr);
        _sum_512 = _mm512_add_ps(_sum_512, _cur);
    }
#endif // __AVX512F__
    __m256 _sum_256 = _mm256_setzero_ps();
    for (; i + 8 <= size; i += 8, ptr += 8)
    {
        __m256 _cur = _mm256_loadu_ps(ptr);
        _sum_256 = _mm256_add_ps(_sum_256, _cur);
    }
#endif // __AVX__
    __m128 _sum_128 = _mm_setzero_ps();
    for (; i + 4 <= size; i += 4, ptr += 4)
    {
        __m128 _cur = _mm_loadu_ps(ptr);
        _sum_128 = _mm_add_ps(_sum_128, _cur);
    }
#endif // __SSE2__
    for (; i < size; ++i, ++ptr)
    {
        sum += *ptr;
    }

    if (elempack == 16)
    {
        __m512 _mean = _mm512_div_ps(_sum_512, _mm512_set1_ps((float)elemcount));
        _mm512_storeu_ps(mean, _mean);
    }
    if (elempack == 8)
    {
#if __AVX512F__
        {
            __m256 _low = _mm512_castps512_ps256(_sum_512);
            __m256 _high = _mm256_castpd_ps(_mm512_extractf64x4_pd(_mm512_castps_pd(_sum_512), 1));
            _sum_256 = _mm256_add_ps(_sum_256, _high);
            _sum_256 = _mm256_add_ps(_sum_256, _low);
        }
#endif // __AVX512F__
        __m256 _mean = _mm256_div_ps(_sum_256, _mm256_set1_ps((float)elemcount));
        _mm256_storeu_ps(mean, _mean);
        // duplicate until len is 16
        _mm256_storeu_ps(mean + 8, _mean);
    }
    if (elempack == 4)
    {
#if __AVX__
#if __AVX512F__
        {
            __m256 _low = _mm512_castps512_ps256(_sum_512);
            __m256 _high = _mm256_castpd_ps(_mm512_extractf64x4_pd(_mm512_castps_pd(_sum_512), 1));
            _sum_256 = _mm256_add_ps(_sum_256, _high);
            _sum_256 = _mm256_add_ps(_sum_256, _low);
        }
#endif // __AVX512F__
        {
            __m128 _low = _mm256_castps256_ps128(_sum_256);
            __m128 _high = _mm256_extractf128_ps(_sum_256, 1);
            _sum_128 = _mm_add_ps(_sum_128, _low);
            _sum_128 = _mm_add_ps(_sum_128, _high);
        }
#endif // __AVX__
        __m128 _mean = _mm_div_ps(_sum_128, _mm_set1_ps((float)elemcount));
        _mm_storeu_ps(mean, _mean);
        // duplicate until len is 16
        _mm_storeu_ps(mean + 4, _mean);
        _mm_storeu_ps(mean + 8, _mean);
        _mm_storeu_ps(mean + 12, _mean);
    }
    if (elempack == 1)
    {
#if __SSE2__
#if __AVX__
#if __AVX512F__
        sum += _mm512_reduce_add_ps(_sum_512);
#endif // __AVX512F__
        sum += _mm256_reduce_add_ps(_sum_256);
#endif // __AVX__
        sum += _mm_reduce_add_ps(_sum_128);
#endif // __SSE2__
#if __SSE2__
#if __AVX__
#if __AVX512F__
        {
            _mm512_storeu_ps(mean, _mm512_set1_ps(sum / elemcount));
            return;
        }
#endif // __AVX512F__
        {
            __m256 _mean = _mm256_set1_ps(sum / elemcount);
            _mm256_storeu_ps(mean, _mean);
            _mm256_storeu_ps(mean + 8, _mean);
            return;
        }
#endif // __AVX__
        {
            __m128 _mean = _mm_set1_ps(sum / elemcount);
            _mm_storeu_ps(mean, _mean);
            _mm_storeu_ps(mean + 4, _mean);
            _mm_storeu_ps(mean + 8, _mean);
            _mm_storeu_ps(mean + 12, _mean);
            return;
        }
#endif // __SSE2__
        float _mean = sum / elemcount;
        for (int i = 0; i < 16; ++i)
        {
            mean[i] = _mean;
        }
    }
}

static NCNN_FORCEINLINE void fast_var(float* ptr, float* var, float* mean, int elempack, int elemcount, int size)
{
    int i = 0;
    float sq_sum = 0.0f;
#if __SSE2__
#if __AVX__
#if __AVX512F__
    __m512 _mean_512 = _mm512_loadu_ps(mean);
    __m512 _sq_sum_512 = _mm512_setzero_ps();
    for (; i + 16 <= size; i += 16, ptr += 16)
    {
        __m512 _cur = _mm512_loadu_ps(ptr);
        _cur = _mm512_sub_ps(_cur, _mean_512);
        _sq_sum_512 = _mm512_fmadd_ps(_cur, _cur, _sq_sum_512);
    }
#endif // __AVX512F__
    __m256 _mean_256 = _mm256_loadu_ps(mean);
    __m256 _sq_sum_256 = _mm256_setzero_ps();
    for (; i + 8 <= size; i += 8, ptr += 8)
    {
        __m256 _cur = _mm256_loadu_ps(ptr);
        _cur = _mm256_sub_ps(_cur, _mean_256);
        _sq_sum_256 = _mm256_comp_fmadd_ps(_cur, _cur, _sq_sum_256);
    }
#endif // __AVX__
    __m128 _mean_128 = _mm_loadu_ps(mean);
    __m128 _sq_sum_128 = _mm_setzero_ps();
    for (; i + 4 <= size; i += 4, ptr += 4)
    {
        __m128 _cur = _mm_loadu_ps(ptr);
        _cur = _mm_sub_ps(_cur, _mean_128);
        _sq_sum_128 = _mm_comp_fmadd_ps(_cur, _cur, _sq_sum_128);
    }
#endif // __SSE2__
    float _mean = *mean;
    for (; i < size; ++i, ++ptr)
    {
        float tmp = *ptr - _mean;
        sq_sum += tmp * tmp;
    }

    if (elempack == 16)
    {
        __m512 _var = _mm512_div_ps(_sq_sum_512, _mm512_set1_ps((float)elemcount));
        _mm512_storeu_ps(var, _var);
    }
    if (elempack == 8)
    {
#if __AVX512F__
        {
            __m256 _low = _mm512_castps512_ps256(_sq_sum_512);
            __m256 _high = _mm256_castpd_ps(_mm512_extractf64x4_pd(_mm512_castps_pd(_sq_sum_512), 1));
            _sq_sum_256 = _mm256_add_ps(_sq_sum_256, _low);
            _sq_sum_256 = _mm256_add_ps(_sq_sum_256, _high);
        }
#endif // __AVX512F__
        __m256 _var = _mm256_div_ps(_sq_sum_256, _mm256_set1_ps((float)elemcount));
        _mm256_storeu_ps(var, _var);
        _mm256_storeu_ps(var + 8, _var);
    }
    if (elempack == 4)
    {
#if __AVX__
#if __AVX512F__
        {
            __m256 _low = _mm512_castps512_ps256(_sq_sum_512);
            __m256 _high = _mm256_castpd_ps(_mm512_extractf64x4_pd(_mm512_castps_pd(_sq_sum_512), 1));
            _sq_sum_256 = _mm256_add_ps(_sq_sum_256, _high);
            _sq_sum_256 = _mm256_add_ps(_sq_sum_256, _low);
        }
#endif // __AVX512F__
        {
            __m128 _low = _mm256_castps256_ps128(_sq_sum_256);
            __m128 _high = _mm256_extractf128_ps(_sq_sum_256, 1);
            _sq_sum_128 = _mm_add_ps(_sq_sum_128, _low);
            _sq_sum_128 = _mm_add_ps(_sq_sum_128, _high);
        }
#endif // __AVX__
        __m128 _var = _mm_div_ps(_sq_sum_128, _mm_set1_ps((float)elemcount));
        _mm_storeu_ps(var, _var);
        _mm_storeu_ps(var + 4, _var);
        _mm_storeu_ps(var + 8, _var);
        _mm_storeu_ps(var + 12, _var);
    }
    if (elempack == 1)
    {
#if __SSE2__
#if __AVX__
#if __AVX512F__
        sq_sum += _mm512_reduce_add_ps(_sq_sum_512);
#endif // __AVX512F__
        sq_sum += _mm256_reduce_add_ps(_sq_sum_256);
#endif // __AVX__
        sq_sum += _mm_reduce_add_ps(_sq_sum_128);
#endif // __SSE2__
#if __SSE2__
#if __AVX__
#if __AVX512F__
        {
            _mm512_storeu_ps(var, _mm512_set1_ps(sq_sum / elemcount));
            return;
        }
#endif // __AVX512F__
        {
            __m256 _var = _mm256_set1_ps(sq_sum / elemcount);
            _mm256_storeu_ps(var, _var);
            _mm256_storeu_ps(var + 8, _var);
            return;
        }
#endif // __AVX__
        {
            __m128 _var = _mm_set1_ps(sq_sum / elemcount);
            _mm_storeu_ps(var, _var);
            _mm_storeu_ps(var + 4, _var);
            _mm_storeu_ps(var + 8, _var);
            _mm_storeu_ps(var + 12, _var);
            return;
        }
#endif // __SSE2__
        float _var = sq_sum / elemcount;
        for (int i = 0; i < 16; ++i)
        {
            var[i] = _var;
        }
    }
}

static NCNN_FORCEINLINE void fast_fmadd(float* ptr, float* a, float* b, int elempack, int elemcount, int size)
{
    int i = 0;
#if __SSE2__
#if __AVX__
#if __AVX512F__
    if (elempack == 16 || elempack == 1)
    {
        __m512 _a = elempack == 1 ? _mm512_set1_ps(*a) : _mm512_loadu_ps(a);
        __m512 _b = elempack == 1 ? _mm512_set1_ps(*b) : _mm512_loadu_ps(b);
        for (; i + 16 <= size; i += 16, ptr += 16)
        {
            __m512 _cur = _mm512_loadu_ps(ptr);
            _cur = _mm512_fmadd_ps(_cur, _a, _b);
            _mm512_storeu_ps(ptr, _cur);
        }
    }
#endif // __AVX512F__
    if (elempack == 8 || elempack == 1)
    {
        __m256 _a = elempack == 1 ? _mm256_set1_ps(*a) : _mm256_loadu_ps(a);
        __m256 _b = elempack == 1 ? _mm256_set1_ps(*b) : _mm256_loadu_ps(b);
        for (; i + 8 <= size; i += 8, ptr += 8)
        {
            __m256 _cur = _mm256_loadu_ps(ptr);
            _cur = _mm256_comp_fmadd_ps(_cur, _a, _b);
            _mm256_storeu_ps(ptr, _cur);
        }
    }
#endif // __AVX__
    if (elempack == 4 || elempack == 1)
    {
        __m128 _a = elempack == 1 ? _mm_set1_ps(*a) : _mm_loadu_ps(a);
        __m128 _b = elempack == 1 ? _mm_set1_ps(*b) : _mm_loadu_ps(b);
        for (; i + 4 <= size; i += 4, ptr += 4)
        {
            __m128 _cur = _mm_loadu_ps(ptr);
            _cur = _mm_comp_fmadd_ps(_cur, _a, _b);
            _mm_storeu_ps(ptr, _cur);
        }
    }
#endif // __SSE2__
    if (elempack == 1)
    {
        for (; i < elemcount; ++i, ++ptr)
        {
            *ptr = (*ptr) * (*a) + (*b);
        }
    }
}

namespace ncnn {

LayerNorm_x86::LayerNorm_x86()
{
#if __SSE2__
    support_packing = true;
#endif // __SSE2__
}

NCNN_FORCEINLINE static void fast_fmadd_fmadd(float* ptr, float* a, float* b, const float* gamma, const float* beta, int elempack, int elemcount, int size)
{
    int i = 0;
    // const float* gamma = static_cast<const float*>(gamma_data);
    // const float* beta = static_cast<const float*>(beta_data);

#if __SSE2__
#if __AVX__
#if __AVX512F__
    if (elempack == 16)
    {
        __m512 _a = _mm512_loadu_ps(a);
        __m512 _b = _mm512_loadu_ps(b);
        for (; i + 16 <= size; i += 16, ptr += 16, ++gamma, ++beta)
        {
            __m512 _cur = _mm512_loadu_ps(ptr);
            __m512 _gamma = _mm512_set1_ps(*gamma);
            __m512 _beta = _mm512_set1_ps(*beta);
            _cur = _mm512_fmadd_ps(_cur, _a, _b);
            _cur = _mm512_fmadd_ps(_cur, _gamma, _beta);
            _mm512_storeu_ps(ptr, _cur);
        }
    }
    else if (elempack == 1)
    {
        __m512 _a = _mm512_set1_ps(*a);
        __m512 _b = _mm512_set1_ps(*b);
        for (; i + 16 <= elemcount; i += 16, ptr += 16, gamma += 16, beta += 16)
        {
            __m512 _cur = _mm512_loadu_ps(ptr);
            __m512 _gamma = _mm512_loadu_ps(gamma);
            __m512 _beta = _mm512_loadu_ps(beta);
            _cur = _mm512_fmadd_ps(_cur, _a, _b);
            _cur = _mm512_fmadd_ps(_cur, _gamma, _beta);
            _mm512_storeu_ps(ptr, _cur);
        }
    }
#endif // __AVX512F__
    if (elempack == 8)
    {
        __m256 _a = _mm256_loadu_ps(a);
        __m256 _b = _mm256_loadu_ps(b);
        for (; i + 8 <= size; i += 8, ptr += 8, ++gamma, ++beta)
        {
            __m256 _cur = _mm256_loadu_ps(ptr);
            __m256 _gamma = _mm256_set1_ps(*gamma);
            __m256 _beta = _mm256_set1_ps(*beta);
            _cur = _mm256_comp_fmadd_ps(_cur, _a, _b);
            _cur = _mm256_comp_fmadd_ps(_cur, _gamma, _beta);
            _mm256_storeu_ps(ptr, _cur);
        }
    }
    else if (elempack == 1)
    {
        __m256 _a = _mm256_set1_ps(*a);
        __m256 _b = _mm256_set1_ps(*b);
        for (; i + 8 <= elemcount; i += 8, ptr += 8, gamma += 8, beta += 8)
        {
            __m256 _cur = _mm256_loadu_ps(ptr);
            __m256 _gamma = _mm256_loadu_ps(gamma);
            __m256 _beta = _mm256_loadu_ps(beta);
            _cur = _mm256_comp_fmadd_ps(_cur, _a, _b);
            _cur = _mm256_comp_fmadd_ps(_cur, _gamma, _beta);
            _mm256_storeu_ps(ptr, _cur);
        }
    }
#endif // __AVX__
    if (elempack == 4)
    {
        __m128 _a = _mm_loadu_ps(a);
        __m128 _b = _mm_loadu_ps(b);
        for (; i + 4 <= size; i += 4, ptr += 4, ++gamma, ++beta)
        {
            __m128 _cur = _mm_loadu_ps(ptr);
            __m128 _gamma = _mm_set1_ps(*gamma);
            __m128 _beta = _mm_set1_ps(*beta);
            _cur = _mm_comp_fmadd_ps(_cur, _a, _b);
            _cur = _mm_comp_fmadd_ps(_cur, _gamma, _beta);
            _mm_storeu_ps(ptr, _cur);
        }
    }
    else if (elempack == 1)
    {
        __m128 _a = _mm_set1_ps(*a);
        __m128 _b = _mm_set1_ps(*b);
        for (; i + 4 <= elemcount; i += 4, ptr += 4, gamma += 4, beta += 4)
        {
            __m128 _cur = _mm_loadu_ps(ptr);
            __m128 _gamma = _mm_loadu_ps(gamma);
            __m128 _beta = _mm_loadu_ps(beta);
            _cur = _mm_comp_fmadd_ps(_cur, _a, _b);
            _cur = _mm_comp_fmadd_ps(_cur, _gamma, _beta);
            _mm_storeu_ps(ptr, _cur);
        }
    }
#endif // __SSE2__
    if (elempack == 1)
    {
        for (; i < elemcount; ++i, ++ptr, ++gamma, ++beta)
        {
            *ptr = ((*ptr) * (*a) + (*b)) * (*gamma) + (*beta);
        }
    }
}

NCNN_FORCEINLINE static void fast_1d_layer_norm(float* ptr, int elempack, int elemcount, int size, const float* gamma, const float* beta, int affine, float eps)
{
    float mean[16], var[16];
    fast_mean(ptr, mean, elempack, elemcount, size);
    fast_var(ptr, var, mean, elempack, elemcount, size);
    float *a = var, *b = mean;

#if __SSE2__
#if __AVX__
#if __AVX512F__
    if (elempack == 16)
    {
        __m512 _a = _mm512_set1_ps(1.0f);
        __m512 _eps = _mm512_set1_ps(eps);
        __m512 _b = _mm512_setzero_ps();
        __m512 _var = _mm512_loadu_ps(var);
        _var = _mm512_add_ps(_var, _eps);
        __m512 _sqrt_var = _mm512_sqrt_ps(_var);
        _a = _mm512_div_ps(_a, _sqrt_var);
        __m512 _mean = _mm512_loadu_ps(mean);
        _b = _mm512_fnmadd_ps(_mean, _a, _b);

        _mm512_storeu_ps(a, _a);
        _mm512_storeu_ps(b, _b);
    }
#endif // __AVX512F__
    if (elempack == 8)
    {
        __m256 _a = _mm256_set1_ps(1.0f);
        __m256 _eps = _mm256_set1_ps(eps);
        __m256 _b = _mm256_setzero_ps();
        __m256 _var = _mm256_loadu_ps(var);
        _var = _mm256_add_ps(_var, _eps);
        __m256 _sqrt_var = _mm256_sqrt_ps(_var);
        _a = _mm256_div_ps(_a, _sqrt_var);
        __m256 _mean = _mm256_loadu_ps(mean);
        _b = _mm256_comp_fnmadd_ps(_mean, _a, _b);
        _mm256_storeu_ps(a, _a);
        _mm256_storeu_ps(b, _b);
    }
#endif // __AVX__
    if (elempack == 4)
    {
        __m128 _a = _mm_set1_ps(1.0f);
        __m128 _eps = _mm_set1_ps(eps);
        __m128 _b = _mm_setzero_ps();
        __m128 _var = _mm_loadu_ps(var);
        _var = _mm_add_ps(_var, _eps);
        __m128 _sqrt_var = _mm_sqrt_ps(_var);
        _a = _mm_div_ps(_a, _sqrt_var);
        __m128 _mean = _mm_loadu_ps(mean);
        _b = _mm_comp_fnmadd_ps(_mean, _a, _b);

        _mm_storeu_ps(a, _a);
        _mm_storeu_ps(b, _b);
    }
#endif // __SSE2__
    if (elempack == 1)
    {
        *a = static_cast<float>(1.0f / sqrt(*var + eps));
        *b = -*mean * (*a);
    }

    if (affine)
    {
        fast_fmadd_fmadd(ptr, a, b, gamma, beta, elempack, elemcount, size);
    }
    else
    {
        fast_fmadd(ptr, a, b, elempack, elemcount, size);
    }
}

int LayerNorm_x86::forward_inplace(Mat& bottom_top_blob, const Option& opt) const
{
    int dims = bottom_top_blob.dims;
    int elempack = bottom_top_blob.elempack;
    int w = bottom_top_blob.w;
    int h = bottom_top_blob.h;
    int channels = bottom_top_blob.c;

    const float* gamma = static_cast<const float*>(gamma_data);
    const float* beta = static_cast<const float*>(beta_data);

    if (dims == 1)
    {
        int elemcount = w * elempack;
        float* ptr = bottom_top_blob;
        // 1D layer norm is special. Treat them as unpacked.
        fast_1d_layer_norm(ptr, 1, elemcount, elemcount, gamma, beta, affine, eps);
    }
    else if (dims == 2)
    {
#pragma omp parallel for num_threads(opt.num_threads)
        for (int i = 0; i < h; ++i)
        {
            float* ptr = bottom_top_blob.row(i);
            fast_1d_layer_norm(ptr, elempack, w, w * elempack, gamma, beta, affine, eps);
        }
    }
    else if (dims == 3)
    {
        if (affine_size == w)
        {
#pragma omp parallel for num_threads(opt.num_threads)
            for (int q = 0; q < channels; ++q)
            {
                for (int i = 0; i < h; ++i)
                {
                    float* ptr = bottom_top_blob.channel(q).row(i);
                    fast_1d_layer_norm(ptr, elempack, w, w * elempack, gamma, beta, affine, eps);
                }
            }
        }
        else // if(affine_size == w * h)
        {
#pragma omp parallel for num_threads(opt.num_threads)
            for (int q = 0; q < channels; ++q)
            {
                float* ptr = bottom_top_blob.channel(q);
                fast_1d_layer_norm(ptr, elempack, w * h, w * h * elempack, gamma, beta, affine, eps);
            }
        }
    }

    return 0;
}

} // namespace ncnn
