// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "unaryop_riscv.h"

#if __riscv_vector
#include <riscv_vector.h>
#include "rvv_mathfun.h"
#if __riscv_zvfh
#include "rvv_mathfun_fp16s.h"
#endif
#endif // __riscv_vector

namespace ncnn {

#if __riscv_zvfh
template<typename Op>
static int unary_op_inplace_fp16s(Mat& a, const Option& opt)
{
    Op op;

    int w = a.w;
    int h = a.h;
    int d = a.d;
    int channels = a.c;
    int size = w * h * d;
    int elempack = a.elempack;

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int q = 0; q < channels; q++)
    {
        __fp16* ptr = a.channel(q);

        int n = size * elempack;
        while (n > 0)
        {
            size_t vl = __riscv_vsetvl_e16m8(n);

            vfloat16m8_t _p = __riscv_vle16_v_f16m8(ptr, vl);
            _p = op(_p, vl);
            __riscv_vse16_v_f16m8(ptr, _p, vl);

            ptr += vl;
            n -= vl;
        }
    }

    return 0;
}

namespace UnaryOp_riscv_functor {

struct unary_op_abs_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
        return __riscv_vfsgnj_vf_f16m8(x, 1.f, vl);
    }
};

struct unary_op_neg_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
        return __riscv_vfneg_v_f16m8(x, vl);
    }
};

struct unary_op_floor_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
        return __riscv_vfcvt_f_x_v_f16m8(__riscv_vfcvt_x_f_v_i16m8_rm(x, __RISCV_FRM_RDN, vl), vl);
    }
};

struct unary_op_ceil_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
        return __riscv_vfcvt_f_x_v_f16m8(__riscv_vfcvt_x_f_v_i16m8_rm(x, __RISCV_FRM_RUP, vl), vl);
    }
};

struct unary_op_square_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
        return __riscv_vfmul_vv_f16m8(x, x, vl);
    }
};

struct unary_op_sqrt_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
        return __riscv_vfsqrt_v_f16m8(x, vl);
    }
};

struct unary_op_rsqrt_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
#if C906
        vfloat16m8_t _reciprocal = __riscv_vfrdiv_vf_f16m8(__riscv_vfsqrt_v_f16m8(x, vl), 1.f, vl);
#else
        vfloat16m8_t _reciprocal = __riscv_vfrsqrt7_v_f16m8(x, vl);
        _reciprocal = __riscv_vfmul_vv_f16m8(__riscv_vfrsub_vf_f16m8(__riscv_vfmul_vv_f16m8(__riscv_vfmul_vf_f16m8(x, 0.5f, vl), __riscv_vfmul_vv_f16m8(_reciprocal, _reciprocal, vl), vl), 1.5f, vl), _reciprocal, vl);
        // _reciprocal = __riscv_vfmul_vv_f16m8(__riscv_vfrsub_vf_f16m8(__riscv_vfmul_vv_f16m8(__riscv_vfmul_vf_f16m8(x, 0.5f, vl), __riscv_vfmul_vv_f16m8(_reciprocal, _reciprocal, vl), vl), 1.5f, vl), _reciprocal, vl);
#endif
        return _reciprocal;
    }
};

struct unary_op_exp_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
        return exp_ps(x, vl);
    }
};

struct unary_op_log_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
        return log_ps(x, vl);
    }
};

struct unary_op_sin_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
        return sin_ps(x, vl);
    }
};

struct unary_op_cos_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
        return cos_ps(x, vl);
    }
};

struct unary_op_tan_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
        // TODO rvv optimize
        std::vector<__fp16> tmp(vl);
        __riscv_vse16_v_f16m8(tmp.data(), x, vl);
        for (size_t i = 0; i < vl; i++)
        {
            tmp[i] = tanf((float)tmp[i]);
        }
        return __riscv_vle16_v_f16m8(tmp.data(), vl);
    }
};

struct unary_op_asin_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
        // TODO rvv optimize
        std::vector<__fp16> tmp(vl);
        __riscv_vse16_v_f16m8(tmp.data(), x, vl);
        for (size_t i = 0; i < vl; i++)
        {
            tmp[i] = asinf((float)tmp[i]);
        }
        return __riscv_vle16_v_f16m8(tmp.data(), vl);
    }
};

struct unary_op_acos_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
        // TODO rvv optimize
        std::vector<__fp16> tmp(vl);
        __riscv_vse16_v_f16m8(tmp.data(), x, vl);
        for (size_t i = 0; i < vl; i++)
        {
            tmp[i] = acosf((float)tmp[i]);
        }
        return __riscv_vle16_v_f16m8(tmp.data(), vl);
    }
};

struct unary_op_atan_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
        // TODO rvv optimize
        std::vector<__fp16> tmp(vl);
        __riscv_vse16_v_f16m8(tmp.data(), x, vl);
        for (size_t i = 0; i < vl; i++)
        {
            tmp[i] = atanf((float)tmp[i]);
        }
        return __riscv_vle16_v_f16m8(tmp.data(), vl);
    }
};

