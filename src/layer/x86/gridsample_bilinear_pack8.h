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

static void gridsample_bilinear_align0_zeros_blob_pack8(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    const auto vImgWf = _mm256_set1_ps(src.w);
    const auto vImgHf = _mm256_set1_ps(src.h);
    const auto vImgWi = _mm256_set1_epi32(src.w);
    const auto vImgHi = _mm256_set1_epi32(src.h);

    const auto vElempacki = _mm256_set1_epi32(src.elempack);

#pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < dst.h; y++)
    {
        for (int x = 0; x < dst.w; x++)
        {
            //grid tensor has been packed
            const float* gridptr = grid.channel(y / grid.elempack).row(x) + y % grid.elempack;
            auto gx = _mm256_set1_ps(gridptr[0]);
            auto gy = _mm256_set1_ps(gridptr[grid.elempack]);

            // compute coord
            {
                const auto two = _mm256_set1_ps(2.f);

                // x
                gx = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gx, v1fp8), vImgWf, v1fp8), two);

                // y
                gy = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gy, v1fp8), vImgHf, v1fp8), two);
            }

            auto x_w = _mm256_floor_ps(gx);
            auto y_n = _mm256_floor_ps(gy);

            auto w = _mm256_sub_ps(gx, x_w);
            auto e = _mm256_sub_ps(v1fp8, w);
            auto n = _mm256_sub_ps(gy, y_n);
            auto s = _mm256_sub_ps(v1fp8, n);

            auto nw = _mm256_mul_ps(s, e);
            auto ne = _mm256_mul_ps(s, w);
            auto sw = _mm256_mul_ps(n, e);
            auto se = _mm256_mul_ps(n, w);

            auto x0 = _mm256_cvtps_epi32(x_w);
            auto x1 = _mm256_add_epi32(x0, v1ip8);
            auto y0 = _mm256_cvtps_epi32(y_n);
            auto y1 = _mm256_add_epi32(y0, v1ip8);

            auto x0_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x0, vn1ip8), _mm256_cmpgt_epi32(vImgWi, x0));
            auto x1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x1, vn1ip8), _mm256_cmpgt_epi32(vImgWi, x1));
            auto y0_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y0, vn1ip8), _mm256_cmpgt_epi32(vImgHi, y0));
            auto y1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y1, vn1ip8), _mm256_cmpgt_epi32(vImgHi, y1));

            auto v00_in_range = _mm256_and_si256(x0_in_range, y0_in_range);
            auto v01_in_range = _mm256_and_si256(x0_in_range, y1_in_range);
            auto v10_in_range = _mm256_and_si256(x1_in_range, y0_in_range);
            auto v11_in_range = _mm256_and_si256(x1_in_range, y1_in_range);

            // (W*y + x) * elempack + vec(8)
            auto i_nw_offset = _mm256_add_epi32(_mm256_mullo_epi32(_mm256_add_epi32(_mm256_mullo_epi32(y0, vImgWi), x0), vElempacki),
                _mm256_set_epi32(7, 6, 5, 4, 3, 2, 1, 0));
            auto i_ne_offset = _mm256_add_epi32(i_nw_offset, vElempacki);
            auto i_sw_offset = _mm256_add_epi32(i_nw_offset, _mm256_mullo_epi32(vImgWi, vElempacki));
            auto i_se_offset = _mm256_add_epi32(i_sw_offset, vElempacki);

            for (int q = 0; q < dst.c; q++)
            {
                auto nw_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_nw_offset, *reinterpret_cast<__m256*>(&v00_in_range), sizeof(float));
                auto ne_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_ne_offset, *reinterpret_cast<__m256*>(&v10_in_range), sizeof(float));
                auto sw_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_sw_offset, *reinterpret_cast<__m256*>(&v01_in_range), sizeof(float));
                auto se_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_se_offset, *reinterpret_cast<__m256*>(&v11_in_range), sizeof(float));

                auto _v = _mm256_mul_ps(nw_val, nw);
                _v = _mm256_comp_fmadd_ps(ne_val, ne, _v);
                _v = _mm256_comp_fmadd_ps(sw_val, sw, _v);
                _v = _mm256_comp_fmadd_ps(se_val, se, _v);

                _mm256_storeu_ps(dst.channel(q).row(y) + x * dst.elempack, _v);
            }
        }
    }
}

