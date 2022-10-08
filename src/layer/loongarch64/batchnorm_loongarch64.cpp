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

#include "batchnorm_loongarch64.h"

#if __loongarch64_sx
#include <lsxintrin.h>
#endif // __loongarch64_sx

#include "loongarch64_usability.h"

namespace ncnn {

BatchNorm_loongarch64::BatchNorm_loongarch64()
{
#if __loongarch64_sx
    support_packing = true;
#endif // __loongarch64_sx
}

int BatchNorm_loongarch64::forward_inplace(Mat& bottom_top_blob, const Option& opt) const
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

#if __loongarch64_sx
        #pragma omp parallel for num_threads(opt.num_threads)
        for (int i = 0; i < nn_w; i++)
        {
            float* ptr0 = ptr + i * 4;

            v4f32 _p = (v4f32)__lsx_vld(ptr0, 0);
            v4f32 _a = (v4f32)__lsx_vld((const float*)a_data + i * 4, 0);
            v4f32 _b = (v4f32)__lsx_vld((const float*)b_data + i * 4, 0);
            _p = __lsx_vfmadd_s(_a, _p, _b);
            __lsx_vst((v4i32)_p, ptr0, 0);
        }
#endif // __loongarch64_sx

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int i = remain_w_start; i < w; i++)
        {
            ptr[i] = b_data[i] * ptr[i] + a_data[i];
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
            float a = a_data[i];
            float b = b_data[i];

            int j = 0;
#if __loongarch64_sx
            v4f32 _a = elempack == 4 ? (v4f32)__lsx_vld((const float*)a_data + i * 4, 0) : (v4f32)__lsx_vreplgr2vr(a);
            v4f32 _b = elempack == 4 ? (v4f32)__lsx_vld((const float*)b_data + i * 4, 0) : (v4f32)__lsx_vreplgr2vr(b);
            for (; j + 3 < w; j += 4)
            {
                __builtin_prefetch(ptr + 16);
                v4f32 _p = (v4f32)__lsx_vld(ptr, 0);
                _p = __lsx_vfmadd_s(_a, _p, _b);
                __lsx_vst((v4i32)_p, ptr, 0);

                ptr += 4;
            }
#endif // __loongarch64_sx
            for (; j < w; j++)
            {
                *ptr = b * *ptr + a;
                ptr++;
            }
        }
    }

    if (dims == 3 || dims == 4)
    {
        int w = bottom_top_blob.w;
        int h = bottom_top_blob.h;
        int d = bottom_top_blob.d;
        int c = bottom_top_blob.c;
        int size = w * h * d * elempack;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < c; q++)
        {
            float* ptr = bottom_top_blob.channel(q);
            float a = a_data[q];
            float b = b_data[q];

            int i = 0;
#if __loongarch64_sx
            v4f32 _a = elempack == 4 ? (v4f32)__lsx_vld((const float*)a_data + q * 4, 0) : (v4f32)__lsx_vreplgr2vr(a);
            v4f32 _b = elempack == 4 ? (v4f32)__lsx_vld((const float*)b_data + q * 4, 0) : (v4f32)__lsx_vreplgr2vr(b);
            for (; i + 3 < size; i += 4)
            {
                __builtin_prefetch(ptr + 16);
                v4f32 _p = (v4f32)__lsx_vld(ptr, 0);
                _p = __lsx_vfmadd_s(_a, _p, _b);
                __lsx_vst((v4i32)_p, ptr, 0);

                ptr += 4;
            }
#endif // __loongarch64_sx
            for (; i < size; i++)
            {
                *ptr = b * *ptr + a;
                ptr++;
            }
        }
    }

    return 0;
}

} // namespace ncnn