struct unary_op_reciprocal_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
#if C906
        vfloat16m8_t _reciprocal = __riscv_vfrdiv_vf_f16m8(x, 1.f, vl);
#else
        vfloat16m8_t _reciprocal = __riscv_vfrec7_v_f16m8(x, vl);
        _reciprocal = __riscv_vfmul_vv_f16m8(__riscv_vfrsub_vf_f16m8(__riscv_vfmul_vv_f16m8(x, _reciprocal, vl), 2.f, vl), _reciprocal, vl);
        // _reciprocal = __riscv_vfmul_vv_f16m8(__riscv_vfrsub_vf_f16m8(__riscv_vfmul_vv_f16m8(x, _reciprocal, vl), 2.f, vl), _reciprocal, vl);
#endif
        return _reciprocal;
    }
};

struct unary_op_tanh_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
        return tanh_ps(x, vl);
    }
};

struct unary_op_log10_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
        return __riscv_vfmul_vf_f16m8(log_ps(x, vl), 0.434294481903, vl);
    }
};

struct unary_op_round_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
        return __riscv_vfcvt_f_x_v_f16m8(__riscv_vfcvt_x_f_v_i16m8(x, vl), vl);
    }
};

struct unary_op_trunc_fp16s
{
    vfloat16m8_t operator()(const vfloat16m8_t& x, const size_t& vl) const
    {
#if C906
        // simulate trunc with floor positives and ceil negative
        // xi = round(x)
        // floorx = xi - (xi > x)
        // ceilx = xi + (xi < x)
        // truncx = x >= 0 ? floorx : ceilx
        vint16m8_t _xi = __riscv_vfcvt_x_f_v_i16m8(x, vl);
        vfloat16m8_t _xf = __riscv_vfcvt_f_x_v_f16m8(_xi, vl);
        vbool2_t _floormask = __riscv_vmfgt_vv_f16m8_b2(_xf, x, vl);
        vint16m8_t _floorx = __riscv_vsub_vx_i16m8_m(_floormask, _xi, _xi, 1, vl);
        vbool2_t _ceilmask = __riscv_vmflt_vv_f16m8_b2(_xf, x, vl);
        vint16m8_t _ceilx = __riscv_vadd_vx_i16m8_m(_ceilmask, _xi, _xi, 1, vl);
        vbool2_t _negative = __riscv_vmflt_vf_f16m8_b2(x, 0.f, vl);
        return __riscv_vfcvt_f_x_v_f16m8(__riscv_vmerge_vvm_i16m8(_negative, _floorx, _ceilx, vl), vl);
#else
        return __riscv_vfcvt_f_x_v_f16m8(__riscv_vfcvt_rtz_x_f_v_i16m8(x, vl), vl);
#endif
    }
};

} // namespace UnaryOp_riscv_functor

int UnaryOp_riscv::forward_inplace_fp16s(Mat& bottom_top_blob, const Option& opt) const
{
    using namespace UnaryOp_riscv_functor;

    if (op_type == Operation_ABS)
        return unary_op_inplace_fp16s<unary_op_abs_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_NEG)
        return unary_op_inplace_fp16s<unary_op_neg_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_FLOOR)
        return unary_op_inplace_fp16s<unary_op_floor_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_CEIL)
        return unary_op_inplace_fp16s<unary_op_ceil_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_SQUARE)
        return unary_op_inplace_fp16s<unary_op_square_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_SQRT)
        return unary_op_inplace_fp16s<unary_op_sqrt_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_RSQRT)
        return unary_op_inplace_fp16s<unary_op_rsqrt_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_EXP)
        return unary_op_inplace_fp16s<unary_op_exp_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_LOG)
        return unary_op_inplace_fp16s<unary_op_log_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_SIN)
        return unary_op_inplace_fp16s<unary_op_sin_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_COS)
        return unary_op_inplace_fp16s<unary_op_cos_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_TAN)
        return unary_op_inplace_fp16s<unary_op_tan_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_ASIN)
        return unary_op_inplace_fp16s<unary_op_asin_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_ACOS)
        return unary_op_inplace_fp16s<unary_op_acos_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_ATAN)
        return unary_op_inplace_fp16s<unary_op_atan_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_RECIPROCAL)
        return unary_op_inplace_fp16s<unary_op_reciprocal_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_TANH)
        return unary_op_inplace_fp16s<unary_op_tanh_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_LOG10)
        return unary_op_inplace_fp16s<unary_op_log10_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_ROUND)
        return unary_op_inplace_fp16s<unary_op_round_fp16s>(bottom_top_blob, opt);

    if (op_type == Operation_TRUNC)
        return unary_op_inplace_fp16s<unary_op_trunc_fp16s>(bottom_top_blob, opt);

    return 0;
}
#endif // __riscv_zvfh

} // namespace ncnn
