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

#if __ANDROID_API__ >= 26
#include <android/hardware_buffer.h>
#endif // __ANDROID_API__ >= 26

namespace ncnn {

#if NCNN_VULKAN
class PipelinePrivate
{
public:
    VkShaderModule shader_module;
    VkDescriptorSetLayout descriptorset_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkDescriptorUpdateTemplateKHR descriptor_update_template;

    ShaderInfo shader_info;

    uint32_t local_size_x;
    uint32_t local_size_y;
    uint32_t local_size_z;
};

Pipeline::Pipeline(const VulkanDevice* _vkdev)
    : vkdev(_vkdev), d(new PipelinePrivate)
{
    d->shader_module = 0;
    d->descriptorset_layout = 0;
    d->pipeline_layout = 0;
    d->pipeline = 0;
    d->descriptor_update_template = 0;

    d->local_size_x = 1;
    d->local_size_y = 1;
    d->local_size_z = 1;
}

Pipeline::~Pipeline()
{
    delete d;
}

Pipeline::Pipeline(const Pipeline&)
    : d(0)
{
}

Pipeline& Pipeline::operator=(const Pipeline&)
{
    return *this;
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

    w = std::min(w, (int)vkdev->info.max_workgroup_size_x());
    h = std::min(h, (int)vkdev->info.max_workgroup_size_y());
    c = std::min(c, (int)vkdev->info.max_workgroup_size_z());

    if (w * h * c <= (int)vkdev->info.max_workgroup_invocations())
    {
        return set_local_size_xyz(w, h, c);
    }

    int max_local_size_xy = (int)vkdev->info.max_workgroup_invocations() / c;

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
    int local_size = w * h * c;

    // be multiple of subgroup size
    int subgroup_size = vkdev->info.subgroup_size();
    int local_size2 = std::max(1, local_size / subgroup_size) * subgroup_size;
    for (; local_size > local_size2; local_size = w * h * c)
    {
        if (local_size == local_size2 * 2)
        {
            if (c % 2 == 0)
                c /= 2;
            else if (h % 2 == 0)
                w /= 2;
            else if (w % 2 == 0)
                h /= 2;
            else
                c /= 2;
        }
        else if (local_size == local_size2 * 4)
        {
            if (w % 2 == 0 && h % 2 == 0)
            {
                w /= 2;
                h /= 2;
            }
            else if (h % 2 == 0 && c % 2 == 0)
            {
                h /= 2;
                c /= 2;
            }
            else if (w % 2 == 0 && c % 2 == 0)
            {
                w /= 2;
                c /= 2;
            }
            else if (c % 4 == 0)
            {
                c /= 4;
            }
            else if (h % 4 == 0)
            {
                h /= 4;
            }
            else if (w % 4 == 0)
            {
                w /= 4;
            }
            else if (c % 2 == 0)
            {
                h /= 2;
                c /= 2;
            }
            else if (h % 2 == 0)
            {
                w /= 2;
                h /= 2;
            }
            else if (w % 2 == 0)
            {
                w /= 2;
                h /= 2;
            }
            else
            {
                w /= 2;
                h /= 2;
            }
        }
        else
        {
            if (w % 2 != 0 && h % 2 != 0 && c % 2 != 0)
            {
                w /= 2;
                h /= 2;
                c /= 2;
            }
            else
            {
                if (c % 2 == 0)
                    c /= 2;
                if (h % 2 == 0)
                    h /= 2;
                if (w % 2 == 0)
                    w /= 2;
            }
        }

        w = std::max(1, w);
        h = std::max(1, h);
        c = std::max(1, c);
    }

    d->local_size_x = w;
    d->local_size_y = h;
    d->local_size_z = c;

    //     NCNN_LOGE("local size = %d %d %d", local_size_x, local_size_y, local_size_z);
}

int Pipeline::create(const uint32_t* spv_data, size_t spv_data_size, const std::vector<vk_specialization_type>& specializations)
{
    const PipelineCache* pipeline_cache = vkdev->get_pipeline_cache();

    // get from pipeline cache
    return pipeline_cache->get_pipeline(spv_data, spv_data_size, specializations, d->local_size_x, d->local_size_y, d->local_size_z,
                                        &d->shader_module, &d->descriptorset_layout, &d->pipeline_layout, &d->pipeline, &d->descriptor_update_template,
                                        d->shader_info);
}

int Pipeline::create(int shader_type_index, const Option& opt, const std::vector<vk_specialization_type>& specializations)
{
    const PipelineCache* pipeline_cache = opt.pipeline_cache ? opt.pipeline_cache : vkdev->get_pipeline_cache();

    // get from pipeline cache
    return pipeline_cache->get_pipeline(shader_type_index, opt, specializations, d->local_size_x, d->local_size_y, d->local_size_z,
                                        &d->shader_module, &d->descriptorset_layout, &d->pipeline_layout, &d->pipeline, &d->descriptor_update_template,
                                        d->shader_info);
}

VkShaderModule Pipeline::shader_module() const
{
    return d->shader_module;
}

VkDescriptorSetLayout Pipeline::descriptorset_layout() const
{
    return d->descriptorset_layout;
}

VkPipelineLayout Pipeline::pipeline_layout() const
{
    return d->pipeline_layout;
}

VkPipeline Pipeline::pipeline() const
{
    return d->pipeline;
}

VkDescriptorUpdateTemplateKHR Pipeline::descriptor_update_template() const
{
    return d->descriptor_update_template;
}

const ShaderInfo& Pipeline::shader_info() const
{
    return d->shader_info;
}

uint32_t Pipeline::local_size_x() const
{
    return d->local_size_x;
}

uint32_t Pipeline::local_size_y() const
{
    return d->local_size_y;
}

uint32_t Pipeline::local_size_z() const
{
    return d->local_size_z;
}

void Pipeline::set_shader_module(VkShaderModule shader_module)
{
    d->shader_module = shader_module;
}

void Pipeline::set_descriptorset_layout(VkDescriptorSetLayout descriptorset_layout)
{
    d->descriptorset_layout = descriptorset_layout;
}

void Pipeline::set_pipeline_layout(VkPipelineLayout pipeline_layout)
{
    d->pipeline_layout = pipeline_layout;
}

void Pipeline::set_pipeline(VkPipeline pipeline)
{
    d->pipeline = pipeline;
}

void Pipeline::set_descriptor_update_template(VkDescriptorUpdateTemplateKHR descriptor_update_template)
{
    d->descriptor_update_template = descriptor_update_template;
}

void Pipeline::set_shader_info(const ShaderInfo& shader_info)
{
    d->shader_info = shader_info;
}

#if NCNN_PLATFORM_API
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
    //         out_elemsize = vkdev->info.support_fp16_storage() && opt.use_fp16_storage ? 2u : 4u;
    //         out_elempack = 1;
    //     }
    //     else if (type_to == 3)
    //     {
    //         outc = 1;
    //         out_elemsize = vkdev->info.support_fp16_storage() && opt.use_fp16_storage ? 2u : 4u;
    //         out_elempack = 1;
    //     }
    //     else // if (type_to == 4 || type_to == 5)
    //     {
    //         outc = 1;
    //         out_elemsize = ((vkdev->info.support_fp16_packed() && opt.use_fp16_packed) || (vkdev->info.support_fp16_storage() && opt.use_fp16_storage)) ? 8u : 16u;
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