static void gridsample_bilinear_align1_zeros_blob_pack8(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    const auto vImgWf = _mm256_set1_ps(src.w);
    const auto vImgHf = _mm256_set1_ps(src.h);
    const auto vImgWi = _mm256_set1_epi32(src.w);
    const auto vImgHi = _mm256_set1_epi32(src.h);

    const auto vElempacki = _mm256_set1_epi32(src.elempack);

#pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < dst.h; y++)
    {
        for (int x = 0; x < dst.w; x++)
        {
            //grid tensor has been packed
            const float* gridptr = grid.channel(y / grid.elempack).row(x) + y % grid.elempack;
            auto gx = _mm256_set1_ps(gridptr[0]);
            auto gy = _mm256_set1_ps(gridptr[grid.elempack]);

            // compute coord
            {
                const auto two = _mm256_set1_ps(2.f);

                // x
                gx = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gx, v1fp8), two), _mm256_sub_ps(vImgWf, v1fp8));

                // y
                gy = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gy, v1fp8), two), _mm256_sub_ps(vImgHf, v1fp8));
            }

            auto x_w = _mm256_floor_ps(gx);
            auto y_n = _mm256_floor_ps(gy);

            auto w = _mm256_sub_ps(gx, x_w);
            auto e = _mm256_sub_ps(v1fp8, w);
            auto n = _mm256_sub_ps(gy, y_n);
            auto s = _mm256_sub_ps(v1fp8, n);

            auto nw = _mm256_mul_ps(s, e);
            auto ne = _mm256_mul_ps(s, w);
            auto sw = _mm256_mul_ps(n, e);
            auto se = _mm256_mul_ps(n, w);

            auto x0 = _mm256_cvtps_epi32(x_w);
            auto x1 = _mm256_add_epi32(x0, v1ip8);
            auto y0 = _mm256_cvtps_epi32(y_n);
            auto y1 = _mm256_add_epi32(y0, v1ip8);

            auto x0_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x0, vn1ip8), _mm256_cmpgt_epi32(vImgWi, x0));
            auto x1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x1, vn1ip8), _mm256_cmpgt_epi32(vImgWi, x1));
            auto y0_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y0, vn1ip8), _mm256_cmpgt_epi32(vImgHi, y0));
            auto y1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y1, vn1ip8), _mm256_cmpgt_epi32(vImgHi, y1));

            auto v00_in_range = _mm256_and_si256(x0_in_range, y0_in_range);
            auto v01_in_range = _mm256_and_si256(x0_in_range, y1_in_range);
            auto v10_in_range = _mm256_and_si256(x1_in_range, y0_in_range);
            auto v11_in_range = _mm256_and_si256(x1_in_range, y1_in_range);

            // (W*y + x) * elempack + vec(8)
            auto i_nw_offset = _mm256_add_epi32(_mm256_mullo_epi32(_mm256_add_epi32(_mm256_mullo_epi32(y0, vImgWi), x0), vElempacki),
                _mm256_set_epi32(7, 6, 5, 4, 3, 2, 1, 0));
            auto i_ne_offset = _mm256_add_epi32(i_nw_offset, vElempacki);
            auto i_sw_offset = _mm256_add_epi32(i_nw_offset, _mm256_mullo_epi32(vImgWi, vElempacki));
            auto i_se_offset = _mm256_add_epi32(i_sw_offset, vElempacki);


            for (int q = 0; q < dst.c; q++)
            {
                auto nw_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_nw_offset, *reinterpret_cast<__m256*>(&v00_in_range), sizeof(float));
                auto ne_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_ne_offset, *reinterpret_cast<__m256*>(&v10_in_range), sizeof(float));
                auto sw_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_sw_offset, *reinterpret_cast<__m256*>(&v01_in_range), sizeof(float));
                auto se_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_se_offset, *reinterpret_cast<__m256*>(&v11_in_range), sizeof(float));

                auto _v = _mm256_mul_ps(nw_val, nw);
                _v = _mm256_comp_fmadd_ps(ne_val, ne, _v);
                _v = _mm256_comp_fmadd_ps(sw_val, sw, _v);
                _v = _mm256_comp_fmadd_ps(se_val, se, _v);

                _mm256_storeu_ps(dst.channel(q).row(y) + x * dst.elempack, _v);
            }
        }
    }
}

