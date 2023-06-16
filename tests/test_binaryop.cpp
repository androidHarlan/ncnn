// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "layer/binaryop.h"
#include "testutil.h"

#define OP_TYPE_MAX 12

static int op_type = 0;

static int test_binaryop(const ncnn::Mat& _a, const ncnn::Mat& _b)
{
    ncnn::Mat a = _a;
    ncnn::Mat b = _b;
    if (op_type == 6 || op_type == 9)
    {
        // value must be positive for pow/rpow
        a = a.clone();
        b = b.clone();
        Randomize(a, 0.001f, 2.f);
        Randomize(b, 0.001f, 2.f);
    }
    if (op_type == 3 || op_type == 8)
    {
        // value must be positive for div/rdiv
        a = a.clone();
        b = b.clone();
        Randomize(a, 0.1f, 10.f);
        Randomize(b, 0.1f, 10.f);
    }

    ncnn::ParamDict pd;
    pd.set(0, op_type);
    pd.set(1, 0);   // with_scalar
    pd.set(2, 0.f); // b

    std::vector<ncnn::Mat> weights(0);

    std::vector<ncnn::Mat> ab(2);
    ab[0] = a;
    ab[1] = b;

    int ret = test_layer<ncnn::BinaryOp>("BinaryOp", pd, weights, ab);
    if (ret != 0)
    {
        fprintf(stderr, "test_binaryop failed a.dims=%d a=(%d %d %d %d) b.dims=%d b=(%d %d %d %d) op_type=%d\n", a.dims, a.w, a.h, a.d, a.c, b.dims, b.w, b.h, b.d, b.c, op_type);
    }

    return ret;
}

static int test_binaryop(const ncnn::Mat& _a, float b)
{
    ncnn::Mat a = _a;
    if (op_type == 6 || op_type == 9)
    {
        // value must be positive for pow
        Randomize(a, 0.001f, 2.f);
        b = RandomFloat(0.001f, 2.f);
    }
    if (op_type == 3 || op_type == 8)
    {
        // value must be positive for div/rdiv
        a = a.clone();
        Randomize(a, 0.1f, 10.f);
    }

    ncnn::ParamDict pd;
    pd.set(0, op_type);
    pd.set(1, 1); // with_scalar
    pd.set(2, b); // b

    std::vector<ncnn::Mat> weights(0);

    int ret = test_layer<ncnn::BinaryOp>("BinaryOp", pd, weights, a);
    if (ret != 0)
    {
        fprintf(stderr, "test_binaryop failed a.dims=%d a=(%d %d %d %d) b=%f op_type=%d\n", a.dims, a.w, a.h, a.d, a.c, b, op_type);
    }

    return ret;
}

static int test_binaryop_1()
{
    const int ws[] = {31, 28, 24, 32};

    for (int i = 0; i < 4; i++)
    {
        const int w = ws[i];

        ncnn::Mat b[2];
        for (int j = 0; j < 2; j++)
        {
            int bw = j % 2 == 0 ? w : 1;
            b[j] = RandomMat(bw);
        }

        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                int ret = test_binaryop(b[j], b[k]);
                if (ret != 0)
                    return ret;
            }

            int ret = test_binaryop(b[j], 0.2f);
            if (ret != 0)
                return ret;
        }
    }

    return 0;
}

static int test_binaryop_2()
{
    const int ws[] = {13, 14, 15, 16};
    const int hs[] = {31, 28, 24, 32};

    for (int i = 0; i < 4; i++)
    {
        const int w = ws[i];
        const int h = hs[i];

        ncnn::Mat b[4];
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                int bw = j % 2 == 0 ? w : 1;
                int bh = k % 2 == 0 ? h : 1;
                b[j * 2 + k] = RandomMat(bw, bh);
            }
        }

        for (int j = 0; j < 4; j++)
        {
            for (int k = 0; k < 4; k++)
            {
                int ret = test_binaryop(b[j], b[k]);
                if (ret != 0)
                    return ret;
            }

            int ret = test_binaryop(b[j], 0.2f);
            if (ret != 0)
                return ret;
        }
    }

    return 0;
}

static int test_binaryop_3()
{
    const int ws[] = {7, 6, 5, 4};
    const int hs[] = {3, 4, 5, 6};
    const int cs[] = {31, 28, 24, 32};

    for (int i = 0; i < 4; i++)
    {
        const int w = ws[i];
        const int h = hs[i];
        const int c = cs[i];

        ncnn::Mat b[8];
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                for (int l = 0; l < 2; l++)
                {
                    int bw = j % 2 == 0 ? w : 1;
                    int bh = k % 2 == 0 ? h : 1;
                    int bc = l % 2 == 0 ? c : 1;
                    b[j * 4 + k * 2 + l] = RandomMat(bw, bh, bc);
                }
            }
        }

        for (int j = 0; j < 8; j++)
        {
            for (int k = 0; k < 8; k++)
            {
                int ret = test_binaryop(b[j], b[k]);
                if (ret != 0)
                    return ret;
            }

            int ret = test_binaryop(b[j], 0.2f);
            if (ret != 0)
                return ret;
        }
    }

    return 0;
}

static int test_binaryop_4()
{
    const int ws[] = {2, 3, 4, 5};
    const int hs[] = {7, 6, 5, 4};
    const int ds[] = {3, 4, 5, 6};
    const int cs[] = {31, 28, 24, 32};

    for (int i = 0; i < 4; i++)
    {
        const int w = ws[i];
        const int h = hs[i];
        const int d = ds[i];
        const int c = cs[i];

        ncnn::Mat b[16];
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                for (int l = 0; l < 2; l++)
                {
                    for (int m = 0; m < 2; m++)
                    {
                        int bw = j % 2 == 0 ? w : 1;
                        int bh = k % 2 == 0 ? h : 1;
                        int bd = l % 2 == 0 ? d : 1;
                        int bc = m % 2 == 0 ? c : 1;
                        b[j * 8 + k * 4 + l * 2 + m] = RandomMat(bw, bh, bd, bc);
                    }
                }
            }
        }

        for (int j = 0; j < 16; j++)
        {
            for (int k = 0; k < 16; k++)
            {
                int ret = test_binaryop(b[j], b[k]);
                if (ret != 0)
                    return ret;
            }

            int ret = test_binaryop(b[j], 0.2f);
            if (ret != 0)
                return ret;
        }
    }

    return 0;
}

int main()
{
    SRAND(7767517);

    for (op_type = 0; op_type < 3; op_type++)
    {
        int ret = 0
                  || test_binaryop_1()
                  || test_binaryop_2()
                  || test_binaryop_3()
                  || test_binaryop_4();

        if (ret != 0)
            return ret;
    }

    return 0;
}
