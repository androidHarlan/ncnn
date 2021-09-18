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

static void convolution_bf16s(const Mat& bottom_blob, Mat& top_blob, const Mat& weight_data_bf16, const Mat& bias_data, int kernel_w, int kernel_h, int dilation_w, int dilation_h, int stride_w, int stride_h, int activation_type, const Mat& activation_params, const Option& opt)
{
    int w = bottom_blob.w;
    int channels = bottom_blob.c;

    int outw = top_blob.w;
    int outh = top_blob.h;
    int outch = top_blob.c;

    const int maxk = kernel_w * kernel_h;

    // kernel offsets
    std::vector<int> _space_ofs(maxk);
    int* space_ofs = &_space_ofs[0];
    {
        int p1 = 0;
        int p2 = 0;
        int gap = w * dilation_h - kernel_w * dilation_w;
        for (int i = 0; i < kernel_h; i++)
        {
            for (int j = 0; j < kernel_w; j++)
            {
                space_ofs[p1] = p2;
                p1++;
                p2 += dilation_w;
            }
            p2 += gap;
        }
    }

    const float* bias_data_ptr = bias_data;

    // num_output
    #pragma omp parallel for num_threads(opt.num_threads)
    for (int p = 0; p < outch; p++)
    {
        unsigned short* outptr = top_blob.channel(p);

        for (int i = 0; i < outh; i++)
        {
            for (int j = 0; j < outw; j++)
            {
                float sum = 0.f;

                if (bias_data_ptr)
                {
                    sum = bias_data_ptr[p];
                }

                const unsigned short* kptr = (const unsigned short*)weight_data_bf16 + maxk * channels * p;

                // channels
                for (int q = 0; q < channels; q++)
                {
                    const Mat m = bottom_blob.channel(q);
                    const unsigned short* sptr = m.row<unsigned short>(i * stride_h) + j * stride_w;

                    for (int k = 0; k < maxk; k++)
                    {
                        float val = bfloat16_to_float32(sptr[space_ofs[k]]);
                        float wt = bfloat16_to_float32(kptr[k]);
                        sum += val * wt;
                    }

                    kptr += maxk;
                }

                if (activation_type == 1)
                {
                    sum = std::max(sum, 0.f);
                }
                else if (activation_type == 2)
                {
                    float slope = activation_params[0];
                    sum = sum > 0.f ? sum : sum * slope;
                }
                else if (activation_type == 3)
                {
                    float min = activation_params[0];
                    float max = activation_params[1];
                    if (sum < min)
                        sum = min;
                    if (sum > max)
                        sum = max;
                }
                else if (activation_type == 4)
                {
                    sum = static_cast<float>(1.f / (1.f + exp(-sum)));
                }
                else if (activation_type == 5)
                {
                    sum = static_cast<float>(sum * tanh(log(exp(sum) + 1.f)));
                }
                else if (activation_type == 6)
                {
                    float alpha = activation_params[0];
                    float beta = activation_params[1];
                    float lower = -beta / alpha;
                    float upper = (1.f / alpha) + lower;
                    if (sum < lower)
                        sum = 0.f;
                    else if (sum > upper)
                        ;
                    else
                        sum = sum * (sum * alpha + beta);
                }

                outptr[j] = float32_to_bfloat16(sum);
            }

            outptr += outw;
        }
    }
}
