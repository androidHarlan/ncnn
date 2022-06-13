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

#include "layer/multiheadattention.h"
#include "testutil.h"

static int test_multiheadattention(const ncnn::Mat& a, int num_heads)
{
    int embed_dim = a.w;

    ncnn::ParamDict pd;
    pd.set(0, embed_dim);
    pd.set(1, num_heads);
    pd.set(2, embed_dim * embed_dim);

    std::vector<ncnn::Mat> weights(8);
    weights[0] = RandomMat(embed_dim * embed_dim);
    weights[1] = RandomMat(embed_dim);
    weights[2] = RandomMat(embed_dim * embed_dim);
    weights[3] = RandomMat(embed_dim);
    weights[4] = RandomMat(embed_dim * embed_dim);
    weights[5] = RandomMat(embed_dim);
    weights[6] = RandomMat(embed_dim * embed_dim);
    weights[7] = RandomMat(embed_dim);

    std::vector<ncnn::Mat> as(3);
    as[0] = a;
    as[1] = a;
    as[2] = a;

    int ret = test_layer<ncnn::MultiHeadAttention>("MultiHeadAttention", pd, weights, as);
    if (ret != 0)
    {
        fprintf(stderr, "test_multiheadattention failed a=(%d %d)\n", a.w, a.h);
    }

    return ret;
}

static int test_multiheadattention_sameqkv(const ncnn::Mat& a, int num_heads)
{
    int embed_dim = a.w;

    ncnn::ParamDict pd;
    pd.set(0, embed_dim);
    pd.set(1, num_heads);
    pd.set(2, embed_dim * embed_dim);

    std::vector<ncnn::Mat> weights(8);
    weights[0] = RandomMat(embed_dim * embed_dim);
    weights[1] = RandomMat(embed_dim);
    weights[2] = RandomMat(embed_dim * embed_dim);
    weights[3] = RandomMat(embed_dim);
    weights[4] = RandomMat(embed_dim * embed_dim);
    weights[5] = RandomMat(embed_dim);
    weights[6] = RandomMat(embed_dim * embed_dim);
    weights[7] = RandomMat(embed_dim);

    std::vector<ncnn::Mat> as(1);
    as[0] = a;

    int ret = test_layer<ncnn::MultiHeadAttention>("MultiHeadAttention", pd, weights, as);
    if (ret != 0)
    {
        fprintf(stderr, "test_multiheadattention failed a=(%d %d)\n", a.w, a.h);
    }

    return ret;
}

static int test_multiheadattention_0()
{
    return 0
           || test_multiheadattention(RandomMat(64, 128), 4)
           || test_multiheadattention(RandomMat(64, 127), 16);
}

static int test_multiheadattention_1()
{
    return 0
           || test_multiheadattention_sameqkv(RandomMat(64, 128), 8)
           || test_multiheadattention_sameqkv(RandomMat(64, 127), 32);
}

#ifdef NCNN_INT8
static int test_multiheadattention_int8(const ncnn::Mat& a, int num_heads)
{
    int embed_dim = a.w;

    ncnn::ParamDict pd;
    pd.set(0, embed_dim);
    pd.set(1, num_heads);
    pd.set(2, embed_dim * embed_dim);
    pd.set(3, 1);

    std::vector<ncnn::Mat> weights(16);
    weights[0] = RandomIntMat(embed_dim * embed_dim);
    weights[1] = RandomIntMat(embed_dim);
    weights[2] = RandomIntMat(embed_dim * embed_dim);
    weights[3] = RandomIntMat(embed_dim);
    weights[4] = RandomIntMat(embed_dim * embed_dim);
    weights[5] = RandomIntMat(embed_dim);
    weights[6] = RandomIntMat(embed_dim * embed_dim);
    weights[7] = RandomIntMat(embed_dim);

    weights[8] = RandomMat(1);
    weights[9] = RandomMat(1);
    weights[10] = RandomMat(1);

    weights[11] = RandomMat(embed_dim);
    weights[12] = RandomMat(embed_dim);
    weights[13] = RandomMat(embed_dim);
    weights[14] = RandomMat(embed_dim);

    weights[15] = RandomMat(5);

    std::vector<ncnn::Mat> as(1);
    as[0] = a;

    int ret = test_layer<ncnn::MultiHeadAttention>("MultiHeadAttention", pd, weights, as);
    if (ret != 0)
    {
        fprintf(stderr, "test_multiheadattention failed a=(%d %d)\n", a.w, a.h);
    }

    return ret;
}

static int test_multiheadattention_2()
{
    return 0
           || test_multiheadattention_int8(RandomMat(64, 128), 8)
           || test_multiheadattention_int8(RandomMat(64, 127), 32);
}
#endif

int main()
{
    SRAND(7767517);
#ifdef NCNN_INT8
    return 0
           || test_multiheadattention_0()
           || test_multiheadattention_1()
           || test_multiheadattention_2();
#else
    return 0
           || test_multiheadattention_0()
           || test_multiheadattention_1();
#endif
}
