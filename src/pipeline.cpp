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

#include "pipeline.h"

#include "layer_shader_type.h"
#include "mat.h"
#include "pipelinecache.h"
#include "option.h"

#include <algorithm>
#include <math.h>

#if __ANDROID_API__ >= 26
#include <android/hardware_buffer.h>
#endif // __ANDROID_API__ >= 26

namespace ncnn {

#if NCNN_VULKAN
Pipeline::Pipeline(const VulkanDevice* _vkdev)
    : vkdev(_vkdev)
{
    pipeline_cache = 0;

    shader_module = 0;
    descriptorset_layout = 0;
    pipeline_layout = 0;
    pipeline = 0;
    descriptor_update_template = 0;

    local_size_x = 1;
    local_size_y = 1;
    local_size_z = 1;
}

Pipeline::~Pipeline()
{
    destroy();
}

int Pipeline::create(const uint32_t* spv_data, size_t spv_data_size, const std::vector<vk_specialization_type>& specializations)
{
    ShaderInfo si;
    int ret = resolve_shader_info(spv_data, spv_data_size, si);
    if (ret != 0)
    {
        NCNN_LOGE("resolve_shader_info failed %d", ret);
        return -1;
    }

    // -3 for local_size_xyz
    int specialization_count_expected = si.specialization_count - 3;
    if ((int)specializations.size() != specialization_count_expected)
    {
        NCNN_LOGE("pipeline specialization count mismatch, expect %d but got %d", specialization_count_expected, (int)specializations.size());
        return -1;
    }

    shader_module = vkdev->compile_shader_module(spv_data, spv_data_size, local_size_x, local_size_y, local_size_z);

    //     NCNN_LOGE("shader_module %p created", shader_module);

    return create(shader_module, si, specializations);
}

int Pipeline::create(int shader_type_index, const Option& opt, const std::vector<vk_specialization_type>& specializations)
{
    if (opt.pipeline_cache)
    {
        pipeline_cache = opt.pipeline_cache;

        // get from pipeline cache
        return pipeline_cache->get_pipeline(shader_type_index, opt, specializations, local_size_x, local_size_y, local_size_z,
                                            &shader_module, &descriptorset_layout, &pipeline_layout, &pipeline, &descriptor_update_template,
                                            shader_info);
    }

#if NCNN_VULKAN_ONLINE_SPIRV
    std::vector<uint32_t> spirv;
    int retc = compile_spirv_module(shader_type_index, opt, spirv);
    if (retc != 0)
    {
        NCNN_LOGE("compile_spirv_module failed %d", retc);
        return -1;
    }

    const uint32_t* spv_data = spirv.data();
    size_t spv_data_size = spirv.size() * 4;

    ShaderInfo si;
    int ret = resolve_shader_info(spv_data, spv_data_size, si);
    if (ret != 0)
    {
        NCNN_LOGE("resolve_shader_info failed %d", ret);
        return -1;
    }

    if ((int)specializations.size() != si.specialization_count)
    {
        NCNN_LOGE("pipeline specialization count mismatch, expect %d but got %d", si.specialization_count, (int)specializations.size());
        return -1;
    }

    shader_module = vkdev->compile_shader_module(spv_data, spv_data_size, local_size_x, local_size_y, local_size_z);
#else
    // ncnn_add_shader cmake macro
    // 0 = fp32
    // 1 = fp16p
    // 2 = fp16pa
    // 3 = fp16s
    // 4 = fp16sa
    // 5 = image
    // 6 = image_fp16p
    // 7 = image_fp16pa
    // 8 = image_fp16s
    // 9 = image_fp16sa

    if (!vkdev->info.bug_layout_binding_id_alias && opt.use_image_storage && vkdev->info.support_fp16_storage && opt.use_fp16_storage && vkdev->info.support_fp16_arithmetic && opt.use_fp16_arithmetic)
    {
        shader_type_index += 9;
    }
    else if (!vkdev->info.bug_layout_binding_id_alias && opt.use_image_storage && vkdev->info.support_fp16_packed && opt.use_fp16_packed && vkdev->info.support_fp16_arithmetic && opt.use_fp16_arithmetic)
    {
        shader_type_index += 7;
    }
    else if (!vkdev->info.bug_layout_binding_id_alias && opt.use_image_storage && vkdev->info.support_fp16_storage && opt.use_fp16_storage)
    {
        shader_type_index += 8;
    }
    else if (!vkdev->info.bug_layout_binding_id_alias && opt.use_image_storage && vkdev->info.support_fp16_packed && opt.use_fp16_packed)
    {
        shader_type_index += 6;
    }
    else if (!vkdev->info.bug_layout_binding_id_alias && opt.use_image_storage)
    {
        shader_type_index += 5;
    }
    else if (vkdev->info.support_fp16_storage && opt.use_fp16_storage && vkdev->info.support_fp16_arithmetic && opt.use_fp16_arithmetic)
    {
        shader_type_index += 4;
    }
    else if (vkdev->info.support_fp16_packed && opt.use_fp16_packed && vkdev->info.support_fp16_arithmetic && opt.use_fp16_arithmetic)
    {
        shader_type_index += 2;
    }
    else if (vkdev->info.support_fp16_storage && opt.use_fp16_storage)
    {
        shader_type_index += 3;
    }
    else if (vkdev->info.support_fp16_packed && opt.use_fp16_packed)
    {
        shader_type_index += 1;
    }

    const ShaderInfo& si = get_shader_info(shader_type_index);

    if ((int)specializations.size() != si.specialization_count)
    {
        NCNN_LOGE("pipeline %d specialization count mismatch, expect %d but got %d", shader_type_index, si.specialization_count, (int)specializations.size());
        return -1;
    }

    shader_module = vkdev->create_shader_module(shader_type_index, local_size_x, local_size_y, local_size_z);
#endif

    //     NCNN_LOGE("shader_module %p created", shader_module);

    return create(shader_module, si, specializations);
}

int Pipeline::create(VkShaderModule shader_module, const ShaderInfo& _shader_info, const std::vector<vk_specialization_type>& specializations)
{
    shader_info = _shader_info;

    int ret = 0;

    // create new pipeline
    if ((int)specializations.size() != shader_info.specialization_count)
    {
        NCNN_LOGE("pipeline specialization count mismatch, expect %d but got %d", shader_info.specialization_count, (int)specializations.size());
        goto ERROR_Pipeline;
    }

    ret = vkdev->create_descriptorset_layout(shader_info.binding_count, shader_info.binding_types, &descriptorset_layout);
    if (ret != 0)
        goto ERROR_Pipeline;

    ret = vkdev->create_pipeline_layout(shader_info.push_constant_count, descriptorset_layout, &pipeline_layout);
    if (ret != 0)
        goto ERROR_Pipeline;

    ret = vkdev->create_pipeline(shader_module, pipeline_layout, specializations, &pipeline);
    if (ret != 0)
        goto ERROR_Pipeline;

    if (vkdev->info.support_VK_KHR_descriptor_update_template)
    {
        ret = vkdev->create_descriptor_update_template(shader_info.binding_count, shader_info.binding_types, descriptorset_layout, pipeline_layout, &descriptor_update_template);
        if (ret != 0)
            goto ERROR_Pipeline;
    }

    return 0;

ERROR_Pipeline:

    destroy();

    return -1;
}

void Pipeline::destroy()
{
    if (pipeline_cache)
        return;

    if (vkdev->info.support_VK_KHR_descriptor_update_template)
    {
        if (descriptor_update_template)
        {
            vkdev->vkDestroyDescriptorUpdateTemplateKHR(vkdev->vkdevice(), descriptor_update_template, 0);
            descriptor_update_template = 0;
        }
    }

    if (pipeline)
    {
        vkDestroyPipeline(vkdev->vkdevice(), pipeline, 0);
        pipeline = 0;
    }

    if (pipeline_layout)
    {
        vkDestroyPipelineLayout(vkdev->vkdevice(), pipeline_layout, 0);
        pipeline_layout = 0;
    }

    if (descriptorset_layout)
    {
        vkDestroyDescriptorSetLayout(vkdev->vkdevice(), descriptorset_layout, 0);
        descriptorset_layout = 0;
    }

    if (shader_module)
    {
        vkDestroyShaderModule(vkdev->vkdevice(), shader_module, 0);
        shader_module = 0;
    }
}

void Pipeline::set_optimal_local_size_xyz(int w, int h, int c)
{
    set_optimal_local_size_xyz(Mat(w, h, c, (void*)0));
}

void Pipeline::set_optimal_local_size_xyz(const Mat& local_size_xyz)
{
    int w = local_size_xyz.w;
    int h = local_size_xyz.h;
    int c = local_size_xyz.c;

    if (w == 0 && h == 0 && c == 0)
    {
        // fallback to the common and safe 4x4x4
        w = 4;
        h = 4;
        c = 4;
    }

    w = std::min(w, (int)vkdev->info.max_workgroup_size[0]);
    h = std::min(h, (int)vkdev->info.max_workgroup_size[1]);
    c = std::min(c, (int)vkdev->info.max_workgroup_size[2]);

    if (w * h * c <= (int)vkdev->info.max_workgroup_invocations)
    {
        return set_local_size_xyz(w, h, c);
    }

    int max_local_size_xy = (int)vkdev->info.max_workgroup_invocations / c;

    int wh_max = std::max(1, (int)sqrt(max_local_size_xy));
    while (w * h >= wh_max)
    {
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
    }

    set_local_size_xyz(w, h, c);
}

void Pipeline::set_local_size_xyz(int w, int h, int c)
{
    local_size_x = w;
    local_size_y = h;
    local_size_z = c;

    //     NCNN_LOGE("local size = %d %d %d", local_size_x, local_size_y, local_size_z);
}

#if __ANDROID_API__ >= 26
ImportAndroidHardwareBufferPipeline::ImportAndroidHardwareBufferPipeline(const VulkanDevice* _vkdev)
    : Pipeline(_vkdev)
{
    sampler = 0;
}

ImportAndroidHardwareBufferPipeline::~ImportAndroidHardwareBufferPipeline()
{
    destroy();
}

int ImportAndroidHardwareBufferPipeline::create(VkAndroidHardwareBufferImageAllocator* ahb_im_allocator, int _type_to, int _rotate_from, const Option& opt)
{
    int target_width;
    int target_height;

    if (rotate_from < 5) // 1 2 3 4
    {
        target_width = ahb_im_allocator->width();
        target_height = ahb_im_allocator->height();
    }
    else // 5 6 7 8
    {
        target_width = ahb_im_allocator->height();
        target_height = ahb_im_allocator->width();
    }

    return create(ahb_im_allocator, _type_to, _rotate_from, target_width, target_height, opt);
}

int ImportAndroidHardwareBufferPipeline::create(VkAndroidHardwareBufferImageAllocator* ahb_im_allocator, int _type_to, int _rotate_from, int target_width, int target_height, const Option& opt)
{
    int w = ahb_im_allocator->width();
    int h = ahb_im_allocator->height();

    type_to = _type_to;
    rotate_from = _rotate_from;

    need_resize = false;
    if (rotate_from < 5) // 1 2 3 4
    {
        if (target_width != w || target_height != h)
            need_resize = true;
    }
    else // 5 6 7 8
    {
        if (target_width != h || target_height != w)
            need_resize = true;
    }

    //     if (type_to == 1 || type_to == 2)
    //     {
    //         outc = 3;
    //         out_elemsize = vkdev->info.support_fp16_storage && opt.use_fp16_storage ? 2u : 4u;
    //         out_elempack = 1;
    //     }
    //     else if (type_to == 3)
    //     {
    //         outc = 1;
    //         out_elemsize = vkdev->info.support_fp16_storage && opt.use_fp16_storage ? 2u : 4u;
    //         out_elempack = 1;
    //     }
    //     else // if (type_to == 4 || type_to == 5)
    //     {
    //         outc = 1;
    //         out_elemsize = ((vkdev->info.support_fp16_packed && opt.use_fp16_packed) || (vkdev->info.support_fp16_storage && opt.use_fp16_storage)) ? 8u : 16u;
    //         out_elempack = 4;
    //     }

    set_local_size_xyz(8, 8, 1);

    std::vector<vk_specialization_type> specializations(7);
    specializations[0].i = ahb_im_allocator->width();
    specializations[1].i = ahb_im_allocator->height();
    specializations[2].i = target_width;
    specializations[3].i = target_height;
    specializations[4].i = type_to;
    specializations[5].i = rotate_from;
    specializations[6].i = need_resize;

    create_sampler(ahb_im_allocator);

    create_descriptorset_layout();

    create_pipeline_layout();

    int shader_type_index = LayerShaderType::convert_ycbcr;

#if NCNN_VULKAN_ONLINE_SPIRV
    std::vector<uint32_t> spirv;
    int retc = compile_spirv_module(shader_type_index, opt, spirv);
    if (retc != 0)
    {
        NCNN_LOGE("compile_spirv_module failed %d", retc);
        return -1;
    }

    const uint32_t* spv_data = spirv.data();
    size_t spv_data_size = spirv.size() * 4;

    ShaderInfo si;
    int ret = resolve_shader_info(spv_data, spv_data_size, si);
    if (ret != 0)
    {
        NCNN_LOGE("resolve_shader_info failed %d", ret);
        return -1;
    }

    // -3 for local_size_xyz
    int specialization_count_expected = si.specialization_count - 3;
    if ((int)specializations.size() != specialization_count_expected)
    {
        NCNN_LOGE("pipeline specialization count mismatch, expect %d but got %d", specialization_count_expected, (int)specializations.size());
        return -1;
    }

    shader_module = vkdev->compile_shader_module(spv_data, spv_data_size, local_size_x, local_size_y, local_size_z);
#else
    // ncnn_add_shader cmake macro
    // 0 = fp32
    // 1 = fp16p
    // 2 = fp16pa
    // 3 = fp16s
    // 4 = fp16sa
    // 5 = image
    // 6 = image_fp16p
    // 7 = image_fp16pa
    // 8 = image_fp16s
    // 9 = image_fp16sa

    if (!vkdev->info.bug_layout_binding_id_alias && opt.use_image_storage && vkdev->info.support_fp16_storage && opt.use_fp16_storage && vkdev->info.support_fp16_arithmetic && opt.use_fp16_arithmetic)
    {
        shader_type_index += 9;
    }
    else if (!vkdev->info.bug_layout_binding_id_alias && opt.use_image_storage && vkdev->info.support_fp16_packed && opt.use_fp16_packed && vkdev->info.support_fp16_arithmetic && opt.use_fp16_arithmetic)
    {
        shader_type_index += 7;
    }
    else if (!vkdev->info.bug_layout_binding_id_alias && opt.use_image_storage && vkdev->info.support_fp16_storage && opt.use_fp16_storage)
    {
        shader_type_index += 8;
    }
    else if (!vkdev->info.bug_layout_binding_id_alias && opt.use_image_storage && vkdev->info.support_fp16_packed && opt.use_fp16_packed)
    {
        shader_type_index += 6;
    }
    else if (!vkdev->info.bug_layout_binding_id_alias && opt.use_image_storage)
    {
        shader_type_index += 5;
    }
    else if (vkdev->info.support_fp16_storage && opt.use_fp16_storage && vkdev->info.support_fp16_arithmetic && opt.use_fp16_arithmetic)
    {
        shader_type_index += 4;
    }
    else if (vkdev->info.support_fp16_packed && opt.use_fp16_packed && vkdev->info.support_fp16_arithmetic && opt.use_fp16_arithmetic)
    {
        shader_type_index += 2;
    }
    else if (vkdev->info.support_fp16_storage && opt.use_fp16_storage)
    {
        shader_type_index += 3;
    }
    else if (vkdev->info.support_fp16_packed && opt.use_fp16_packed)
    {
        shader_type_index += 1;
    }

    shader_module = vkdev->get_shader_module(shader_type_index);
#endif
    create_pipeline(shader_module, specializations);

    if (vkdev->info.support_VK_KHR_descriptor_update_template)
    {
        create_descriptor_update_template();
    }

    return 0;
}

void ImportAndroidHardwareBufferPipeline::destroy()
{
    if (sampler)
    {
        vkDestroySampler(vkdev->vkdevice(), sampler, 0);
        sampler = 0;
    }

    Pipeline::destroy();
}

int ImportAndroidHardwareBufferPipeline::create_sampler(VkAndroidHardwareBufferImageAllocator* ahb_im_allocator)
{
    VkResult ret;

    VkExternalFormatANDROID externalFormatANDROID;
    externalFormatANDROID.sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID;
    externalFormatANDROID.pNext = 0;
    externalFormatANDROID.externalFormat = ahb_im_allocator->external_format();

    VkSamplerYcbcrConversionInfoKHR samplerYcbcrConversionInfo;
    samplerYcbcrConversionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO_KHR;
    samplerYcbcrConversionInfo.pNext = &externalFormatANDROID;
    samplerYcbcrConversionInfo.conversion = ahb_im_allocator->samplerYcbcrConversion;

    VkSamplerCreateInfo samplerCreateInfo;
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.pNext = &samplerYcbcrConversionInfo;
    samplerCreateInfo.magFilter = need_resize ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter = need_resize ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    samplerCreateInfo.maxAnisotropy = 1;
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK; //VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; FIXME
    samplerCreateInfo.unnormalizedCoordinates = VK_TRUE;                     //VK_FALSE; FIXME ?

    ret = vkCreateSampler(vkdev->vkdevice(), &samplerCreateInfo, 0, &sampler);
    if (ret != VK_SUCCESS)
    {
        NCNN_LOGE("vkCreateSampler failed %d", ret);
        return -1;
    }

    return 0;
}

int ImportAndroidHardwareBufferPipeline::create_descriptorset_layout()
{
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[3];
    descriptorSetLayoutBindings[0].binding = 0;
    descriptorSetLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorSetLayoutBindings[0].descriptorCount = 1;
    descriptorSetLayoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    descriptorSetLayoutBindings[0].pImmutableSamplers = &sampler;
    descriptorSetLayoutBindings[1].binding = 1;
    descriptorSetLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorSetLayoutBindings[1].descriptorCount = 1;
    descriptorSetLayoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    descriptorSetLayoutBindings[1].pImmutableSamplers = 0;
    descriptorSetLayoutBindings[2].binding = 2;
    descriptorSetLayoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorSetLayoutBindings[2].descriptorCount = 1;
    descriptorSetLayoutBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    descriptorSetLayoutBindings[2].pImmutableSamplers = 0;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pNext = 0;
    descriptorSetLayoutCreateInfo.flags = 0;
    descriptorSetLayoutCreateInfo.bindingCount = 3;
    descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;

    if (vkdev->info.support_VK_KHR_push_descriptor)
    {
        descriptorSetLayoutCreateInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    }

    VkResult ret = vkCreateDescriptorSetLayout(vkdev->vkdevice(), &descriptorSetLayoutCreateInfo, 0, &descriptorset_layout);
    if (ret != VK_SUCCESS)
    {
        NCNN_LOGE("vkCreateDescriptorSetLayout failed %d", ret);
        return -1;
    }

    return 0;
}

int ImportAndroidHardwareBufferPipeline::create_descriptor_update_template()
{
    VkDescriptorUpdateTemplateEntryKHR descriptorUpdateTemplateEntries[3];
    descriptorUpdateTemplateEntries[0].dstBinding = 0;
    descriptorUpdateTemplateEntries[0].dstArrayElement = 0;
    descriptorUpdateTemplateEntries[0].descriptorCount = 1;
    descriptorUpdateTemplateEntries[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorUpdateTemplateEntries[0].offset = 0;
    descriptorUpdateTemplateEntries[0].stride = sizeof(VkDescriptorImageInfo);
    descriptorUpdateTemplateEntries[1].dstBinding = 1;
    descriptorUpdateTemplateEntries[1].dstArrayElement = 0;
    descriptorUpdateTemplateEntries[1].descriptorCount = 1;
    descriptorUpdateTemplateEntries[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorUpdateTemplateEntries[1].offset = sizeof(VkDescriptorImageInfo);
    descriptorUpdateTemplateEntries[1].stride = sizeof(VkDescriptorBufferInfo);
    descriptorUpdateTemplateEntries[2].dstBinding = 2;
    descriptorUpdateTemplateEntries[2].dstArrayElement = 0;
    descriptorUpdateTemplateEntries[2].descriptorCount = 1;
    descriptorUpdateTemplateEntries[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorUpdateTemplateEntries[2].offset = sizeof(VkDescriptorImageInfo) + sizeof(VkDescriptorBufferInfo);
    descriptorUpdateTemplateEntries[2].stride = sizeof(VkDescriptorBufferInfo);

    VkDescriptorUpdateTemplateCreateInfoKHR descriptorUpdateTemplateCreateInfo;
    descriptorUpdateTemplateCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR;
    descriptorUpdateTemplateCreateInfo.pNext = 0;
    descriptorUpdateTemplateCreateInfo.flags = 0;
    descriptorUpdateTemplateCreateInfo.descriptorUpdateEntryCount = 3;
    descriptorUpdateTemplateCreateInfo.pDescriptorUpdateEntries = descriptorUpdateTemplateEntries;
    if (vkdev->info.support_VK_KHR_push_descriptor)
    {
        descriptorUpdateTemplateCreateInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR;
    }
    else
    {
        descriptorUpdateTemplateCreateInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET_KHR;
    }
    // descriptorSetLayout should be ignored if VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR
    // FIXME HACK WARNING TODO NOTE but crash on radv if set NULL  :(
    descriptorUpdateTemplateCreateInfo.descriptorSetLayout = descriptorset_layout;
    descriptorUpdateTemplateCreateInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    descriptorUpdateTemplateCreateInfo.pipelineLayout = pipeline_layout;
    descriptorUpdateTemplateCreateInfo.set = 0;

    VkResult ret = vkdev->vkCreateDescriptorUpdateTemplateKHR(vkdev->vkdevice(), &descriptorUpdateTemplateCreateInfo, 0, &descriptor_update_template);
    if (ret != VK_SUCCESS)
    {
        NCNN_LOGE("vkCreateDescriptorUpdateTemplateKHR failed %d", ret);
        return -1;
    }

    return 0;
}
#endif // __ANDROID_API__ >= 26
#endif // NCNN_VULKAN

} // namespace ncnn