static void gridsample_bilinear_align0_border_blob_pack8(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    const auto vImgWf = _mm256_set1_ps(src.w);
    const auto vImgHf = _mm256_set1_ps(src.h);
    const auto vImgWi = _mm256_set1_epi32(src.w);
    const auto vImgHi = _mm256_set1_epi32(src.h);

    const auto vElempacki = _mm256_set1_epi32(src.elempack);

#pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < dst.h; y++)
    {
        for (int x = 0; x < dst.w; x++)
        {
            //grid tensor has been packed
            const float* gridptr = grid.channel(y / grid.elempack).row(x) + y % grid.elempack;
            auto gx = _mm256_set1_ps(gridptr[0]);
            auto gy = _mm256_set1_ps(gridptr[grid.elempack]);

            // compute coord
            {
                const auto two = _mm256_set1_ps(2.f);

                // x
                gx = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gx, v1fp8), vImgWf, v1fp8), two);

                const auto border_x = _mm256_sub_ps(vImgWf, v1fp8);

                gx = _mm256_min_ps(border_x, _mm256_max_ps(gx, _mm256_setzero_ps()));


                // y
                gy = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gy, v1fp8), vImgHf, v1fp8), two);

                const auto border_y = _mm256_sub_ps(vImgHf, v1fp8);

                gy = _mm256_min_ps(border_y, _mm256_max_ps(gy, _mm256_setzero_ps()));
            }

            auto x_w = _mm256_floor_ps(gx);
            auto y_n = _mm256_floor_ps(gy);

            auto w = _mm256_sub_ps(gx, x_w);
            auto e = _mm256_sub_ps(v1fp8, w);
            auto n = _mm256_sub_ps(gy, y_n);
            auto s = _mm256_sub_ps(v1fp8, n);

            auto nw = _mm256_mul_ps(s, e);
            auto ne = _mm256_mul_ps(s, w);
            auto sw = _mm256_mul_ps(n, e);
            auto se = _mm256_mul_ps(n, w);

            auto x0 = _mm256_cvtps_epi32(x_w);
            auto x1 = _mm256_add_epi32(x0, v1ip8);
            auto y0 = _mm256_cvtps_epi32(y_n);
            auto y1 = _mm256_add_epi32(y0, v1ip8);

            auto x1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x1, vn1ip8), _mm256_cmpgt_epi32(vImgWi, x1));
            auto y1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y1, vn1ip8), _mm256_cmpgt_epi32(vImgHi, y1));

            auto v11_in_range = _mm256_and_si256(x1_in_range, y1_in_range);

            // (W*y + x) * elempack + vec(8)
            auto i_nw_offset = _mm256_add_epi32(_mm256_mullo_epi32(_mm256_add_epi32(_mm256_mullo_epi32(y0, vImgWi), x0), vElempacki),
                _mm256_set_epi32(7, 6, 5, 4, 3, 2, 1, 0));
            auto i_ne_offset = _mm256_add_epi32(i_nw_offset, vElempacki);
            auto i_sw_offset = _mm256_add_epi32(i_nw_offset, _mm256_mullo_epi32(vImgWi, vElempacki));
            auto i_se_offset = _mm256_add_epi32(i_sw_offset, vElempacki);


            for (int q = 0; q < dst.c; q++)
            {
                auto nw_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_nw_offset, vn1fp8, sizeof(float));
                auto ne_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_ne_offset, *reinterpret_cast<__m256*>(&x1_in_range), sizeof(float));
                auto sw_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_sw_offset, *reinterpret_cast<__m256*>(&y1_in_range), sizeof(float));
                auto se_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_se_offset, *reinterpret_cast<__m256*>(&v11_in_range), sizeof(float));

                auto _v = _mm256_mul_ps(nw_val, nw);
                _v = _mm256_comp_fmadd_ps(ne_val, ne, _v);
                _v = _mm256_comp_fmadd_ps(sw_val, sw, _v);
                _v = _mm256_comp_fmadd_ps(se_val, se, _v);

                _mm256_storeu_ps(dst.channel(q).row(y) + x * dst.elempack, _v);
            }
        }
    }
}

