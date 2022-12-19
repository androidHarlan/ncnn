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

static void gridsample_2d_bilinear_align0_zeros_blob_pack1(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    const int grid_size = grid.w * grid.h;
#if __AVX__
    const __m256 vImgWf = _mm256_set1_ps(src.w);
    const __m256 vImgHf = _mm256_set1_ps(src.h);
#if __AVX2__
    const __m256i vImgWi = _mm256_set1_epi32(src.w);
    const __m256i vImgHi = _mm256_set1_epi32(src.h);
#endif // __AVX2__
#endif // __AVX__

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < grid.c; y++)
    {
        const float* gridptr = grid.channel(y);
        int nn = grid_size;
#if __AVX__
        for (int x = 0; x + 15 < nn; x += 16)
        {
            __m256 tmp_x = _mm256_loadu_ps(gridptr + x);
            __m256 gy = _mm256_loadu_ps(gridptr + x + 8);

            __m256 gx = _mm256_permute2f128_ps(tmp_x, gy, 0b00100000);
            gy = _mm256_permute2f128_ps(tmp_x, gy, 0b00110001);
            tmp_x = _mm256_or_ps(gx, _mm256_setzero_ps());

            gx = _mm256_shuffle_ps(gx, gy, 0b10001000);
            gy = _mm256_shuffle_ps(tmp_x, gy, 0b11011101);

            // compute coord
            {
                // x
                gx = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gx, *(__m256*)_ps256_1), vImgWf, *(__m256*)_ps256_1), *(__m256*)_ps256_2);

                // y
                gy = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gy, *(__m256*)_ps256_1), vImgHf, *(__m256*)_ps256_1), *(__m256*)_ps256_2);
            }

            __m256 x_w = _mm256_floor_ps(gx);
            __m256 y_n = _mm256_floor_ps(gy);

            __m256 w = _mm256_sub_ps(gx, x_w);
            __m256 e = _mm256_sub_ps(*(__m256*)_ps256_1, w);
            __m256 n = _mm256_sub_ps(gy, y_n);
            __m256 s = _mm256_sub_ps(*(__m256*)_ps256_1, n);

            __m256 nw = _mm256_mul_ps(s, e);
            __m256 ne = _mm256_mul_ps(s, w);
            __m256 sw = _mm256_mul_ps(n, e);
            __m256 se = _mm256_mul_ps(n, w);

#if __AVX2__
            __m256i x0 = _mm256_cvtps_epi32(x_w);
            __m256i y0 = _mm256_cvtps_epi32(y_n);
            __m256i x1 = _mm256_add_epi32(x0, *(__m256i*)_pi32_256_1);
            __m256i y1 = _mm256_add_epi32(y0, *(__m256i*)_pi32_256_1);

            __m256i x0_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x0, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgWi, x0));
            __m256i x1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgWi, x1));
            __m256i y0_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y0, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgHi, y0));
            __m256i y1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgHi, y1));

            __m256i v00_in_range = _mm256_and_si256(x0_in_range, y0_in_range);
            __m256i v01_in_range = _mm256_and_si256(x0_in_range, y1_in_range);
            __m256i v10_in_range = _mm256_and_si256(x1_in_range, y0_in_range);
            __m256i v11_in_range = _mm256_and_si256(x1_in_range, y1_in_range);

            __m256i i_nw_offset = _mm256_add_epi32(_mm256_mullo_epi32(y0, vImgWi), x0);
            __m256i i_ne_offset = _mm256_add_epi32(i_nw_offset, *(__m256i*)_pi32_256_1);
            __m256i i_sw_offset = _mm256_add_epi32(i_nw_offset, vImgWi);
            __m256i i_se_offset = _mm256_add_epi32(i_sw_offset, *(__m256i*)_pi32_256_1);
#else
            __m256 x1 = _mm256_add_ps(x_w, *(__m256*)_ps256_1);
            __m256 y1 = _mm256_add_ps(y_n, *(__m256*)_ps256_1);

            __m256 x0_in_range = _mm256_and_ps(_mm256_cmp_ps(x_w, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgWf, x_w, _CMP_GT_OS));
            __m256 x1_in_range = _mm256_and_ps(_mm256_cmp_ps(x1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgWf, x1, _CMP_GT_OS));
            __m256 y0_in_range = _mm256_and_ps(_mm256_cmp_ps(y_n, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgHf, y_n, _CMP_GT_OS));
            __m256 y1_in_range = _mm256_and_ps(_mm256_cmp_ps(y1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgHf, y1, _CMP_GT_OS));

            __m256 v00_in_range = _mm256_and_ps(x0_in_range, y0_in_range);
            __m256 v01_in_range = _mm256_and_ps(x0_in_range, y1_in_range);
            __m256 v10_in_range = _mm256_and_ps(x1_in_range, y0_in_range);
            __m256 v11_in_range = _mm256_and_ps(x1_in_range, y1_in_range);

            __m256 nw_offset = _mm256_add_ps(_mm256_mul_ps(y_n, vImgWf), x_w);
            __m256 ne_offset = _mm256_add_ps(nw_offset, *(__m256*)_ps256_1);
            __m256 sw_offset = _mm256_add_ps(nw_offset, vImgWf);
            __m256 se_offset = _mm256_add_ps(sw_offset, *(__m256*)_ps256_1);

            __m256i i_nw_offset = _mm256_cvtps_epi32(nw_offset);
            __m256i i_ne_offset = _mm256_cvtps_epi32(ne_offset);
            __m256i i_sw_offset = _mm256_cvtps_epi32(sw_offset);
            __m256i i_se_offset = _mm256_cvtps_epi32(se_offset);
#endif // __AVX2__

            for (int q = 0; q < src.c; q++)
            {
#if __AVX2__
                __m256 nw_val = mask_gather_ps256(src.channel(q), i_nw_offset, _mm256_castsi256_ps(v00_in_range));
                __m256 ne_val = mask_gather_ps256(src.channel(q), i_ne_offset, _mm256_castsi256_ps(v10_in_range));
                __m256 sw_val = mask_gather_ps256(src.channel(q), i_sw_offset, _mm256_castsi256_ps(v01_in_range));
                __m256 se_val = mask_gather_ps256(src.channel(q), i_se_offset, _mm256_castsi256_ps(v11_in_range));
#else                                             
                __m256 nw_val = mask_gather_ps256(src.channel(q), i_nw_offset, v00_in_range);
                __m256 ne_val = mask_gather_ps256(src.channel(q), i_ne_offset, v10_in_range);
                __m256 sw_val = mask_gather_ps256(src.channel(q), i_sw_offset, v01_in_range);
                __m256 se_val = mask_gather_ps256(src.channel(q), i_se_offset, v11_in_range);
#endif // __AVX2__

                __m256 _v = _mm256_mul_ps(nw_val, nw);
                _v = _mm256_comp_fmadd_ps(ne_val, ne, _v);
                _v = _mm256_comp_fmadd_ps(sw_val, sw, _v);
                _v = _mm256_comp_fmadd_ps(se_val, se, _v);

                _mm256_storeu_ps(dst.channel(q).row(y) + x / 2, _v);
            }
        }

        nn = grid_size & 15;
#endif // __AVX__

        for (int x = grid_size - nn; x < grid_size; x += 2)
        {
            float sample_x = gridptr[x];
            float sample_y = gridptr[x + 1];

            sample_x = ((sample_x + 1) * src.w - 1) / 2.f;
            sample_y = ((sample_y + 1) * src.h - 1) / 2.f;

            // bilinear interpolate
            int x0 = (int)floor(sample_x);
            int y0 = (int)floor(sample_y);
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            bool v00_in_range = (x0 > -1) & (x0 < src.w) & (y0 > -1) & (y0 < src.h);
            bool v01_in_range = (x1 > -1) & (x1 < src.w) & (y0 > -1) & (y0 < src.h);
            bool v10_in_range = (x0 > -1) & (x0 < src.w) & (y1 > -1) & (y1 < src.h);
            bool v11_in_range = (x1 > -1) & (x1 < src.w) & (y1 > -1) & (y1 < src.h);

            float alpha = sample_x - x0;
            float beta = sample_y - y0;

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
                float v00 = v00_in_range ? image.row(y0)[x0] : 0;
                float v01 = v01_in_range ? image.row(y0)[x1] : 0;
                float v10 = v10_in_range ? image.row(y1)[x0] : 0;
                float v11 = v11_in_range ? image.row(y1)[x1] : 0;

                float v0 = v00 * (1 - alpha) + v01 * alpha;
                float v1 = v10 * (1 - alpha) + v11 * alpha;

                dst.channel(q).row(y)[x / 2] = v0 * (1 - beta) + v1 * beta;
            }
        }
    }
}


static void gridsample_2d_bilinear_align1_zeros_blob_pack1(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    const int grid_size = grid.w * grid.h;
#if __AVX__
    const __m256 vImgWf = _mm256_set1_ps(src.w);
    const __m256 vImgHf = _mm256_set1_ps(src.h);
#if __AVX2__
    const __m256i vImgWi = _mm256_set1_epi32(src.w);
    const __m256i vImgHi = _mm256_set1_epi32(src.h);
#endif // __AVX2__
#endif // __AVX__

#pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < grid.c; y++)
    {
        const float* gridptr = grid.channel(y);
        int nn = grid_size;
#if __AVX__
        for (int x = 0; x + 15 < grid_size; x += 16)
        {
            __m256 tmp_x = _mm256_loadu_ps(gridptr + x);
            __m256 gy = _mm256_loadu_ps(gridptr + x + 8);

            __m256 gx = _mm256_permute2f128_ps(tmp_x, gy, 0b00100000);
            gy = _mm256_permute2f128_ps(tmp_x, gy, 0b00110001);
            tmp_x = _mm256_or_ps(gx, _mm256_setzero_ps());

            gx = _mm256_shuffle_ps(gx, gy, 0b10001000);
            gy = _mm256_shuffle_ps(tmp_x, gy, 0b11011101);

            // compute coord
            {
                // x
                gx = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gx, *(__m256*)_ps256_1), *(__m256*)_ps256_2), _mm256_sub_ps(vImgWf, *(__m256*)_ps256_1));

                // y
                gy = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gy, *(__m256*)_ps256_1), *(__m256*)_ps256_2), _mm256_sub_ps(vImgHf, *(__m256*)_ps256_1));
            }

            __m256 x_w = _mm256_floor_ps(gx);
            __m256 y_n = _mm256_floor_ps(gy);

            __m256 w = _mm256_sub_ps(gx, x_w);
            __m256 e = _mm256_sub_ps(*(__m256*)_ps256_1, w);
            __m256 n = _mm256_sub_ps(gy, y_n);
            __m256 s = _mm256_sub_ps(*(__m256*)_ps256_1, n);

            __m256 nw = _mm256_mul_ps(s, e);
            __m256 ne = _mm256_mul_ps(s, w);
            __m256 sw = _mm256_mul_ps(n, e);
            __m256 se = _mm256_mul_ps(n, w);

#if __AVX2__
            __m256i x0 = _mm256_cvtps_epi32(x_w);
            __m256i y0 = _mm256_cvtps_epi32(y_n);
            __m256i x1 = _mm256_add_epi32(x0, *(__m256i*)_pi32_256_1);
            __m256i y1 = _mm256_add_epi32(y0, *(__m256i*)_pi32_256_1);

            __m256i x0_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x0, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgWi, x0));
            __m256i x1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgWi, x1));
            __m256i y0_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y0, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgHi, y0));
            __m256i y1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgHi, y1));

            __m256i v00_in_range = _mm256_and_si256(x0_in_range, y0_in_range);
            __m256i v01_in_range = _mm256_and_si256(x0_in_range, y1_in_range);
            __m256i v10_in_range = _mm256_and_si256(x1_in_range, y0_in_range);
            __m256i v11_in_range = _mm256_and_si256(x1_in_range, y1_in_range);

            __m256i i_nw_offset = _mm256_add_epi32(_mm256_mullo_epi32(y0, vImgWi), x0);
            __m256i i_ne_offset = _mm256_add_epi32(i_nw_offset, *(__m256i*)_pi32_256_1);
            __m256i i_sw_offset = _mm256_add_epi32(i_nw_offset, vImgWi);
            __m256i i_se_offset = _mm256_add_epi32(i_sw_offset, *(__m256i*)_pi32_256_1);
#else
            __m256 x1 = _mm256_add_ps(x_w, *(__m256*)_ps256_1);
            __m256 y1 = _mm256_add_ps(y_n, *(__m256*)_ps256_1);

            __m256 x0_in_range = _mm256_and_ps(_mm256_cmp_ps(x_w, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgWf, x_w, _CMP_GT_OS));
            __m256 x1_in_range = _mm256_and_ps(_mm256_cmp_ps(x1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgWf, x1, _CMP_GT_OS));
            __m256 y0_in_range = _mm256_and_ps(_mm256_cmp_ps(y_n, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgHf, y_n, _CMP_GT_OS));
            __m256 y1_in_range = _mm256_and_ps(_mm256_cmp_ps(y1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgHf, y1, _CMP_GT_OS));

            __m256 v00_in_range = _mm256_and_ps(x0_in_range, y0_in_range);
            __m256 v01_in_range = _mm256_and_ps(x0_in_range, y1_in_range);
            __m256 v10_in_range = _mm256_and_ps(x1_in_range, y0_in_range);
            __m256 v11_in_range = _mm256_and_ps(x1_in_range, y1_in_range);

            __m256 nw_offset = _mm256_add_ps(_mm256_mul_ps(y_n, vImgWf), x_w);
            __m256 ne_offset = _mm256_add_ps(nw_offset, *(__m256*)_ps256_1);
            __m256 sw_offset = _mm256_add_ps(nw_offset, vImgWf);
            __m256 se_offset = _mm256_add_ps(sw_offset, *(__m256*)_ps256_1);

            __m256i i_nw_offset = _mm256_cvtps_epi32(nw_offset);
            __m256i i_ne_offset = _mm256_cvtps_epi32(ne_offset);
            __m256i i_sw_offset = _mm256_cvtps_epi32(sw_offset);
            __m256i i_se_offset = _mm256_cvtps_epi32(se_offset);
#endif // __AVX2__

            for (int q = 0; q < src.c; q++)
            {
#if __AVX2__
                __m256 nw_val = mask_gather_ps256(src.channel(q), i_nw_offset, _mm256_castsi256_ps(v00_in_range));
                __m256 ne_val = mask_gather_ps256(src.channel(q), i_ne_offset, _mm256_castsi256_ps(v10_in_range));
                __m256 sw_val = mask_gather_ps256(src.channel(q), i_sw_offset, _mm256_castsi256_ps(v01_in_range));
                __m256 se_val = mask_gather_ps256(src.channel(q), i_se_offset, _mm256_castsi256_ps(v11_in_range));
#else                                             
                __m256 nw_val = mask_gather_ps256(src.channel(q), i_nw_offset, v00_in_range);
                __m256 ne_val = mask_gather_ps256(src.channel(q), i_ne_offset, v10_in_range);
                __m256 sw_val = mask_gather_ps256(src.channel(q), i_sw_offset, v01_in_range);
                __m256 se_val = mask_gather_ps256(src.channel(q), i_se_offset, v11_in_range);
#endif // __AVX2__

                __m256 _v = _mm256_mul_ps(nw_val, nw);
                _v = _mm256_comp_fmadd_ps(ne_val, ne, _v);
                _v = _mm256_comp_fmadd_ps(sw_val, sw, _v);
                _v = _mm256_comp_fmadd_ps(se_val, se, _v);

                _mm256_storeu_ps(dst.channel(q).row(y) + x / 2, _v);
            }
        }

        nn = grid_size & 15;
