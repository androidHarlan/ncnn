// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "prelu_loongarch64.h"

#if __loongarch64_sx
#include <lsxintrin.h>
#endif // __loongarch64_sx

#include "loongarch64_usability.h"

namespace ncnn {

PReLU_loongarch64::PReLU_loongarch64()
{
#if __loongarch64_sx
    support_packing = true;
#endif // __loongarch64_sx
}

int PReLU_loongarch64::forward_inplace(Mat& bottom_top_blob, const Option& opt) const
{
    int dims = bottom_top_blob.dims;
    int elempack = bottom_top_blob.elempack;

    if (dims == 1)
    {
        int w = bottom_top_blob.w * elempack;

#if __loongarch64_sx
        int nn_w = w / 4;
        int remain_w_start = nn_w * 4;
#else
        int remain_w_start = 0;
#endif // __loongarch64_sx

        float* ptr = bottom_top_blob;

        if (num_slope > 1)
        {
            const float* slope = slope_data;

#if __loongarch64_sx
            #pragma omp parallel for num_threads(opt.num_threads)
            for (int i = 0; i < nn_w; i++)
            {
                float* ptr0 = ptr + i * 4;

                v4f32 _p = (v4f32)__lsx_vld(ptr0, 0);
                v4f32 _zero = (v4f32)__lsx_vreplgr2vr_w(0);
                v4f32 _slope = (v4f32)__lsx_vld(slope + i * 4, 0);
                v4i32_w _lemask = __lsx_vfcmp_cle_s(_p, _zero);
                v4f32 _ps = __lsx_vfmul_s(_p, _slope);
                _p = (v4f32)__lsx_vbitsel_v((v16u8)_lemask, (v16u8)_p, (v16u8)_ps);
                __lsx_vst((v4i32)_p, ptr0, 0);
            }
#endif // __loongarch64_sx

            #pragma omp parallel for num_threads(opt.num_threads)
            for (int i = remain_w_start; i < w; i++)
            {
                float v = ptr[i];
                if (v < 0.f)
                    ptr[i] = v * slope[i];
            }
        }
        else
        {
            const float slope = slope_data[0];

#if __loongarch64_sx
            #pragma omp parallel for num_threads(opt.num_threads)
            for (int i = 0; i < nn_w; i++)
            {
                float* ptr0 = ptr + i * 4;

                v4f32 _p = (v4f32)__lsx_vld(ptr0, 0);
                v4f32 _zero = (v4f32)__lsx_vreplgr2vr_w(0);
                v4f32 _slope = (v4f32)__lsx_vreplgr2vr_w(slope);
                v4i32_w _lemask = __lsx_vfcmp_cle_s(_p, _zero);
                v4f32 _ps = __lsx_vfmul_s(_p, _slope);
                _p = (v4f32)__lsx_vbitsel_v((v16u8)_lemask, (v16u8)_p, (v16u8)_ps);
                __lsx_vst((v4i32)_p, ptr0, 0);
            }
#endif // __loongarch64_sx

            #pragma omp parallel for num_threads(opt.num_threads)
            for (int i = remain_w_start; i < w; i++)
            {
                float v = ptr[i];
                if (v < 0.f)
                    ptr[i] = v * slope;
            }
        }
    }

    if (dims == 2)
    {
        int w = bottom_top_blob.w * elempack;
        int h = bottom_top_blob.h;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int i = 0; i < h; i++)
        {
            float* ptr = bottom_top_blob.row(i);

            const float slope = num_slope > 1 ? slope_data[i] : slope_data[0];

            int j = 0;
#if __loongarch64_sx
            v4f32 _zero = (v4f32)__lsx_vreplgr2vr_w(0);
            v4f32 _slope = (elempack == 4 && num_slope > 1) ? (v4f32)__lsx_vld((const float*)slope_data + i * 4, 0) : (v4f32)__lsx_vreplgr2vr_w(slope);

            for (; j + 3 < w; j += 4)
            {
                __builtin_prefetch(ptr + 16);
                v4f32 _p = (v4f32)__lsx_vld(ptr, 0);
                v4i32_w _lemask = __lsx_vfcmp_cle_s(_p, _zero);
                v4f32 _ps = __lsx_vfmul_s(_p, _slope);
                _p = (v4f32)__lsx_vbitsel_v((v16u8)_lemask, (v16u8)_p, (v16u8)_ps);
                __lsx_vst((v4i32)_p, ptr, 0);

                ptr += 4;
            }
#endif // __loongarch64_sx
            for (; j < w; j++)
            {
                float v = *ptr;
                if (v < 0.f)
                    *ptr = v * slope;

                ptr++;
            }
        }
    }

    if (dims == 3)
    {
        int w = bottom_top_blob.w;
        int h = bottom_top_blob.h;
        int channels = bottom_top_blob.c;
        int size = w * h * elempack;

        const float* slope_data_ptr = slope_data;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < channels; q++)
        {
            float* ptr = bottom_top_blob.channel(q);
            float slope = num_slope > 1 ? slope_data_ptr[q] : slope_data_ptr[0];

            int i = 0;
#if __loongarch64_sx
            v4f32 _zero = (v4f32)__lsx_vreplgr2vr_w(0);
            v4f32 _slope = (elempack == 4 && num_slope > 1) ? (v4f32)__lsx_vld((const float*)slope_data + q * 4, 0) : (v4f32)__lsx_vreplgr2vr_w(slope);

            for (; i + 3 < size; i += 4)
            {
                __builtin_prefetch(ptr + 16);
                v4f32 _p = (v4f32)__lsx_vld(ptr, 0);
                v4i32_w _lemask = __lsx_vfcmp_cle_s(_p, _zero);
                v4f32 _ps = __lsx_vfmul_s(_p, _slope);
                _p = (v4f32)__lsx_vbitsel_v((v16u8)_lemask, (v16u8)_p, (v16u8)_ps);
                __lsx_vst((v4i32)_p, ptr, 0);

                ptr += 4;
            }
#endif // __loongarch64_sx
            for (; i < size; i++)
            {
                if (*ptr < 0)
                    *ptr *= slope;

                ptr++;
            }
        }
    }

    return 0;
}

} // namespace ncnn
