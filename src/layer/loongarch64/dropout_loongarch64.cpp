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

#include "dropout_loongarch64.h"

#if __loongarch64_sx
#include <lsxintrin.h>
#endif // __loongarch64_sx

#include "loongarch64_usability.h"

namespace ncnn {

Dropout_loongarch64::Dropout_loongarch64()
{
#if __loongarch64_sx
    support_packing = true;
#endif // __loongarch64_sx
}

int Dropout_loongarch64::forward_inplace(Mat& bottom_top_blob, const Option& opt) const
{
    if (scale == 1.f)
    {
        return 0;
    }

    int w = bottom_top_blob.w;
    int h = bottom_top_blob.h;
    int d = bottom_top_blob.d;
    int channels = bottom_top_blob.c;
    int elempack = bottom_top_blob.elempack;
    int size = w * h * d * elempack;

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int q = 0; q < channels; q++)
    {
        float* ptr = bottom_top_blob.channel(q);

        int i = 0;
#if __loongarch64_sx
        v4f32 _scale = (v4f32)__lsx_vreplgr2vr_w(scale);
        for (; i + 3 < size; i += 4)
        {
            __builtin_prefetch(ptr + 16);
            v4f32 _p = (v4f32)__lsx_vld(ptr, 0);
            _p = __lsx_vfmul_s(_p, _scale);
            __lsx_vst((v4i32)_p, ptr, 0);

            ptr += 4;
        }
#endif // __loongarch64_sx
        for (; i < size; i++)
        {
            *ptr = *ptr * scale;

            ptr++;
        }
    }

    return 0;
}

} // namespace ncnn