#endif // __AVX__
        for (int x = grid_size - nn; x < grid_size; x += 2)
        {
            float sample_x = gridptr[x];
            float sample_y = gridptr[x + 1];

            sample_x = (sample_x + 1) / 2.f * (src.w - 1);
            sample_y = (sample_y + 1) / 2.f * (src.h - 1);

            // bilinear interpolate
            int x0 = (int)floor(sample_x);
            int y0 = (int)floor(sample_y);
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            bool v00_in_range = (x0 > -1) & (x0 < src.w) & (y0 > -1) & (y0 < src.h);
            bool v01_in_range = (x1 > -1) & (x1 < src.w) & (y0 > -1) & (y0 < src.h);
            bool v10_in_range = (x0 > -1) & (x0 < src.w) & (y1 > -1) & (y1 < src.h);
            bool v11_in_range = (x1 > -1) & (x1 < src.w) & (y1 > -1) & (y1 < src.h);

            float alpha = sample_x - x0;
            float beta = sample_y - y0;

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
                float v00 = v00_in_range ? image.row(y0)[x0] : 0;
                float v01 = v01_in_range ? image.row(y0)[x1] : 0;
                float v10 = v10_in_range ? image.row(y1)[x0] : 0;
                float v11 = v11_in_range ? image.row(y1)[x1] : 0;

                float v0 = v00 * (1 - alpha) + v01 * alpha;
                float v1 = v10 * (1 - alpha) + v11 * alpha;

                dst.channel(q).row(y)[x / 2] = v0 * (1 - beta) + v1 * beta;
            }
        }
    }
}

static void gridsample_2d_bilinear_align0_border_blob_pack1(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    const int grid_size = grid.w * grid.h;
#if __AVX__
    const __m256 vImgWf = _mm256_set1_ps(src.w);
    const __m256 vImgHf = _mm256_set1_ps(src.h);
#if __AVX2__
    const __m256i vImgWi = _mm256_set1_epi32(src.w);
    const __m256i vImgHi = _mm256_set1_epi32(src.h);
#endif // __AVX2__
#endif // __AVX__

#pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < grid.c; y++)
    {
        const float* gridptr = grid.channel(y);
        int nn = grid_size;
#if __AVX__
        for (int x = 0; x + 15 < nn; x += 16)
        {
            __m256 tmp_x = _mm256_loadu_ps(gridptr + x);
            __m256 gy = _mm256_loadu_ps(gridptr + x + 8);

            __m256 gx = _mm256_permute2f128_ps(tmp_x, gy, 0b00100000);
            gy = _mm256_permute2f128_ps(tmp_x, gy, 0b00110001);
            tmp_x = _mm256_or_ps(gx, _mm256_setzero_ps());

            gx = _mm256_shuffle_ps(gx, gy, 0b10001000);
            gy = _mm256_shuffle_ps(tmp_x, gy, 0b11011101);

            // compute coord
            {
                // x
                gx = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gx, *(__m256*)_ps256_1), vImgWf, *(__m256*)_ps256_1), *(__m256*)_ps256_2);

                const __m256 border_x = _mm256_sub_ps(vImgWf, *(__m256*)_ps256_1);

                gx = _mm256_min_ps(border_x, _mm256_max_ps(gx, _mm256_setzero_ps()));

                // y
                gy = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gy, *(__m256*)_ps256_1), vImgHf, *(__m256*)_ps256_1), *(__m256*)_ps256_2);

                const __m256 border_y = _mm256_sub_ps(vImgHf, *(__m256*)_ps256_1);

                gy = _mm256_min_ps(border_y, _mm256_max_ps(gy, _mm256_setzero_ps()));
            }

            __m256 x_w = _mm256_floor_ps(gx);
            __m256 y_n = _mm256_floor_ps(gy);

            __m256 w = _mm256_sub_ps(gx, x_w);
            __m256 e = _mm256_sub_ps(*(__m256*)_ps256_1, w);
            __m256 n = _mm256_sub_ps(gy, y_n);
            __m256 s = _mm256_sub_ps(*(__m256*)_ps256_1, n);

            __m256 nw = _mm256_mul_ps(s, e);
            __m256 ne = _mm256_mul_ps(s, w);
            __m256 sw = _mm256_mul_ps(n, e);
            __m256 se = _mm256_mul_ps(n, w);

#if __AVX2__
            __m256i x0 = _mm256_cvtps_epi32(x_w);
            __m256i y0 = _mm256_cvtps_epi32(y_n);
            __m256i x1 = _mm256_add_epi32(x0, *(__m256i*)_pi32_256_1);
            __m256i y1 = _mm256_add_epi32(y0, *(__m256i*)_pi32_256_1);

            __m256i x1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgWi, x1));
            __m256i y1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgHi, y1));

            __m256i v11_in_range = _mm256_and_si256(x1_in_range, y1_in_range);

            __m256i i_nw_offset = _mm256_add_epi32(_mm256_mullo_epi32(y0, vImgWi), x0);
            __m256i i_ne_offset = _mm256_add_epi32(i_nw_offset, *(__m256i*)_pi32_256_1);
            __m256i i_sw_offset = _mm256_add_epi32(i_nw_offset, vImgWi);
            __m256i i_se_offset = _mm256_add_epi32(i_sw_offset, *(__m256i*)_pi32_256_1);
#else
            __m256 x1 = _mm256_add_ps(x_w, *(__m256*)_ps256_1);
            __m256 y1 = _mm256_add_ps(y_n, *(__m256*)_ps256_1);

            __m256 x1_in_range = _mm256_and_ps(_mm256_cmp_ps(x1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgWf, x1, _CMP_GT_OS));
            __m256 y1_in_range = _mm256_and_ps(_mm256_cmp_ps(y1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgHf, y1, _CMP_GT_OS));

            __m256 v11_in_range = _mm256_and_ps(x1_in_range, y1_in_range);

            __m256 nw_offset = _mm256_add_ps(_mm256_mul_ps(y_n, vImgWf), x_w);
            __m256 ne_offset = _mm256_add_ps(nw_offset, *(__m256*)_ps256_1);
            __m256 sw_offset = _mm256_add_ps(nw_offset, vImgWf);
            __m256 se_offset = _mm256_add_ps(sw_offset, *(__m256*)_ps256_1);

            __m256i i_nw_offset = _mm256_cvtps_epi32(nw_offset);
            __m256i i_ne_offset = _mm256_cvtps_epi32(ne_offset);
            __m256i i_sw_offset = _mm256_cvtps_epi32(sw_offset);
            __m256i i_se_offset = _mm256_cvtps_epi32(se_offset);
#endif

            for (int q = 0; q < src.c; q++)
            {
                __m256 nw_val = mask_gather_ps256(src.channel(q), i_nw_offset, *(__m256*)_ps256_n1);
#if __AVX2__
                __m256 ne_val = mask_gather_ps256(src.channel(q), i_ne_offset, _mm256_castsi256_ps(x1_in_range));
                __m256 sw_val = mask_gather_ps256(src.channel(q), i_sw_offset, _mm256_castsi256_ps(y1_in_range));
                __m256 se_val = mask_gather_ps256(src.channel(q), i_se_offset, _mm256_castsi256_ps(v11_in_range));
#else
                __m256 ne_val = mask_gather_ps256(src.channel(q), i_ne_offset, x1_in_range);
                __m256 sw_val = mask_gather_ps256(src.channel(q), i_sw_offset, y1_in_range);
                __m256 se_val = mask_gather_ps256(src.channel(q), i_se_offset, v11_in_range);
#endif

                __m256 _v = _mm256_mul_ps(nw_val, nw);
                _v = _mm256_comp_fmadd_ps(ne_val, ne, _v);
                _v = _mm256_comp_fmadd_ps(sw_val, sw, _v);
                _v = _mm256_comp_fmadd_ps(se_val, se, _v);

                _mm256_storeu_ps(dst.channel(q).row(y) + x / 2, _v);
            }
        }

        nn = grid_size & 15;
#endif // __AVX__

        for (int x = grid_size - nn; x < grid_size; x += 2)
        {
            float sample_x = gridptr[x];
            float sample_y = gridptr[x + 1];

            sample_x = ((sample_x + 1) * src.w - 1) / 2.f;
            sample_y = ((sample_y + 1) * src.h - 1) / 2.f;

            sample_x = std::min(src.w - 1.0f, std::max(sample_x, 0.0f));
            sample_y = std::min(src.h - 1.0f, std::max(sample_y, 0.0f));

            // bilinear interpolate
            int x0 = (int)floor(sample_x);
            int y0 = (int)floor(sample_y);
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            bool x1_in_range = (x1 > -1) & (x1 < src.w);
            bool y1_in_range = (y1 > -1) & (y1 < src.h);
            bool v11_in_range = x1_in_range & y1_in_range;

            float alpha = sample_x - x0;
            float beta = sample_y - y0;

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
                float v00 = image.row(y0)[x0];
                float v01 = x1_in_range ? image.row(y0)[x1] : 0;
                float v10 = y1_in_range ? image.row(y1)[x0] : 0;
                float v11 = v11_in_range ? image.row(y1)[x1] : 0;

                float v0 = v00 * (1 - alpha) + v01 * alpha;
                float v1 = v10 * (1 - alpha) + v11 * alpha;

                dst.channel(q).row(y)[x / 2] = v0 * (1 - beta) + v1 * beta;
            }
        }
    }
}

static void gridsample_2d_bilinear_align1_border_blob_pack1(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    const int grid_size = grid.w * grid.h;
#if __AVX__
    const __m256 vImgWf = _mm256_set1_ps(src.w);
    const __m256 vImgHf = _mm256_set1_ps(src.h);
#if __AVX2__
    const __m256i vImgWi = _mm256_set1_epi32(src.w);
    const __m256i vImgHi = _mm256_set1_epi32(src.h);
#endif // __AVX2__
#endif // __AVX__

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < grid.c; y++)
    {
        const float* gridptr = grid.channel(y);
        int nn = grid_size;
#if __AVX__
        for (int x = 0; x + 15 < grid_size; x += 16)
        {
            __m256 tmp_x = _mm256_loadu_ps(gridptr + x);
            __m256 gy = _mm256_loadu_ps(gridptr + x + 8);

            __m256 gx = _mm256_permute2f128_ps(tmp_x, gy, 0b00100000);
            gy = _mm256_permute2f128_ps(tmp_x, gy, 0b00110001);
            tmp_x = _mm256_or_ps(gx, _mm256_setzero_ps());

            gx = _mm256_shuffle_ps(gx, gy, 0b10001000);
            gy = _mm256_shuffle_ps(tmp_x, gy, 0b11011101);

            // compute coord
            {
                // x
                gx = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gx, *(__m256*)_ps256_1), *(__m256*)_ps256_2), _mm256_sub_ps(vImgWf, *(__m256*)_ps256_1));

                const __m256 border_x = _mm256_sub_ps(vImgWf, *(__m256*)_ps256_1);

                gx = _mm256_min_ps(border_x, _mm256_max_ps(gx, _mm256_setzero_ps()));

                // y
                gy = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gy, *(__m256*)_ps256_1), *(__m256*)_ps256_2), _mm256_sub_ps(vImgHf, *(__m256*)_ps256_1));

                const __m256 border_y = _mm256_sub_ps(vImgHf, *(__m256*)_ps256_1);

                gy = _mm256_min_ps(border_y, _mm256_max_ps(gy, _mm256_setzero_ps()));
            }

            __m256 x_w = _mm256_floor_ps(gx);
            __m256 y_n = _mm256_floor_ps(gy);

            __m256 w = _mm256_sub_ps(gx, x_w);
            __m256 e = _mm256_sub_ps(*(__m256*)_ps256_1, w);
            __m256 n = _mm256_sub_ps(gy, y_n);
            __m256 s = _mm256_sub_ps(*(__m256*)_ps256_1, n);

            __m256 nw = _mm256_mul_ps(s, e);
            __m256 ne = _mm256_mul_ps(s, w);
            __m256 sw = _mm256_mul_ps(n, e);
            __m256 se = _mm256_mul_ps(n, w);

#if __AVX2__
            __m256i x0 = _mm256_cvtps_epi32(x_w);
            __m256i y0 = _mm256_cvtps_epi32(y_n);
            __m256i x1 = _mm256_add_epi32(x0, *(__m256i*)_pi32_256_1);
            __m256i y1 = _mm256_add_epi32(y0, *(__m256i*)_pi32_256_1);

            __m256i x1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgWi, x1));
            __m256i y1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgHi, y1));

            __m256i v11_in_range = _mm256_and_si256(x1_in_range, y1_in_range);

            __m256i i_nw_offset = _mm256_add_epi32(_mm256_mullo_epi32(y0, vImgWi), x0);
            __m256i i_ne_offset = _mm256_add_epi32(i_nw_offset, *(__m256i*)_pi32_256_1);
            __m256i i_sw_offset = _mm256_add_epi32(i_nw_offset, vImgWi);
            __m256i i_se_offset = _mm256_add_epi32(i_sw_offset, *(__m256i*)_pi32_256_1);
#else
            __m256 x1 = _mm256_add_ps(x_w, *(__m256*)_ps256_1);
            __m256 y1 = _mm256_add_ps(y_n, *(__m256*)_ps256_1);

            __m256 x1_in_range = _mm256_and_ps(_mm256_cmp_ps(x1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgWf, x1, _CMP_GT_OS));
            __m256 y1_in_range = _mm256_and_ps(_mm256_cmp_ps(y1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgHf, y1, _CMP_GT_OS));

            __m256 v11_in_range = _mm256_and_ps(x1_in_range, y1_in_range);

            __m256 nw_offset = _mm256_add_ps(_mm256_mul_ps(y_n, vImgWf), x_w);
            __m256 ne_offset = _mm256_add_ps(nw_offset, *(__m256*)_ps256_1);
            __m256 sw_offset = _mm256_add_ps(nw_offset, vImgWf);
            __m256 se_offset = _mm256_add_ps(sw_offset, *(__m256*)_ps256_1);

            __m256i i_nw_offset = _mm256_cvtps_epi32(nw_offset);
            __m256i i_ne_offset = _mm256_cvtps_epi32(ne_offset);
            __m256i i_sw_offset = _mm256_cvtps_epi32(sw_offset);
            __m256i i_se_offset = _mm256_cvtps_epi32(se_offset);
#endif

            for (int q = 0; q < src.c; q++)
            {
                __m256 nw_val = mask_gather_ps256(src.channel(q), i_nw_offset, *(__m256*)_ps256_n1);
#if __AVX2__
                __m256 ne_val = mask_gather_ps256(src.channel(q), i_ne_offset, _mm256_castsi256_ps(x1_in_range));
                __m256 sw_val = mask_gather_ps256(src.channel(q), i_sw_offset, _mm256_castsi256_ps(y1_in_range));
                __m256 se_val = mask_gather_ps256(src.channel(q), i_se_offset, _mm256_castsi256_ps(v11_in_range));
#else
                __m256 ne_val = mask_gather_ps256(src.channel(q), i_ne_offset, x1_in_range);
                __m256 sw_val = mask_gather_ps256(src.channel(q), i_sw_offset, y1_in_range);
                __m256 se_val = mask_gather_ps256(src.channel(q), i_se_offset, v11_in_range);
#endif

                __m256 _v = _mm256_mul_ps(nw_val, nw);
                _v = _mm256_comp_fmadd_ps(ne_val, ne, _v);
                _v = _mm256_comp_fmadd_ps(sw_val, sw, _v);
                _v = _mm256_comp_fmadd_ps(se_val, se, _v);

                _mm256_storeu_ps(dst.channel(q).row(y) + x / 2, _v);
            }
        }

        nn = grid_size & 15;
#endif // __AVX__
        for (int x = grid_size - nn; x < grid_size; x += 2)
        {
            float sample_x = gridptr[x];
            float sample_y = gridptr[x + 1];

            sample_x = (sample_x + 1) / 2.f * (src.w - 1);
            sample_y = (sample_y + 1) / 2.f * (src.h - 1);

            sample_x = std::min(src.w - 1.0f, std::max(sample_x, 0.0f));
            sample_y = std::min(src.h - 1.0f, std::max(sample_y, 0.0f));

            // bilinear interpolate
            int x0 = (int)floor(sample_x);
            int y0 = (int)floor(sample_y);
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            bool x1_in_range = (x1 > -1) & (x1 < src.w);
            bool y1_in_range = (y1 > -1) & (y1 < src.h);
            bool v11_in_range = x1_in_range & y1_in_range;

            float alpha = sample_x - x0;
            float beta = sample_y - y0;

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
                float v00 = image.row(y0)[x0];
                float v01 = x1_in_range ? image.row(y0)[x1] : 0;
                float v10 = y1_in_range ? image.row(y1)[x0] : 0;
                float v11 = v11_in_range ? image.row(y1)[x1] : 0;

                float v0 = v00 * (1 - alpha) + v01 * alpha;
                float v1 = v10 * (1 - alpha) + v11 * alpha;

                dst.channel(q).row(y)[x / 2] = v0 * (1 - beta) + v1 * beta;
            }
        }
    }
}

