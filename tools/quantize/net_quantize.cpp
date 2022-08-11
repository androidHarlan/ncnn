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

#include "layer.h"
#include "layer_type.h"
#include "net.h"
#include "net_quantize.h"
#include <map>
#include <set>
#include <vector>

void NetQuantize::set_weight_suffix(std::string str)
{
    suffix = str;
}

bool NetQuantize::read_txt_format(const char* filepath)
{
    blob_int8scale_table.clear();
    weight_int8scale_table.clear();

    FILE* fp = fopen(filepath, "rb");
    if (!fp)
    {
        fprintf(stderr, "Open %s failed.\n", filepath);
        return false;
    }

    std::string key_str;
    std::vector<float> scales;

    std::vector<char> line(10240000);
    char* pch = NULL;
    size_t len = 0;

    while (!feof(fp))
    {
        char* s = fgets(line.data(), (int)line.size(), fp);
        if (!s)
            break;

        float scale = 1.f;
        char key[256];
        line[strcspn(line.data(), "\r\n")] = 0;

        pch = strtok(line.data(), " ");

        if (pch == NULL) break;

        bool is_key = true;
        while (pch != NULL)
        {
            if (is_key)
            {
                sscanf(pch, "%255s", key);

                key_str = key;
                is_key = false;
            }
            else
            {
                sscanf(pch, "%f", &scale);

                scales.push_back(scale);
            }

            pch = strtok(NULL, " ");
        }

        // XYZ_param_N pattern
        if (strstr(key_str.c_str(), "_param_"))
        {
            weight_int8scale_table[key_str] = ncnn::Mat((int)scales.size(), (void*)scales.data()).clone();
        }
        else
        {
            blob_int8scale_table[key_str] = ncnn::Mat((int)scales.size(), (void*)scales.data()).clone();
        }
        key_str.clear();
        scales.clear();
    }

    fclose(fp);

    return true;
}

bool NetQuantize::read_ini_format(const char* path)
{
    blob_int8scale_table.clear();
    weight_int8scale_table.clear();
    mha_table.clear();

    ini::Config root;
    root.read(std::string(path));

    size_t len = root.size();
    std::string name, type;
    std::shared_ptr<ini::Table> ptable;
    for (size_t i = 0; i < len; ++i)
    {
        std::tie(name, ptable) = root[i];
        type = ptable->get<std::string>("type");

        if (type == "Conv" || type == "Gemm")
        {
            // load weight scales
            {
                std::vector<float> scales = ptable->get_list<float>("weight");
                weight_int8scale_table[name] = ncnn::Mat((int)scales.size(), (void*)scales.data()).clone();
            }

            // load input scale
            {
                std::vector<float> scales = {ptable->get<float>("input_scale")};
                blob_int8scale_table[name] = ncnn::Mat((int)scales.size(), (void*)scales.data()).clone();
            }
        }
        else if (type == "MultiHeadAttention")
        {
            mha_table[name] = ptable;
        }
        else if (type == "LayerNorm")
        {
            layernorm_table[name] = ptable;
        }
        else if (type == "Add")
        {
            binaryop_table[name] = ptable;
        }
    }

    return true;
}

