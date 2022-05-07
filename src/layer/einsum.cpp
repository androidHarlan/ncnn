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

#include "einsum.h"

namespace ncnn {

Einsum::Einsum()
{
    one_blob_only = false;
    support_inplace = false;
}

int Einsum::load_param(const ParamDict& pd)
{
    Mat equation_mat = pd.get(0, Mat());

    const int equation_len = equation_mat.w;

    // restore to lexical equation string
    std::string equation;
    {
        equation.resize(equation_len);
        const int* p = equation_mat;
        for (int i = 0; i < equation_len; i++)
        {
            equation[i] = p[i];
        }
    }

    // split into tokens
    size_t arrow = equation.find("->");
    std::string lhs = equation.substr(0, arrow);
    std::string rhs = equation.substr(arrow + 2);

    {
        size_t start = 0;
        size_t comma = lhs.find(",");
        while (comma != std::string::npos)
        {
            std::string token = lhs.substr(start, comma - start);
            lhs_tokens.push_back(token);

            start = comma + 1;
            comma = lhs.find(",", start);
        }

        std::string token = lhs.substr(start);
        lhs_tokens.push_back(token);
    }

    rhs_token = rhs;

    // check token always in ijkl
    {
        for (size_t i = 0; i < rhs_token.size(); i++)
        {
            if (rhs_token[i] < 'i' || rhs_token[i] > 'l')
            {
                NCNN_LOGE("invalid rhs_token %s", rhs_token.c_str());
                return -1;
            }
        }

        NCNN_LOGE("rhs_token = %s", rhs_token.c_str());

        for (size_t i = 0; i < lhs_tokens.size(); i++)
        {
            const std::string& lhs_token = lhs_tokens[i];
            for (size_t j = 0; j < lhs_token.size(); j++)
            {
                if (lhs_token[j] < 'i' || lhs_token[j] > 'l')
                {
                    NCNN_LOGE("invalid lhs_token %s", lhs_token.c_str());
                    return -1;
                }
            }

            NCNN_LOGE("lhs_token = %s", lhs_token.c_str());
        }
    }

    return 0;
}

static float get_indexed_value(const Mat& m, const std::string& token, std::vector<int>& indexes)
{
    const int dims = m.dims;

    if (dims == 1)
    {
        int x = indexes[token[0] - 'i'];
        return m[x];
    }

    if (dims == 2)
    {
        int y = indexes[token[0] - 'i'];
        int x = indexes[token[1] - 'i'];
        return m.row(y)[x];
    }

    if (dims == 3)
    {
        int c = indexes[token[0] - 'i'];
        int y = indexes[token[1] - 'i'];
        int x = indexes[token[2] - 'i'];
        return m.channel(c).row(y)[x];
    }

    if (dims == 4)
    {
        int c = indexes[token[0] - 'i'];
        int z = indexes[token[1] - 'i'];
        int y = indexes[token[2] - 'i'];
        int x = indexes[token[3] - 'i'];
        return m.channel(c).depth(z).row(y)[x];
    }

    // should never reach here
    return 0;
}

int Einsum::forward(const std::vector<Mat>& bottom_blobs, std::vector<Mat>& top_blobs, const Option& opt) const
{
    // assert bottom_blobs.size() == lhs_tokens.size()
    // assert top_blobs.size() == 1

    size_t elemsize = bottom_blobs[0].elemsize;

    // resolve dimension sizes
    std::vector<int> dim_sizes(4, 1); // map ijkl -> dim_size

    for (size_t b = 0; b < bottom_blobs.size(); b++)
    {
        const std::string& lhs_token = lhs_tokens[b];
        const Mat& bottom_blob = bottom_blobs[b];
        const int in_dims = bottom_blob.dims;

        for (int s = 0; s < in_dims; s++)
        {
            int dim_size = 1;
            if (in_dims == 1) dim_size = bottom_blob.w;
            if (in_dims == 2 && s == 0) dim_size = bottom_blob.h;
            if (in_dims == 2 && s == 1) dim_size = bottom_blob.w;
            if (in_dims == 3 && s == 0) dim_size = bottom_blob.c;
            if (in_dims == 3 && s == 1) dim_size = bottom_blob.h;
            if (in_dims == 3 && s == 2) dim_size = bottom_blob.w;
            if (in_dims == 4 && s == 0) dim_size = bottom_blob.c;
            if (in_dims == 4 && s == 1) dim_size = bottom_blob.d;
            if (in_dims == 4 && s == 2) dim_size = bottom_blob.h;
            if (in_dims == 4 && s == 3) dim_size = bottom_blob.w;

            int dim_sizes_index = lhs_token[s] - 'i';
            dim_sizes[dim_sizes_index] = dim_size;
        }
    }

    NCNN_LOGE("dim_sizes = %d %d %d %d", dim_sizes[0], dim_sizes[1], dim_sizes[2], dim_sizes[3]);

    const int out_dims = (int)rhs_token.size();

    if (out_dims == 1)
    {
        Mat& top_blob = top_blobs[0];
        top_blob.create(dim_sizes[0], elemsize, opt.blob_allocator);
        if (top_blob.empty())
            return -100;

        std::vector<int> indexes(4);

        for (int i = 0; i < top_blob.w; i++)
        {
            indexes[0] = i;

            float sum = 0.f;

            for (int j = 0; j < dim_sizes[1]; j++)
            {
                indexes[1] = j;

                for (int k = 0; k < dim_sizes[2]; k++)
                {
                    indexes[2] = k;

                    for (int l = 0; l < dim_sizes[3]; l++)
                    {
                        indexes[3] = l;

                        float v = 1.f;
                        for (size_t b = 0; b < bottom_blobs.size(); b++)
                        {
                            v *= get_indexed_value(bottom_blobs[b], lhs_tokens[b], indexes);
                        }

                        sum += v;
                    }
                }
            }

            top_blob[i] = sum;
        }
    }

    if (out_dims == 2)
    {
        Mat& top_blob = top_blobs[0];
        top_blob.create(dim_sizes[1], dim_sizes[0], elemsize, opt.blob_allocator);
        if (top_blob.empty())
            return -100;

        std::vector<int> indexes(4);

        for (int i = 0; i < top_blob.h; i++)
        {
            indexes[0] = i;

            for (int j = 0; j < top_blob.w; j++)
            {
                indexes[1] = j;

                float sum = 0.f;

                for (int k = 0; k < dim_sizes[2]; k++)
                {
                    indexes[2] = k;

                    for (int l = 0; l < dim_sizes[3]; l++)
                    {
                        indexes[3] = l;

                        float v = 1.f;
                        for (size_t b = 0; b < bottom_blobs.size(); b++)
                        {
                            v *= get_indexed_value(bottom_blobs[b], lhs_tokens[b], indexes);
                        }

                        sum += v;
                    }
                }

                top_blob.row(i)[j] = sum;
            }
        }
    }

    if (out_dims == 3)
    {
        Mat& top_blob = top_blobs[0];
        top_blob.create(dim_sizes[2], dim_sizes[1], dim_sizes[0], elemsize, opt.blob_allocator);
        if (top_blob.empty())
            return -100;

        std::vector<int> indexes(4);

        for (int i = 0; i < top_blob.c; i++)
        {
            indexes[0] = i;

            for (int j = 0; j < top_blob.h; j++)
            {
                indexes[1] = j;

                for (int k = 0; k < top_blob.w; k++)
                {
                    indexes[2] = k;

                    float sum = 0.f;

                    for (int l = 0; l < dim_sizes[3]; l++)
                    {
                        indexes[3] = l;

                        float v = 1.f;
                        for (size_t b = 0; b < bottom_blobs.size(); b++)
                        {
                            v *= get_indexed_value(bottom_blobs[b], lhs_tokens[b], indexes);
                        }

                        sum += v;
                    }

                    top_blob.channel(i).row(j)[k] = sum;
                }
            }
        }
    }

    if (out_dims == 4)
    {
        Mat& top_blob = top_blobs[0];
        top_blob.create(dim_sizes[3], dim_sizes[2], dim_sizes[1], dim_sizes[0], elemsize, opt.blob_allocator);
        if (top_blob.empty())
            return -100;

        std::vector<int> indexes(4);

        for (int i = 0; i < top_blob.c; i++)
        {
            indexes[0] = i;

            for (int j = 0; j < top_blob.d; j++)
            {
                indexes[1] = j;

                for (int k = 0; k < top_blob.h; k++)
                {
                    indexes[2] = k;

                    for (int l = 0; l < top_blob.w; l++)
                    {
                        indexes[3] = l;

                        float sum = 0.f;

                        {
                            float v = 1.f;
                            for (size_t b = 0; b < bottom_blobs.size(); b++)
                            {
                                v *= get_indexed_value(bottom_blobs[b], lhs_tokens[b], indexes);
                            }

                            sum += v;
                        }

                        top_blob.channel(i).depth(j).row(k)[l] = sum;
                    }
                }
            }
        }
    }

    return 0;
}

} // namespace ncnn