static void gridsample_2d_bilinear_align0_reflection_blob_pack1(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    const int grid_size = grid.w * grid.h;
#if __AVX__
    const __m256 vImgWf = _mm256_set1_ps(src.w);
    const __m256 vImgHf = _mm256_set1_ps(src.h);
#if __AVX2__
    const __m256i vImgWi = _mm256_set1_epi32(src.w);
    const __m256i vImgHi = _mm256_set1_epi32(src.h);
#endif // __AVX2__
#endif // __AVX__

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < grid.c; y++)
    {
        const float* gridptr = grid.channel(y);
        int nn = grid_size;
#if __AVX__
        for (int x = 0; x + 15 < nn; x += 16)
        {
            __m256 tmp_x = _mm256_loadu_ps(gridptr + x);
            __m256 gy = _mm256_loadu_ps(gridptr + x + 8);

            __m256 gx = _mm256_permute2f128_ps(tmp_x, gy, 0b00100000);
            gy = _mm256_permute2f128_ps(tmp_x, gy, 0b00110001);
            tmp_x = _mm256_or_ps(gx, _mm256_setzero_ps());

            gx = _mm256_shuffle_ps(gx, gy, 0b10001000);
            gy = _mm256_shuffle_ps(tmp_x, gy, 0b11011101);

            // compute coord
            {
                // x
                gx = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gx, *(__m256*)_ps256_1), vImgWf, *(__m256*)_ps256_1), *(__m256*)_ps256_2);

                const __m256 border_x = _mm256_sub_ps(vImgWf, *(__m256*)_ps256_1);

                __m256 v0p5fp8 = _mm256_set1_ps(0.5f);
                gx = _mm256_add_ps(gx, v0p5fp8);

                gx = _mm256_and_ps(gx, *(__m256*)_ps256_inv_sign_mask);

                __m256 reflectx_v = _mm256_and_ps(_mm256_sub_ps(gx, vImgWf), *(__m256*)_ps256_inv_sign_mask);
                gx = _mm256_sub_ps(vImgWf, reflectx_v);

                gx = _mm256_sub_ps(gx, v0p5fp8);

                _mm256_sub_ps(gx, v0p5fp8);

                gx = _mm256_min_ps(border_x, _mm256_max_ps(gx, _mm256_setzero_ps()));

                // y
                gy = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gy, *(__m256*)_ps256_1), vImgHf, *(__m256*)_ps256_1), *(__m256*)_ps256_2);

                const __m256 border_y = _mm256_sub_ps(vImgHf, *(__m256*)_ps256_1);

                gy = _mm256_add_ps(gy, v0p5fp8);

                gy = _mm256_and_ps(gy, *(__m256*)_ps256_inv_sign_mask);

                __m256 reflecty_v = _mm256_and_ps(_mm256_sub_ps(gy, vImgHf), *(__m256*)_ps256_inv_sign_mask);
                gy = _mm256_sub_ps(vImgHf, reflecty_v);

                gy = _mm256_sub_ps(gy, v0p5fp8);

                _mm256_sub_ps(gy, v0p5fp8);

                gy = _mm256_min_ps(border_y, _mm256_max_ps(gy, _mm256_setzero_ps()));
            }

            __m256 x_w = _mm256_floor_ps(gx);
            __m256 y_n = _mm256_floor_ps(gy);

            __m256 w = _mm256_sub_ps(gx, x_w);
            __m256 e = _mm256_sub_ps(*(__m256*)_ps256_1, w);
            __m256 n = _mm256_sub_ps(gy, y_n);
            __m256 s = _mm256_sub_ps(*(__m256*)_ps256_1, n);

            __m256 nw = _mm256_mul_ps(s, e);
            __m256 ne = _mm256_mul_ps(s, w);
            __m256 sw = _mm256_mul_ps(n, e);
            __m256 se = _mm256_mul_ps(n, w);

#if __AVX2__
            __m256i x0 = _mm256_cvtps_epi32(x_w);
            __m256i y0 = _mm256_cvtps_epi32(y_n);
            __m256i x1 = _mm256_add_epi32(x0, *(__m256i*)_pi32_256_1);
            __m256i y1 = _mm256_add_epi32(y0, *(__m256i*)_pi32_256_1);

            __m256i x1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgWi, x1));
            __m256i y1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgHi, y1));

            __m256i v11_in_range = _mm256_and_si256(x1_in_range, y1_in_range);

            __m256i i_nw_offset = _mm256_add_epi32(_mm256_mullo_epi32(y0, vImgWi), x0);
            __m256i i_ne_offset = _mm256_add_epi32(i_nw_offset, *(__m256i*)_pi32_256_1);
            __m256i i_sw_offset = _mm256_add_epi32(i_nw_offset, vImgWi);
            __m256i i_se_offset = _mm256_add_epi32(i_sw_offset, *(__m256i*)_pi32_256_1);
#else
            __m256 x1 = _mm256_add_ps(x_w, *(__m256*)_ps256_1);
            __m256 y1 = _mm256_add_ps(y_n, *(__m256*)_ps256_1);

            __m256 x1_in_range = _mm256_and_ps(_mm256_cmp_ps(x1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgWf, x1, _CMP_GT_OS));
            __m256 y1_in_range = _mm256_and_ps(_mm256_cmp_ps(y1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgHf, y1, _CMP_GT_OS));

            __m256 v11_in_range = _mm256_and_ps(x1_in_range, y1_in_range);

            __m256 nw_offset = _mm256_add_ps(_mm256_mul_ps(y_n, vImgWf), x_w);
            __m256 ne_offset = _mm256_add_ps(nw_offset, *(__m256*)_ps256_1);
            __m256 sw_offset = _mm256_add_ps(nw_offset, vImgWf);
            __m256 se_offset = _mm256_add_ps(sw_offset, *(__m256*)_ps256_1);

            __m256i i_nw_offset = _mm256_cvtps_epi32(nw_offset);
            __m256i i_ne_offset = _mm256_cvtps_epi32(ne_offset);
            __m256i i_sw_offset = _mm256_cvtps_epi32(sw_offset);
            __m256i i_se_offset = _mm256_cvtps_epi32(se_offset);
#endif

            for (int q = 0; q < src.c; q++)
            {
                __m256 nw_val = mask_gather_ps256(src.channel(q), i_nw_offset, *(__m256*)_ps256_n1);
#if __AVX2__
                __m256 ne_val = mask_gather_ps256(src.channel(q), i_ne_offset, _mm256_castsi256_ps(x1_in_range));
                __m256 sw_val = mask_gather_ps256(src.channel(q), i_sw_offset, _mm256_castsi256_ps(y1_in_range));
                __m256 se_val = mask_gather_ps256(src.channel(q), i_se_offset, _mm256_castsi256_ps(v11_in_range));
#else
                __m256 ne_val = mask_gather_ps256(src.channel(q), i_ne_offset, x1_in_range);
                __m256 sw_val = mask_gather_ps256(src.channel(q), i_sw_offset, y1_in_range);
                __m256 se_val = mask_gather_ps256(src.channel(q), i_se_offset, v11_in_range);
#endif

                __m256 _v = _mm256_mul_ps(nw_val, nw);
                _v = _mm256_comp_fmadd_ps(ne_val, ne, _v);
                _v = _mm256_comp_fmadd_ps(sw_val, sw, _v);
                _v = _mm256_comp_fmadd_ps(se_val, se, _v);

                _mm256_storeu_ps(dst.channel(q).row(y) + x / 2, _v);
            }
        }

        nn = grid_size & 15;
#endif // __AVX__

        for (int x = grid_size - nn; x < grid_size; x += 2)
        {
            float sample_x = gridptr[x];
            float sample_y = gridptr[x + 1];

            sample_x = ((sample_x + 1) * src.w - 1) / 2.f;
            sample_y = ((sample_y + 1) * src.h - 1) / 2.f;

            sample_x = abs(sample_x + 0.5f);
            sample_x = src.w - abs(sample_x - src.w) - 0.5;

            sample_y = abs(sample_y + 0.5f);
            sample_y = src.h - abs(sample_y - src.h) - 0.5;

            sample_x = std::min(src.w - 1.0f, std::max(sample_x, 0.0f));
            sample_y = std::min(src.h - 1.0f, std::max(sample_y, 0.0f));

            // bilinear interpolate
            int x0 = (int)floor(sample_x);
            int y0 = (int)floor(sample_y);
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            bool x1_in_range = (x1 > -1) & (x1 < src.w);
            bool y1_in_range = (y1 > -1) & (y1 < src.h);
            bool v11_in_range = x1_in_range & y1_in_range;

            float alpha = sample_x - x0;
            float beta = sample_y - y0;

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
                float v00 = image.row(y0)[x0];
                float v01 = x1_in_range ? image.row(y0)[x1] : 0;
                float v10 = y1_in_range ? image.row(y1)[x0] : 0;
                float v11 = v11_in_range ? image.row(y1)[x1] : 0;

                float v0 = v00 * (1 - alpha) + v01 * alpha;
                float v1 = v10 * (1 - alpha) + v11 * alpha;

                dst.channel(q).row(y)[x / 2] = v0 * (1 - beta) + v1 * beta;
            }
        }
    }
}

static void gridsample_2d_bilinear_align1_reflection_blob_pack1(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    const int grid_size = grid.w * grid.h;
#if __AVX__
    const __m256 vImgWf = _mm256_set1_ps(src.w);
    const __m256 vImgHf = _mm256_set1_ps(src.h);
#if __AVX2__
    const __m256i vImgWi = _mm256_set1_epi32(src.w);
    const __m256i vImgHi = _mm256_set1_epi32(src.h);
#endif // __AVX2__
#endif // __AVX__

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < grid.c; y++)
    {
        const float* gridptr = grid.channel(y);
        int nn = grid_size;
#if __AVX__
        for (int x = 0; x + 15 < grid_size; x += 16)
        {
            __m256 tmp_x = _mm256_loadu_ps(gridptr + x);
            __m256 gy = _mm256_loadu_ps(gridptr + x + 8);

            __m256 gx = _mm256_permute2f128_ps(tmp_x, gy, 0b00100000);
            gy = _mm256_permute2f128_ps(tmp_x, gy, 0b00110001);
            tmp_x = _mm256_or_ps(gx, _mm256_setzero_ps());

            gx = _mm256_shuffle_ps(gx, gy, 0b10001000);
            gy = _mm256_shuffle_ps(tmp_x, gy, 0b11011101);

            // compute coord
            {
                // x
                gx = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gx, *(__m256*)_ps256_1), *(__m256*)_ps256_2), _mm256_sub_ps(vImgWf, *(__m256*)_ps256_1));

                const __m256 border_x = _mm256_sub_ps(vImgWf, *(__m256*)_ps256_1);

                gx = _mm256_and_ps(gx, *(__m256*)_ps256_inv_sign_mask);

                __m256 reflectx_v = _mm256_and_ps(_mm256_sub_ps(gx, border_x), *(__m256*)_ps256_inv_sign_mask);
                gx = _mm256_sub_ps(border_x, reflectx_v);

                // y
                gy = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gy, *(__m256*)_ps256_1), *(__m256*)_ps256_2), _mm256_sub_ps(vImgHf, *(__m256*)_ps256_1));

                const __m256 border_y = _mm256_sub_ps(vImgHf, *(__m256*)_ps256_1);

                gy = _mm256_and_ps(gy, *(__m256*)_ps256_inv_sign_mask);

                __m256 reflecty_v = _mm256_and_ps(_mm256_sub_ps(gy, border_y), *(__m256*)_ps256_inv_sign_mask);
                gy = _mm256_sub_ps(border_y, reflecty_v);
            }

            __m256 x_w = _mm256_floor_ps(gx);
            __m256 y_n = _mm256_floor_ps(gy);

            __m256 w = _mm256_sub_ps(gx, x_w);
            __m256 e = _mm256_sub_ps(*(__m256*)_ps256_1, w);
            __m256 n = _mm256_sub_ps(gy, y_n);
            __m256 s = _mm256_sub_ps(*(__m256*)_ps256_1, n);

            __m256 nw = _mm256_mul_ps(s, e);
            __m256 ne = _mm256_mul_ps(s, w);
            __m256 sw = _mm256_mul_ps(n, e);
            __m256 se = _mm256_mul_ps(n, w);

#if __AVX2__
            __m256i x0 = _mm256_cvtps_epi32(x_w);
            __m256i y0 = _mm256_cvtps_epi32(y_n);
            __m256i x1 = _mm256_add_epi32(x0, *(__m256i*)_pi32_256_1);
            __m256i y1 = _mm256_add_epi32(y0, *(__m256i*)_pi32_256_1);

            __m256i x1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgWi, x1));
            __m256i y1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgHi, y1));

            __m256i v11_in_range = _mm256_and_si256(x1_in_range, y1_in_range);

            __m256i i_nw_offset = _mm256_add_epi32(_mm256_mullo_epi32(y0, vImgWi), x0);
            __m256i i_ne_offset = _mm256_add_epi32(i_nw_offset, *(__m256i*)_pi32_256_1);
            __m256i i_sw_offset = _mm256_add_epi32(i_nw_offset, vImgWi);
            __m256i i_se_offset = _mm256_add_epi32(i_sw_offset, *(__m256i*)_pi32_256_1);
#else
            __m256 x1 = _mm256_add_ps(x_w, *(__m256*)_ps256_1);
            __m256 y1 = _mm256_add_ps(y_n, *(__m256*)_ps256_1);

            __m256 x1_in_range = _mm256_and_ps(_mm256_cmp_ps(x1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgWf, x1, _CMP_GT_OS));
            __m256 y1_in_range = _mm256_and_ps(_mm256_cmp_ps(y1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgHf, y1, _CMP_GT_OS));

            __m256 v11_in_range = _mm256_and_ps(x1_in_range, y1_in_range);

            __m256 nw_offset = _mm256_add_ps(_mm256_mul_ps(y_n, vImgWf), x_w);
            __m256 ne_offset = _mm256_add_ps(nw_offset, *(__m256*)_ps256_1);
            __m256 sw_offset = _mm256_add_ps(nw_offset, vImgWf);
            __m256 se_offset = _mm256_add_ps(sw_offset, *(__m256*)_ps256_1);

            __m256i i_nw_offset = _mm256_cvtps_epi32(nw_offset);
            __m256i i_ne_offset = _mm256_cvtps_epi32(ne_offset);
            __m256i i_sw_offset = _mm256_cvtps_epi32(sw_offset);
            __m256i i_se_offset = _mm256_cvtps_epi32(se_offset);
#endif

            for (int q = 0; q < src.c; q++)
            {
                __m256 nw_val = mask_gather_ps256(src.channel(q), i_nw_offset, *(__m256*)_ps256_n1);
#if __AVX2__
                __m256 ne_val = mask_gather_ps256(src.channel(q), i_ne_offset, _mm256_castsi256_ps(x1_in_range));
                __m256 sw_val = mask_gather_ps256(src.channel(q), i_sw_offset, _mm256_castsi256_ps(y1_in_range));
                __m256 se_val = mask_gather_ps256(src.channel(q), i_se_offset, _mm256_castsi256_ps(v11_in_range));
#else
                __m256 ne_val = mask_gather_ps256(src.channel(q), i_ne_offset, x1_in_range);
                __m256 sw_val = mask_gather_ps256(src.channel(q), i_sw_offset, y1_in_range);
                __m256 se_val = mask_gather_ps256(src.channel(q), i_se_offset, v11_in_range);
#endif

                __m256 _v = _mm256_mul_ps(nw_val, nw);
                _v = _mm256_comp_fmadd_ps(ne_val, ne, _v);
                _v = _mm256_comp_fmadd_ps(sw_val, sw, _v);
                _v = _mm256_comp_fmadd_ps(se_val, se, _v);

                _mm256_storeu_ps(dst.channel(q).row(y) + x / 2, _v);
            }
        }

        nn = grid_size & 15;