int NetQuantize::quantize_mha()
{
    const int layer_count = static_cast<int>(layers.size());
    auto base_opt = opt;

    for (int i = 0; i < layer_count; i++)
    {
        // find mha layer
        if (layers[i]->type != "MultiHeadAttention")
            continue;

        std::string name = layers[i]->name;
        if (mha_table.find(name) == mha_table.end())
        {
            fprintf(stderr, "cannot find %s quant param.\n", name.c_str());
            continue;
        }

        ncnn::MultiHeadAttention* mha = (ncnn::MultiHeadAttention*)layers[i];
        fprintf(stderr, "quantize_multiheadattention %s\n", mha->name.c_str());

        auto& table = mha_table.at(name);
        {
            // write weights
            // convert fp32 mat to int8 mat with the scales
            auto convert = [table, mha, base_opt](ncnn::Mat& weight, std::string key, ncnn::Mat& w_scales) -> int {
                ncnn::Option opt_q = base_opt;
                opt_q.blob_allocator = weight.allocator;
                opt_q.use_packing_layout = false;

                auto scales = table->get_list<float>(key);
                if (scales.empty())
                {
                    return -100;
                }
                w_scales = ncnn::Mat((int)scales.size(), (void*)scales.data()).clone();

                {
                    ncnn::Mat weight_int8;
                    const int num_input = mha->embed_dim;
                    const int num_output = mha->weight_data_size / num_input;

                    ncnn::Mat weight_data_r2 = weight.reshape(num_input, num_output);
                    ncnn::quantize_to_int8(weight_data_r2, weight_int8, w_scales, opt_q);
                    if (weight_int8.empty())
                        return -100;

                    weight = weight_int8.reshape(mha->weight_data_size).clone();
                }
                return 0;
            };

            int success = 0;
            success += convert(mha->q_weight_data, "weight_q", mha->q_weight_scales);
            success += convert(mha->k_weight_data, "weight_k", mha->k_weight_scales);
            success += convert(mha->v_weight_data, "weight_v", mha->v_weight_scales);
            success += convert(mha->out_weight_data, "weight_o", mha->o_weight_scales);

            if (success != 0)
            {
                fprintf(stderr, "convert fp32 weight to int8 failed. \n");
                return -1;
            }
        }

        {
            // write input scale
            auto convert = [table, base_opt](const std::string key, ncnn::Mat& mat) -> int {
                std::vector<float> scales = {table->get<float>(key)};
                if (scales.empty())
                {
                    return -100;
                }

                mat = ncnn::Mat((int)scales.size(), (void*)scales.data()).clone();
                return 0;
            };

            int success = 0;
            success += convert("input_scale_q", mha->q_input_scale);
            success += convert("input_scale_k", mha->k_input_scale);
            success += convert("input_scale_v", mha->v_input_scale);
            if (success != 0)
            {
                fprintf(stderr, "load input scale failed. \n");
                return -100;
            }
        }

        {
            // write internal scales
            std::vector<float> internal_scales;
            internal_scales.emplace_back(table->get<float>("internal_scale_q"));
            internal_scales.emplace_back(table->get<float>("internal_scale_k"));
            internal_scales.emplace_back(table->get<float>("internal_scale_v"));
            internal_scales.emplace_back(table->get<float>("internal_scale_energy"));
            internal_scales.emplace_back(table->get<float>("internal_scale_feat"));

            mha->internal_scales = ncnn::Mat((int)internal_scales.size(), (void*)internal_scales.data()).clone();
        }

        {
            // write control variable
            mha->int8_scale_term = 1;
        }
    }

    return 0;
}

int NetQuantize::quantize_convolution()
{
    const int layer_count = static_cast<int>(layers.size());
    for (int i = 0; i < layer_count; i++)
    {
        // find convolution layer
        if (layers[i]->type != "Convolution")
            continue;

        // find convolution layer
        std::map<std::string, ncnn::Mat>::iterator iter_data = blob_int8scale_table.find(layers[i]->name);
        if (iter_data == blob_int8scale_table.end())
            continue;

        char key[256];
        sprintf(key, "%s%s", layers[i]->name.c_str(), suffix.c_str());

        std::map<std::string, ncnn::Mat>::iterator iter = weight_int8scale_table.find(key);
        if (iter == weight_int8scale_table.end())
        {
            fprintf(stderr, "%s need to be quantized, but no scale param!\n", key);
            return -1;
        }

        // Convolution - quantize weight from fp32 to int8
        ncnn::Convolution* convolution = (ncnn::Convolution*)layers[i];

        ncnn::Mat bottom_blob_int8_scales = iter_data->second;
        ncnn::Mat weight_data_int8_scales = iter->second;

        fprintf(stderr, "quantize_convolution %s\n", convolution->name.c_str());

        {
            const int maxk = convolution->kernel_w * convolution->kernel_h;
            const int num_input = convolution->weight_data_size / convolution->num_output / maxk;

            ncnn::Mat weight_data_r2 = convolution->weight_data.reshape(maxk, num_input, convolution->num_output);

            ncnn::Mat weight_data_int8;

            ncnn::Option opt_q = opt;
            opt_q.blob_allocator = convolution->weight_data.allocator;
            opt_q.use_packing_layout = false;
            ncnn::quantize_to_int8(weight_data_r2, weight_data_int8, weight_data_int8_scales, opt_q);
            if (weight_data_int8.empty())
                return -100;

            convolution->weight_data = weight_data_int8.reshape(convolution->weight_data_size);
        }

        convolution->int8_scale_term = 2;
        convolution->weight_data_int8_scales = weight_data_int8_scales;
        convolution->bottom_blob_int8_scales = bottom_blob_int8_scales;
    }

    return 0;
}

