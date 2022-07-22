// Xavier Hsinyuan is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2022 Xavier Hsinyuan <me@lstlx.com>. All rights reserved.
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

#include "batchnorm_riscv.h"

#if __riscv_vector
#ifdef RVV_SPEC_0_7
#include "riscv_v_071_fix.h"
#else
#include <riscv_vector.h>
#endif
#endif // __riscv_vector

#include "riscv_usability.h"

namespace ncnn {

BatchNorm_riscv::BatchNorm_riscv()
{
#if __riscv_vector
    support_packing = true;
#endif
}

int BatchNorm_riscv::forward_inplace(Mat& bottom_top_blob, const Option& opt) const
{
#if __riscv_vector
    int dims = bottom_top_blob.dims;
    int elempack = bottom_top_blob.elempack;
    if (dims == 1)
    {
        int n = bottom_top_blob.w * elempack;
        float* ptr = bottom_top_blob;
        const float* ptr_a = a_data;
        const float* ptr_b = b_data;
        while (n > 0)
        {
            word_type vl = vsetvl_e32m8(n);

            vfloat32m8_t _p = vle32_v_f32m8(ptr, vl);
            vfloat32m8_t _a = vle32_v_f32m8(ptr_a, vl);
            vfloat32m8_t _b = vle32_v_f32m8(ptr_b, vl);

            _p = vfmadd_vv_f32m8(_p, _b, _a, vl);

            vse32_v_f32m8(ptr, _p, vl);

            ptr += vl;
            ptr_a += vl;
            ptr_b += vl;
            n -= vl;
        }
        return 0;
    }

    if (elempack == 1)
    {

        int w = bottom_top_blob.w;
        int h = bottom_top_blob.h;
        if (dims == 2)
        {
            #pragma omp parallel for num_threads(opt.num_threads)
            for (int i = 0; i < h; i++)
            {
                float* ptr = bottom_top_blob.row(i);
                float a = a_data[i];
                float b = b_data[i];

                int n = w;
                while (n > 0)
                {
                    word_type vl = vsetvl_e32m8(n);
                    vfloat32m8_t _p = vle32_v_f32m8(ptr, vl);
                    _p = vfmul_vf_f32m8(_p, b, vl);
                    _p = vfadd_vf_f32m8(_p, a, vl);
                    vse32_v_f32m8(ptr, _p, vl);

                    ptr += vl;
                    n -= vl;
                }
            }
        } 
        if (dims == 3 || dims == 4)
        {
            int d = bottom_top_blob.d;
            int c = bottom_top_blob.c;
            int size = w * h * d;

            #pragma omp parallel for num_threads(opt.num_threads)
            for (int q = 0; q < c; q++)
            {
                float* ptr = bottom_top_blob.channel(q);
                float a = a_data[q];
                float b = b_data[q];

                int n = size;
                while (n > 0)
                {
                    word_type vl = vsetvl_e32m8(n);
                    vfloat32m8_t _p = vle32_v_f32m8(ptr, vl);
                    _p = vfmul_vf_f32m8(_p, b, vl);
                    _p = vfadd_vf_f32m8(_p, a, vl);
                    vse32_v_f32m8(ptr, _p, vl);

                    ptr += vl;
                    n -= vl;
                }
            }
     
        }
        return 0;
    }

    const int packn = csrr_vlenb() / 4;
    if (elempack == packn)
    {
        int w = bottom_top_blob.w;
        int h = bottom_top_blob.h;

        const word_type vl = vsetvl_e32m1(packn);
        if (dims == 2)
        {
            #pragma omp parallel for num_threads(opt.num_threads)
            for (int i = 0; i < h; i++)
            {
                float* ptr = bottom_top_blob.row(i);
                const float* ptr_a = a_data;
                ptr_a += i * elempack;
                const float* ptr_b = b_data;
                ptr_b += i * elempack;
                int n = w * elempack;

                vfloat32m1_t _a = vle32_v_f32m1(ptr_a, vl);
                vfloat32m1_t _b = vle32_v_f32m1(ptr_b, vl);
                while (n > 0)
                {
                    vfloat32m1_t _p = vle32_v_f32m1(ptr, vl);
                    _p = vfmadd_vv_f32m1(_p, _b, _a, vl);
                    vse32_v_f32m1(ptr, _p, vl);

                    ptr += vl;
                    n -= vl;
                }
            }
        }

        if (dims == 3 || dims == 4)
        {
            int d = bottom_top_blob.d;
            int c = bottom_top_blob.c;
            int size = w * h * d * elempack;
            
            #pragma omp parallel for num_threads(opt.num_threads)
            for (int q = 0; q < c; q++)
            {
                float* ptr = bottom_top_blob.channel(q);
                const float* ptr_a = (const float*) a_data + q * elempack;
                const float* ptr_b = (const float*) b_data + q * elempack;

                vfloat32m1_t _a = vle32_v_f32m1(ptr_a, vl);
                vfloat32m1_t _b = vle32_v_f32m1(ptr_b, vl);

                int n = size;
                while (n > 0)
                {
                    vfloat32m1_t _p = vle32_v_f32m1(ptr, vl);
                    _p = vfmadd_vv_f32m1(_p, _b, _a, vl);
                    vse32_v_f32m1(ptr, _p, vl);

                    ptr += vl;
                    n -= vl;
                }
            }
            
        }

    }

 #else
    return BatchNorm::forward_inplace(bottom_top_blob, opt);
#endif
    return 0;
}
} // namespace ncnn