#endif // __AVX__
        for (int x = grid_size - nn; x < grid_size; x += 2)
        {
            float sample_x = gridptr[x];
            float sample_y = gridptr[x + 1];

            sample_x = (sample_x + 1) / 2.f * (src.w - 1);
            sample_y = (sample_y + 1) / 2.f * (src.h - 1);

            sample_x = abs(sample_x);
            sample_x = (src.w - 1) - abs(sample_x - (src.w - 1));

            sample_y = abs(sample_y);
            sample_y = (src.h - 1) - abs(sample_y - (src.h - 1));

            sample_x = std::min(src.w - 1.0f, std::max(sample_x, 0.0f));
            sample_y = std::min(src.h - 1.0f, std::max(sample_y, 0.0f));

            // bilinear interpolate
            int x0 = (int)floor(sample_x);
            int y0 = (int)floor(sample_y);
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            bool x1_in_range = (x1 > -1) & (x1 < src.w);
            bool y1_in_range = (y1 > -1) & (y1 < src.h);
            bool v11_in_range = x1_in_range & y1_in_range;

            float alpha = sample_x - x0;
            float beta = sample_y - y0;

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
                float v00 = image.row(y0)[x0];
                float v01 = x1_in_range ? image.row(y0)[x1] : 0;
                float v10 = y1_in_range ? image.row(y1)[x0] : 0;
                float v11 = v11_in_range ? image.row(y1)[x1] : 0;

                float v0 = v00 * (1 - alpha) + v01 * alpha;
                float v1 = v10 * (1 - alpha) + v11 * alpha;

                dst.channel(q).row(y)[x / 2] = v0 * (1 - beta) + v1 * beta;
            }
        }
    }
}

static void gridsample_3d_bilinear_align0_zeros_blob_pack1(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    const int grid_size = grid.w * grid.h * grid.d;
#if __AVX__
    const __m256 vImgWf = _mm256_set1_ps(src.w);
    const __m256 vImgHf = _mm256_set1_ps(src.h);
    const __m256 vImgDf = _mm256_set1_ps(src.d);
#if __AVX2__
    const __m256i vImgWi = _mm256_set1_epi32(src.w);
    const __m256i vImgHi = _mm256_set1_epi32(src.h);
    const __m256i vImgDi = _mm256_set1_epi32(src.d);
#endif // __AVX2__
#endif // __AVX__

     #pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < grid.c; y++)
    {
        const float* gridptr = grid.channel(y);
        int nn = grid_size;
#if __AVX__
        for (int x = 0; x + 23 < nn; x += 24)
        {
            //upzip (3)
            __m256 tmp_x = _mm256_loadu_ps(gridptr + x);
            __m256 tmp_y = _mm256_loadu_ps(gridptr + x + 8);
            __m256 gz = _mm256_loadu_ps(gridptr + x + 16);

            __m256 gx = _mm256_permute2f128_ps(tmp_x, tmp_y, 0b00110000);
            __m256 gy = _mm256_permute2f128_ps(tmp_x, gz, 0b00100001);
            gz = _mm256_permute2f128_ps(tmp_y, gz, 0b00110000);

            tmp_x = _mm256_shuffle_ps(gx, gy, 0b01001001);
            tmp_y = _mm256_shuffle_ps(gy, gz, 0b10011110);

            gy = _mm256_shuffle_ps(tmp_x, tmp_y, 0b11011000);
            gx = _mm256_shuffle_ps(gx, tmp_y, 0b10001100);
            gz = _mm256_shuffle_ps(tmp_x, gz, 0b11001101);

            // compute coord
            {
                // x
                gx = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gx, *(__m256*)_ps256_1), vImgWf, *(__m256*)_ps256_1), *(__m256*)_ps256_2);

                // y
                gy = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gy, *(__m256*)_ps256_1), vImgHf, *(__m256*)_ps256_1), *(__m256*)_ps256_2);

                // z
                gz = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gz, *(__m256*)_ps256_1), vImgDf, *(__m256*)_ps256_1), *(__m256*)_ps256_2);
            }

            __m256 x_w = _mm256_floor_ps(gx);
            __m256 y_n = _mm256_floor_ps(gy);
            __m256 z_t = _mm256_floor_ps(gz);

            __m256 w = _mm256_sub_ps(gx, x_w);
            __m256 e = _mm256_sub_ps(*(__m256*)_ps256_1, w);
            __m256 n = _mm256_sub_ps(gy, y_n);
            __m256 s = _mm256_sub_ps(*(__m256*)_ps256_1, n);
            __m256 t = _mm256_sub_ps(gz, z_t);
            __m256 b = _mm256_sub_ps(*(__m256*)_ps256_1, t);

            __m256 tnw, tne, tsw, tse, bnw, bne, bsw, bse;
            {
                __m256 nw = _mm256_mul_ps(s, e);
                __m256 ne = _mm256_mul_ps(s, w);
                __m256 sw = _mm256_mul_ps(n, e);
                __m256 se = _mm256_mul_ps(n, w);

                tnw = _mm256_mul_ps(b, nw);
                tne = _mm256_mul_ps(b, ne);
                tsw = _mm256_mul_ps(b, sw);
                tse = _mm256_mul_ps(b, se);

                bnw = _mm256_mul_ps(t, nw);
                bne = _mm256_mul_ps(t, ne);
                bsw = _mm256_mul_ps(t, sw);
                bse = _mm256_mul_ps(t, se);
            }

#if __AVX2__
            __m256i x0 = _mm256_cvtps_epi32(x_w);
            __m256i y0 = _mm256_cvtps_epi32(y_n);
            __m256i z0 = _mm256_cvtps_epi32(z_t);

            __m256i x1 = _mm256_add_epi32(x0, *(__m256i*)_pi32_256_1);
            __m256i y1 = _mm256_add_epi32(y0, *(__m256i*)_pi32_256_1);
            __m256i z1 = _mm256_add_epi32(z0, *(__m256i*)_pi32_256_1);

            __m256i x0_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x0, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgWi, x0));
            __m256i x1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgWi, x1));
            __m256i y0_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y0, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgHi, y0));
            __m256i y1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgHi, y1));
            __m256i z0_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(z0, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgDi, z0));
            __m256i z1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(z1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgDi, z1));

            __m256i v000_in_range, v010_in_range, v100_in_range, v110_in_range, v001_in_range, v011_in_range, v101_in_range, v111_in_range;
            {
                __m256i v00_in_range = _mm256_and_si256(x0_in_range, y0_in_range);
                __m256i v01_in_range = _mm256_and_si256(x0_in_range, y1_in_range);
                __m256i v10_in_range = _mm256_and_si256(x1_in_range, y0_in_range);
                __m256i v11_in_range = _mm256_and_si256(x1_in_range, y1_in_range);

                v000_in_range = _mm256_and_si256(v00_in_range, z0_in_range);
                v010_in_range = _mm256_and_si256(v01_in_range, z0_in_range);
                v100_in_range = _mm256_and_si256(v10_in_range, z0_in_range);
                v110_in_range = _mm256_and_si256(v11_in_range, z0_in_range);

                v001_in_range = _mm256_and_si256(v00_in_range, z1_in_range);
                v011_in_range = _mm256_and_si256(v01_in_range, z1_in_range);
                v101_in_range = _mm256_and_si256(v10_in_range, z1_in_range);
                v111_in_range = _mm256_and_si256(v11_in_range, z1_in_range);
            }

            __m256i i_tnw_offset = _mm256_add_epi32(_mm256_mullo_epi32(_mm256_mullo_epi32(vImgWi, vImgHi), z0), _mm256_add_epi32(_mm256_mullo_epi32(y0, vImgWi), x0));
            __m256i i_tne_offset = _mm256_add_epi32(i_tnw_offset, *(__m256i*)_pi32_256_1);
            __m256i i_tsw_offset = _mm256_add_epi32(i_tnw_offset, vImgWi);
            __m256i i_tse_offset = _mm256_add_epi32(i_tsw_offset, *(__m256i*)_pi32_256_1);

            __m256i i_bnw_offset = _mm256_add_epi32(_mm256_mullo_epi32(vImgWi, vImgHi), i_tnw_offset);
            __m256i i_bne_offset = _mm256_add_epi32(i_bnw_offset, *(__m256i*)_pi32_256_1);
            __m256i i_bsw_offset = _mm256_add_epi32(i_bnw_offset, vImgWi);
            __m256i i_bse_offset = _mm256_add_epi32(i_bsw_offset, *(__m256i*)_pi32_256_1);
#else
            __m256 x1 = _mm256_add_ps(x_w, *(__m256*)_ps256_1);
            __m256 y1 = _mm256_add_ps(y_n, *(__m256*)_ps256_1);
            __m256 z1 = _mm256_add_ps(z_t, *(__m256*)_ps256_1);

            __m256 x0_in_range = _mm256_and_ps(_mm256_cmp_ps(x_w, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgWf, x_w, _CMP_GT_OS));
            __m256 x1_in_range = _mm256_and_ps(_mm256_cmp_ps(x1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgWf, x1, _CMP_GT_OS));
            __m256 y0_in_range = _mm256_and_ps(_mm256_cmp_ps(y_n, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgHf, y_n, _CMP_GT_OS));
            __m256 y1_in_range = _mm256_and_ps(_mm256_cmp_ps(y1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgHf, y1, _CMP_GT_OS));
            __m256 z0_in_range = _mm256_and_ps(_mm256_cmp_ps(z_t, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgDf, z_t, _CMP_GT_OS));
            __m256 z1_in_range = _mm256_and_ps(_mm256_cmp_ps(z1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgDf, z1, _CMP_GT_OS));

            __m256 v000_in_range, v010_in_range, v100_in_range, v110_in_range, v001_in_range, v011_in_range, v101_in_range, v111_in_range;
            {
                __m256 v00_in_range = _mm256_and_ps(x0_in_range, y0_in_range);
                __m256 v01_in_range = _mm256_and_ps(x0_in_range, y1_in_range);
                __m256 v10_in_range = _mm256_and_ps(x1_in_range, y0_in_range);
                __m256 v11_in_range = _mm256_and_ps(x1_in_range, y1_in_range);

                v000_in_range = _mm256_and_ps(v00_in_range, z0_in_range);
                v010_in_range = _mm256_and_ps(v01_in_range, z0_in_range);
                v100_in_range = _mm256_and_ps(v10_in_range, z0_in_range);
                v110_in_range = _mm256_and_ps(v11_in_range, z0_in_range);

                v001_in_range = _mm256_and_ps(v00_in_range, z1_in_range);
                v011_in_range = _mm256_and_ps(v01_in_range, z1_in_range);
                v101_in_range = _mm256_and_ps(v10_in_range, z1_in_range);
                v111_in_range = _mm256_and_ps(v11_in_range, z1_in_range);
            }

            __m256 tnw_offset = _mm256_add_ps(_mm256_mul_ps(_mm256_mul_ps(vImgWf, vImgHf), z_t),
                                              _mm256_add_ps(_mm256_mul_ps(y_n, vImgWf), x_w));
            __m256 tne_offset = _mm256_add_ps(tnw_offset, *(__m256*)_ps256_1);
            __m256 tsw_offset = _mm256_add_ps(tnw_offset, vImgWf);
            __m256 tse_offset = _mm256_add_ps(tsw_offset, *(__m256*)_ps256_1);

            __m256 bnw_offset = _mm256_add_ps(_mm256_mul_ps(vImgWf, vImgHf), tnw_offset);
            __m256 bne_offset = _mm256_add_ps(bnw_offset, *(__m256*)_ps256_1);
            __m256 bsw_offset = _mm256_add_ps(bnw_offset, vImgWf);
            __m256 bse_offset = _mm256_add_ps(bsw_offset, *(__m256*)_ps256_1);

            __m256i i_tnw_offset = _mm256_cvtps_epi32(tnw_offset);
            __m256i i_tne_offset = _mm256_cvtps_epi32(tne_offset);
            __m256i i_tsw_offset = _mm256_cvtps_epi32(tsw_offset);
            __m256i i_tse_offset = _mm256_cvtps_epi32(tse_offset);

            __m256i i_bnw_offset = _mm256_cvtps_epi32(bnw_offset);
            __m256i i_bne_offset = _mm256_cvtps_epi32(bne_offset);
            __m256i i_bsw_offset = _mm256_cvtps_epi32(bsw_offset);
            __m256i i_bse_offset = _mm256_cvtps_epi32(bse_offset);
#endif // __AVX2__

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
#if __AVX2__
                __m256 tnw_val = mask_gather_ps256(image, i_tnw_offset, _mm256_castsi256_ps(v000_in_range));
                __m256 tne_val = mask_gather_ps256(image, i_tne_offset, _mm256_castsi256_ps(v100_in_range));
                __m256 tsw_val = mask_gather_ps256(image, i_tsw_offset, _mm256_castsi256_ps(v010_in_range));
                __m256 tse_val = mask_gather_ps256(image, i_tse_offset, _mm256_castsi256_ps(v110_in_range));

                __m256 bnw_val = mask_gather_ps256(image, i_bnw_offset, _mm256_castsi256_ps(v001_in_range));
                __m256 bne_val = mask_gather_ps256(image, i_bne_offset, _mm256_castsi256_ps(v101_in_range));
                __m256 bsw_val = mask_gather_ps256(image, i_bsw_offset, _mm256_castsi256_ps(v011_in_range));
                __m256 bse_val = mask_gather_ps256(image, i_bse_offset, _mm256_castsi256_ps(v111_in_range));
#else
                __m256 tnw_val = mask_gather_ps256(image, i_tnw_offset, v000_in_range);
                __m256 tne_val = mask_gather_ps256(image, i_tne_offset, v100_in_range);
                __m256 tsw_val = mask_gather_ps256(image, i_tsw_offset, v010_in_range);
                __m256 tse_val = mask_gather_ps256(image, i_tse_offset, v110_in_range);

                __m256 bnw_val = mask_gather_ps256(image, i_bnw_offset, v001_in_range);
                __m256 bne_val = mask_gather_ps256(image, i_bne_offset, v101_in_range);
                __m256 bsw_val = mask_gather_ps256(image, i_bsw_offset, v011_in_range);
                __m256 bse_val = mask_gather_ps256(image, i_bse_offset, v111_in_range);
#endif

                __m256 _v = _mm256_mul_ps(tnw_val, tnw);
                _v = _mm256_comp_fmadd_ps(tne_val, tne, _v);
                _v = _mm256_comp_fmadd_ps(tsw_val, tsw, _v);
                _v = _mm256_comp_fmadd_ps(tse_val, tse, _v);

                _v = _mm256_comp_fmadd_ps(bnw_val, bnw, _v);
                _v = _mm256_comp_fmadd_ps(bne_val, bne, _v);
                _v = _mm256_comp_fmadd_ps(bsw_val, bsw, _v);
                _v = _mm256_comp_fmadd_ps(bse_val, bse, _v);

                _mm256_storeu_ps(static_cast<float*>(dst.channel(q).depth(y).data) + x / 3, _v);
            }
        }

        nn = grid_size % 24;

#endif // __AVX__
        for (int x = grid_size - nn; x < grid_size; x += 3)
        {
            float gx = gridptr[x];
            float gy = gridptr[x + 1];
            float gz = gridptr[x + 2];

            gx = ((gx + 1) * src.w - 1) / 2.f;
            gy = ((gy + 1) * src.h - 1) / 2.f;
            gz = ((gz + 1) * src.d - 1) / 2.f;

            // bilinear interpolate
            int x0 = (int)floor(gx);
            int y0 = (int)floor(gy);
            int z0 = (int)floor(gz);
            int x1 = x0 + 1;
            int y1 = y0 + 1;
            int z1 = z0 + 1;

            bool x0_in_range = (x0 > -1) & (x0 < src.w);
            bool y0_in_range = (y0 > -1) & (y0 < src.h);
            bool z0_in_range = (z0 > -1) & (z0 < src.d);
            bool x1_in_range = (x1 > -1) & (x1 < src.w);
            bool y1_in_range = (y1 > -1) & (y1 < src.h);
            bool z1_in_range = (z1 > -1) & (z1 < src.d);

            bool v00_in_range = x0_in_range & y0_in_range;
            bool v01_in_range = x1_in_range & y0_in_range;
            bool v10_in_range = x0_in_range & y1_in_range;
            bool v11_in_range = x1_in_range & y1_in_range;

            bool v000_in_range = v00_in_range & z0_in_range;
            bool v010_in_range = v10_in_range & z0_in_range;
            bool v100_in_range = v00_in_range & z1_in_range;
            bool v110_in_range = v10_in_range & z1_in_range;

            bool v001_in_range = v01_in_range & z0_in_range;
            bool v011_in_range = v11_in_range & z0_in_range;
            bool v101_in_range = v01_in_range & z1_in_range;
            bool v111_in_range = v11_in_range & z1_in_range;

            float alpha = gx - x0;
            float beta = gy - y0;
            float gamma = gz - z0;

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
                float v000 = v000_in_range ? image.depth(z0).row(y0)[x0] : 0;
                float v010 = v010_in_range ? image.depth(z0).row(y1)[x0] : 0;
                float v100 = v100_in_range ? image.depth(z1).row(y0)[x0] : 0;
                float v110 = v110_in_range ? image.depth(z1).row(y1)[x0] : 0;

                float v001 = v001_in_range ? image.depth(z0).row(y0)[x1] : 0;
                float v011 = v011_in_range ? image.depth(z0).row(y1)[x1] : 0;
                float v101 = v101_in_range ? image.depth(z1).row(y0)[x1] : 0;
                float v111 = v111_in_range ? image.depth(z1).row(y1)[x1] : 0;

                float v00 = v000 * (1 - alpha) + v001 * alpha;
                float v01 = v010 * (1 - alpha) + v011 * alpha;
                float v10 = v100 * (1 - alpha) + v101 * alpha;
                float v11 = v110 * (1 - alpha) + v111 * alpha;

                float v0 = v00 * (1 - beta) + v01 * beta;
                float v1 = v10 * (1 - beta) + v11 * beta;

                dst.channel(q).depth(y)[x / 3] = v0 * (1 - gamma) + v1 * gamma;
            }
        }
    }
}

