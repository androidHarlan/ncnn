// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "cpu.h"
#include "mat.h"
#include "x86_usability.h"

namespace ncnn {
#include "gridsample_apply_interpolation.h"

void gridsample_2d_bilinear_apply_interpolation_p8_avx2(const Mat& src, Mat& dst, const Mat& offset_value, const Option& opt)
{
    gridsample_2d_bilinear_apply_interpolation_p8(src, dst, offset_value, opt);
}
void gridsample_2d_bilinear_apply_interpolation_p4_avx2(const Mat& src, Mat& dst, const Mat& offset_value, const Option& opt)
{
    gridsample_2d_bilinear_apply_interpolation_p4(src, dst, offset_value, opt);
}

void gridsample_3d_bilinear_apply_interpolation_p8_avx2(const Mat& src, Mat& dst, const Mat& offset_value, const Option& opt)
{
    gridsample_3d_bilinear_apply_interpolation_p8(src, dst, offset_value, opt);
}
void gridsample_3d_bilinear_apply_interpolation_p4_avx2(const Mat& src, Mat& dst, const Mat& offset_value, const Option& opt)
{
    gridsample_3d_bilinear_apply_interpolation_p4(src, dst, offset_value, opt);
}

void gridsample_nearest_apply_interpolation_p8_avx2(const Mat& src, Mat& dst, const Mat& offset_value, const Option& opt)
{
    gridsample_nearest_apply_interpolation_p8(src, dst, offset_value, opt);
}
void gridsample_nearest_apply_interpolation_p4_avx2(const Mat& src, Mat& dst, const Mat& offset_value, const Option& opt)
{
    gridsample_nearest_apply_interpolation_p4(src, dst, offset_value, opt);
}
void gridsample_nearest_apply_interpolation_p1_avx2(const Mat& src, Mat& dst, const Mat& offset_value, const Option& opt)
{
    gridsample_nearest_apply_interpolation_p1(src, dst, offset_value, opt);
}

void gridsample_2d_bicubic_apply_interpolation_p8_avx2(const Mat& src, Mat& dst, Mat& offset_value, const Option& opt)
{
    gridsample_2d_bicubic_apply_interpolation_p8(src, dst, offset_value, opt);
}
void gridsample_2d_bicubic_apply_interpolation_p4_avx2(const Mat& src, Mat& dst, Mat& offset_value, const Option& opt)
{
    gridsample_2d_bicubic_apply_interpolation_p4(src, dst, offset_value, opt);
}

} // namespace ncnn