static void gridsample_bilinear_align1_border_blob_pack8(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    const auto vImgWf = _mm256_set1_ps(src.w);
    const auto vImgHf = _mm256_set1_ps(src.h);
    const auto vImgWi = _mm256_set1_epi32(src.w);
    const auto vImgHi = _mm256_set1_epi32(src.h);

    const auto vElempacki = _mm256_set1_epi32(src.elempack);

#pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < dst.h; y++)
    {
        for (int x = 0; x < dst.w; x++)
        {
            //grid tensor has been packed
            const float* gridptr = grid.channel(y / grid.elempack).row(x) + y % grid.elempack;
            auto gx = _mm256_set1_ps(gridptr[0]);
            auto gy = _mm256_set1_ps(gridptr[grid.elempack]);

            // compute coord
            {
                const auto two = _mm256_set1_ps(2.f);

                // x
                gx = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gx, v1fp8), two), _mm256_sub_ps(vImgWf, v1fp8));

                const auto border_x = _mm256_sub_ps(vImgWf, v1fp8);

                gx = _mm256_min_ps(border_x, _mm256_max_ps(gx, _mm256_setzero_ps()));


                // y
                gy = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gy, v1fp8), two), _mm256_sub_ps(vImgHf, v1fp8));

                const auto border_y = _mm256_sub_ps(vImgHf, v1fp8);

                gy = _mm256_min_ps(border_y, _mm256_max_ps(gy, _mm256_setzero_ps()));
            }

            auto x_w = _mm256_floor_ps(gx);
            auto y_n = _mm256_floor_ps(gy);

            auto w = _mm256_sub_ps(gx, x_w);
            auto e = _mm256_sub_ps(v1fp8, w);
            auto n = _mm256_sub_ps(gy, y_n);
            auto s = _mm256_sub_ps(v1fp8, n);

            auto nw = _mm256_mul_ps(s, e);
            auto ne = _mm256_mul_ps(s, w);
            auto sw = _mm256_mul_ps(n, e);
            auto se = _mm256_mul_ps(n, w);

            auto x0 = _mm256_cvtps_epi32(x_w);
            auto x1 = _mm256_add_epi32(x0, v1ip8);
            auto y0 = _mm256_cvtps_epi32(y_n);
            auto y1 = _mm256_add_epi32(y0, v1ip8);

            auto x1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x1, vn1ip8), _mm256_cmpgt_epi32(vImgWi, x1));
            auto y1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y1, vn1ip8), _mm256_cmpgt_epi32(vImgHi, y1));

            auto v11_in_range = _mm256_and_si256(x1_in_range, y1_in_range);

            // (W*y + x) * elempack + vec(8)
            auto i_nw_offset = _mm256_add_epi32(_mm256_mullo_epi32(_mm256_add_epi32(_mm256_mullo_epi32(y0, vImgWi), x0), vElempacki),
                _mm256_set_epi32(7, 6, 5, 4, 3, 2, 1, 0));
            auto i_ne_offset = _mm256_add_epi32(i_nw_offset, vElempacki);
            auto i_sw_offset = _mm256_add_epi32(i_nw_offset, _mm256_mullo_epi32(vImgWi, vElempacki));
            auto i_se_offset = _mm256_add_epi32(i_sw_offset, vElempacki);

            for (int q = 0; q < dst.c; q++)
            {
                auto nw_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_nw_offset, vn1fp8, sizeof(float));
                auto ne_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_ne_offset, *reinterpret_cast<__m256*>(&x1_in_range), sizeof(float));
                auto sw_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_sw_offset, *reinterpret_cast<__m256*>(&y1_in_range), sizeof(float));
                auto se_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_se_offset, *reinterpret_cast<__m256*>(&v11_in_range), sizeof(float));

                auto _v = _mm256_mul_ps(nw_val, nw);
                _v = _mm256_comp_fmadd_ps(ne_val, ne, _v);
                _v = _mm256_comp_fmadd_ps(sw_val, sw, _v);
                _v = _mm256_comp_fmadd_ps(se_val, se, _v);

                _mm256_storeu_ps(dst.channel(q).row(y) + x * dst.elempack, _v);
            }
        }
    }
}

