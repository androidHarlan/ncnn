// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "prelu_vulkan.h"

namespace ncnn {

DEFINE_LAYER_CREATOR(PReLU_vulkan)

PReLU_vulkan::PReLU_vulkan()
{
    support_vulkan = true;

    pipeline_prelu = 0;
    pipeline_prelu_pack4 = 0;
}

int PReLU_vulkan::create_pipeline(const Option& opt)
{
    std::vector<vk_specialization_type> specializations(1);
    specializations[0].i = num_slope;

    // pack1
    if (num_slope == 1 || num_slope % 4 != 0)
    {
        pipeline_prelu = new Pipeline(vkdev);
        pipeline_prelu->set_optimal_local_size_xyz(8, 8, num_slope);
        pipeline_prelu->create("prelu", opt, specializations, 2, 5);
    }

    // pack4
    if (num_slope == 1 || num_slope % 4 == 0)
    {
        pipeline_prelu_pack4 = new Pipeline(vkdev);
        pipeline_prelu_pack4->set_optimal_local_size_xyz(8, 8, num_slope / 4);
        pipeline_prelu_pack4->create("prelu_pack4", opt, specializations, 2, 5);
    }

    return 0;
}

int PReLU_vulkan::destroy_pipeline(const Option& opt)
{
    UNUSED(opt);
    delete pipeline_prelu;
    pipeline_prelu = 0;

    delete pipeline_prelu_pack4;
    pipeline_prelu_pack4 = 0;

    return 0;
}

int PReLU_vulkan::upload_model(VkTransfer& cmd, const Option& opt)
{
    if (num_slope == 1)
    {
        // dup4 for pack4
        Mat slope_data4(4);
        slope_data4.fill(slope_data[0]);
        cmd.record_upload(slope_data4, slope_data_gpu, opt);
    }
    else
    {
        cmd.record_upload(slope_data, slope_data_gpu, opt);
    }

    return 0;
}

int PReLU_vulkan::forward_inplace(VkMat& bottom_top_blob, VkCompute& cmd, const Option& opt) const
{
    UNUSED(opt);
    int elempack = bottom_top_blob.elempack;

    std::vector<VkMat> bindings(2);
    bindings[0] = bottom_top_blob;
    bindings[1] = slope_data_gpu;

    std::vector<vk_constant_type> constants(5);
    constants[0].i = bottom_top_blob.dims;
    constants[1].i = bottom_top_blob.w;
    constants[2].i = bottom_top_blob.h;
    constants[3].i = bottom_top_blob.c;
    constants[4].i = bottom_top_blob.cstep;

    const Pipeline* pipeline = elempack == 4 ? pipeline_prelu_pack4 : pipeline_prelu;

    cmd.record_pipeline(pipeline, bindings, constants, bottom_top_blob);

    return 0;
}

} // namespace ncnn