static void gridsample_3d_bilinear_align1_zeros_blob_pack1(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    const int grid_size = grid.w * grid.h * grid.d;
#if __AVX__
    const __m256 vImgWf = _mm256_set1_ps(src.w);
    const __m256 vImgHf = _mm256_set1_ps(src.h);
    const __m256 vImgDf = _mm256_set1_ps(src.d);
#if __AVX2__
    const __m256i vImgWi = _mm256_set1_epi32(src.w);
    const __m256i vImgHi = _mm256_set1_epi32(src.h);
    const __m256i vImgDi = _mm256_set1_epi32(src.d);
#endif // __AVX2__
#endif // __AVX__

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < grid.c; y++)
    {
        const float* gridptr = grid.channel(y);
        int nn = grid_size;
#if __AVX__
        for (int x = 0; x + 23 < nn; x += 24)
        {
            __m256 tmp_x = _mm256_loadu_ps(gridptr + x);
            __m256 tmp_y = _mm256_loadu_ps(gridptr + x + 8);
            __m256 gz = _mm256_loadu_ps(gridptr + x + 16);

            __m256 gx = _mm256_permute2f128_ps(tmp_x, tmp_y, 0b00110000);
            __m256 gy = _mm256_permute2f128_ps(tmp_x, gz, 0b00100001);
            gz = _mm256_permute2f128_ps(tmp_y, gz, 0b00110000);

            tmp_x = _mm256_shuffle_ps(gx, gy, 0b01001001);
            tmp_y = _mm256_shuffle_ps(gy, gz, 0b10011110);

            gy = _mm256_shuffle_ps(tmp_x, tmp_y, 0b11011000);
            gx = _mm256_shuffle_ps(gx, tmp_y, 0b10001100);
            gz = _mm256_shuffle_ps(tmp_x, gz, 0b11001101);

            // compute coord
            {
                // x
                gx = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gx, *(__m256*)_ps256_1), *(__m256*)_ps256_2), _mm256_sub_ps(vImgWf, *(__m256*)_ps256_1));

                // y
                gy = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gy, *(__m256*)_ps256_1), *(__m256*)_ps256_2), _mm256_sub_ps(vImgHf, *(__m256*)_ps256_1));

                // z
                gz = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gz, *(__m256*)_ps256_1), *(__m256*)_ps256_2), _mm256_sub_ps(vImgDf, *(__m256*)_ps256_1));
            }

            __m256 x_w = _mm256_floor_ps(gx);
            __m256 y_n = _mm256_floor_ps(gy);
            __m256 z_t = _mm256_floor_ps(gz);

            __m256 w = _mm256_sub_ps(gx, x_w);
            __m256 e = _mm256_sub_ps(*(__m256*)_ps256_1, w);
            __m256 n = _mm256_sub_ps(gy, y_n);
            __m256 s = _mm256_sub_ps(*(__m256*)_ps256_1, n);
            __m256 t = _mm256_sub_ps(gz, z_t);
            __m256 b = _mm256_sub_ps(*(__m256*)_ps256_1, t);

            __m256 tnw, tne, tsw, tse, bnw, bne, bsw, bse;
            {
                __m256 nw = _mm256_mul_ps(s, e);
                __m256 ne = _mm256_mul_ps(s, w);
                __m256 sw = _mm256_mul_ps(n, e);
                __m256 se = _mm256_mul_ps(n, w);

                tnw = _mm256_mul_ps(b, nw);
                tne = _mm256_mul_ps(b, ne);
                tsw = _mm256_mul_ps(b, sw);
                tse = _mm256_mul_ps(b, se);

                bnw = _mm256_mul_ps(t, nw);
                bne = _mm256_mul_ps(t, ne);
                bsw = _mm256_mul_ps(t, sw);
                bse = _mm256_mul_ps(t, se);
            }

#if __AVX2__
            __m256i x0 = _mm256_cvtps_epi32(x_w);
            __m256i y0 = _mm256_cvtps_epi32(y_n);
            __m256i z0 = _mm256_cvtps_epi32(z_t);

            __m256i x1 = _mm256_add_epi32(x0, *(__m256i*)_pi32_256_1);
            __m256i y1 = _mm256_add_epi32(y0, *(__m256i*)_pi32_256_1);
            __m256i z1 = _mm256_add_epi32(z0, *(__m256i*)_pi32_256_1);

            __m256i x0_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x0, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgWi, x0));
            __m256i x1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgWi, x1));
            __m256i y0_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y0, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgHi, y0));
            __m256i y1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgHi, y1));
            __m256i z0_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(z0, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgDi, z0));
            __m256i z1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(z1, *(__m256i*)_pi32_256_n1), _mm256_cmpgt_epi32(vImgDi, z1));

            __m256i v000_in_range, v010_in_range, v100_in_range, v110_in_range, v001_in_range, v011_in_range, v101_in_range, v111_in_range;
            {
                __m256i v00_in_range = _mm256_and_si256(x0_in_range, y0_in_range);
                __m256i v01_in_range = _mm256_and_si256(x0_in_range, y1_in_range);
                __m256i v10_in_range = _mm256_and_si256(x1_in_range, y0_in_range);
                __m256i v11_in_range = _mm256_and_si256(x1_in_range, y1_in_range);

                v000_in_range = _mm256_and_si256(v00_in_range, z0_in_range);
                v010_in_range = _mm256_and_si256(v01_in_range, z0_in_range);
                v100_in_range = _mm256_and_si256(v10_in_range, z0_in_range);
                v110_in_range = _mm256_and_si256(v11_in_range, z0_in_range);

                v001_in_range = _mm256_and_si256(v00_in_range, z1_in_range);
                v011_in_range = _mm256_and_si256(v01_in_range, z1_in_range);
                v101_in_range = _mm256_and_si256(v10_in_range, z1_in_range);
                v111_in_range = _mm256_and_si256(v11_in_range, z1_in_range);
            }

            __m256i i_tnw_offset = _mm256_add_epi32(_mm256_mullo_epi32(_mm256_mullo_epi32(vImgWi, vImgHi), z0), _mm256_add_epi32(_mm256_mullo_epi32(y0, vImgWi), x0));
            __m256i i_tne_offset = _mm256_add_epi32(i_tnw_offset, *(__m256i*)_pi32_256_1);
            __m256i i_tsw_offset = _mm256_add_epi32(i_tnw_offset, vImgWi);
            __m256i i_tse_offset = _mm256_add_epi32(i_tsw_offset, *(__m256i*)_pi32_256_1);

            __m256i i_bnw_offset = _mm256_add_epi32(_mm256_mullo_epi32(vImgWi, vImgHi), i_tnw_offset);
            __m256i i_bne_offset = _mm256_add_epi32(i_bnw_offset, *(__m256i*)_pi32_256_1);
            __m256i i_bsw_offset = _mm256_add_epi32(i_bnw_offset, vImgWi);
            __m256i i_bse_offset = _mm256_add_epi32(i_bsw_offset, *(__m256i*)_pi32_256_1);
#else
            __m256 x1 = _mm256_add_ps(x_w, *(__m256*)_ps256_1);
            __m256 y1 = _mm256_add_ps(y_n, *(__m256*)_ps256_1);
            __m256 z1 = _mm256_add_ps(z_t, *(__m256*)_ps256_1);

            __m256 x0_in_range = _mm256_and_ps(_mm256_cmp_ps(x_w, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgWf, x_w, _CMP_GT_OS));
            __m256 x1_in_range = _mm256_and_ps(_mm256_cmp_ps(x1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgWf, x1, _CMP_GT_OS));
            __m256 y0_in_range = _mm256_and_ps(_mm256_cmp_ps(y_n, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgHf, y_n, _CMP_GT_OS));
            __m256 y1_in_range = _mm256_and_ps(_mm256_cmp_ps(y1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgHf, y1, _CMP_GT_OS));
            __m256 z0_in_range = _mm256_and_ps(_mm256_cmp_ps(z_t, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgDf, z_t, _CMP_GT_OS));
            __m256 z1_in_range = _mm256_and_ps(_mm256_cmp_ps(z1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgDf, z1, _CMP_GT_OS));

            __m256 v000_in_range, v010_in_range, v100_in_range, v110_in_range, v001_in_range, v011_in_range, v101_in_range, v111_in_range;
            {
                __m256 v00_in_range = _mm256_and_ps(x0_in_range, y0_in_range);
                __m256 v01_in_range = _mm256_and_ps(x0_in_range, y1_in_range);
                __m256 v10_in_range = _mm256_and_ps(x1_in_range, y0_in_range);
                __m256 v11_in_range = _mm256_and_ps(x1_in_range, y1_in_range);

                v000_in_range = _mm256_and_ps(v00_in_range, z0_in_range);
                v010_in_range = _mm256_and_ps(v01_in_range, z0_in_range);
                v100_in_range = _mm256_and_ps(v10_in_range, z0_in_range);
                v110_in_range = _mm256_and_ps(v11_in_range, z0_in_range);

                v001_in_range = _mm256_and_ps(v00_in_range, z1_in_range);
                v011_in_range = _mm256_and_ps(v01_in_range, z1_in_range);
                v101_in_range = _mm256_and_ps(v10_in_range, z1_in_range);
                v111_in_range = _mm256_and_ps(v11_in_range, z1_in_range);
            }

            __m256 tnw_offset = _mm256_add_ps(_mm256_mul_ps(_mm256_mul_ps(vImgWf, vImgHf), z_t),
                                              _mm256_add_ps(_mm256_mul_ps(y_n, vImgWf), x_w));
            __m256 tne_offset = _mm256_add_ps(tnw_offset, *(__m256*)_ps256_1);
            __m256 tsw_offset = _mm256_add_ps(tnw_offset, vImgWf);
            __m256 tse_offset = _mm256_add_ps(tsw_offset, *(__m256*)_ps256_1);

            __m256 bnw_offset = _mm256_add_ps(_mm256_mul_ps(vImgWf, vImgHf), tnw_offset);
            __m256 bne_offset = _mm256_add_ps(bnw_offset, *(__m256*)_ps256_1);
            __m256 bsw_offset = _mm256_add_ps(bnw_offset, vImgWf);
            __m256 bse_offset = _mm256_add_ps(bsw_offset, *(__m256*)_ps256_1);

            __m256i i_tnw_offset = _mm256_cvtps_epi32(tnw_offset);
            __m256i i_tne_offset = _mm256_cvtps_epi32(tne_offset);
            __m256i i_tsw_offset = _mm256_cvtps_epi32(tsw_offset);
            __m256i i_tse_offset = _mm256_cvtps_epi32(tse_offset);

            __m256i i_bnw_offset = _mm256_cvtps_epi32(bnw_offset);
            __m256i i_bne_offset = _mm256_cvtps_epi32(bne_offset);
            __m256i i_bsw_offset = _mm256_cvtps_epi32(bsw_offset);
            __m256i i_bse_offset = _mm256_cvtps_epi32(bse_offset);
#endif // __AVX2__

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
#if __AVX2__
                __m256 tnw_val = mask_gather_ps256(image, i_tnw_offset, _mm256_castsi256_ps(v000_in_range));
                __m256 tne_val = mask_gather_ps256(image, i_tne_offset, _mm256_castsi256_ps(v100_in_range));
                __m256 tsw_val = mask_gather_ps256(image, i_tsw_offset, _mm256_castsi256_ps(v010_in_range));
                __m256 tse_val = mask_gather_ps256(image, i_tse_offset, _mm256_castsi256_ps(v110_in_range));

                __m256 bnw_val = mask_gather_ps256(image, i_bnw_offset, _mm256_castsi256_ps(v001_in_range));
                __m256 bne_val = mask_gather_ps256(image, i_bne_offset, _mm256_castsi256_ps(v101_in_range));
                __m256 bsw_val = mask_gather_ps256(image, i_bsw_offset, _mm256_castsi256_ps(v011_in_range));
                __m256 bse_val = mask_gather_ps256(image, i_bse_offset, _mm256_castsi256_ps(v111_in_range));
#else
                __m256 tnw_val = mask_gather_ps256(image, i_tnw_offset, v000_in_range);
                __m256 tne_val = mask_gather_ps256(image, i_tne_offset, v100_in_range);
                __m256 tsw_val = mask_gather_ps256(image, i_tsw_offset, v010_in_range);
                __m256 tse_val = mask_gather_ps256(image, i_tse_offset, v110_in_range);

                __m256 bnw_val = mask_gather_ps256(image, i_bnw_offset, v001_in_range);
                __m256 bne_val = mask_gather_ps256(image, i_bne_offset, v101_in_range);
                __m256 bsw_val = mask_gather_ps256(image, i_bsw_offset, v011_in_range);
                __m256 bse_val = mask_gather_ps256(image, i_bse_offset, v111_in_range);
#endif

                __m256 _v = _mm256_mul_ps(tnw_val, tnw);
                _v = _mm256_comp_fmadd_ps(tne_val, tne, _v);
                _v = _mm256_comp_fmadd_ps(tsw_val, tsw, _v);
                _v = _mm256_comp_fmadd_ps(tse_val, tse, _v);

                _v = _mm256_comp_fmadd_ps(bnw_val, bnw, _v);
                _v = _mm256_comp_fmadd_ps(bne_val, bne, _v);
                _v = _mm256_comp_fmadd_ps(bsw_val, bsw, _v);
                _v = _mm256_comp_fmadd_ps(bse_val, bse, _v);

                _mm256_storeu_ps(static_cast<float*>(dst.channel(q).depth(y).data) + x / 3, _v);
            }
        }
        nn = grid_size % 24;
#endif // __AVX__
        for (int x = grid_size - nn; x < grid_size; x += 3)
        {
            float gx = gridptr[x];
            float gy = gridptr[x + 1];
            float gz = gridptr[x + 2];

            gx = (gx + 1) / 2.f * (src.w - 1);
            gy = (gy + 1) / 2.f * (src.h - 1);
            gz = (gz + 1) / 2.f * (src.d - 1);

            // bilinear interpolate
            int x0 = (int)floor(gx);
            int y0 = (int)floor(gy);
            int z0 = (int)floor(gz);
            int x1 = x0 + 1;
            int y1 = y0 + 1;
            int z1 = z0 + 1;

            bool x0_in_range = (x0 > -1) & (x0 < src.w);
            bool y0_in_range = (y0 > -1) & (y0 < src.h);
            bool z0_in_range = (z0 > -1) & (z0 < src.d);
            bool x1_in_range = (x1 > -1) & (x1 < src.w);
            bool y1_in_range = (y1 > -1) & (y1 < src.h);
            bool z1_in_range = (z1 > -1) & (z1 < src.d);

            bool v00_in_range = x0_in_range & y0_in_range;
            bool v01_in_range = x1_in_range & y0_in_range;
            bool v10_in_range = x0_in_range & y1_in_range;
            bool v11_in_range = x1_in_range & y1_in_range;

            bool v000_in_range = v00_in_range & z0_in_range;
            bool v010_in_range = v10_in_range & z0_in_range;
            bool v100_in_range = v00_in_range & z1_in_range;
            bool v110_in_range = v10_in_range & z1_in_range;

            bool v001_in_range = v01_in_range & z0_in_range;
            bool v011_in_range = v11_in_range & z0_in_range;
            bool v101_in_range = v01_in_range & z1_in_range;
            bool v111_in_range = v11_in_range & z1_in_range;

            float alpha = gx - x0;
            float beta = gy - y0;
            float gamma = gz - z0;

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
                float v000 = v000_in_range ? image.depth(z0).row(y0)[x0] : 0;
                float v010 = v010_in_range ? image.depth(z0).row(y1)[x0] : 0;
                float v100 = v100_in_range ? image.depth(z1).row(y0)[x0] : 0;
                float v110 = v110_in_range ? image.depth(z1).row(y1)[x0] : 0;

                float v001 = v001_in_range ? image.depth(z0).row(y0)[x1] : 0;
                float v011 = v011_in_range ? image.depth(z0).row(y1)[x1] : 0;
                float v101 = v101_in_range ? image.depth(z1).row(y0)[x1] : 0;
                float v111 = v111_in_range ? image.depth(z1).row(y1)[x1] : 0;

                float v00 = v000 * (1 - alpha) + v001 * alpha;
                float v01 = v010 * (1 - alpha) + v011 * alpha;
                float v10 = v100 * (1 - alpha) + v101 * alpha;
                float v11 = v110 * (1 - alpha) + v111 * alpha;

                float v0 = v00 * (1 - beta) + v01 * beta;
                float v1 = v10 * (1 - beta) + v11 * beta;

                dst.channel(q).depth(y)[x / 3] = v0 * (1 - gamma) + v1 * gamma;
            }
        }
    }
}