int NetQuantize::quantize_convolutiondepthwise()
{
    const int layer_count = static_cast<int>(layers.size());
    for (int i = 0; i < layer_count; i++)
    {
        // find convolution layer
        if (layers[i]->type != "ConvolutionDepthWise")
            continue;

        // find convolutiondepthwise layer
        std::map<std::string, ncnn::Mat>::iterator iter_data = blob_int8scale_table.find(layers[i]->name);
        if (iter_data == blob_int8scale_table.end())
            continue;

        char key[256];
        sprintf(key, "%s%s", layers[i]->name.c_str(), suffix.c_str());

        std::map<std::string, ncnn::Mat>::iterator iter = weight_int8scale_table.find(key);
        if (iter == weight_int8scale_table.end())
        {
            fprintf(stderr, "this layer need to be quantized, but no scale param!\n");
            return -1;
        }

        // Convolution - quantize weight from fp32 to int8
        ncnn::ConvolutionDepthWise* convdw = (ncnn::ConvolutionDepthWise*)layers[i];

        ncnn::Mat bottom_blob_int8_scales = iter_data->second;
        ncnn::Mat weight_data_int8_scales = iter->second;

        fprintf(stderr, "quantize_convolutiondepthwise %s\n", convdw->name.c_str());

        {
            ncnn::Mat int8_weight_data(convdw->weight_data_size, (size_t)1u);
            if (int8_weight_data.empty())
                return -100;

            const int weight_data_size_g = convdw->weight_data_size / convdw->group;

            for (int g = 0; g < convdw->group; g++)
            {
                ncnn::Option opt_q = opt;
                opt_q.blob_allocator = int8_weight_data.allocator;
                opt_q.use_packing_layout = false;

                const ncnn::Mat weight_data_g = convdw->weight_data.range(weight_data_size_g * g, weight_data_size_g);
                ncnn::Mat int8_weight_data_g = int8_weight_data.range(weight_data_size_g * g, weight_data_size_g);
                const ncnn::Mat weight_data_int8_scales_g = weight_data_int8_scales.range(g, 1);
                ncnn::quantize_to_int8(weight_data_g, int8_weight_data_g, weight_data_int8_scales_g, opt_q);
            }

            convdw->weight_data = int8_weight_data;
        }

        convdw->int8_scale_term = 1;
        convdw->weight_data_int8_scales = weight_data_int8_scales;
        convdw->bottom_blob_int8_scales = bottom_blob_int8_scales;
    }

    return 0;
}

int NetQuantize::quantize_innerproduct()
{
    const int layer_count = static_cast<int>(layers.size());
    for (int i = 0; i < layer_count; i++)
    {
        // find convolution layer
        if (layers[i]->type != "InnerProduct")
            continue;

        // find InnerProduct layer
        std::map<std::string, ncnn::Mat>::iterator iter_data = blob_int8scale_table.find(layers[i]->name);
        if (iter_data == blob_int8scale_table.end())
            continue;

        char key[256];
        sprintf(key, "%s%s", layers[i]->name.c_str(), suffix.c_str());

        std::map<std::string, ncnn::Mat>::iterator iter = weight_int8scale_table.find(key);
        if (iter == weight_int8scale_table.end())
        {
            fprintf(stderr, "this layer need to be quantized, but no scale param!\n");
            return -1;
        }

        // InnerProduct - quantize weight from fp32 to int8
        ncnn::InnerProduct* fc = (ncnn::InnerProduct*)layers[i];

        ncnn::Mat bottom_blob_int8_scales = iter_data->second;
        ncnn::Mat weight_data_int8_scales = iter->second;

        fprintf(stderr, "quantize_innerproduct %s\n", fc->name.c_str());

        {
            const int num_input = fc->weight_data_size / fc->num_output;

            ncnn::Mat weight_data_r2 = fc->weight_data.reshape(num_input, fc->num_output);

            ncnn::Mat weight_data_int8;
            ncnn::Option opt_q = opt;
            opt_q.use_packing_layout = false;
            ncnn::quantize_to_int8(weight_data_r2, weight_data_int8, weight_data_int8_scales, opt_q);
            if (weight_data_int8.empty())
                return -100;

            fc->weight_data = weight_data_int8.reshape(fc->weight_data_size);
        }

        fc->int8_scale_term = 2;
        fc->weight_data_int8_scales = weight_data_int8_scales;
        fc->bottom_blob_int8_scales = bottom_blob_int8_scales;
    }

    return 0;
}

