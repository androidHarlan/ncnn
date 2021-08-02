// Xavier Hsinyuan is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2021 Xavier Hsinyuan <me@lstlx.com>. All rights reserved.
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

#include "gru_riscv.h"

#if __riscv_vector
#ifdef RVV_SPEC_0_7
#include "riscv_v_071_fix.h"
#else
#include <riscv_vector.h>
#endif
#endif // __riscv_vector

namespace ncnn {

//core rvv-optimized gru impl.
#if __riscv_vector
static int gru(const Mat& bottom_blob, Mat& top_blob, int reverse, const Mat& weight_xc, const Mat& bias_c, const Mat& weight_hc, Mat& hidden_state, const Option& opt)
{
    int size = bottom_blob.w;
    int T = bottom_blob.h;

    int num_output = top_blob.w;

    // 2 x num_output
    Mat gates(2, num_output, 4u, opt.workspace_allocator);
    if (gates.empty())
        return -100;

    // unroll
    for (int t = 0; t < T; t++)
    {
        int ti = reverse ? T - 1 - t : t;

        const float* x = bottom_blob.row(ti);
        for (int q = 0; q < num_output; q++)
        {
            float* gates_data = gates.row(q);

            // gate reset update
            const float* bias_c_R = bias_c.row(0);
            const float* bias_c_U = bias_c.row(1);

            const float* weight_xc_R = weight_xc.row(num_output * 0 + q);
            const float* weight_xc_U = weight_xc.row(num_output * 1 + q);
            const float* weight_hc_R = weight_hc.row(num_output * 0 + q);
            const float* weight_hc_U = weight_hc.row(num_output * 1 + q);

            float R = bias_c_R[q];
            float U = bias_c_U[q];

            int n = size;
            const float* ptr_x = x;
            const float* ptr_xcr = weight_xc_R;
            const float* ptr_xcu = weight_xc_U;
            while (n > 0)
            {
                word_type vl = vsetvl_e32m8(n);
                vfloat32m8_t _x = vle32_v_f32m8(ptr_x, vl);
                vfloat32m8_t _xcr = vle32_v_f32m8(ptr_xcr, vl);
                vfloat32m8_t _xcu = vle32_v_f32m8(ptr_xcu, vl);
                vfloat32m1_t _scalar_r = vfmv_s_f_f32m1(vundefined_f32m1(), R, vl);
                vfloat32m1_t _scalar_u = vfmv_s_f_f32m1(vundefined_f32m1(), U, vl);

                _xcr = vfmul_vv_f32m8(_xcr, _x, vl);
                _xcu = vfmul_vv_f32m8(_xcu, _x, vl);
                _scalar_r = vfredsum_vs_f32m8_f32m1(_scalar_r, _xcr, _scalar_r, vl);
                _scalar_u = vfredsum_vs_f32m8_f32m1(_scalar_u, _xcu, _scalar_u, vl);

                R = vfmv_f_s_f32m1_f32(_scalar_r);
                U = vfmv_f_s_f32m1_f32(_scalar_u);

                ptr_x += vl;
                ptr_xcr += vl;
                ptr_xcu += vl;
                n -= vl;
            }

            int n_out = num_output;
            const float* ptr_hc = hidden_state;
            const float* ptr_hcr = weight_hc_R;
            const float* ptr_hcu = weight_hc_U;
            while (n_out > 0)
            {
                word_type vl = vsetvl_e32m8(n_out);
                vfloat32m8_t _h_cont = vle32_v_f32m8(ptr_hc, vl);
                vfloat32m8_t _hcr = vle32_v_f32m8(ptr_hcr, vl);
                vfloat32m8_t _hcu = vle32_v_f32m8(ptr_hcu, vl);
                vfloat32m1_t _scalar_r = vfmv_s_f_f32m1(vundefined_f32m1(), R, vl);
                vfloat32m1_t _scalar_u = vfmv_s_f_f32m1(vundefined_f32m1(), U, vl);

                _hcr = vfmul_vv_f32m8(_hcr, _h_cont, vl);
                _hcu = vfmul_vv_f32m8(_hcu, _h_cont, vl);
                _scalar_r = vfredsum_vs_f32m8_f32m1(_scalar_r, _hcr, _scalar_r, vl);
                _scalar_u = vfredsum_vs_f32m8_f32m1(_scalar_u, _hcu, _scalar_u, vl);

                R = vfmv_f_s_f32m1_f32(_scalar_r);
                U = vfmv_f_s_f32m1_f32(_scalar_u);

                ptr_hc += vl;
                ptr_hcr += vl;
                ptr_hcu += vl;
                n_out -= vl;
            }

            // sigmoid(R)
            // sigmoid(U)
            R = 1.f / (1.f + exp(-R));
            U = 1.f / (1.f + exp(-U));

            // gate new
            const float* bias_c_WN = bias_c.row(2);
            const float* bias_c_BN = bias_c.row(3);

            const float* weight_xc_N = weight_xc.row(num_output * 2 + q);
            const float* weight_hc_N = weight_hc.row(num_output * 2 + q);

            float N = bias_c_BN[q];

            int n_out2 = num_output;
            const float* ptr_hc2 = hidden_state;
            const float* ptr_whc_n = weight_hc_N;
            while (n_out2 > 0)
            {
                word_type vl = vsetvl_e32m8(n_out2);

                vfloat32m8_t _h_cont = vle32_v_f32m8(ptr_hc2, vl);
                vfloat32m8_t _whc_n = vle32_v_f32m8(ptr_whc_n, vl);
                vfloat32m1_t _scalar_n = vfmv_s_f_f32m1(vundefined_f32m1(), N, vl);

                _h_cont = vfmul_vv_f32m8(_whc_n, _h_cont, vl);
                _scalar_n = vfredsum_vs_f32m8_f32m1(_scalar_n, _h_cont, _scalar_n, vl);

                N = vfmv_f_s_f32m1_f32(_scalar_n);
                n_out2 -= vl;
                ptr_hc2 += vl;
                ptr_whc_n += vl;
            }

            N = bias_c_WN[q] + R * N;

            int n2 = size;
            const float* ptr_x2 = x;
            const float* ptr_xcn = weight_xc_N;
            while (n2 > 0)
            {
                word_type vl = vsetvl_e32m8(n2);

                vfloat32m8_t _x = vle32_v_f32m8(ptr_x2, vl);
                vfloat32m8_t _xcn = vle32_v_f32m8(ptr_xcn, vl);
                vfloat32m1_t _scalar_n = vfmv_s_f_f32m1(vundefined_f32m1(), N, vl);

                _xcn = vfmul_vv_f32m8(_x, _xcn, vl);
                _scalar_n = vfredsum_vs_f32m8_f32m1(_scalar_n, _xcn, _scalar_n, vl);
                N = vfmv_f_s_f32m1_f32(_scalar_n);

                n2 -= vl;
                ptr_x2 += vl;
                ptr_xcn += vl;
            }

            // tanh(N)
            N = tanh(N);

            gates_data[0] = U;
            gates_data[1] = N;
        }

        // h_t := (1 - update) .* new + update .* h_{t-1}
        float* output_data = top_blob.row(ti);
        for (int q = 0; q < num_output; q++)
        {
            const float* gates_data = gates.row(q);

            float U = gates_data[0];
            float N = gates_data[1];

            float H = (1 - U) * N + U * hidden_state[q];

            hidden_state[q] = H;
            output_data[q] = H;
        }
    }

    return 0;
}

#endif

GRU_riscv::GRU_riscv()
{
    // TODO: fp16
}

int GRU_riscv::forward(const Mat& bottom_blob, Mat& top_blob, const Option& opt) const
{
    int elembits = bottom_blob.elembits();
#if __riscv_vector
    int T = bottom_blob.h;

    int num_directions = direction == 2 ? 2 : 1;

    // initial hidden state
    Mat hidden(num_output, 4u, opt.workspace_allocator);
    if (hidden.empty())
        return -100;
    hidden.fill(0.f);

    top_blob.create(num_output * num_directions, T, 4u, opt.blob_allocator);
    if (top_blob.empty())
        return -100;

    // Uni directional
    if (direction == 0 || direction == 1)
    {
        int ret = gru(bottom_blob, top_blob, direction, weight_xc_data.channel(0), bias_c_data.channel(0), weight_hc_data.channel(0), hidden, opt);
        if (ret != 0)
            return ret;
    }

    if (direction == 2)
    {
        Mat top_blob_forward(num_output, T, 4u, opt.workspace_allocator);
        if (top_blob_forward.empty())
            return -100;

        Mat top_blob_reverse(num_output, T, 4u, opt.workspace_allocator);
        if (top_blob_reverse.empty())
            return -100;

        int ret0 = gru(bottom_blob, top_blob_forward, 0, weight_xc_data.channel(0), bias_c_data.channel(0), weight_hc_data.channel(0), hidden, opt);
        if (ret0 != 0)
            return ret0;

        hidden.fill(0.0f);

        int ret1 = gru(bottom_blob, top_blob_reverse, 1, weight_xc_data.channel(1), bias_c_data.channel(1), weight_hc_data.channel(1), hidden, opt);
        if (ret1 != 0)
            return ret1;

        // concat w
        for (int i = 0; i < T; i++)
        {
            const float* pf = top_blob_forward.row(i);
            const float* pr = top_blob_reverse.row(i);
            float* ptr = top_blob.row(i);

            memcpy(ptr, pf, num_output * sizeof(float));
            memcpy(ptr + num_output, pr, num_output * sizeof(float));
        }
    }

    return 0;
#endif
    return GRU::forward(bottom_blob, top_blob, opt);
}

int GRU_riscv::forward(const std::vector<Mat>& bottom_blobs, std::vector<Mat>& top_blobs, const Option& opt) const
{
    if (bottom_blobs.size() != 2 || top_blobs.size() != 2)
    {
        return forward(bottom_blobs[0], top_blobs[0], opt);
    }

    const Mat& bottom_blob = bottom_blobs[0];
    int elembits = bottom_blob.elembits();

#if __riscv_vector
    int T = bottom_blob.h;
    Mat& top_blob = top_blobs[0];
    Mat& hidden_state = top_blobs[1];

    //Copy previous states
    hidden_state = bottom_blobs[1].clone(opt.blob_allocator);

    top_blob.create(num_output, T, 4u, opt.blob_allocator);
    if (top_blob.empty())
        return -100;

    // Uni directional
    if (direction == 0 || direction == 1)
    {
        int ret = gru(bottom_blob, top_blob, direction, weight_xc_data.channel(0), bias_c_data.channel(0), weight_hc_data.channel(0), hidden_state, opt);
        if (ret != 0)
            return ret;
    }

    return 0;
#endif
    return GRU::forward(bottom_blobs, top_blobs, opt);
}
} // namespace ncnn