static void gridsample_3d_bilinear_align0_border_blob_pack1(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    const int grid_size = grid.w * grid.h * grid.d;
#if __AVX__
    const __m256 vImgWf = _mm256_set1_ps(src.w);
    const __m256 vImgHf = _mm256_set1_ps(src.h);
    const __m256 vImgDf = _mm256_set1_ps(src.d);
#endif // __AVX__

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < grid.c; y++)
    {
        const float* gridptr = grid.channel(y);
        int nn = grid_size;
#if __AVX__
        for (int x = 0; x + 23 < nn; x += 24)
        {
            __m256 tmp_x = _mm256_loadu_ps(gridptr + x);
            __m256 tmp_y = _mm256_loadu_ps(gridptr + x + 8);
            __m256 gz = _mm256_loadu_ps(gridptr + x + 16);

            __m256 gx = _mm256_permute2f128_ps(tmp_x, tmp_y, 0b00110000);
            __m256 gy = _mm256_permute2f128_ps(tmp_x, gz, 0b00100001);
            gz = _mm256_permute2f128_ps(tmp_y, gz, 0b00110000);

            tmp_x = _mm256_shuffle_ps(gx, gy, 0b01001001);
            tmp_y = _mm256_shuffle_ps(gy, gz, 0b10011110);

            gy = _mm256_shuffle_ps(tmp_x, tmp_y, 0b11011000);
            gx = _mm256_shuffle_ps(gx, tmp_y, 0b10001100);
            gz = _mm256_shuffle_ps(tmp_x, gz, 0b11001101);

            // compute coord
            {
                // x
                gx = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gx, *(__m256*)_ps256_1), vImgWf, *(__m256*)_ps256_1), *(__m256*)_ps256_2);

                const __m256 border_x = _mm256_sub_ps(vImgWf, *(__m256*)_ps256_1);

                gx = _mm256_min_ps(border_x, _mm256_max_ps(gx, _mm256_setzero_ps()));

                // y
                gy = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gy, *(__m256*)_ps256_1), vImgHf, *(__m256*)_ps256_1), *(__m256*)_ps256_2);

                const __m256 border_y = _mm256_sub_ps(vImgHf, *(__m256*)_ps256_1);

                gy = _mm256_min_ps(border_y, _mm256_max_ps(gy, _mm256_setzero_ps()));

                // z
                gz = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gz, *(__m256*)_ps256_1), vImgDf, *(__m256*)_ps256_1), *(__m256*)_ps256_2);

                const __m256 border_z = _mm256_sub_ps(vImgDf, *(__m256*)_ps256_1);

                gz = _mm256_min_ps(border_z, _mm256_max_ps(gz, _mm256_setzero_ps()));
            }

            __m256 x_w = _mm256_floor_ps(gx);
            __m256 y_n = _mm256_floor_ps(gy);
            __m256 z_t = _mm256_floor_ps(gz);

            __m256 w = _mm256_sub_ps(gx, x_w);
            __m256 e = _mm256_sub_ps(*(__m256*)_ps256_1, w);
            __m256 n = _mm256_sub_ps(gy, y_n);
            __m256 s = _mm256_sub_ps(*(__m256*)_ps256_1, n);
            __m256 t = _mm256_sub_ps(gz, z_t);
            __m256 b = _mm256_sub_ps(*(__m256*)_ps256_1, t);

            __m256 tnw, tne, tsw, tse, bnw, bne, bsw, bse;
            {
                __m256 nw = _mm256_mul_ps(s, e);
                __m256 ne = _mm256_mul_ps(s, w);
                __m256 sw = _mm256_mul_ps(n, e);
                __m256 se = _mm256_mul_ps(n, w);

                tnw = _mm256_mul_ps(b, nw);
                tne = _mm256_mul_ps(b, ne);
                tsw = _mm256_mul_ps(b, sw);
                tse = _mm256_mul_ps(b, se);

                bnw = _mm256_mul_ps(t, nw);
                bne = _mm256_mul_ps(t, ne);
                bsw = _mm256_mul_ps(t, sw);
                bse = _mm256_mul_ps(t, se);
            }

            __m256 x1 = _mm256_add_ps(x_w, *(__m256*)_ps256_1);
            __m256 y1 = _mm256_add_ps(y_n, *(__m256*)_ps256_1);
            __m256 z1 = _mm256_add_ps(z_t, *(__m256*)_ps256_1);

            __m256 x1_in_range = _mm256_and_ps(_mm256_cmp_ps(x1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgWf, x1, _CMP_GT_OS));
            __m256 y1_in_range = _mm256_and_ps(_mm256_cmp_ps(y1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgHf, y1, _CMP_GT_OS));
            __m256 z1_in_range = _mm256_and_ps(_mm256_cmp_ps(z1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgDf, z1, _CMP_GT_OS));

            __m256 v110_in_range, v011_in_range, v101_in_range, v111_in_range;
            {
                __m256 v11_in_range = _mm256_and_ps(x1_in_range, y1_in_range);

                v110_in_range = _mm256_and_ps(x1_in_range, y1_in_range);

                v011_in_range = _mm256_and_ps(y1_in_range, z1_in_range);
                v101_in_range = _mm256_and_ps(x1_in_range, z1_in_range);
                v111_in_range = _mm256_and_ps(v11_in_range, z1_in_range);
            }

            __m256 tnw_offset = _mm256_add_ps(_mm256_mul_ps(_mm256_mul_ps(vImgWf, vImgHf), z_t),
                                              _mm256_add_ps(_mm256_mul_ps(y_n, vImgWf), x_w));
            __m256 tne_offset = _mm256_add_ps(tnw_offset, *(__m256*)_ps256_1);
            __m256 tsw_offset = _mm256_add_ps(tnw_offset, vImgWf);
            __m256 tse_offset = _mm256_add_ps(tsw_offset, *(__m256*)_ps256_1);

            __m256 bnw_offset = _mm256_add_ps(_mm256_mul_ps(vImgWf, vImgHf), tnw_offset);
            __m256 bne_offset = _mm256_add_ps(bnw_offset, *(__m256*)_ps256_1);
            __m256 bsw_offset = _mm256_add_ps(bnw_offset, vImgWf);
            __m256 bse_offset = _mm256_add_ps(bsw_offset, *(__m256*)_ps256_1);

            __m256i i_tnw_offset = _mm256_cvtps_epi32(tnw_offset);
            __m256i i_tne_offset = _mm256_cvtps_epi32(tne_offset);
            __m256i i_tsw_offset = _mm256_cvtps_epi32(tsw_offset);
            __m256i i_tse_offset = _mm256_cvtps_epi32(tse_offset);

            __m256i i_bnw_offset = _mm256_cvtps_epi32(bnw_offset);
            __m256i i_bne_offset = _mm256_cvtps_epi32(bne_offset);
            __m256i i_bsw_offset = _mm256_cvtps_epi32(bsw_offset);
            __m256i i_bse_offset = _mm256_cvtps_epi32(bse_offset);

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
                __m256 tnw_val = mask_gather_ps256(image, i_tnw_offset, *(__m256*)_ps256_n1);
                __m256 tne_val = mask_gather_ps256(image, i_tne_offset, x1_in_range);
                __m256 tsw_val = mask_gather_ps256(image, i_tsw_offset, y1_in_range);
                __m256 tse_val = mask_gather_ps256(image, i_tse_offset, v110_in_range);

                __m256 bnw_val = mask_gather_ps256(image, i_bnw_offset, z1_in_range);
                __m256 bne_val = mask_gather_ps256(image, i_bne_offset, v101_in_range);
                __m256 bsw_val = mask_gather_ps256(image, i_bsw_offset, v011_in_range);
                __m256 bse_val = mask_gather_ps256(image, i_bse_offset, v111_in_range);

                __m256 _v = _mm256_mul_ps(tnw_val, tnw);
                _v = _mm256_comp_fmadd_ps(tne_val, tne, _v);
                _v = _mm256_comp_fmadd_ps(tsw_val, tsw, _v);
                _v = _mm256_comp_fmadd_ps(tse_val, tse, _v);

                _v = _mm256_comp_fmadd_ps(bnw_val, bnw, _v);
                _v = _mm256_comp_fmadd_ps(bne_val, bne, _v);
                _v = _mm256_comp_fmadd_ps(bsw_val, bsw, _v);
                _v = _mm256_comp_fmadd_ps(bse_val, bse, _v);

                _mm256_storeu_ps(static_cast<float*>(dst.channel(q).depth(y).data) + x / 3, _v);
            }
        }
        nn = grid_size % 24;
#endif // __AVX__
        for (int x = grid_size - nn; x < grid_size; x += 3)
        {
            float gx = gridptr[x];
            float gy = gridptr[x + 1];
            float gz = gridptr[x + 2];

            gx = ((gx + 1) * src.w - 1) / 2.f;
            gy = ((gy + 1) * src.h - 1) / 2.f;
            gz = ((gz + 1) * src.d - 1) / 2.f;

            gx = std::min(src.w - 1.0f, std::max(gx, 0.0f));
            gy = std::min(src.h - 1.0f, std::max(gy, 0.0f));
            gz = std::min(src.d - 1.0f, std::max(gz, 0.0f));

            // bilinear interpolate
            int x0 = (int)floor(gx);
            int y0 = (int)floor(gy);
            int z0 = (int)floor(gz);
            int x1 = x0 + 1;
            int y1 = y0 + 1;
            int z1 = z0 + 1;

            bool x1_in_range = (x1 > -1) & (x1 < src.w);
            bool y1_in_range = (y1 > -1) & (y1 < src.h);
            bool z1_in_range = (z1 > -1) & (z1 < src.d);

            bool v11_in_range = x1_in_range & y1_in_range;

            bool v110_in_range = y1_in_range & z1_in_range;

            bool v101_in_range = x1_in_range & z1_in_range;
            bool v111_in_range = v11_in_range & z1_in_range;

            float alpha = gx - x0;
            float beta = gy - y0;
            float gamma = gz - z0;

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
                float v000 = image.depth(z0).row(y0)[x0];
                float v010 = y1_in_range ? image.depth(z0).row(y1)[x0] : 0;
                float v100 = z1_in_range ? image.depth(z1).row(y0)[x0] : 0;
                float v110 = v110_in_range ? image.depth(z1).row(y1)[x0] : 0;

                float v001 = x1_in_range ? image.depth(z0).row(y0)[x1] : 0;
                float v011 = v11_in_range ? image.depth(z0).row(y1)[x1] : 0;
                float v101 = v101_in_range ? image.depth(z1).row(y0)[x1] : 0;
                float v111 = v111_in_range ? image.depth(z1).row(y1)[x1] : 0;

                float v00 = v000 * (1 - alpha) + v001 * alpha;
                float v01 = v010 * (1 - alpha) + v011 * alpha;
                float v10 = v100 * (1 - alpha) + v101 * alpha;
                float v11 = v110 * (1 - alpha) + v111 * alpha;

                float v0 = v00 * (1 - beta) + v01 * beta;
                float v1 = v10 * (1 - beta) + v11 * beta;

                dst.channel(q).depth(y)[x / 3] = v0 * (1 - gamma) + v1 * gamma;
            }
        }
    }
}