int NetQuantize::quantize_layernorm()
{
    const int layer_count = static_cast<int>(layers.size());
    auto base_opt = opt;

    for (int i = 0; i < layer_count; i++)
    {
        // find layernorm layer
        if (layers[i]->type != "LayerNorm")
            continue;

        std::string name = layers[i]->name;
        if (layernorm_table.find(name) == layernorm_table.end())
        {
            fprintf(stderr, "cannot find %s quant param.\n", name.c_str());
            continue;
        }

        ncnn::LayerNorm* ln = (ncnn::LayerNorm*)layers[i];
        fprintf(stderr, "quantize_layernorm %s\n", ln->name.c_str());

        auto& table = layernorm_table.at(name);
        {
            std::vector<float> scales = table->get_list<float>("input_scales");
            if (scales.empty())
            {
                fprintf(stderr, "quantize_layernorm input scales empty.\n");
                return -100;
            }
            ln->input_scales = ncnn::Mat((int)scales.size(), (void*)scales.data()).clone();

            scales = {table->get<float>("output_scale")};
            if (std::abs(scales[0]) <= 1e-6)
            {
                fprintf(stderr, "quantize_layernorm output scale unavailable.\n");
                return -100;
            }
            ln->output_scale = ncnn::Mat((int)scales.size(), (void*)scales.data()).clone();

            // write control variable
            ln->int8_scale_term = 1;
        }
    }

    return 0;
}