static void gridsample_bilinear_align0_reflection_blob_pack8(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    const auto vImgWf = _mm256_set1_ps(src.w);
    const auto vImgHf = _mm256_set1_ps(src.h);
    const auto vImgWi = _mm256_set1_epi32(src.w);
    const auto vImgHi = _mm256_set1_epi32(src.h);

    const auto vElempacki = _mm256_set1_epi32(src.elempack);

#pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < dst.h; y++)
    {
        for (int x = 0; x < dst.w; x++)
        {
            //grid tensor has been packed
            const float* gridptr = grid.channel(y / grid.elempack).row(x) + y % grid.elempack;
            auto gx = _mm256_set1_ps(gridptr[0]);
            auto gy = _mm256_set1_ps(gridptr[grid.elempack]);

            // compute coord
            {
                const auto two = _mm256_set1_ps(2.f);

                // x
                gx = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gx, v1fp8), vImgWf, v1fp8), two);

                const auto border_x = _mm256_sub_ps(vImgWf, v1fp8);

                auto v0p5fp8 = _mm256_set1_ps(0.5f);
                gx = _mm256_add_ps(gx, v0p5fp8);

                gx = _mm256_and_ps(gx, *(__m256*)_ps256_inv_sign_mask);

                auto reflectx_v = _mm256_and_ps(_mm256_sub_ps(gx, vImgWf), *(__m256*)_ps256_inv_sign_mask);
                gx = _mm256_sub_ps(vImgWf, reflectx_v);

                gx = _mm256_sub_ps(gx, v0p5fp8);

                _mm256_sub_ps(gx, v0p5fp8);

                gx = _mm256_min_ps(border_x, _mm256_max_ps(gx, _mm256_setzero_ps()));


                // y
                gy = _mm256_div_ps(_mm256_comp_fmsub_ps(_mm256_add_ps(gy, v1fp8), vImgHf, v1fp8), two);

                const auto border_y = _mm256_sub_ps(vImgHf, v1fp8);

                gy = _mm256_add_ps(gy, v0p5fp8);

                gy = _mm256_and_ps(gy, *(__m256*)_ps256_inv_sign_mask);

                auto reflecty_v = _mm256_and_ps(_mm256_sub_ps(gy, vImgHf), *(__m256*)_ps256_inv_sign_mask);
                gy = _mm256_sub_ps(vImgHf, reflecty_v);

                gy = _mm256_sub_ps(gy, v0p5fp8);

                _mm256_sub_ps(gy, v0p5fp8);

                gy = _mm256_min_ps(border_y, _mm256_max_ps(gy, _mm256_setzero_ps()));
            }

            auto x_w = _mm256_floor_ps(gx);
            auto y_n = _mm256_floor_ps(gy);

            auto w = _mm256_sub_ps(gx, x_w);
            auto e = _mm256_sub_ps(v1fp8, w);
            auto n = _mm256_sub_ps(gy, y_n);
            auto s = _mm256_sub_ps(v1fp8, n);

            auto nw = _mm256_mul_ps(s, e);
            auto ne = _mm256_mul_ps(s, w);
            auto sw = _mm256_mul_ps(n, e);
            auto se = _mm256_mul_ps(n, w);

            auto x0 = _mm256_cvtps_epi32(x_w);
            auto x1 = _mm256_add_epi32(x0, v1ip8);
            auto y0 = _mm256_cvtps_epi32(y_n);
            auto y1 = _mm256_add_epi32(y0, v1ip8);

            auto x1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x1, vn1ip8), _mm256_cmpgt_epi32(vImgWi, x1));
            auto y1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y1, vn1ip8), _mm256_cmpgt_epi32(vImgHi, y1));

            auto v11_in_range = _mm256_and_si256(x1_in_range, y1_in_range);

            // (W*y + x) * elempack + vec(8)
            auto i_nw_offset = _mm256_add_epi32(_mm256_mullo_epi32(_mm256_add_epi32(_mm256_mullo_epi32(y0, vImgWi), x0), vElempacki),
                _mm256_set_epi32(7, 6, 5, 4, 3, 2, 1, 0));
            auto i_ne_offset = _mm256_add_epi32(i_nw_offset, vElempacki);
            auto i_sw_offset = _mm256_add_epi32(i_nw_offset, _mm256_mullo_epi32(vImgWi, vElempacki));
            auto i_se_offset = _mm256_add_epi32(i_sw_offset, vElempacki);

            for (int q = 0; q < dst.c; q++)
            {
                auto nw_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_nw_offset, vn1fp8, sizeof(float));
                auto ne_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_ne_offset, *reinterpret_cast<__m256*>(&x1_in_range), sizeof(float));
                auto sw_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_sw_offset, *reinterpret_cast<__m256*>(&y1_in_range), sizeof(float));
                auto se_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_se_offset, *reinterpret_cast<__m256*>(&v11_in_range), sizeof(float));

                auto _v = _mm256_mul_ps(nw_val, nw);
                _v = _mm256_comp_fmadd_ps(ne_val, ne, _v);
                _v = _mm256_comp_fmadd_ps(sw_val, sw, _v);
                _v = _mm256_comp_fmadd_ps(se_val, se, _v);

                _mm256_storeu_ps(dst.channel(q).row(y) + x * dst.elempack, _v);
            }
        }
    }
}