static void gridsample_3d_bilinear_align1_border_blob_pack1(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    const int grid_size = grid.w * grid.h * grid.d;
#if __AVX__
    const __m256 vImgWf = _mm256_set1_ps(src.w);
    const __m256 vImgHf = _mm256_set1_ps(src.h);
    const __m256 vImgDf = _mm256_set1_ps(src.d);
#endif // __AVX__

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < grid.c; y++)
    {
        const float* gridptr = grid.channel(y);
        int nn = grid_size;
#if __AVX__
        for (int x = 0; x + 23 < nn; x += 24)
        {
            __m256 tmp_x = _mm256_loadu_ps(gridptr + x);
            __m256 tmp_y = _mm256_loadu_ps(gridptr + x + 8);
            __m256 gz = _mm256_loadu_ps(gridptr + x + 16);

            __m256 gx = _mm256_permute2f128_ps(tmp_x, tmp_y, 0b00110000);
            __m256 gy = _mm256_permute2f128_ps(tmp_x, gz, 0b00100001);
            gz = _mm256_permute2f128_ps(tmp_y, gz, 0b00110000);

            tmp_x = _mm256_shuffle_ps(gx, gy, 0b01001001);
            tmp_y = _mm256_shuffle_ps(gy, gz, 0b10011110);

            gy = _mm256_shuffle_ps(tmp_x, tmp_y, 0b11011000);
            gx = _mm256_shuffle_ps(gx, tmp_y, 0b10001100);
            gz = _mm256_shuffle_ps(tmp_x, gz, 0b11001101);

            // compute coord
            {
                // x
                gx = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gx, *(__m256*)_ps256_1), *(__m256*)_ps256_2), _mm256_sub_ps(vImgWf, *(__m256*)_ps256_1));

                const __m256 border_x = _mm256_sub_ps(vImgWf, *(__m256*)_ps256_1);

                gx = _mm256_min_ps(border_x, _mm256_max_ps(gx, _mm256_setzero_ps()));

                // y
                gy = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gy, *(__m256*)_ps256_1), *(__m256*)_ps256_2), _mm256_sub_ps(vImgHf, *(__m256*)_ps256_1));

                const __m256 border_y = _mm256_sub_ps(vImgHf, *(__m256*)_ps256_1);

                gy = _mm256_min_ps(border_y, _mm256_max_ps(gy, _mm256_setzero_ps()));

                // z
                gz = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gz, *(__m256*)_ps256_1), *(__m256*)_ps256_2), _mm256_sub_ps(vImgDf, *(__m256*)_ps256_1));

                const __m256 border_z = _mm256_sub_ps(vImgDf, *(__m256*)_ps256_1);

                gz = _mm256_min_ps(border_z, _mm256_max_ps(gz, _mm256_setzero_ps()));
            }

            __m256 x_w = _mm256_floor_ps(gx);
            __m256 y_n = _mm256_floor_ps(gy);
            __m256 z_t = _mm256_floor_ps(gz);

            __m256 w = _mm256_sub_ps(gx, x_w);
            __m256 e = _mm256_sub_ps(*(__m256*)_ps256_1, w);
            __m256 n = _mm256_sub_ps(gy, y_n);
            __m256 s = _mm256_sub_ps(*(__m256*)_ps256_1, n);
            __m256 t = _mm256_sub_ps(gz, z_t);
            __m256 b = _mm256_sub_ps(*(__m256*)_ps256_1, t);

            __m256 tnw, tne, tsw, tse, bnw, bne, bsw, bse;
            {
                __m256 nw = _mm256_mul_ps(s, e);
                __m256 ne = _mm256_mul_ps(s, w);
                __m256 sw = _mm256_mul_ps(n, e);
                __m256 se = _mm256_mul_ps(n, w);

                tnw = _mm256_mul_ps(b, nw);
                tne = _mm256_mul_ps(b, ne);
                tsw = _mm256_mul_ps(b, sw);
                tse = _mm256_mul_ps(b, se);

                bnw = _mm256_mul_ps(t, nw);
                bne = _mm256_mul_ps(t, ne);
                bsw = _mm256_mul_ps(t, sw);
                bse = _mm256_mul_ps(t, se);
            }

            __m256 x1 = _mm256_add_ps(x_w, *(__m256*)_ps256_1);
            __m256 y1 = _mm256_add_ps(y_n, *(__m256*)_ps256_1);
            __m256 z1 = _mm256_add_ps(z_t, *(__m256*)_ps256_1);

            __m256 x1_in_range = _mm256_and_ps(_mm256_cmp_ps(x1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgWf, x1, _CMP_GT_OS));
            __m256 y1_in_range = _mm256_and_ps(_mm256_cmp_ps(y1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgHf, y1, _CMP_GT_OS));
            __m256 z1_in_range = _mm256_and_ps(_mm256_cmp_ps(z1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgDf, z1, _CMP_GT_OS));

            __m256 v110_in_range, v011_in_range, v101_in_range, v111_in_range;
            {
                __m256 v11_in_range = _mm256_and_ps(x1_in_range, y1_in_range);

                v110_in_range = _mm256_and_ps(x1_in_range, y1_in_range);

                v011_in_range = _mm256_and_ps(y1_in_range, z1_in_range);
                v101_in_range = _mm256_and_ps(x1_in_range, z1_in_range);
                v111_in_range = _mm256_and_ps(v11_in_range, z1_in_range);
            }

            __m256 tnw_offset = _mm256_add_ps(_mm256_mul_ps(_mm256_mul_ps(vImgWf, vImgHf), z_t),
                                              _mm256_add_ps(_mm256_mul_ps(y_n, vImgWf), x_w));
            __m256 tne_offset = _mm256_add_ps(tnw_offset, *(__m256*)_ps256_1);
            __m256 tsw_offset = _mm256_add_ps(tnw_offset, vImgWf);
            __m256 tse_offset = _mm256_add_ps(tsw_offset, *(__m256*)_ps256_1);

            __m256 bnw_offset = _mm256_add_ps(_mm256_mul_ps(vImgWf, vImgHf), tnw_offset);
            __m256 bne_offset = _mm256_add_ps(bnw_offset, *(__m256*)_ps256_1);
            __m256 bsw_offset = _mm256_add_ps(bnw_offset, vImgWf);
            __m256 bse_offset = _mm256_add_ps(bsw_offset, *(__m256*)_ps256_1);

            __m256i i_tnw_offset = _mm256_cvtps_epi32(tnw_offset);
            __m256i i_tne_offset = _mm256_cvtps_epi32(tne_offset);
            __m256i i_tsw_offset = _mm256_cvtps_epi32(tsw_offset);
            __m256i i_tse_offset = _mm256_cvtps_epi32(tse_offset);

            __m256i i_bnw_offset = _mm256_cvtps_epi32(bnw_offset);
            __m256i i_bne_offset = _mm256_cvtps_epi32(bne_offset);
            __m256i i_bsw_offset = _mm256_cvtps_epi32(bsw_offset);
            __m256i i_bse_offset = _mm256_cvtps_epi32(bse_offset);

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
                __m256 tnw_val = mask_gather_ps256(image, i_tnw_offset, *(__m256*)_ps256_n1);
                __m256 tne_val = mask_gather_ps256(image, i_tne_offset, x1_in_range);
                __m256 tsw_val = mask_gather_ps256(image, i_tsw_offset, y1_in_range);
                __m256 tse_val = mask_gather_ps256(image, i_tse_offset, v110_in_range);

                __m256 bnw_val = mask_gather_ps256(image, i_bnw_offset, z1_in_range);
                __m256 bne_val = mask_gather_ps256(image, i_bne_offset, v101_in_range);
                __m256 bsw_val = mask_gather_ps256(image, i_bsw_offset, v011_in_range);
                __m256 bse_val = mask_gather_ps256(image, i_bse_offset, v111_in_range);

                __m256 _v = _mm256_mul_ps(tnw_val, tnw);
                _v = _mm256_comp_fmadd_ps(tne_val, tne, _v);
                _v = _mm256_comp_fmadd_ps(tsw_val, tsw, _v);
                _v = _mm256_comp_fmadd_ps(tse_val, tse, _v);

                _v = _mm256_comp_fmadd_ps(bnw_val, bnw, _v);
                _v = _mm256_comp_fmadd_ps(bne_val, bne, _v);
                _v = _mm256_comp_fmadd_ps(bsw_val, bsw, _v);
                _v = _mm256_comp_fmadd_ps(bse_val, bse, _v);

                _mm256_storeu_ps(static_cast<float*>(dst.channel(q).depth(y).data) + x / 3, _v);
            }
        }
        nn = grid_size % 24;
#endif // __AVX__
        for (int x = grid_size - nn; x < grid_size; x += 3)
        {
            float gx = gridptr[x];
            float gy = gridptr[x + 1];
            float gz = gridptr[x + 2];

            gx = (gx + 1) / 2.f * (src.w - 1);
            gy = (gy + 1) / 2.f * (src.h - 1);
            gz = (gz + 1) / 2.f * (src.d - 1);

            gx = std::min(src.w - 1.0f, std::max(gx, 0.0f));
            gy = std::min(src.h - 1.0f, std::max(gy, 0.0f));
            gz = std::min(src.d - 1.0f, std::max(gz, 0.0f));

            // bilinear interpolate
            int x0 = (int)floor(gx);
            int y0 = (int)floor(gy);
            int z0 = (int)floor(gz);
            int x1 = x0 + 1;
            int y1 = y0 + 1;
            int z1 = z0 + 1;

            bool x1_in_range = (x1 > -1) & (x1 < src.w);
            bool y1_in_range = (y1 > -1) & (y1 < src.h);
            bool z1_in_range = (z1 > -1) & (z1 < src.d);

            bool v11_in_range = x1_in_range & y1_in_range;

            bool v110_in_range = y1_in_range & z1_in_range;

            bool v101_in_range = x1_in_range & z1_in_range;
            bool v111_in_range = v11_in_range & z1_in_range;

            float alpha = gx - x0;
            float beta = gy - y0;
            float gamma = gz - z0;

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
                float v000 = image.depth(z0).row(y0)[x0];
                float v010 = y1_in_range ? image.depth(z0).row(y1)[x0] : 0;
                float v100 = z1_in_range ? image.depth(z1).row(y0)[x0] : 0;
                float v110 = v110_in_range ? image.depth(z1).row(y1)[x0] : 0;

                float v001 = x1_in_range ? image.depth(z0).row(y0)[x1] : 0;
                float v011 = v11_in_range ? image.depth(z0).row(y1)[x1] : 0;
                float v101 = v101_in_range ? image.depth(z1).row(y0)[x1] : 0;
                float v111 = v111_in_range ? image.depth(z1).row(y1)[x1] : 0;

                float v00 = v000 * (1 - alpha) + v001 * alpha;
                float v01 = v010 * (1 - alpha) + v011 * alpha;
                float v10 = v100 * (1 - alpha) + v101 * alpha;
                float v11 = v110 * (1 - alpha) + v111 * alpha;

                float v0 = v00 * (1 - beta) + v01 * beta;
                float v1 = v10 * (1 - beta) + v11 * beta;

                dst.channel(q).depth(y)[x / 3] = v0 * (1 - gamma) + v1 * gamma;
            }
        }
    }
}

static void gridsample_3d_bilinear_align0_reflection_blob_pack1(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    const int grid_size = grid.w * grid.h * grid.d;
#if __AVX__
    const __m256 vImgWf = _mm256_set1_ps(src.w);
    const __m256 vImgHf = _mm256_set1_ps(src.h);
    const __m256 vImgDf = _mm256_set1_ps(src.d);
#endif // __AVX__

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < grid.c; y++)
    {
        const float* gridptr = grid.channel(y);
        int nn = grid_size;
#if __AVX__
        for (int x = 0; x + 23 < nn; x += 24)
        {
            __m256 tmp_x = _mm256_loadu_ps(gridptr + x);
            __m256 tmp_y = _mm256_loadu_ps(gridptr + x + 8);
            __m256 gz = _mm256_loadu_ps(gridptr + x + 16);

            __m256 gx = _mm256_permute2f128_ps(tmp_x, tmp_y, 0b00110000);
            __m256 gy = _mm256_permute2f128_ps(tmp_x, gz, 0b00100001);
            gz = _mm256_permute2f128_ps(tmp_y, gz, 0b00110000);

            tmp_x = _mm256_shuffle_ps(gx, gy, 0b01001001);
            tmp_y = _mm256_shuffle_ps(gy, gz, 0b10011110);

            gy = _mm256_shuffle_ps(tmp_x, tmp_y, 0b11011000);
            gx = _mm256_shuffle_ps(gx, tmp_y, 0b10001100);
            gz = _mm256_shuffle_ps(tmp_x, gz, 0b11001101);

            // compute coord
            {
                // x
                gx = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gx, *(__m256*)_ps256_1), vImgWf, *(__m256*)_ps256_1), *(__m256*)_ps256_2);
                const __m256 border_x = _mm256_sub_ps(vImgWf, *(__m256*)_ps256_1);

                __m256 v0p5fp8 = _mm256_set1_ps(0.5f);
                gx = _mm256_add_ps(gx, v0p5fp8);

                gx = _mm256_and_ps(gx, *(__m256*)_ps256_inv_sign_mask);

                __m256 reflectx_v = _mm256_and_ps(_mm256_sub_ps(gx, vImgWf), *(__m256*)_ps256_inv_sign_mask);
                gx = _mm256_sub_ps(vImgWf, reflectx_v);

                gx = _mm256_sub_ps(gx, v0p5fp8);

                _mm256_sub_ps(gx, v0p5fp8);

                gx = _mm256_min_ps(border_x, _mm256_max_ps(gx, _mm256_setzero_ps()));

                // y
                gy = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gy, *(__m256*)_ps256_1), vImgHf, *(__m256*)_ps256_1), *(__m256*)_ps256_2);
                const __m256 border_y = _mm256_sub_ps(vImgHf, *(__m256*)_ps256_1);

                gy = _mm256_add_ps(gy, v0p5fp8);

                gy = _mm256_and_ps(gy, *(__m256*)_ps256_inv_sign_mask);

                __m256 reflecty_v = _mm256_and_ps(_mm256_sub_ps(gy, vImgHf), *(__m256*)_ps256_inv_sign_mask);
                gy = _mm256_sub_ps(vImgHf, reflecty_v);

                gy = _mm256_sub_ps(gy, v0p5fp8);

                _mm256_sub_ps(gy, v0p5fp8);

                gy = _mm256_min_ps(border_y, _mm256_max_ps(gy, _mm256_setzero_ps()));

                // z
                gz = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gz, *(__m256*)_ps256_1), vImgDf, *(__m256*)_ps256_1), *(__m256*)_ps256_2);
                const __m256 border_z = _mm256_sub_ps(vImgDf, *(__m256*)_ps256_1);

                gz = _mm256_add_ps(gz, v0p5fp8);

                gz = _mm256_and_ps(gz, *(__m256*)_ps256_inv_sign_mask);

                __m256 reflectz_v = _mm256_and_ps(_mm256_sub_ps(gz, vImgDf), *(__m256*)_ps256_inv_sign_mask);
                gz = _mm256_sub_ps(vImgDf, reflectz_v);

                gz = _mm256_sub_ps(gz, v0p5fp8);

                _mm256_sub_ps(gz, v0p5fp8);

                gz = _mm256_min_ps(border_z, _mm256_max_ps(gz, _mm256_setzero_ps()));
            }

            __m256 x_w = _mm256_floor_ps(gx);
            __m256 y_n = _mm256_floor_ps(gy);
            __m256 z_t = _mm256_floor_ps(gz);

            __m256 w = _mm256_sub_ps(gx, x_w);
            __m256 e = _mm256_sub_ps(*(__m256*)_ps256_1, w);
            __m256 n = _mm256_sub_ps(gy, y_n);
            __m256 s = _mm256_sub_ps(*(__m256*)_ps256_1, n);
            __m256 t = _mm256_sub_ps(gz, z_t);
            __m256 b = _mm256_sub_ps(*(__m256*)_ps256_1, t);

            __m256 tnw, tne, tsw, tse, bnw, bne, bsw, bse;
            {
                __m256 nw = _mm256_mul_ps(s, e);
                __m256 ne = _mm256_mul_ps(s, w);
                __m256 sw = _mm256_mul_ps(n, e);
                __m256 se = _mm256_mul_ps(n, w);

                tnw = _mm256_mul_ps(b, nw);
                tne = _mm256_mul_ps(b, ne);
                tsw = _mm256_mul_ps(b, sw);
                tse = _mm256_mul_ps(b, se);

                bnw = _mm256_mul_ps(t, nw);
                bne = _mm256_mul_ps(t, ne);
                bsw = _mm256_mul_ps(t, sw);
                bse = _mm256_mul_ps(t, se);
            }

            __m256 x1 = _mm256_add_ps(x_w, *(__m256*)_ps256_1);
            __m256 y1 = _mm256_add_ps(y_n, *(__m256*)_ps256_1);
            __m256 z1 = _mm256_add_ps(z_t, *(__m256*)_ps256_1);

            __m256 x1_in_range = _mm256_and_ps(_mm256_cmp_ps(x1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgWf, x1, _CMP_GT_OS));
            __m256 y1_in_range = _mm256_and_ps(_mm256_cmp_ps(y1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgHf, y1, _CMP_GT_OS));
            __m256 z1_in_range = _mm256_and_ps(_mm256_cmp_ps(z1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgDf, z1, _CMP_GT_OS));

            __m256 v110_in_range, v011_in_range, v101_in_range, v111_in_range;
            {
                __m256 v11_in_range = _mm256_and_ps(x1_in_range, y1_in_range);

                v110_in_range = _mm256_and_ps(x1_in_range, y1_in_range);

                v011_in_range = _mm256_and_ps(y1_in_range, z1_in_range);
                v101_in_range = _mm256_and_ps(x1_in_range, z1_in_range);
                v111_in_range = _mm256_and_ps(v11_in_range, z1_in_range);
            }

            __m256 tnw_offset = _mm256_add_ps(_mm256_mul_ps(_mm256_mul_ps(vImgWf, vImgHf), z_t),
                                              _mm256_add_ps(_mm256_mul_ps(y_n, vImgWf), x_w));
            __m256 tne_offset = _mm256_add_ps(tnw_offset, *(__m256*)_ps256_1);
            __m256 tsw_offset = _mm256_add_ps(tnw_offset, vImgWf);
            __m256 tse_offset = _mm256_add_ps(tsw_offset, *(__m256*)_ps256_1);

            __m256 bnw_offset = _mm256_add_ps(_mm256_mul_ps(vImgWf, vImgHf), tnw_offset);
            __m256 bne_offset = _mm256_add_ps(bnw_offset, *(__m256*)_ps256_1);
            __m256 bsw_offset = _mm256_add_ps(bnw_offset, vImgWf);
            __m256 bse_offset = _mm256_add_ps(bsw_offset, *(__m256*)_ps256_1);

            __m256i i_tnw_offset = _mm256_cvtps_epi32(tnw_offset);
            __m256i i_tne_offset = _mm256_cvtps_epi32(tne_offset);
            __m256i i_tsw_offset = _mm256_cvtps_epi32(tsw_offset);
            __m256i i_tse_offset = _mm256_cvtps_epi32(tse_offset);

            __m256i i_bnw_offset = _mm256_cvtps_epi32(bnw_offset);
            __m256i i_bne_offset = _mm256_cvtps_epi32(bne_offset);
            __m256i i_bsw_offset = _mm256_cvtps_epi32(bsw_offset);
            __m256i i_bse_offset = _mm256_cvtps_epi32(bse_offset);

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
                __m256 tnw_val = mask_gather_ps256(image, i_tnw_offset, *(__m256*)_ps256_n1);
                __m256 tne_val = mask_gather_ps256(image, i_tne_offset, x1_in_range);
                __m256 tsw_val = mask_gather_ps256(image, i_tsw_offset, y1_in_range);
                __m256 tse_val = mask_gather_ps256(image, i_tse_offset, v110_in_range);

                __m256 bnw_val = mask_gather_ps256(image, i_bnw_offset, z1_in_range);
                __m256 bne_val = mask_gather_ps256(image, i_bne_offset, v101_in_range);
                __m256 bsw_val = mask_gather_ps256(image, i_bsw_offset, v011_in_range);
                __m256 bse_val = mask_gather_ps256(image, i_bse_offset, v111_in_range);

                __m256 _v = _mm256_mul_ps(tnw_val, tnw);
                _v = _mm256_comp_fmadd_ps(tne_val, tne, _v);
                _v = _mm256_comp_fmadd_ps(tsw_val, tsw, _v);
                _v = _mm256_comp_fmadd_ps(tse_val, tse, _v);

                _v = _mm256_comp_fmadd_ps(bnw_val, bnw, _v);
                _v = _mm256_comp_fmadd_ps(bne_val, bne, _v);
                _v = _mm256_comp_fmadd_ps(bsw_val, bsw, _v);
                _v = _mm256_comp_fmadd_ps(bse_val, bse, _v);

                _mm256_storeu_ps(static_cast<float*>(dst.channel(q).depth(y).data) + x / 3, _v);
            }
        }
        nn = grid_size % 24;
