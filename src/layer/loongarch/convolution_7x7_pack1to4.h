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

static void conv7x7s2_pack1to4_lsx(const Mat& bottom_blob, Mat& top_blob, const Mat& kernel, const Mat& _bias, const Option& opt)
{
    int w = bottom_blob.w;
    int inch = bottom_blob.c;

    int outw = top_blob.w;
    int outh = top_blob.h;
    int outch = top_blob.c;

    const int tailstep = w - 2 * outw + w;

    const float* bias = _bias;

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int p = 0; p < outch; p++)
    {
        Mat out0 = top_blob.channel(p);

        v4f32 _bias0 = bias ? (v4f32)__lsx_vld(bias + p * 4, 0) : (v4f32)__lsx_vreplgr2vr_w(0);
        out0.fill(_bias0);

        for (int q = 0; q < inch; q++)
        {
            float* outptr0 = out0;

            const Mat img0 = bottom_blob.channel(q);

            const float* r0 = img0.row(0);
            const float* r1 = img0.row(1);
            const float* r2 = img0.row(2);
            const float* r3 = img0.row(3);
            const float* r4 = img0.row(4);
            const float* r5 = img0.row(5);
            const float* r6 = img0.row(6);

            const float* kptr = kernel.channel(p).row(q);

            int i = 0;

            for (; i < outh; i++)
            {
                int j = 0;
                for (; j + 3 < outw; j += 4)
                {
                    v4f32 _sum0 = (v4f32)__lsx_vld(outptr0, 0);
                    v4f32 _sum1 = (v4f32)__lsx_vld(outptr0 + 4, 0);
                    v4f32 _sum2 = (v4f32)__lsx_vld(outptr0 + 4 * 2, 0);
                    v4f32 _sum3 = (v4f32)__lsx_vld(outptr0 + 4 * 3, 0);

                    v4f32 _k00 = (v4f32)__lsx_vld(kptr, 0);
                    v4f32 _k01 = (v4f32)__lsx_vld(kptr + 4, 0);
                    v4f32 _k02 = (v4f32)__lsx_vld(kptr + 4 * 2, 0);
                    v4f32 _k03 = (v4f32)__lsx_vld(kptr + 4 * 3, 0);
                    v4f32 _k04 = (v4f32)__lsx_vld(kptr + 4 * 4, 0);
                    v4f32 _k05 = (v4f32)__lsx_vld(kptr + 4 * 5, 0);
                    v4f32 _k06 = (v4f32)__lsx_vld(kptr + 4 * 6, 0);

                    kptr += 4 * 7;

                    __m128i _r0 = __lsx_vld(r0, 0);
                    __m128i _r0n = __lsx_vld(r0 + 4, 0);
                    __m128i _r0nn = __lsx_vld(r0 + 8, 0);

                    v4f32 _r00 = (v4f32)__lsx_vreplvei_w(_r0, 0);
                    v4f32 _r01 = (v4f32)__lsx_vreplvei_w(_r0, 1);
                    v4f32 _r02 = (v4f32)__lsx_vreplvei_w(_r0, 2);
                    v4f32 _r03 = (v4f32)__lsx_vreplvei_w(_r0, 3);
                    v4f32 _r04 = (v4f32)__lsx_vreplvei_w(_r0n, 0);
                    v4f32 _r05 = (v4f32)__lsx_vreplvei_w(_r0n, 1);
                    v4f32 _r06 = (v4f32)__lsx_vreplvei_w(_r0n, 2);
                    v4f32 _r07 = (v4f32)__lsx_vreplvei_w(_r0n, 3);
                    v4f32 _r08 = (v4f32)__lsx_vreplvei_w(_r0nn, 0);
                    v4f32 _r09 = (v4f32)__lsx_vreplvei_w(_r0nn, 1);
                    v4f32 _r0a = (v4f32)__lsx_vreplvei_w(_r0nn, 2);
                    v4f32 _r0b = (v4f32)__lsx_vreplvei_w(_r0nn, 3);
                    v4f32 _r0c = __lsx_vreplfr2vr_s(r0[12]);

                    _sum0 = __lsx_vfmadd_s(_k00, _r00, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k00, _r02, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k00, _r04, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k00, _r06, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k01, _r01, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k01, _r03, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k01, _r05, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k01, _r07, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k02, _r02, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k02, _r04, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k02, _r06, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k02, _r08, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k03, _r03, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k03, _r05, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k03, _r07, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k03, _r09, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k04, _r04, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k04, _r06, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k04, _r08, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k04, _r0a, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k05, _r05, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k05, _r07, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k05, _r09, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k05, _r0b, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k06, _r06, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k06, _r08, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k06, _r0a, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k06, _r0c, _sum3);

                    v4f32 _k10 = (v4f32)__lsx_vld(kptr, 0);
                    v4f32 _k11 = (v4f32)__lsx_vld(kptr + 4, 0);
                    v4f32 _k12 = (v4f32)__lsx_vld(kptr + 4 * 2, 0);
                    v4f32 _k13 = (v4f32)__lsx_vld(kptr + 4 * 3, 0);
                    v4f32 _k14 = (v4f32)__lsx_vld(kptr + 4 * 4, 0);
                    v4f32 _k15 = (v4f32)__lsx_vld(kptr + 4 * 5, 0);
                    v4f32 _k16 = (v4f32)__lsx_vld(kptr + 4 * 6, 0);

                    kptr += 4 * 7;

                    __m128i _r1 = __lsx_vld(r1, 0);
                    __m128i _r1n = __lsx_vld(r1 + 4, 0);
                    __m128i _r1nn = __lsx_vld(r1 + 8, 0);

                    v4f32 _r10 = (v4f32)__lsx_vreplvei_w(_r1, 0);
                    v4f32 _r11 = (v4f32)__lsx_vreplvei_w(_r1, 1);
                    v4f32 _r12 = (v4f32)__lsx_vreplvei_w(_r1, 2);
                    v4f32 _r13 = (v4f32)__lsx_vreplvei_w(_r1, 3);
                    v4f32 _r14 = (v4f32)__lsx_vreplvei_w(_r1n, 0);
                    v4f32 _r15 = (v4f32)__lsx_vreplvei_w(_r1n, 1);
                    v4f32 _r16 = (v4f32)__lsx_vreplvei_w(_r1n, 2);
                    v4f32 _r17 = (v4f32)__lsx_vreplvei_w(_r1n, 3);
                    v4f32 _r18 = (v4f32)__lsx_vreplvei_w(_r1nn, 0);
                    v4f32 _r19 = (v4f32)__lsx_vreplvei_w(_r1nn, 1);
                    v4f32 _r1a = (v4f32)__lsx_vreplvei_w(_r1nn, 2);
                    v4f32 _r1b = (v4f32)__lsx_vreplvei_w(_r1nn, 3);
                    v4f32 _r1c = __lsx_vreplfr2vr_s(r1[12]);

                    _sum0 = __lsx_vfmadd_s(_k10, _r10, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k10, _r12, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k10, _r14, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k10, _r16, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k11, _r11, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k11, _r13, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k11, _r15, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k11, _r17, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k12, _r12, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k12, _r14, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k12, _r16, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k12, _r18, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k13, _r13, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k13, _r15, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k13, _r17, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k13, _r19, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k14, _r14, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k14, _r16, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k14, _r18, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k14, _r1a, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k15, _r15, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k15, _r17, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k15, _r19, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k15, _r1b, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k16, _r16, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k16, _r18, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k16, _r1a, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k16, _r1c, _sum3);

                    v4f32 _k20 = (v4f32)__lsx_vld(kptr, 0);
                    v4f32 _k21 = (v4f32)__lsx_vld(kptr + 4, 0);
                    v4f32 _k22 = (v4f32)__lsx_vld(kptr + 4 * 2, 0);
                    v4f32 _k23 = (v4f32)__lsx_vld(kptr + 4 * 3, 0);
                    v4f32 _k24 = (v4f32)__lsx_vld(kptr + 4 * 4, 0);
                    v4f32 _k25 = (v4f32)__lsx_vld(kptr + 4 * 5, 0);
                    v4f32 _k26 = (v4f32)__lsx_vld(kptr + 4 * 6, 0);

                    kptr += 4 * 7;

                    __m128i _r2 = __lsx_vld(r2, 0);
                    __m128i _r2n = __lsx_vld(r2 + 4, 0);
                    __m128i _r2nn = __lsx_vld(r2 + 8, 0);

                    v4f32 _r20 = (v4f32)__lsx_vreplvei_w(_r2, 0);
                    v4f32 _r21 = (v4f32)__lsx_vreplvei_w(_r2, 1);
                    v4f32 _r22 = (v4f32)__lsx_vreplvei_w(_r2, 2);
                    v4f32 _r23 = (v4f32)__lsx_vreplvei_w(_r2, 3);
                    v4f32 _r24 = (v4f32)__lsx_vreplvei_w(_r2n, 0);
                    v4f32 _r25 = (v4f32)__lsx_vreplvei_w(_r2n, 1);
                    v4f32 _r26 = (v4f32)__lsx_vreplvei_w(_r2n, 2);
                    v4f32 _r27 = (v4f32)__lsx_vreplvei_w(_r2n, 3);
                    v4f32 _r28 = (v4f32)__lsx_vreplvei_w(_r2nn, 0);
                    v4f32 _r29 = (v4f32)__lsx_vreplvei_w(_r2nn, 1);
                    v4f32 _r2a = (v4f32)__lsx_vreplvei_w(_r2nn, 2);
                    v4f32 _r2b = (v4f32)__lsx_vreplvei_w(_r2nn, 3);
                    v4f32 _r2c = __lsx_vreplfr2vr_s(r2[12]);

                    _sum0 = __lsx_vfmadd_s(_k20, _r20, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k20, _r22, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k20, _r24, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k20, _r26, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k21, _r21, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k21, _r23, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k21, _r25, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k21, _r27, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k22, _r22, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k22, _r24, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k22, _r26, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k22, _r28, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k23, _r23, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k23, _r25, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k23, _r27, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k23, _r29, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k24, _r24, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k24, _r26, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k24, _r28, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k24, _r2a, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k25, _r25, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k25, _r27, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k25, _r29, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k25, _r2b, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k26, _r26, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k26, _r28, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k26, _r2a, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k26, _r2c, _sum3);

                    v4f32 _k30 = (v4f32)__lsx_vld(kptr, 0);
                    v4f32 _k31 = (v4f32)__lsx_vld(kptr + 4, 0);
                    v4f32 _k32 = (v4f32)__lsx_vld(kptr + 4 * 2, 0);
                    v4f32 _k33 = (v4f32)__lsx_vld(kptr + 4 * 3, 0);
                    v4f32 _k34 = (v4f32)__lsx_vld(kptr + 4 * 4, 0);
                    v4f32 _k35 = (v4f32)__lsx_vld(kptr + 4 * 5, 0);
                    v4f32 _k36 = (v4f32)__lsx_vld(kptr + 4 * 6, 0);

                    kptr += 4 * 7;

                    __m128i _r3 = __lsx_vld(r3, 0);
                    __m128i _r3n = __lsx_vld(r3 + 4, 0);
                    __m128i _r3nn = __lsx_vld(r3 + 8, 0);

                    v4f32 _r30 = (v4f32)__lsx_vreplvei_w(_r3, 0);
                    v4f32 _r31 = (v4f32)__lsx_vreplvei_w(_r3, 1);
                    v4f32 _r32 = (v4f32)__lsx_vreplvei_w(_r3, 2);
                    v4f32 _r33 = (v4f32)__lsx_vreplvei_w(_r3, 3);
                    v4f32 _r34 = (v4f32)__lsx_vreplvei_w(_r3n, 0);
                    v4f32 _r35 = (v4f32)__lsx_vreplvei_w(_r3n, 1);
                    v4f32 _r36 = (v4f32)__lsx_vreplvei_w(_r3n, 2);
                    v4f32 _r37 = (v4f32)__lsx_vreplvei_w(_r3n, 3);
                    v4f32 _r38 = (v4f32)__lsx_vreplvei_w(_r3nn, 0);
                    v4f32 _r39 = (v4f32)__lsx_vreplvei_w(_r3nn, 1);
                    v4f32 _r3a = (v4f32)__lsx_vreplvei_w(_r3nn, 2);
                    v4f32 _r3b = (v4f32)__lsx_vreplvei_w(_r3nn, 3);
                    v4f32 _r3c = __lsx_vreplfr2vr_s(r3[12]);

                    _sum0 = __lsx_vfmadd_s(_k30, _r30, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k30, _r32, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k30, _r34, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k30, _r36, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k31, _r31, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k31, _r33, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k31, _r35, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k31, _r37, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k32, _r32, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k32, _r34, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k32, _r36, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k32, _r38, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k33, _r33, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k33, _r35, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k33, _r37, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k33, _r39, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k34, _r34, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k34, _r36, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k34, _r38, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k34, _r3a, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k35, _r35, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k35, _r37, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k35, _r39, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k35, _r3b, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k36, _r36, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k36, _r38, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k36, _r3a, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k36, _r3c, _sum3);

                    v4f32 _k40 = (v4f32)__lsx_vld(kptr, 0);
                    v4f32 _k41 = (v4f32)__lsx_vld(kptr + 4, 0);
                    v4f32 _k42 = (v4f32)__lsx_vld(kptr + 4 * 2, 0);
                    v4f32 _k43 = (v4f32)__lsx_vld(kptr + 4 * 3, 0);
                    v4f32 _k44 = (v4f32)__lsx_vld(kptr + 4 * 4, 0);
                    v4f32 _k45 = (v4f32)__lsx_vld(kptr + 4 * 5, 0);
                    v4f32 _k46 = (v4f32)__lsx_vld(kptr + 4 * 6, 0);

                    kptr += 4 * 7;

                    __m128i _r4 = __lsx_vld(r4, 0);
                    __m128i _r4n = __lsx_vld(r4 + 4, 0);
                    __m128i _r4nn = __lsx_vld(r4 + 8, 0);

                    v4f32 _r40 = (v4f32)__lsx_vreplvei_w(_r4, 0);
                    v4f32 _r41 = (v4f32)__lsx_vreplvei_w(_r4, 1);
                    v4f32 _r42 = (v4f32)__lsx_vreplvei_w(_r4, 2);
                    v4f32 _r43 = (v4f32)__lsx_vreplvei_w(_r4, 3);
                    v4f32 _r44 = (v4f32)__lsx_vreplvei_w(_r4n, 0);
                    v4f32 _r45 = (v4f32)__lsx_vreplvei_w(_r4n, 1);
                    v4f32 _r46 = (v4f32)__lsx_vreplvei_w(_r4n, 2);
                    v4f32 _r47 = (v4f32)__lsx_vreplvei_w(_r4n, 3);
                    v4f32 _r48 = (v4f32)__lsx_vreplvei_w(_r4nn, 0);
                    v4f32 _r49 = (v4f32)__lsx_vreplvei_w(_r4nn, 1);
                    v4f32 _r4a = (v4f32)__lsx_vreplvei_w(_r4nn, 2);
                    v4f32 _r4b = (v4f32)__lsx_vreplvei_w(_r4nn, 3);
                    v4f32 _r4c = __lsx_vreplfr2vr_s(r4[12]);

                    _sum0 = __lsx_vfmadd_s(_k40, _r40, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k40, _r42, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k40, _r44, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k40, _r46, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k41, _r41, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k41, _r43, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k41, _r45, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k41, _r47, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k42, _r42, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k42, _r44, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k42, _r46, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k42, _r48, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k43, _r43, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k43, _r45, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k43, _r47, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k43, _r49, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k44, _r44, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k44, _r46, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k44, _r48, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k44, _r4a, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k45, _r45, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k45, _r47, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k45, _r49, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k45, _r4b, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k46, _r46, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k46, _r48, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k46, _r4a, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k46, _r4c, _sum3);

                    v4f32 _k50 = (v4f32)__lsx_vld(kptr, 0);
                    v4f32 _k51 = (v4f32)__lsx_vld(kptr + 4, 0);
                    v4f32 _k52 = (v4f32)__lsx_vld(kptr + 4 * 2, 0);
                    v4f32 _k53 = (v4f32)__lsx_vld(kptr + 4 * 3, 0);
                    v4f32 _k54 = (v4f32)__lsx_vld(kptr + 4 * 4, 0);
                    v4f32 _k55 = (v4f32)__lsx_vld(kptr + 4 * 5, 0);
                    v4f32 _k56 = (v4f32)__lsx_vld(kptr + 4 * 6, 0);

                    kptr += 4 * 7;

                    __m128i _r5 = __lsx_vld(r5, 0);
                    __m128i _r5n = __lsx_vld(r5 + 4, 0);
                    __m128i _r5nn = __lsx_vld(r5 + 8, 0);

                    v4f32 _r50 = (v4f32)__lsx_vreplvei_w(_r5, 0);
                    v4f32 _r51 = (v4f32)__lsx_vreplvei_w(_r5, 1);
                    v4f32 _r52 = (v4f32)__lsx_vreplvei_w(_r5, 2);
                    v4f32 _r53 = (v4f32)__lsx_vreplvei_w(_r5, 3);
                    v4f32 _r54 = (v4f32)__lsx_vreplvei_w(_r5n, 0);
                    v4f32 _r55 = (v4f32)__lsx_vreplvei_w(_r5n, 1);
                    v4f32 _r56 = (v4f32)__lsx_vreplvei_w(_r5n, 2);
                    v4f32 _r57 = (v4f32)__lsx_vreplvei_w(_r5n, 3);
                    v4f32 _r58 = (v4f32)__lsx_vreplvei_w(_r5nn, 0);
                    v4f32 _r59 = (v4f32)__lsx_vreplvei_w(_r5nn, 1);
                    v4f32 _r5a = (v4f32)__lsx_vreplvei_w(_r5nn, 2);
                    v4f32 _r5b = (v4f32)__lsx_vreplvei_w(_r5nn, 3);
                    v4f32 _r5c = __lsx_vreplfr2vr_s(r5[12]);

                    _sum0 = __lsx_vfmadd_s(_k50, _r50, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k50, _r52, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k50, _r54, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k50, _r56, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k51, _r51, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k51, _r53, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k51, _r55, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k51, _r57, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k52, _r52, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k52, _r54, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k52, _r56, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k52, _r58, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k53, _r53, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k53, _r55, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k53, _r57, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k53, _r59, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k54, _r54, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k54, _r56, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k54, _r58, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k54, _r5a, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k55, _r55, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k55, _r57, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k55, _r59, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k55, _r5b, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k56, _r56, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k56, _r58, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k56, _r5a, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k56, _r5c, _sum3);

                    v4f32 _k60 = (v4f32)__lsx_vld(kptr, 0);
                    v4f32 _k61 = (v4f32)__lsx_vld(kptr + 4, 0);
                    v4f32 _k62 = (v4f32)__lsx_vld(kptr + 4 * 2, 0);
                    v4f32 _k63 = (v4f32)__lsx_vld(kptr + 4 * 3, 0);
                    v4f32 _k64 = (v4f32)__lsx_vld(kptr + 4 * 4, 0);
                    v4f32 _k65 = (v4f32)__lsx_vld(kptr + 4 * 5, 0);
                    v4f32 _k66 = (v4f32)__lsx_vld(kptr + 4 * 6, 0);

                    kptr -= 4 * 42;

                    __m128i _r6 = __lsx_vld(r6, 0);
                    __m128i _r6n = __lsx_vld(r6 + 4, 0);
                    __m128i _r6nn = __lsx_vld(r6 + 8, 0);

                    v4f32 _r60 = (v4f32)__lsx_vreplvei_w(_r6, 0);
                    v4f32 _r61 = (v4f32)__lsx_vreplvei_w(_r6, 1);
                    v4f32 _r62 = (v4f32)__lsx_vreplvei_w(_r6, 2);
                    v4f32 _r63 = (v4f32)__lsx_vreplvei_w(_r6, 3);
                    v4f32 _r64 = (v4f32)__lsx_vreplvei_w(_r6n, 0);
                    v4f32 _r65 = (v4f32)__lsx_vreplvei_w(_r6n, 1);
                    v4f32 _r66 = (v4f32)__lsx_vreplvei_w(_r6n, 2);
                    v4f32 _r67 = (v4f32)__lsx_vreplvei_w(_r6n, 3);
                    v4f32 _r68 = (v4f32)__lsx_vreplvei_w(_r6nn, 0);
                    v4f32 _r69 = (v4f32)__lsx_vreplvei_w(_r6nn, 1);
                    v4f32 _r6a = (v4f32)__lsx_vreplvei_w(_r6nn, 2);
                    v4f32 _r6b = (v4f32)__lsx_vreplvei_w(_r6nn, 3);
                    v4f32 _r6c = __lsx_vreplfr2vr_s(r6[12]);

                    _sum0 = __lsx_vfmadd_s(_k60, _r60, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k60, _r62, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k60, _r64, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k60, _r66, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k61, _r61, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k61, _r63, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k61, _r65, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k61, _r67, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k62, _r62, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k62, _r64, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k62, _r66, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k62, _r68, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k63, _r63, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k63, _r65, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k63, _r67, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k63, _r69, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k64, _r64, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k64, _r66, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k64, _r68, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k64, _r6a, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k65, _r65, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k65, _r67, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k65, _r69, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k65, _r6b, _sum3);
                    _sum0 = __lsx_vfmadd_s(_k66, _r66, _sum0);
                    _sum1 = __lsx_vfmadd_s(_k66, _r68, _sum1);
                    _sum2 = __lsx_vfmadd_s(_k66, _r6a, _sum2);
                    _sum3 = __lsx_vfmadd_s(_k66, _r6c, _sum3);

                    __lsx_vst((__m128i)_sum0, outptr0, 0);
                    __lsx_vst((__m128i)_sum1, outptr0 + 4, 0);
                    __lsx_vst((__m128i)_sum2, outptr0 + 4 * 2, 0);
                    __lsx_vst((__m128i)_sum3, outptr0 + 4 * 3, 0);

                    outptr0 += 4 * 4;

                    r0 += 8;
                    r1 += 8;
                    r2 += 8;
                    r3 += 8;
                    r4 += 8;
                    r5 += 8;
                    r6 += 8;
                }
                for (; j < outw; j++)
                {
                    v4f32 _sum0 = (v4f32)__lsx_vld(outptr0, 0);

                    v4f32 _k00 = (v4f32)__lsx_vld(kptr, 0);
                    v4f32 _k01 = (v4f32)__lsx_vld(kptr + 4, 0);
                    v4f32 _k02 = (v4f32)__lsx_vld(kptr + 4 * 2, 0);
                    v4f32 _k03 = (v4f32)__lsx_vld(kptr + 4 * 3, 0);
                    v4f32 _k04 = (v4f32)__lsx_vld(kptr + 4 * 4, 0);
                    v4f32 _k05 = (v4f32)__lsx_vld(kptr + 4 * 5, 0);
                    v4f32 _k06 = (v4f32)__lsx_vld(kptr + 4 * 6, 0);

                    kptr += 4 * 7;

                    __m128i _r0 = __lsx_vld(r0, 0);
                    __m128i _r0n = __lsx_vld(r0 + 4, 0);

                    _sum0 = __lsx_vfmadd_s(_k00, (v4f32)__lsx_vreplvei_w(_r0, 0), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k01, (v4f32)__lsx_vreplvei_w(_r0, 1), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k02, (v4f32)__lsx_vreplvei_w(_r0, 2), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k03, (v4f32)__lsx_vreplvei_w(_r0, 3), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k04, (v4f32)__lsx_vreplvei_w(_r0n, 0), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k05, (v4f32)__lsx_vreplvei_w(_r0n, 1), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k06, (v4f32)__lsx_vreplvei_w(_r0n, 2), _sum0);

                    v4f32 _k10 = (v4f32)__lsx_vld(kptr, 0);
                    v4f32 _k11 = (v4f32)__lsx_vld(kptr + 4, 0);
                    v4f32 _k12 = (v4f32)__lsx_vld(kptr + 4 * 2, 0);
                    v4f32 _k13 = (v4f32)__lsx_vld(kptr + 4 * 3, 0);
                    v4f32 _k14 = (v4f32)__lsx_vld(kptr + 4 * 4, 0);
                    v4f32 _k15 = (v4f32)__lsx_vld(kptr + 4 * 5, 0);
                    v4f32 _k16 = (v4f32)__lsx_vld(kptr + 4 * 6, 0);

                    kptr += 4 * 7;

                    __m128i _r1 = __lsx_vld(r1, 0);
                    __m128i _r1n = __lsx_vld(r1 + 4, 0);

                    _sum0 = __lsx_vfmadd_s(_k10, (v4f32)__lsx_vreplvei_w(_r1, 0), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k11, (v4f32)__lsx_vreplvei_w(_r1, 1), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k12, (v4f32)__lsx_vreplvei_w(_r1, 2), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k13, (v4f32)__lsx_vreplvei_w(_r1, 3), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k14, (v4f32)__lsx_vreplvei_w(_r1n, 0), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k15, (v4f32)__lsx_vreplvei_w(_r1n, 1), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k16, (v4f32)__lsx_vreplvei_w(_r1n, 2), _sum0);

                    v4f32 _k20 = (v4f32)__lsx_vld(kptr, 0);
                    v4f32 _k21 = (v4f32)__lsx_vld(kptr + 4, 0);
                    v4f32 _k22 = (v4f32)__lsx_vld(kptr + 4 * 2, 0);
                    v4f32 _k23 = (v4f32)__lsx_vld(kptr + 4 * 3, 0);
                    v4f32 _k24 = (v4f32)__lsx_vld(kptr + 4 * 4, 0);
                    v4f32 _k25 = (v4f32)__lsx_vld(kptr + 4 * 5, 0);
                    v4f32 _k26 = (v4f32)__lsx_vld(kptr + 4 * 6, 0);

                    kptr += 4 * 7;

                    __m128i _r2 = __lsx_vld(r2, 0);
                    __m128i _r2n = __lsx_vld(r2 + 4, 0);

                    _sum0 = __lsx_vfmadd_s(_k20, (v4f32)__lsx_vreplvei_w(_r2, 0), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k21, (v4f32)__lsx_vreplvei_w(_r2, 1), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k22, (v4f32)__lsx_vreplvei_w(_r2, 2), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k23, (v4f32)__lsx_vreplvei_w(_r2, 3), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k24, (v4f32)__lsx_vreplvei_w(_r2n, 0), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k25, (v4f32)__lsx_vreplvei_w(_r2n, 1), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k26, (v4f32)__lsx_vreplvei_w(_r2n, 2), _sum0);

                    v4f32 _k30 = (v4f32)__lsx_vld(kptr, 0);
                    v4f32 _k31 = (v4f32)__lsx_vld(kptr + 4, 0);
                    v4f32 _k32 = (v4f32)__lsx_vld(kptr + 4 * 2, 0);
                    v4f32 _k33 = (v4f32)__lsx_vld(kptr + 4 * 3, 0);
                    v4f32 _k34 = (v4f32)__lsx_vld(kptr + 4 * 4, 0);
                    v4f32 _k35 = (v4f32)__lsx_vld(kptr + 4 * 5, 0);
                    v4f32 _k36 = (v4f32)__lsx_vld(kptr + 4 * 6, 0);

                    kptr += 4 * 7;

                    __m128i _r3 = __lsx_vld(r3, 0);
                    __m128i _r3n = __lsx_vld(r3 + 4, 0);

                    _sum0 = __lsx_vfmadd_s(_k30, (v4f32)__lsx_vreplvei_w(_r3, 0), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k31, (v4f32)__lsx_vreplvei_w(_r3, 1), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k32, (v4f32)__lsx_vreplvei_w(_r3, 2), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k33, (v4f32)__lsx_vreplvei_w(_r3, 3), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k34, (v4f32)__lsx_vreplvei_w(_r3n, 0), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k35, (v4f32)__lsx_vreplvei_w(_r3n, 1), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k36, (v4f32)__lsx_vreplvei_w(_r3n, 2), _sum0);

                    v4f32 _k40 = (v4f32)__lsx_vld(kptr, 0);
                    v4f32 _k41 = (v4f32)__lsx_vld(kptr + 4, 0);
                    v4f32 _k42 = (v4f32)__lsx_vld(kptr + 4 * 2, 0);
                    v4f32 _k43 = (v4f32)__lsx_vld(kptr + 4 * 3, 0);
                    v4f32 _k44 = (v4f32)__lsx_vld(kptr + 4 * 4, 0);
                    v4f32 _k45 = (v4f32)__lsx_vld(kptr + 4 * 5, 0);
                    v4f32 _k46 = (v4f32)__lsx_vld(kptr + 4 * 6, 0);

                    kptr += 4 * 7;

                    __m128i _r4 = __lsx_vld(r4, 0);
                    __m128i _r4n = __lsx_vld(r4 + 4, 0);

                    _sum0 = __lsx_vfmadd_s(_k40, (v4f32)__lsx_vreplvei_w(_r4, 0), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k41, (v4f32)__lsx_vreplvei_w(_r4, 1), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k42, (v4f32)__lsx_vreplvei_w(_r4, 2), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k43, (v4f32)__lsx_vreplvei_w(_r4, 3), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k44, (v4f32)__lsx_vreplvei_w(_r4n, 0), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k45, (v4f32)__lsx_vreplvei_w(_r4n, 1), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k46, (v4f32)__lsx_vreplvei_w(_r4n, 2), _sum0);

                    v4f32 _k50 = (v4f32)__lsx_vld(kptr, 0);
                    v4f32 _k51 = (v4f32)__lsx_vld(kptr + 4, 0);
                    v4f32 _k52 = (v4f32)__lsx_vld(kptr + 4 * 2, 0);
                    v4f32 _k53 = (v4f32)__lsx_vld(kptr + 4 * 3, 0);
                    v4f32 _k54 = (v4f32)__lsx_vld(kptr + 4 * 4, 0);
                    v4f32 _k55 = (v4f32)__lsx_vld(kptr + 4 * 5, 0);
                    v4f32 _k56 = (v4f32)__lsx_vld(kptr + 4 * 6, 0);

                    kptr += 4 * 7;

                    __m128i _r5 = __lsx_vld(r5, 0);
                    __m128i _r5n = __lsx_vld(r5 + 4, 0);

                    _sum0 = __lsx_vfmadd_s(_k50, (v4f32)__lsx_vreplvei_w(_r5, 0), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k51, (v4f32)__lsx_vreplvei_w(_r5, 1), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k52, (v4f32)__lsx_vreplvei_w(_r5, 2), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k53, (v4f32)__lsx_vreplvei_w(_r5, 3), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k54, (v4f32)__lsx_vreplvei_w(_r5n, 0), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k55, (v4f32)__lsx_vreplvei_w(_r5n, 1), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k56, (v4f32)__lsx_vreplvei_w(_r5n, 2), _sum0);

                    v4f32 _k60 = (v4f32)__lsx_vld(kptr, 0);
                    v4f32 _k61 = (v4f32)__lsx_vld(kptr + 4, 0);
                    v4f32 _k62 = (v4f32)__lsx_vld(kptr + 4 * 2, 0);
                    v4f32 _k63 = (v4f32)__lsx_vld(kptr + 4 * 3, 0);
                    v4f32 _k64 = (v4f32)__lsx_vld(kptr + 4 * 4, 0);
                    v4f32 _k65 = (v4f32)__lsx_vld(kptr + 4 * 5, 0);
                    v4f32 _k66 = (v4f32)__lsx_vld(kptr + 4 * 6, 0);

                    kptr -= 4 * 42;

                    __m128i _r6 = __lsx_vld(r6, 0);
                    __m128i _r6n = __lsx_vld(r6 + 4, 0);

                    _sum0 = __lsx_vfmadd_s(_k60, (v4f32)__lsx_vreplvei_w(_r6, 0), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k61, (v4f32)__lsx_vreplvei_w(_r6, 1), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k62, (v4f32)__lsx_vreplvei_w(_r6, 2), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k63, (v4f32)__lsx_vreplvei_w(_r6, 3), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k64, (v4f32)__lsx_vreplvei_w(_r6n, 0), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k65, (v4f32)__lsx_vreplvei_w(_r6n, 1), _sum0);
                    _sum0 = __lsx_vfmadd_s(_k66, (v4f32)__lsx_vreplvei_w(_r6n, 2), _sum0);

                    __lsx_vst((__m128i)_sum0, outptr0, 0);

                    outptr0 += 4;

                    r0 += 2;
                    r1 += 2;
                    r2 += 2;
                    r3 += 2;
                    r4 += 2;
                    r5 += 2;
                    r6 += 2;
                }

                r0 += tailstep;
                r1 += tailstep;
                r2 += tailstep;
                r3 += tailstep;
                r4 += tailstep;
                r5 += tailstep;
                r6 += tailstep;
            }
        }
    }
}