    create_shader_module(opt);

    const ShaderInfo& _shader_info = shader_info();

    if ((int)specializations.size() != _shader_info.specialization_count)
    {
        NCNN_LOGE("pipeline convert_ycbcr specialization count mismatch, expect %d but got %d", _shader_info.specialization_count, (int)specializations.size());
        return -1;
    }

    create_sampler(ahb_im_allocator);

    create_descriptorset_layout();

    VkPipelineLayout pipeline_layout = 0;
    VkPipeline pipeline = 0;
    VkDescriptorUpdateTemplateKHR descriptor_update_template = 0;

    vkdev->create_pipeline_layout(_shader_info.push_constant_count, descriptorset_layout(), &pipeline_layout);

    vkdev->create_pipeline(shader_module(), pipeline_layout, specializations, &pipeline);

    if (vkdev->info.support_VK_KHR_descriptor_update_template())
    {
        vkdev->create_descriptor_update_template(_shader_info.binding_count, _shader_info.binding_types, descriptorset_layout(), pipeline_layout, &descriptor_update_template);
    }

    set_pipeline_layout(pipeline_layout);
    set_pipeline(pipeline);
    set_descriptor_update_template(descriptor_update_template);

    return 0;
}

void ImportAndroidHardwareBufferPipeline::destroy()
{
    if (sampler)
    {
        vkDestroySampler(vkdev->vkdevice(), sampler, 0);
        sampler = 0;
    }
}

int ImportAndroidHardwareBufferPipeline::create_shader_module(const Option& opt)
{
    int shader_type_index = LayerShaderType::convert_ycbcr;

    std::vector<uint32_t> spirv;
    int retc = compile_spirv_module(shader_type_index, opt, spirv);
    if (retc != 0)
    {
        NCNN_LOGE("compile_spirv_module failed %d", retc);
        return -1;
    }

    const uint32_t* spv_data = spirv.data();
    size_t spv_data_size = spirv.size() * 4;

    ShaderInfo shader_info;
    int ret = resolve_shader_info(spv_data, spv_data_size, shader_info);
    if (ret != 0)
    {
        NCNN_LOGE("resolve_shader_info failed %d", ret);
        return -1;
    }

    set_shader_info(shader_info);

    VkShaderModule shader_module = vkdev->compile_shader_module(spv_data, spv_data_size, local_size_x(), local_size_y(), local_size_z());
    set_shader_module(shader_module);

    return 0;
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

    if (vkdev->info.support_VK_KHR_push_descriptor())
    {
        descriptorSetLayoutCreateInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    }

    VkDescriptorSetLayout descriptorset_layout = 0;
    VkResult ret = vkCreateDescriptorSetLayout(vkdev->vkdevice(), &descriptorSetLayoutCreateInfo, 0, &descriptorset_layout);
    if (ret != VK_SUCCESS)
    {
        NCNN_LOGE("vkCreateDescriptorSetLayout failed %d", ret);
        return -1;
    }

    set_descriptorset_layout(descriptorset_layout);

    return 0;
}
#endif // __ANDROID_API__ >= 26
#endif // NCNN_PLATFORM_API

#endif // NCNN_VULKAN

} // namespace ncnn