#endif // __AVX__
        for (int x = grid_size - nn; x < grid_size; x += 3)
        {
            float gx = gridptr[x];
            float gy = gridptr[x + 1];
            float gz = gridptr[x + 2];

            gx = ((gx + 1) * src.w - 1) / 2.f;
            gy = ((gy + 1) * src.h - 1) / 2.f;
            gz = ((gz + 1) * src.d - 1) / 2.f;

            gx = abs(gx + 0.5f);
            gx = src.w - abs(gx - src.w) - 0.5;

            gy = abs(gy + 0.5f);
            gy = src.h - abs(gy - src.h) - 0.5;

            gz = abs(gz + 0.5f);
            gz = src.d - abs(gz - src.d) - 0.5;

            gx = std::min(src.w - 1.0f, std::max(gx, 0.0f));
            gy = std::min(src.h - 1.0f, std::max(gy, 0.0f));
            gz = std::min(src.d - 1.0f, std::max(gz, 0.0f));

            // bilinear interpolate
            int x0 = (int)floor(gx);
            int y0 = (int)floor(gy);
            int z0 = (int)floor(gz);
            int x1 = x0 + 1;
            int y1 = y0 + 1;
            int z1 = z0 + 1;

            bool x1_in_range = (x1 > -1) & (x1 < src.w);
            bool y1_in_range = (y1 > -1) & (y1 < src.h);
            bool z1_in_range = (z1 > -1) & (z1 < src.d);

            bool v11_in_range = x1_in_range & y1_in_range;

            bool v110_in_range = y1_in_range & z1_in_range;

            bool v101_in_range = x1_in_range & z1_in_range;
            bool v111_in_range = v11_in_range & z1_in_range;

            float alpha = gx - x0;
            float beta = gy - y0;
            float gamma = gz - z0;

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
                float v000 = image.depth(z0).row(y0)[x0];
                float v010 = y1_in_range ? image.depth(z0).row(y1)[x0] : 0;
                float v100 = z1_in_range ? image.depth(z1).row(y0)[x0] : 0;
                float v110 = v110_in_range ? image.depth(z1).row(y1)[x0] : 0;

                float v001 = x1_in_range ? image.depth(z0).row(y0)[x1] : 0;
                float v011 = v11_in_range ? image.depth(z0).row(y1)[x1] : 0;
                float v101 = v101_in_range ? image.depth(z1).row(y0)[x1] : 0;
                float v111 = v111_in_range ? image.depth(z1).row(y1)[x1] : 0;

                float v00 = v000 * (1 - alpha) + v001 * alpha;
                float v01 = v010 * (1 - alpha) + v011 * alpha;
                float v10 = v100 * (1 - alpha) + v101 * alpha;
                float v11 = v110 * (1 - alpha) + v111 * alpha;

                float v0 = v00 * (1 - beta) + v01 * beta;
                float v1 = v10 * (1 - beta) + v11 * beta;

                dst.channel(q).depth(y)[x / 3] = v0 * (1 - gamma) + v1 * gamma;
            }
        }
    }
}

static void gridsample_3d_bilinear_align1_reflection_blob_pack1(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    const int grid_size = grid.w * grid.h * grid.d;
#if __AVX__
    const __m256 vImgWf = _mm256_set1_ps(src.w);
    const __m256 vImgHf = _mm256_set1_ps(src.h);
    const __m256 vImgDf = _mm256_set1_ps(src.d);
#endif // __AVX__

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < grid.c; y++)
    {
        const float* gridptr = grid.channel(y);
        int nn = grid_size;
#if __AVX__
        for (int x = 0; x + 23 < nn; x += 24)
        {
            __m256 tmp_x = _mm256_loadu_ps(gridptr + x);
            __m256 tmp_y = _mm256_loadu_ps(gridptr + x + 8);
            __m256 gz = _mm256_loadu_ps(gridptr + x + 16);

            __m256 gx = _mm256_permute2f128_ps(tmp_x, tmp_y, 0b00110000);
            __m256 gy = _mm256_permute2f128_ps(tmp_x, gz, 0b00100001);
            gz = _mm256_permute2f128_ps(tmp_y, gz, 0b00110000);

            tmp_x = _mm256_shuffle_ps(gx, gy, 0b01001001);
            tmp_y = _mm256_shuffle_ps(gy, gz, 0b10011110);

            gy = _mm256_shuffle_ps(tmp_x, tmp_y, 0b11011000);
            gx = _mm256_shuffle_ps(gx, tmp_y, 0b10001100);
            gz = _mm256_shuffle_ps(tmp_x, gz, 0b11001101);

            // compute coord
            {
                // x
                gx = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gx, *(__m256*)_ps256_1), *(__m256*)_ps256_2), _mm256_sub_ps(vImgWf, *(__m256*)_ps256_1));
                const __m256 border_x = _mm256_sub_ps(vImgWf, *(__m256*)_ps256_1);

                gx = _mm256_and_ps(gx, *(__m256*)_ps256_inv_sign_mask);

                __m256 reflectx_v = _mm256_and_ps(_mm256_sub_ps(gx, border_x), *(__m256*)_ps256_inv_sign_mask);
                gx = _mm256_sub_ps(border_x, reflectx_v);

                // y
                gy = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gy, *(__m256*)_ps256_1), *(__m256*)_ps256_2), _mm256_sub_ps(vImgHf, *(__m256*)_ps256_1));
                const __m256 border_y = _mm256_sub_ps(vImgHf, *(__m256*)_ps256_1);

                gy = _mm256_and_ps(gy, *(__m256*)_ps256_inv_sign_mask);

                __m256 reflecty_v = _mm256_and_ps(_mm256_sub_ps(gy, border_y), *(__m256*)_ps256_inv_sign_mask);
                gy = _mm256_sub_ps(border_y, reflecty_v);

                // z
                gz = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gz, *(__m256*)_ps256_1), *(__m256*)_ps256_2), _mm256_sub_ps(vImgDf, *(__m256*)_ps256_1));
                const __m256 border_z = _mm256_sub_ps(vImgDf, *(__m256*)_ps256_1);

                gz = _mm256_and_ps(gz, *(__m256*)_ps256_inv_sign_mask);

                __m256 reflectz_v = _mm256_and_ps(_mm256_sub_ps(gz, border_z), *(__m256*)_ps256_inv_sign_mask);
                gz = _mm256_sub_ps(border_z, reflectz_v);
            }

            __m256 x_w = _mm256_floor_ps(gx);
            __m256 y_n = _mm256_floor_ps(gy);
            __m256 z_t = _mm256_floor_ps(gz);

            __m256 w = _mm256_sub_ps(gx, x_w);
            __m256 e = _mm256_sub_ps(*(__m256*)_ps256_1, w);
            __m256 n = _mm256_sub_ps(gy, y_n);
            __m256 s = _mm256_sub_ps(*(__m256*)_ps256_1, n);
            __m256 t = _mm256_sub_ps(gz, z_t);
            __m256 b = _mm256_sub_ps(*(__m256*)_ps256_1, t);

            __m256 tnw, tne, tsw, tse, bnw, bne, bsw, bse;
            {
                __m256 nw = _mm256_mul_ps(s, e);
                __m256 ne = _mm256_mul_ps(s, w);
                __m256 sw = _mm256_mul_ps(n, e);
                __m256 se = _mm256_mul_ps(n, w);

                tnw = _mm256_mul_ps(b, nw);
                tne = _mm256_mul_ps(b, ne);
                tsw = _mm256_mul_ps(b, sw);
                tse = _mm256_mul_ps(b, se);

                bnw = _mm256_mul_ps(t, nw);
                bne = _mm256_mul_ps(t, ne);
                bsw = _mm256_mul_ps(t, sw);
                bse = _mm256_mul_ps(t, se);
            }

            __m256 x1 = _mm256_add_ps(x_w, *(__m256*)_ps256_1);
            __m256 y1 = _mm256_add_ps(y_n, *(__m256*)_ps256_1);
            __m256 z1 = _mm256_add_ps(z_t, *(__m256*)_ps256_1);

            __m256 x1_in_range = _mm256_and_ps(_mm256_cmp_ps(x1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgWf, x1, _CMP_GT_OS));
            __m256 y1_in_range = _mm256_and_ps(_mm256_cmp_ps(y1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgHf, y1, _CMP_GT_OS));
            __m256 z1_in_range = _mm256_and_ps(_mm256_cmp_ps(z1, *(__m256*)_ps256_n1, _CMP_GT_OS), _mm256_cmp_ps(vImgDf, z1, _CMP_GT_OS));

            __m256 v110_in_range, v011_in_range, v101_in_range, v111_in_range;
            {
                __m256 v11_in_range = _mm256_and_ps(x1_in_range, y1_in_range);

                v110_in_range = _mm256_and_ps(x1_in_range, y1_in_range);

                v011_in_range = _mm256_and_ps(y1_in_range, z1_in_range);
                v101_in_range = _mm256_and_ps(x1_in_range, z1_in_range);
                v111_in_range = _mm256_and_ps(v11_in_range, z1_in_range);
            }

            __m256 tnw_offset = _mm256_add_ps(_mm256_mul_ps(_mm256_mul_ps(vImgWf, vImgHf), z_t),
                                              _mm256_add_ps(_mm256_mul_ps(y_n, vImgWf), x_w));
            __m256 tne_offset = _mm256_add_ps(tnw_offset, *(__m256*)_ps256_1);
            __m256 tsw_offset = _mm256_add_ps(tnw_offset, vImgWf);
            __m256 tse_offset = _mm256_add_ps(tsw_offset, *(__m256*)_ps256_1);

            __m256 bnw_offset = _mm256_add_ps(_mm256_mul_ps(vImgWf, vImgHf), tnw_offset);
            __m256 bne_offset = _mm256_add_ps(bnw_offset, *(__m256*)_ps256_1);
            __m256 bsw_offset = _mm256_add_ps(bnw_offset, vImgWf);
            __m256 bse_offset = _mm256_add_ps(bsw_offset, *(__m256*)_ps256_1);

            __m256i i_tnw_offset = _mm256_cvtps_epi32(tnw_offset);
            __m256i i_tne_offset = _mm256_cvtps_epi32(tne_offset);
            __m256i i_tsw_offset = _mm256_cvtps_epi32(tsw_offset);
            __m256i i_tse_offset = _mm256_cvtps_epi32(tse_offset);

            __m256i i_bnw_offset = _mm256_cvtps_epi32(bnw_offset);
            __m256i i_bne_offset = _mm256_cvtps_epi32(bne_offset);
            __m256i i_bsw_offset = _mm256_cvtps_epi32(bsw_offset);
            __m256i i_bse_offset = _mm256_cvtps_epi32(bse_offset);

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
                __m256 tnw_val = mask_gather_ps256(image, i_tnw_offset, *(__m256*)_ps256_n1);
                __m256 tne_val = mask_gather_ps256(image, i_tne_offset, x1_in_range);
                __m256 tsw_val = mask_gather_ps256(image, i_tsw_offset, y1_in_range);
                __m256 tse_val = mask_gather_ps256(image, i_tse_offset, v110_in_range);

                __m256 bnw_val = mask_gather_ps256(image, i_bnw_offset, z1_in_range);
                __m256 bne_val = mask_gather_ps256(image, i_bne_offset, v101_in_range);
                __m256 bsw_val = mask_gather_ps256(image, i_bsw_offset, v011_in_range);
                __m256 bse_val = mask_gather_ps256(image, i_bse_offset, v111_in_range);

                __m256 _v = _mm256_mul_ps(tnw_val, tnw);
                _v = _mm256_comp_fmadd_ps(tne_val, tne, _v);
                _v = _mm256_comp_fmadd_ps(tsw_val, tsw, _v);
                _v = _mm256_comp_fmadd_ps(tse_val, tse, _v);

                _v = _mm256_comp_fmadd_ps(bnw_val, bnw, _v);
                _v = _mm256_comp_fmadd_ps(bne_val, bne, _v);
                _v = _mm256_comp_fmadd_ps(bsw_val, bsw, _v);
                _v = _mm256_comp_fmadd_ps(bse_val, bse, _v);

                _mm256_storeu_ps(static_cast<float*>(dst.channel(q).depth(y).data) + x / 3, _v);
            }
        }
        nn = grid_size % 24;
#endif // __AVX__
        for (int x = grid_size - nn; x < grid_size; x += 3)
        {
            float gx = gridptr[x];
            float gy = gridptr[x + 1];
            float gz = gridptr[x + 2];

            gx = (gx + 1) / 2.f * (src.w - 1);
            gy = (gy + 1) / 2.f * (src.h - 1);
            gz = (gz + 1) / 2.f * (src.d - 1);

            gx = abs(gx);
            gx = (src.w - 1) - abs(gx - (src.w - 1));

            gy = abs(gy);
            gy = (src.h - 1) - abs(gy - (src.h - 1));

            gz = abs(gz);
            gz = (src.d - 1) - abs(gz - (src.d - 1));

            gx = std::min(src.w - 1.0f, std::max(gx, 0.0f));
            gy = std::min(src.h - 1.0f, std::max(gy, 0.0f));
            gz = std::min(src.d - 1.0f, std::max(gz, 0.0f));

            // bilinear interpolate
            int x0 = (int)floor(gx);
            int y0 = (int)floor(gy);
            int z0 = (int)floor(gz);
            int x1 = x0 + 1;
            int y1 = y0 + 1;
            int z1 = z0 + 1;

            bool x1_in_range = (x1 > -1) & (x1 < src.w);
            bool y1_in_range = (y1 > -1) & (y1 < src.h);
            bool z1_in_range = (z1 > -1) & (z1 < src.d);

            bool v11_in_range = x1_in_range & y1_in_range;

            bool v110_in_range = y1_in_range & z1_in_range;

            bool v101_in_range = x1_in_range & z1_in_range;
            bool v111_in_range = v11_in_range & z1_in_range;

            float alpha = gx - x0;
            float beta = gy - y0;
            float gamma = gz - z0;

            for (int q = 0; q < src.c; q++)
            {
                const Mat& image = src.channel(q);
                float v000 = image.depth(z0).row(y0)[x0];
                float v010 = y1_in_range ? image.depth(z0).row(y1)[x0] : 0;
                float v100 = z1_in_range ? image.depth(z1).row(y0)[x0] : 0;
                float v110 = v110_in_range ? image.depth(z1).row(y1)[x0] : 0;

                float v001 = x1_in_range ? image.depth(z0).row(y0)[x1] : 0;
                float v011 = v11_in_range ? image.depth(z0).row(y1)[x1] : 0;
                float v101 = v101_in_range ? image.depth(z1).row(y0)[x1] : 0;
                float v111 = v111_in_range ? image.depth(z1).row(y1)[x1] : 0;

                float v00 = v000 * (1 - alpha) + v001 * alpha;
                float v01 = v010 * (1 - alpha) + v011 * alpha;
                float v10 = v100 * (1 - alpha) + v101 * alpha;
                float v11 = v110 * (1 - alpha) + v111 * alpha;

                float v0 = v00 * (1 - beta) + v01 * beta;
                float v1 = v10 * (1 - beta) + v11 * beta;

                dst.channel(q).depth(y)[x / 3] = v0 * (1 - gamma) + v1 * gamma;
            }
        }
    }
}