int NetQuantize::fuse_conv_requantize()
{
    const size_t layer_count = layers.size();
    for (size_t i = 0; i < layer_count; i++)
    {
        if (layers[i]->type != "Convolution" && layers[i]->type != "ConvolutionDepthWise")
            continue;

        // Convolution/ConvolutionDepthWise - Convolution/ConvolutionDepthWise
        int top_blob_index = layers[i]->tops[0];

        size_t j = i + 1;
        for (; j < layer_count; j++)
        {
            if (layers[j]->type != "Convolution" && layers[j]->type != "ConvolutionDepthWise")
                continue;

            if (layers[j]->bottoms.size() != 1)
                continue;

            if (layers[j]->bottoms[0] == top_blob_index)
                break;
        }

        if (j == layer_count)
            continue;

        // fuse requantize
        fprintf(stderr, "fuse_requantize %s %s\n", layers[i]->name.c_str(), layers[j]->name.c_str());

        if (layers[i]->type == "Convolution" && layers[j]->type == "Convolution")
        {
            ncnn::Convolution* convolution1 = (ncnn::Convolution*)layers[i];
            ncnn::Convolution* convolution2 = (ncnn::Convolution*)layers[j];

            if (convolution1->weight_data.elemsize != 1u || convolution2->weight_data.elemsize != 1u)
                continue;

            convolution1->int8_scale_term += 100;
            convolution1->top_blob_int8_scales = convolution2->bottom_blob_int8_scales;
        }
        if (layers[i]->type == "Convolution" && layers[j]->type == "ConvolutionDepthWise")
        {
            ncnn::Convolution* convolution1 = (ncnn::Convolution*)layers[i];
            ncnn::ConvolutionDepthWise* convolution2 = (ncnn::ConvolutionDepthWise*)layers[j];

            if (convolution1->weight_data.elemsize != 1u || convolution2->weight_data.elemsize != 1u)
                continue;

            convolution1->int8_scale_term += 100;
            convolution1->top_blob_int8_scales = convolution2->bottom_blob_int8_scales;
        }
        if (layers[i]->type == "ConvolutionDepthWise" && layers[j]->type == "Convolution")
        {
            ncnn::ConvolutionDepthWise* convolution1 = (ncnn::ConvolutionDepthWise*)layers[i];
            ncnn::Convolution* convolution2 = (ncnn::Convolution*)layers[j];

            if (convolution1->weight_data.elemsize != 1u || convolution2->weight_data.elemsize != 1u)
                continue;

            convolution1->int8_scale_term += 100;
            convolution1->top_blob_int8_scales = convolution2->bottom_blob_int8_scales;
        }
        if (layers[i]->type == "ConvolutionDepthWise" && layers[j]->type == "ConvolutionDepthWise")
        {
            ncnn::ConvolutionDepthWise* convolution1 = (ncnn::ConvolutionDepthWise*)layers[i];
            ncnn::ConvolutionDepthWise* convolution2 = (ncnn::ConvolutionDepthWise*)layers[j];

            if (convolution1->weight_data.elemsize != 1u || convolution2->weight_data.elemsize != 1u)
                continue;

            convolution1->int8_scale_term += 100;
            convolution1->top_blob_int8_scales = convolution2->bottom_blob_int8_scales;
        }
    }

    for (size_t i = 0; i < layer_count; i++)
    {
        if (layers[i]->type != "Convolution" && layers[i]->type != "ConvolutionDepthWise")
            continue;

        // Convolution/ConvolutionDepthWise - Split - Convolution/ConvolutionDepthWise
        int top_blob_index = layers[i]->tops[0];

        size_t j = i + 1;
        for (; j < layer_count; j++)
        {
            if (layers[j]->type != "Split")
                continue;

            if (layers[j]->bottoms.size() != 1)
                continue;

            if (layers[j]->bottoms[0] == top_blob_index)
                break;
        }

        if (j == layer_count)
            continue;

        ncnn::Split* split = (ncnn::Split*)layers[j];

        bool all_conv = true;
        for (size_t p = 0; p < split->tops.size(); p++)
        {
            int split_top_blob_index = split->tops[p];

            size_t k = j + 1;
            for (; k < layer_count; k++)
            {
                if (layers[k]->type != "Convolution" && layers[k]->type != "ConvolutionDepthWise")
                    continue;

                if (layers[k]->bottoms.size() != 1)
                    continue;

                if (layers[k]->bottoms[0] == split_top_blob_index)
                    break;
            }

            if (k == layer_count)
            {
                all_conv = false;
                break;
            }

            if (layers[k]->type == "Convolution")
            {
                ncnn::Convolution* convolution = (ncnn::Convolution*)layers[k];
                if (convolution->weight_data.elemsize != 1u)
                {
                    all_conv = false;
                    break;
                }
            }
            if (layers[k]->type == "ConvolutionDepthWise")
            {
                ncnn::ConvolutionDepthWise* convolution = (ncnn::ConvolutionDepthWise*)layers[k];
                if (convolution->weight_data.elemsize != 1u)
                {
                    all_conv = false;
                    break;
                }
            }
        }

        if (!all_conv)
            continue;

        j = blobs[split->tops[0]].consumer;

        // fuse requantize
        fprintf(stderr, "fuse_requantize %s %s\n", layers[i]->name.c_str(), split->name.c_str());

        if (layers[i]->type == "Convolution" && layers[j]->type == "Convolution")
        {
            ncnn::Convolution* convolution1 = (ncnn::Convolution*)layers[i];
            ncnn::Convolution* convolution2 = (ncnn::Convolution*)layers[j];

            if (convolution1->weight_data.elemsize != 1u || convolution2->weight_data.elemsize != 1u)
                continue;

            convolution1->int8_scale_term += 100;
            convolution1->top_blob_int8_scales = convolution2->bottom_blob_int8_scales;
        }
        if (layers[i]->type == "Convolution" && layers[j]->type == "ConvolutionDepthWise")
        {
            ncnn::Convolution* convolution1 = (ncnn::Convolution*)layers[i];
            ncnn::ConvolutionDepthWise* convolution2 = (ncnn::ConvolutionDepthWise*)layers[j];

            if (convolution1->weight_data.elemsize != 1u || convolution2->weight_data.elemsize != 1u)
                continue;

            convolution1->int8_scale_term += 100;
            convolution1->top_blob_int8_scales = convolution2->bottom_blob_int8_scales;
        }
        if (layers[i]->type == "ConvolutionDepthWise" && layers[j]->type == "Convolution")
        {
            ncnn::ConvolutionDepthWise* convolution1 = (ncnn::ConvolutionDepthWise*)layers[i];
            ncnn::Convolution* convolution2 = (ncnn::Convolution*)layers[j];

            if (convolution1->weight_data.elemsize != 1u || convolution2->weight_data.elemsize != 1u)
                continue;

            convolution1->int8_scale_term += 100;
            convolution1->top_blob_int8_scales = convolution2->bottom_blob_int8_scales;
        }
        if (layers[i]->type == "ConvolutionDepthWise" && layers[j]->type == "ConvolutionDepthWise")
        {
            ncnn::ConvolutionDepthWise* convolution1 = (ncnn::ConvolutionDepthWise*)layers[i];
            ncnn::ConvolutionDepthWise* convolution2 = (ncnn::ConvolutionDepthWise*)layers[j];

            if (convolution1->weight_data.elemsize != 1u || convolution2->weight_data.elemsize != 1u)
                continue;

            convolution1->int8_scale_term += 100;
            convolution1->top_blob_int8_scales = convolution2->bottom_blob_int8_scales;
        }
    }

    return 0;
}