static void gridsample_bilinear_align1_reflection_blob_pack8(const Mat& src, Mat& dst, const Mat& grid, const Option& opt)
{
    float* outptr = static_cast<float*>(dst.data);

    const auto vImgWf = _mm256_set1_ps(src.w);
    const auto vImgHf = _mm256_set1_ps(src.h);
    const auto vImgWi = _mm256_set1_epi32(src.w);
    const auto vImgHi = _mm256_set1_epi32(src.h);

    const auto vElempacki = _mm256_set1_epi32(src.elempack);

#pragma omp parallel for num_threads(opt.num_threads)
    for (int y = 0; y < dst.h; y++)
    {
        for (int x = 0; x < dst.w; x++)
        {
            //grid tensor has been packed
            const float* gridptr = grid.channel(y / grid.elempack).row(x) + y % grid.elempack;
            auto gx = _mm256_set1_ps(gridptr[0]);
            auto gy = _mm256_set1_ps(gridptr[grid.elempack]);

            // compute coord
            {
                const auto two = _mm256_set1_ps(2.f);

                // x
                gx = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gx, v1fp8), two), _mm256_sub_ps(vImgWf, v1fp8));

                const auto border_x = _mm256_sub_ps(vImgWf, v1fp8);

                gx = _mm256_and_ps(gx, *(__m256*)_ps256_inv_sign_mask);

                auto reflectx_v = _mm256_and_ps(_mm256_sub_ps(gx, border_x), *(__m256*)_ps256_inv_sign_mask);
                gx = _mm256_sub_ps(border_x, reflectx_v);


                // y
                gy = _mm256_mul_ps(_mm256_div_ps(_mm256_add_ps(gy, v1fp8), two), _mm256_sub_ps(vImgHf, v1fp8));

                const auto border_y = _mm256_sub_ps(vImgHf, v1fp8);

                gy = _mm256_and_ps(gy, *(__m256*)_ps256_inv_sign_mask);

                auto reflecty_v = _mm256_and_ps(_mm256_sub_ps(gy, border_y), *(__m256*)_ps256_inv_sign_mask);
                gy = _mm256_sub_ps(border_y, reflecty_v);
            }

            auto x_w = _mm256_floor_ps(gx);
            auto y_n = _mm256_floor_ps(gy);

            auto w = _mm256_sub_ps(gx, x_w);
            auto e = _mm256_sub_ps(v1fp8, w);
            auto n = _mm256_sub_ps(gy, y_n);
            auto s = _mm256_sub_ps(v1fp8, n);

            auto nw = _mm256_mul_ps(s, e);
            auto ne = _mm256_mul_ps(s, w);
            auto sw = _mm256_mul_ps(n, e);
            auto se = _mm256_mul_ps(n, w);

            auto x0 = _mm256_cvtps_epi32(x_w);
            auto x1 = _mm256_add_epi32(x0, v1ip8);
            auto y0 = _mm256_cvtps_epi32(y_n);
            auto y1 = _mm256_add_epi32(y0, v1ip8);

            auto x1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(x1, vn1ip8), _mm256_cmpgt_epi32(vImgWi, x1));
            auto y1_in_range = _mm256_and_si256(_mm256_cmpgt_epi32(y1, vn1ip8), _mm256_cmpgt_epi32(vImgHi, y1));

            auto v11_in_range = _mm256_and_si256(x1_in_range, y1_in_range);

            // (W*y + x) * elempack + vec(8)
            auto i_nw_offset = _mm256_add_epi32(_mm256_mullo_epi32(_mm256_add_epi32(_mm256_mullo_epi32(y0, vImgWi), x0), vElempacki),
                _mm256_set_epi32(7, 6, 5, 4, 3, 2, 1, 0));
            auto i_ne_offset = _mm256_add_epi32(i_nw_offset, vElempacki);
            auto i_sw_offset = _mm256_add_epi32(i_nw_offset, _mm256_mullo_epi32(vImgWi, vElempacki));
            auto i_se_offset = _mm256_add_epi32(i_sw_offset, vElempacki);

            for (int q = 0; q < dst.c; q++)
            {
                auto nw_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_nw_offset, vn1fp8, sizeof(float));
                auto ne_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_ne_offset, *reinterpret_cast<__m256*>(&x1_in_range), sizeof(float));
                auto sw_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_sw_offset, *reinterpret_cast<__m256*>(&y1_in_range), sizeof(float));
                auto se_val = _mm256_mask_i32gather_ps(_mm256_setzero_ps(), src.channel(q), i_se_offset, *reinterpret_cast<__m256*>(&v11_in_range), sizeof(float));

                auto _v = _mm256_mul_ps(nw_val, nw);
                _v = _mm256_comp_fmadd_ps(ne_val, ne, _v);
                _v = _mm256_comp_fmadd_ps(sw_val, sw, _v);
                _v = _mm256_comp_fmadd_ps(se_val, se, _v);

                _mm256_storeu_ps(dst.channel(q).row(y) + x * dst.elempack, _v);
            }
        }
    }
}