/**
 * @brief if [LayerNorm --> X] and X is a type of quantizable layer, then requant layernorm.output, AKA X.input is quantized tensor
 * 
 * @return int
 */
int NetQuantize::fuse_layernorm_requantize()
{
    const size_t layer_count = layers.size();

    auto direct_connected_outputs = [&](ncnn::Layer* op, int cur) -> std::vector<ncnn::Layer*>
    {
        std::vector<ncnn::Layer*> outputs;
        for (size_t j = cur; j <layer_count; ++j)
        {
            ncnn::Layer* next = layers[j];
            for (auto index: next->bottoms) {
                if (index == op->tops[0] || index == op->tops[1])
                {
                    outputs.emplace_back(next);
                    break;
                }
            }
        }
        return outputs;
    };

    auto all_outputs =  [&](ncnn::Layer* op, int cur) -> std::vector<ncnn::Layer*>
    {
        auto directs = direct_connected_outputs(op, cur);
        std::vector<ncnn::Layer*> outputs;
        for (auto node: directs)
        {
            if (node->type == "Split")
            {
                auto nexts = direct_connected_outputs(node, cur);
                outputs.insert(outputs.end(), nexts.begin(), nexts.end());
                continue;
            }
            outputs.emplace_back(node);
        }
        return outputs;
    };

    auto is_quantized = [=](ncnn::Layer* layer) -> bool
    {
            if (layer->type == "Convolution")
                return ((ncnn::Convolution*)layer)->int8_scale_term > 0;

            if (layer->type == "MultiHeadAttention")
                return ((ncnn::MultiHeadAttention*)layer)->int8_scale_term > 0;

            if (layer->type == "InnerProduct")
                return ((ncnn::InnerProduct*)layer)->int8_scale_term > 0;
            
            if (layer->type == "ConvolutionDepthWise")
                return ((ncnn::ConvolutionDepthWise*)layer)->int8_scale_term > 0;

            if (layer->type == "LayerNorm")
            return ((ncnn::LayerNorm*)layer)->int8_scale_term > 0;

            if (layer->type == "BinaryOp")
                // suppose that future binaryop could be quantized
                return true;

            return false;
    };

    for (size_t i = 0; i < layer_count; i++)
    {
        if (layers[i]->type != "LayerNorm")
            continue;

        ncnn::LayerNorm* ln = (ncnn::LayerNorm*)layers[i];
        auto outputs = all_outputs(ln, i);
        bool all_support_quant = true;
        for (auto node: outputs)
        {
            if (! is_quantized(node))
            {
                all_support_quant = false;
                break;
            }
        }

        if (all_support_quant)
        {
            // fuse requantize

            // layernorm_int8 quantized by <input_scales, output_scale>, so do not need to update next node's output_scale.
            ln->int8_scale_term += 100;
            fprintf(stderr, "fuse_layernorm_requantize %s %s, int8_scale_term %d\n", layers[i]->name.c_str(), outputs[0]->name.c_str(), ln->int8_scale_term);
        }
    }

    return 0;
}
