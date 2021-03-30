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

static void conv3x3s1_winograd42_transform_kernel_pack8_int8_neon(const Mat& kernel, Mat& kernel_tm_pack8, int inch, int outch)
{
    // winograd42 transform kernel
    Mat kernel_tm(6 * 6, inch, outch, 2u);

    const short ktm[6][3] = {
        {6, 0, 0},
        {-4, -4, -4},
        {-4, 4, -4},
        {1, 2, 4},
        {1, -2, 4},
        {0, 0, 6}
    };

    #pragma omp parallel for
    for (int p = 0; p < outch; p++)
    {
        for (int q = 0; q < inch; q++)
        {
            const signed char* kernel0 = (const signed char*)kernel + p * inch * 9 + q * 9;
            short* kernel_tm0 = kernel_tm.channel(p).row<short>(q);

            // transform kernel
            const signed char* k0 = kernel0;
            const signed char* k1 = kernel0 + 3;
            const signed char* k2 = kernel0 + 6;

            // h
            short tmp[6][3];
            for (int i = 0; i < 6; i++)
            {
                tmp[i][0] = k0[0] * ktm[i][0] + k0[1] * ktm[i][1] + k0[2] * ktm[i][2];
                tmp[i][1] = k1[0] * ktm[i][0] + k1[1] * ktm[i][1] + k1[2] * ktm[i][2];
                tmp[i][2] = k2[0] * ktm[i][0] + k2[1] * ktm[i][1] + k2[2] * ktm[i][2];
            }

            // U
            for (int j = 0; j < 6; j++)
            {
                short* tmpp = &tmp[j][0];

                for (int i = 0; i < 6; i++)
                {
                    kernel_tm0[j * 6 + i] = tmpp[0] * ktm[i][0] + tmpp[1] * ktm[i][1] + tmpp[2] * ktm[i][2];
                }
            }
        }
    }

    // interleave
    // src = 36-inch-outch
    // dst = 8b-8a-inch/8a-36-outch/8b
    kernel_tm_pack8.create(inch / 8, 36, outch / 8, (size_t)2u * 64, 64);

    int q = 0;
    for (; q + 7 < outch; q += 8)
    {
        const Mat k0 = kernel_tm.channel(q);
        const Mat k1 = kernel_tm.channel(q + 1);
        const Mat k2 = kernel_tm.channel(q + 2);
        const Mat k3 = kernel_tm.channel(q + 3);
        const Mat k4 = kernel_tm.channel(q + 4);
        const Mat k5 = kernel_tm.channel(q + 5);
        const Mat k6 = kernel_tm.channel(q + 6);
        const Mat k7 = kernel_tm.channel(q + 7);

        Mat g0 = kernel_tm_pack8.channel(q / 8);

        for (int k = 0; k < 36; k++)
        {
            short* g00 = g0.row<short>(k);

            for (int p = 0; p + 7 < inch; p += 8)
            {
                for (int i = 0; i < 8; i++)
                {
                    const short* k00 = k0.row<const short>(p + i);
                    const short* k10 = k1.row<const short>(p + i);
                    const short* k20 = k2.row<const short>(p + i);
                    const short* k30 = k3.row<const short>(p + i);
                    const short* k40 = k4.row<const short>(p + i);
                    const short* k50 = k5.row<const short>(p + i);
                    const short* k60 = k6.row<const short>(p + i);
                    const short* k70 = k7.row<const short>(p + i);

                    g00[0] = k00[k];
                    g00[1] = k10[k];
                    g00[2] = k20[k];
                    g00[3] = k30[k];
                    g00[4] = k40[k];
                    g00[5] = k50[k];
                    g00[6] = k60[k];
                    g00[7] = k70[k];

                    g00 += 8;
                }
            }
        }
    }
}

static void conv3x3s1_winograd42_pack8_int8_neon(const Mat& bottom_blob, Mat& top_blob, const Mat& kernel_tm, const Option& opt)
{
    int w = bottom_blob.w;
    int h = bottom_blob.h;
    int inch = bottom_blob.c;
//     size_t elemsize = bottom_blob.elemsize;
    int elempack = bottom_blob.elempack;

    int outw = top_blob.w;
    int outh = top_blob.h;
    int outch = top_blob.c;

    // pad to 4n+2
    Mat bottom_blob_bordered = bottom_blob;

    outw = (outw + 3) / 4 * 4;
    outh = (outh + 3) / 4 * 4;

    w = outw + 2;
    h = outh + 2;
    copy_make_border(bottom_blob, bottom_blob_bordered, 0, h - bottom_blob.h, 0, w - bottom_blob.w, BORDER_CONSTANT, 0.f, opt);

    // BEGIN transform input
    Mat bottom_blob_tm;
    {
        int w_tm = outw / 4 * 6;
        int h_tm = outh / 4 * 6;

        const int tiles = w_tm / 6 * h_tm / 6;

        bottom_blob_tm.create(tiles, 36, inch, 2u * elempack, elempack, opt.workspace_allocator);

        // const float itm[4][4] = {
        //     {4.0f, 0.0f, -5.0f, 0.0f, 1.0f, 0.0f},
        //     {0.0f,-4.0f, -4.0f, 1.0f, 1.0f, 0.0f},
        //     {0.0f, 4.0f, -4.0f,-1.0f, 1.0f, 0.0f},
        //     {0.0f,-2.0f, -1.0f, 2.0f, 1.0f, 0.0f},
        //     {0.0f, 2.0f, -1.0f,-2.0f, 1.0f, 0.0f},
        //     {0.0f, 4.0f,  0.0f,-5.0f, 0.0f, 1.0f}
        // };

        // 0 =  4 * r00 - 5 * r02 + r04
        // 1 = -4 * (r01 + r02) + r04 + r03
        // 2 =  4 * (r01 - r02) + r04 - r03
        // 3 = -2 * (r01 - r03) + r04 - r02
        // 4 =  2 * (r01 - r03) + r04 - r02
        // 5 =  4 * r01 - 5 * r03 + r05

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < inch; q++)
        {
            const Mat img0 = bottom_blob_bordered.channel(q);
            Mat img0_tm = bottom_blob_tm.channel(q);

            short tmp[6][6][8];

            // tile
            for (int i = 0; i < h_tm / 6; i++)
            {
                for (int j = 0; j < w_tm / 6; j++)
                {
                    const signed char* r0 = img0.row<const signed char>(i * 4) + (j * 4) * 8;

                    for (int m = 0; m < 6; m++)
                    {
                        int8x8_t _r00 = vld1_s8(r0);
                        int8x8_t _r01 = vld1_s8(r0 + 8);
                        int8x8_t _r02 = vld1_s8(r0 + 16);
                        int8x8_t _r03 = vld1_s8(r0 + 24);
                        int8x8_t _r04 = vld1_s8(r0 + 32);
                        int8x8_t _r05 = vld1_s8(r0 + 40);

                        int8x8_t _v4s8 = vdup_n_s8(4);
                        int8x8_t _v5s8 = vdup_n_s8(5);
                        int16x8_t _v2 = vdupq_n_s16(2);
                        int16x8_t _v4 = vdupq_n_s16(4);

//                         int16x8_t _tmp0m = vfmsq_n_f16(vfmaq_n_f16(_r04, _r00, 4.f), _r02, 5.f);
                        int16x8_t _tmp0m = vsubq_s16(vaddw_s8(vmull_s8(_r00, _v4s8), _r04), vmull_s8(_r02, _v5s8));

//                         int16x8_t _tmp1m = vfmsq_n_f16(vaddq_f16(_r04, _r03), vaddq_f16(_r01, _r02), 4.f);
                        int16x8_t _tmp1m = vmlsq_s16(vaddl_s8(_r04, _r03), vaddl_s8(_r01, _r02), _v4);

//                         int16x8_t _tmp2m = vfmaq_n_f16(vsubq_f16(_r04, _r03), vsubq_f16(_r01, _r02), 4.f);
                        int16x8_t _tmp2m = vmlaq_s16(vsubl_s8(_r04, _r03), vsubl_s8(_r01, _r02), _v4);

//                         int16x8_t _tmp3m = vfmsq_n_f16(vsubq_f16(_r04, _r02), vsubq_f16(_r01, _r03), 2.f);
                        int16x8_t _tmp3m = vmlsq_s16(vsubl_s8(_r04, _r02), vsubl_s8(_r01, _r03), _v2);

//                         int16x8_t _tmp4m = vfmaq_n_f16(vsubq_f16(_r04, _r02), vsubq_f16(_r01, _r03), 2.f);
                        int16x8_t _tmp4m = vmlaq_s16(vsubl_s8(_r04, _r02), vsubl_s8(_r01, _r03), _v2);

//                         int16x8_t _tmp5m = vfmsq_n_f16(vfmaq_n_f16(_r05, _r01, 4.f), _r03, 5.f);
                        int16x8_t _tmp5m = vsubq_s16(vaddw_s8(vmull_s8(_r01, _v4s8), _r05), vmull_s8(_r03, _v5s8));

                        vst1q_s16(tmp[0][m], _tmp0m);
                        vst1q_s16(tmp[1][m], _tmp1m);
                        vst1q_s16(tmp[2][m], _tmp2m);
                        vst1q_s16(tmp[3][m], _tmp3m);
                        vst1q_s16(tmp[4][m], _tmp4m);
                        vst1q_s16(tmp[5][m], _tmp5m);

                        r0 += w * 8;
                    }

                    short* r0_tm_0 = (short*)img0_tm + (i * w_tm / 6 + j) * 8;
                    short* r0_tm_1 = r0_tm_0 + tiles * 8;
                    short* r0_tm_2 = r0_tm_0 + tiles * 16;
                    short* r0_tm_3 = r0_tm_0 + tiles * 24;
                    short* r0_tm_4 = r0_tm_0 + tiles * 32;
                    short* r0_tm_5 = r0_tm_0 + tiles * 40;

                    for (int m = 0; m < 6; m++)
                    {
                        int16x8_t _tmp00 = vld1q_s16(tmp[m][0]);
                        int16x8_t _tmp01 = vld1q_s16(tmp[m][1]);
                        int16x8_t _tmp02 = vld1q_s16(tmp[m][2]);
                        int16x8_t _tmp03 = vld1q_s16(tmp[m][3]);
                        int16x8_t _tmp04 = vld1q_s16(tmp[m][4]);
                        int16x8_t _tmp05 = vld1q_s16(tmp[m][5]);

                        int16x8_t _v2 = vdupq_n_s16(2);
                        int16x8_t _v4 = vdupq_n_s16(4);
                        int16x8_t _v5 = vdupq_n_s16(5);

                        int16x8_t _r0tm0 = vmlsq_s16(vmlaq_s16(_tmp04, _tmp00, _v4), _tmp02, _v5);
                        int16x8_t _r0tm1 = vmlsq_s16(vaddq_s16(_tmp04, _tmp03), vaddq_s16(_tmp01, _tmp02), _v4);
                        int16x8_t _r0tm2 = vmlaq_s16(vsubq_s16(_tmp04, _tmp03), vsubq_s16(_tmp01, _tmp02), _v4);
                        int16x8_t _r0tm3 = vmlsq_s16(vsubq_s16(_tmp04, _tmp02), vsubq_s16(_tmp01, _tmp03), _v2);
                        int16x8_t _r0tm4 = vmlaq_s16(vsubq_s16(_tmp04, _tmp02), vsubq_s16(_tmp01, _tmp03), _v2);
                        int16x8_t _r0tm5 = vmlsq_s16(vmlaq_s16(_tmp05, _tmp01, _v4), _tmp03, _v5);

                        vst1q_s16(r0_tm_0, _r0tm0);
                        vst1q_s16(r0_tm_1, _r0tm1);
                        vst1q_s16(r0_tm_2, _r0tm2);
                        vst1q_s16(r0_tm_3, _r0tm3);
                        vst1q_s16(r0_tm_4, _r0tm4);
                        vst1q_s16(r0_tm_5, _r0tm5);

                        r0_tm_0 += tiles * 48;
                        r0_tm_1 += tiles * 48;
                        r0_tm_2 += tiles * 48;
                        r0_tm_3 += tiles * 48;
                        r0_tm_4 += tiles * 48;
                        r0_tm_5 += tiles * 48;
                    }
                }
            }
        }
    }
    bottom_blob_bordered = Mat();
    // END transform input

    // BEGIN dot
    Mat top_blob_tm;
    {
        int w_tm = outw / 4 * 6;
        int h_tm = outh / 4 * 6;

        const int tiles = h_tm / 6 * w_tm / 6;

        // permute
        //         bottom_blob_tm.create(tiles, 36, inch, elemsize, elempack, opt.workspace_allocator);
        Mat bottom_blob_tm2;
//         if (tiles >= 12)
//             bottom_blob_tm2.create(12 * inch, tiles / 12 + (tiles % 12) / 8 + (tiles % 12 % 8) / 4 + (tiles % 12 % 4) / 2 + tiles % 12 % 2, 36, 2u * elempack, elempack, opt.workspace_allocator);
//         else if (tiles >= 8)
//             bottom_blob_tm2.create(8 * inch, tiles / 8 + (tiles % 8) / 4 + (tiles % 4) / 2 + tiles % 2, 36, 2u * elempack, elempack, opt.workspace_allocator);
//         else
        if (tiles >= 4)
            bottom_blob_tm2.create(4 * inch, tiles / 4 + (tiles % 4) / 2 + tiles % 2, 36, 2u * elempack, elempack, opt.workspace_allocator);
        else if (tiles >= 2)
            bottom_blob_tm2.create(2 * inch, tiles / 2 + tiles % 2, 36, 2u * elempack, elempack, opt.workspace_allocator);
        else // if (tiles >= 1)
            bottom_blob_tm2.create(1 * inch, tiles, 36, 2u * elempack, elempack, opt.workspace_allocator);

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int r = 0; r < 36; r++)
        {
            Mat tm2 = bottom_blob_tm2.channel(r);

            // tile
            int i = 0;
#if 0
            for (; i + 11 < tiles; i += 12)
            {
                __fp16* tm2p = tm2.row<__fp16>(i / 12);

                const __fp16* r0 = bottom_blob_tm;

                r0 += (r * tiles + i) * 8;

                for (int q = 0; q < inch; q++)
                {
                    // transpose 12x8
                    asm volatile(
                        "prfm   pldl1keep, [%0, #512]   \n"
                        "ld4    {v0.8h, v1.8h, v2.8h, v3.8h}, [%0], #64 \n"
                        "ld4    {v4.8h, v5.8h, v6.8h, v7.8h}, [%0], #64 \n"
                        "ld4    {v16.8h, v17.8h, v18.8h, v19.8h}, [%0] \n"

                        "sub    %0, %0, #128            \n"

                        "uzp1   v20.8h, v0.8h, v4.8h    \n" // 0
                        "uzp1   v21.8h, v16.8h, v1.8h   \n" // 1
                        "uzp1   v22.8h, v5.8h, v17.8h   \n" // 2
                        "uzp1   v23.8h, v2.8h, v6.8h    \n" // 3
                        "uzp1   v24.8h, v18.8h, v3.8h   \n" // 4
                        "uzp1   v25.8h, v7.8h, v19.8h   \n" // 5
                        "uzp2   v26.8h, v0.8h, v4.8h    \n" // 6
                        "uzp2   v27.8h, v16.8h, v1.8h   \n" // 7
                        "uzp2   v28.8h, v5.8h, v17.8h   \n" // 8
                        "uzp2   v29.8h, v2.8h, v6.8h    \n" // 9
                        "uzp2   v30.8h, v18.8h, v3.8h   \n" // 10
                        "uzp2   v31.8h, v7.8h, v19.8h   \n" // 11

                        "st1    {v20.8h, v21.8h, v22.8h, v23.8h}, [%1], #64 \n"
                        "st1    {v24.8h, v25.8h, v26.8h, v27.8h}, [%1], #64 \n"
                        "st1    {v28.8h, v29.8h, v30.8h, v31.8h}, [%1], #64 \n"
                        : "=r"(r0),  // %0
                        "=r"(tm2p) // %1
                        : "0"(r0),
                        "1"(tm2p)
                        : "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31");

                    r0 += bottom_blob_tm.cstep * 8;
                }
            }
            for (; i + 7 < tiles; i += 8)
            {
                __fp16* tmpptr = tm2.row<__fp16>(i / 12 + (i % 12) / 8);

                const __fp16* r0 = bottom_blob_tm;

                r0 += (r * tiles + i) * 8;

                for (int q = 0; q < inch; q++)
                {
                    // transpose 8x8
                    asm volatile(
                        "prfm   pldl1keep, [%0, #512]   \n"
                        "ld4    {v0.8h, v1.8h, v2.8h, v3.8h}, [%0], #64 \n"
                        "ld4    {v4.8h, v5.8h, v6.8h, v7.8h}, [%0] \n"
                        "sub    %0, %0, #64             \n"

                        "uzp1   v16.8h, v0.8h, v4.8h    \n"
                        "uzp2   v20.8h, v0.8h, v4.8h    \n"
                        "uzp1   v17.8h, v1.8h, v5.8h    \n"
                        "uzp2   v21.8h, v1.8h, v5.8h    \n"
                        "uzp1   v18.8h, v2.8h, v6.8h    \n"
                        "uzp2   v22.8h, v2.8h, v6.8h    \n"
                        "uzp1   v19.8h, v3.8h, v7.8h    \n"
                        "uzp2   v23.8h, v3.8h, v7.8h    \n"

                        "st1    {v16.8h, v17.8h, v18.8h, v19.8h}, [%1], #64 \n"
                        "st1    {v20.8h, v21.8h, v22.8h, v23.8h}, [%1], #64 \n"
                        : "=r"(r0),    // %0
                        "=r"(tmpptr) // %1
                        : "0"(r0),
                        "1"(tmpptr)
                        : "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23");

                    r0 += bottom_blob_tm.cstep * 8;
                }
            }
#endif
            for (; i + 3 < tiles; i += 4)
            {
//                 __fp16* tmpptr = tm2.row<__fp16>(i / 12 + (i % 12) / 8 + (i % 12 % 8) / 4);
                short* tmpptr = tm2.row<short>(i / 4);

                const short* r0 = bottom_blob_tm;

                r0 += (r * tiles + i) * 8;

                for (int q = 0; q < inch; q++)
                {
                    asm volatile(
                        "prfm   pldl1keep, [%0, #512]   \n"
                        "ld1    {v0.8h, v1.8h, v2.8h, v3.8h}, [%0] \n"
                        "st1    {v0.8h, v1.8h, v2.8h, v3.8h}, [%1], #64 \n"
                        : "=r"(r0),    // %0
                        "=r"(tmpptr) // %1
                        : "0"(r0),
                        "1"(tmpptr)
                        : "memory", "v0", "v1", "v2", "v3");

                    r0 += bottom_blob_tm.cstep * 8;
                }
            }
            for (; i + 1 < tiles; i += 2)
            {
//                 __fp16* tmpptr = tm2.row<__fp16>(i / 12 + (i % 12) / 8 + (i % 12 % 8) / 4 + (i % 12 % 4) / 2);
                short* tmpptr = tm2.row<short>(i / 4 + (i % 4) / 2);

                const short* r0 = bottom_blob_tm;

                r0 += (r * tiles + i) * 8;

                for (int q = 0; q < inch; q++)
                {
                    asm volatile(
                        "prfm   pldl1keep, [%0, #256]   \n"
                        "ld1    {v0.8h, v1.8h}, [%0]    \n"
                        "st1    {v0.8h, v1.8h}, [%1], #32 \n"
                        : "=r"(r0),    // %0
                        "=r"(tmpptr) // %1
                        : "0"(r0),
                        "1"(tmpptr)
                        : "memory", "v0", "v1");

                    r0 += bottom_blob_tm.cstep * 8;
                }
            }

            for (; i < tiles; i++)
            {
//                 __fp16* tmpptr = tm2.row<__fp16>(i / 12 + (i % 12) / 8 + (i % 12 % 8) / 4 + (i % 12 % 4) / 2 + i % 12 % 2);
                short* tmpptr = tm2.row<short>(i / 4 + (i % 4) / 2 + i % 2);

                const short* r0 = bottom_blob_tm;

                r0 += (r * tiles + i) * 8;

                for (int q = 0; q < inch; q++)
                {
                    asm volatile(
                        "prfm   pldl1keep, [%0, #128]   \n"
                        "ld1    {v0.8h}, [%0]           \n"
                        "st1    {v0.8h}, [%1], #16      \n"
                        : "=r"(r0),    // %0
                        "=r"(tmpptr) // %1
                        : "0"(r0),
                        "1"(tmpptr)
                        : "memory", "v0");

                    r0 += bottom_blob_tm.cstep * 8;
                }
            }
        }

        bottom_blob_tm = Mat();
        // permute end

        top_blob_tm.create(tiles, 36, outch, 4u * elempack, elempack, opt.workspace_allocator);

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int p = 0; p < outch; p++)
        {
            int* output0_tm = top_blob_tm.channel(p);

            const Mat kernel0_tm = kernel_tm.channel(p);

            for (int r = 0; r < 36; r++)
            {
                const Mat bb2 = bottom_blob_tm2.channel(r);

                int i = 0;
#if 0
                for (; i + 11 < tiles; i += 12)
                {
                    const __fp16* r0 = bb2.row<const __fp16>(i / 12);
                    const __fp16* k0 = kernel0_tm.row<const __fp16>(r);

                    int nn = inch; // inch always > 0

                    asm volatile(
                        "eor    v20.16b, v20.16b, v20.16b   \n"
                        "eor    v21.16b, v21.16b, v21.16b   \n"
                        "eor    v22.16b, v22.16b, v22.16b   \n"
                        "eor    v23.16b, v23.16b, v23.16b   \n"
                        "eor    v24.16b, v24.16b, v24.16b   \n"
                        "eor    v25.16b, v25.16b, v25.16b   \n"
                        "eor    v26.16b, v26.16b, v26.16b   \n"
                        "eor    v27.16b, v27.16b, v27.16b   \n"
                        "eor    v28.16b, v28.16b, v28.16b   \n"
                        "eor    v29.16b, v29.16b, v29.16b   \n"
                        "eor    v30.16b, v30.16b, v30.16b   \n"
                        "eor    v31.16b, v31.16b, v31.16b   \n"

                        "0:                                 \n"

                        "prfm   pldl1keep, [%2, #512]       \n"
                        "ld1    {v0.8h, v1.8h, v2.8h, v3.8h}, [%2], #64 \n" // r0123

                        "prfm   pldl1keep, [%3, #512]       \n"
                        "ld1    {v12.8h, v13.8h, v14.8h, v15.8h}, [%3], #64 \n" // w0123

                        "fmla   v20.8h, v12.8h, v0.h[0]     \n"
                        "fmla   v21.8h, v12.8h, v0.h[1]     \n"
                        "fmla   v22.8h, v12.8h, v0.h[2]     \n"
                        "fmla   v23.8h, v12.8h, v0.h[3]     \n"
                        "fmla   v24.8h, v12.8h, v0.h[4]     \n"
                        "fmla   v25.8h, v12.8h, v0.h[5]     \n"
                        "fmla   v26.8h, v12.8h, v0.h[6]     \n"
                        "fmla   v27.8h, v12.8h, v0.h[7]     \n"
                        "fmla   v28.8h, v12.8h, v1.h[0]     \n"
                        "fmla   v29.8h, v12.8h, v1.h[1]     \n"
                        "fmla   v30.8h, v12.8h, v1.h[2]     \n"
                        "fmla   v31.8h, v12.8h, v1.h[3]     \n"

                        "fmla   v20.8h, v13.8h, v1.h[4]     \n"
                        "fmla   v21.8h, v13.8h, v1.h[5]     \n"
                        "fmla   v22.8h, v13.8h, v1.h[6]     \n"
                        "fmla   v23.8h, v13.8h, v1.h[7]     \n"
                        "fmla   v24.8h, v13.8h, v2.h[0]     \n"
                        "fmla   v25.8h, v13.8h, v2.h[1]     \n"
                        "fmla   v26.8h, v13.8h, v2.h[2]     \n"
                        "fmla   v27.8h, v13.8h, v2.h[3]     \n"
                        "fmla   v28.8h, v13.8h, v2.h[4]     \n"
                        "fmla   v29.8h, v13.8h, v2.h[5]     \n"
                        "fmla   v30.8h, v13.8h, v2.h[6]     \n"
                        "fmla   v31.8h, v13.8h, v2.h[7]     \n"

                        "prfm   pldl1keep, [%2, #512]       \n"
                        "ld1    {v4.8h, v5.8h, v6.8h, v7.8h}, [%2], #64 \n" // r4567

                        "fmla   v20.8h, v14.8h, v3.h[0]     \n"
                        "fmla   v21.8h, v14.8h, v3.h[1]     \n"
                        "fmla   v22.8h, v14.8h, v3.h[2]     \n"
                        "fmla   v23.8h, v14.8h, v3.h[3]     \n"
                        "fmla   v24.8h, v14.8h, v3.h[4]     \n"
                        "fmla   v25.8h, v14.8h, v3.h[5]     \n"
                        "fmla   v26.8h, v14.8h, v3.h[6]     \n"
                        "fmla   v27.8h, v14.8h, v3.h[7]     \n"
                        "fmla   v28.8h, v14.8h, v4.h[0]     \n"
                        "fmla   v29.8h, v14.8h, v4.h[1]     \n"
                        "fmla   v30.8h, v14.8h, v4.h[2]     \n"
                        "fmla   v31.8h, v14.8h, v4.h[3]     \n"

                        "prfm   pldl1keep, [%3, #512]       \n"
                        "ld1    {v16.8h, v17.8h, v18.8h, v19.8h}, [%3], #64 \n" // w4567

                        "fmla   v20.8h, v15.8h, v4.h[4]     \n"
                        "fmla   v21.8h, v15.8h, v4.h[5]     \n"
                        "fmla   v22.8h, v15.8h, v4.h[6]     \n"
                        "fmla   v23.8h, v15.8h, v4.h[7]     \n"
                        "fmla   v24.8h, v15.8h, v5.h[0]     \n"
                        "fmla   v25.8h, v15.8h, v5.h[1]     \n"
                        "fmla   v26.8h, v15.8h, v5.h[2]     \n"
                        "fmla   v27.8h, v15.8h, v5.h[3]     \n"
                        "fmla   v28.8h, v15.8h, v5.h[4]     \n"
                        "fmla   v29.8h, v15.8h, v5.h[5]     \n"
                        "fmla   v30.8h, v15.8h, v5.h[6]     \n"
                        "fmla   v31.8h, v15.8h, v5.h[7]     \n"

                        "fmla   v20.8h, v16.8h, v6.h[0]     \n"
                        "fmla   v21.8h, v16.8h, v6.h[1]     \n"
                        "fmla   v22.8h, v16.8h, v6.h[2]     \n"
                        "fmla   v23.8h, v16.8h, v6.h[3]     \n"
                        "fmla   v24.8h, v16.8h, v6.h[4]     \n"
                        "fmla   v25.8h, v16.8h, v6.h[5]     \n"
                        "fmla   v26.8h, v16.8h, v6.h[6]     \n"
                        "fmla   v27.8h, v16.8h, v6.h[7]     \n"
                        "fmla   v28.8h, v16.8h, v7.h[0]     \n"
                        "fmla   v29.8h, v16.8h, v7.h[1]     \n"
                        "fmla   v30.8h, v16.8h, v7.h[2]     \n"
                        "fmla   v31.8h, v16.8h, v7.h[3]     \n"

                        "prfm   pldl1keep, [%2, #512]       \n"
                        "ld1    {v8.8h, v9.8h, v10.8h, v11.8h}, [%2], #64 \n" // r891011

                        "fmla   v20.8h, v17.8h, v7.h[4]     \n"
                        "fmla   v21.8h, v17.8h, v7.h[5]     \n"
                        "fmla   v22.8h, v17.8h, v7.h[6]     \n"
                        "fmla   v23.8h, v17.8h, v7.h[7]     \n"
                        "fmla   v24.8h, v17.8h, v8.h[0]     \n"
                        "fmla   v25.8h, v17.8h, v8.h[1]     \n"
                        "fmla   v26.8h, v17.8h, v8.h[2]     \n"
                        "fmla   v27.8h, v17.8h, v8.h[3]     \n"
                        "fmla   v28.8h, v17.8h, v8.h[4]     \n"
                        "fmla   v29.8h, v17.8h, v8.h[5]     \n"
                        "fmla   v30.8h, v17.8h, v8.h[6]     \n"
                        "fmla   v31.8h, v17.8h, v8.h[7]     \n"

                        "fmla   v20.8h, v18.8h, v9.h[0]     \n"
                        "fmla   v21.8h, v18.8h, v9.h[1]     \n"
                        "fmla   v22.8h, v18.8h, v9.h[2]     \n"
                        "fmla   v23.8h, v18.8h, v9.h[3]     \n"
                        "fmla   v24.8h, v18.8h, v9.h[4]     \n"
                        "fmla   v25.8h, v18.8h, v9.h[5]     \n"
                        "fmla   v26.8h, v18.8h, v9.h[6]     \n"
                        "fmla   v27.8h, v18.8h, v9.h[7]     \n"
                        "fmla   v28.8h, v18.8h, v10.h[0]    \n"
                        "fmla   v29.8h, v18.8h, v10.h[1]    \n"
                        "fmla   v30.8h, v18.8h, v10.h[2]    \n"
                        "fmla   v31.8h, v18.8h, v10.h[3]    \n"

                        "subs   %w0, %w0, #1                \n"

                        "fmla   v20.8h, v19.8h, v10.h[4]    \n"
                        "fmla   v21.8h, v19.8h, v10.h[5]    \n"
                        "fmla   v22.8h, v19.8h, v10.h[6]    \n"
                        "fmla   v23.8h, v19.8h, v10.h[7]    \n"
                        "fmla   v24.8h, v19.8h, v11.h[0]    \n"
                        "fmla   v25.8h, v19.8h, v11.h[1]    \n"
                        "fmla   v26.8h, v19.8h, v11.h[2]    \n"
                        "fmla   v27.8h, v19.8h, v11.h[3]    \n"
                        "fmla   v28.8h, v19.8h, v11.h[4]    \n"
                        "fmla   v29.8h, v19.8h, v11.h[5]    \n"
                        "fmla   v30.8h, v19.8h, v11.h[6]    \n"
                        "fmla   v31.8h, v19.8h, v11.h[7]    \n"

                        "bne    0b                          \n"

                        "st1    {v20.8h, v21.8h, v22.8h, v23.8h}, [%1], #64 \n"
                        "st1    {v24.8h, v25.8h, v26.8h, v27.8h}, [%1], #64 \n"
                        "st1    {v28.8h, v29.8h, v30.8h, v31.8h}, [%1], #64 \n"

                        : "=r"(nn),         // %0
                        "=r"(output0_tm), // %1
                        "=r"(r0),         // %2
                        "=r"(k0)          // %3
                        : "0"(nn),
                        "1"(output0_tm),
                        "2"(r0),
                        "3"(k0)
                        : "cc", "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31");
                }
                for (; i + 7 < tiles; i += 8)
                {
                    const __fp16* r0 = bb2.row<const __fp16>(i / 12 + (i % 12) / 8);
                    const __fp16* k0 = kernel0_tm.row<const __fp16>(r);

                    int nn = inch; // inch always > 0

                    asm volatile(
                        "eor    v16.16b, v16.16b, v16.16b   \n"
                        "eor    v17.16b, v17.16b, v17.16b   \n"
                        "eor    v18.16b, v18.16b, v18.16b   \n"
                        "eor    v19.16b, v19.16b, v19.16b   \n"
                        "eor    v20.16b, v20.16b, v20.16b   \n"
                        "eor    v21.16b, v21.16b, v21.16b   \n"
                        "eor    v22.16b, v22.16b, v22.16b   \n"
                        "eor    v23.16b, v23.16b, v23.16b   \n"

                        "0:                                 \n"

                        "prfm   pldl1keep, [%2, #512]       \n"
                        "ld1    {v0.8h, v1.8h, v2.8h, v3.8h}, [%2], #64 \n" // r0123

                        "prfm   pldl1keep, [%3, #512]       \n"
                        "ld1    {v8.8h, v9.8h, v10.8h, v11.8h}, [%3], #64 \n" // w0123

                        "fmla   v16.8h, v8.8h, v0.h[0]      \n"
                        "fmla   v17.8h, v8.8h, v0.h[1]      \n"
                        "fmla   v18.8h, v8.8h, v0.h[2]      \n"
                        "fmla   v19.8h, v8.8h, v0.h[3]      \n"
                        "fmla   v20.8h, v8.8h, v0.h[4]      \n"
                        "fmla   v21.8h, v8.8h, v0.h[5]      \n"
                        "fmla   v22.8h, v8.8h, v0.h[6]      \n"
                        "fmla   v23.8h, v8.8h, v0.h[7]      \n"

                        "fmla   v16.8h, v9.8h, v1.h[0]      \n"
                        "fmla   v17.8h, v9.8h, v1.h[1]      \n"
                        "fmla   v18.8h, v9.8h, v1.h[2]      \n"
                        "fmla   v19.8h, v9.8h, v1.h[3]      \n"
                        "fmla   v20.8h, v9.8h, v1.h[4]      \n"
                        "fmla   v21.8h, v9.8h, v1.h[5]      \n"
                        "fmla   v22.8h, v9.8h, v1.h[6]      \n"
                        "fmla   v23.8h, v9.8h, v1.h[7]      \n"

                        "prfm   pldl1keep, [%2, #512]       \n"
                        "ld1    {v4.8h, v5.8h, v6.8h, v7.8h}, [%2], #64 \n" // r4567

                        "fmla   v16.8h, v10.8h, v2.h[0]     \n"
                        "fmla   v17.8h, v10.8h, v2.h[1]     \n"
                        "fmla   v18.8h, v10.8h, v2.h[2]     \n"
                        "fmla   v19.8h, v10.8h, v2.h[3]     \n"
                        "fmla   v20.8h, v10.8h, v2.h[4]     \n"
                        "fmla   v21.8h, v10.8h, v2.h[5]     \n"
                        "fmla   v22.8h, v10.8h, v2.h[6]     \n"
                        "fmla   v23.8h, v10.8h, v2.h[7]     \n"

                        "prfm   pldl1keep, [%3, #512]       \n"
                        "ld1    {v12.8h, v13.8h, v14.8h, v15.8h}, [%3], #64 \n" // w4567

                        "fmla   v16.8h, v11.8h, v3.h[0]     \n"
                        "fmla   v17.8h, v11.8h, v3.h[1]     \n"
                        "fmla   v18.8h, v11.8h, v3.h[2]     \n"
                        "fmla   v19.8h, v11.8h, v3.h[3]     \n"
                        "fmla   v20.8h, v11.8h, v3.h[4]     \n"
                        "fmla   v21.8h, v11.8h, v3.h[5]     \n"
                        "fmla   v22.8h, v11.8h, v3.h[6]     \n"
                        "fmla   v23.8h, v11.8h, v3.h[7]     \n"

                        "fmla   v16.8h, v12.8h, v4.h[0]     \n"
                        "fmla   v17.8h, v12.8h, v4.h[1]     \n"
                        "fmla   v18.8h, v12.8h, v4.h[2]     \n"
                        "fmla   v19.8h, v12.8h, v4.h[3]     \n"
                        "fmla   v20.8h, v12.8h, v4.h[4]     \n"
                        "fmla   v21.8h, v12.8h, v4.h[5]     \n"
                        "fmla   v22.8h, v12.8h, v4.h[6]     \n"
                        "fmla   v23.8h, v12.8h, v4.h[7]     \n"

                        "fmla   v16.8h, v13.8h, v5.h[0]     \n"
                        "fmla   v17.8h, v13.8h, v5.h[1]     \n"
                        "fmla   v18.8h, v13.8h, v5.h[2]     \n"
                        "fmla   v19.8h, v13.8h, v5.h[3]     \n"
                        "fmla   v20.8h, v13.8h, v5.h[4]     \n"
                        "fmla   v21.8h, v13.8h, v5.h[5]     \n"
                        "fmla   v22.8h, v13.8h, v5.h[6]     \n"
                        "fmla   v23.8h, v13.8h, v5.h[7]     \n"

                        "fmla   v16.8h, v14.8h, v6.h[0]     \n"
                        "fmla   v17.8h, v14.8h, v6.h[1]     \n"
                        "fmla   v18.8h, v14.8h, v6.h[2]     \n"
                        "fmla   v19.8h, v14.8h, v6.h[3]     \n"
                        "fmla   v20.8h, v14.8h, v6.h[4]     \n"
                        "fmla   v21.8h, v14.8h, v6.h[5]     \n"
                        "fmla   v22.8h, v14.8h, v6.h[6]     \n"
                        "fmla   v23.8h, v14.8h, v6.h[7]     \n"

                        "subs   %w0, %w0, #1                \n"

                        "fmla   v16.8h, v15.8h, v7.h[0]     \n"
                        "fmla   v17.8h, v15.8h, v7.h[1]     \n"
                        "fmla   v18.8h, v15.8h, v7.h[2]     \n"
                        "fmla   v19.8h, v15.8h, v7.h[3]     \n"
                        "fmla   v20.8h, v15.8h, v7.h[4]     \n"
                        "fmla   v21.8h, v15.8h, v7.h[5]     \n"
                        "fmla   v22.8h, v15.8h, v7.h[6]     \n"
                        "fmla   v23.8h, v15.8h, v7.h[7]     \n"

                        "bne    0b                          \n"

                        "st1    {v16.8h, v17.8h, v18.8h, v19.8h}, [%1], #64 \n"

                        "st1    {v20.8h, v21.8h, v22.8h, v23.8h}, [%1], #64 \n"

                        : "=r"(nn),         // %0
                        "=r"(output0_tm), // %1
                        "=r"(r0),         // %2
                        "=r"(k0)          // %3
                        : "0"(nn),
                        "1"(output0_tm),
                        "2"(r0),
                        "3"(k0)
                        : "cc", "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23");
                }
#endif
                for (; i + 3 < tiles; i += 4)
                {
//                     const __fp16* r0 = bb2.row<const __fp16>(i / 12 + (i % 12) / 8 + (i % 12 % 8) / 4);
                    const short* r0 = bb2.row<const short>(i / 4);
                    const short* k0 = kernel0_tm.row<const short>(r);

                    int nn = inch; // inch always > 0

                    int32x4_t _sum0 = vdupq_n_s32(0);
                    int32x4_t _sum1 = vdupq_n_s32(0);
                    int32x4_t _sum2 = vdupq_n_s32(0);
                    int32x4_t _sum3 = vdupq_n_s32(0);
                    int32x4_t _sum4 = vdupq_n_s32(0);
                    int32x4_t _sum5 = vdupq_n_s32(0);
                    int32x4_t _sum6 = vdupq_n_s32(0);
                    int32x4_t _sum7 = vdupq_n_s32(0);

                    for (int j = 0; j < nn; j++)
                    {
                        int16x8_t _val0 = vld1q_s16(r0);
                        int16x8_t _val1 = vld1q_s16(r0 + 8);
                        int16x8_t _val2 = vld1q_s16(r0 + 16);
                        int16x8_t _val3 = vld1q_s16(r0 + 24);

                        int16x8_t _w0 = vld1q_s16(k0);
                        int16x8_t _w1 = vld1q_s16(k0 + 8);
                        int16x8_t _w2 = vld1q_s16(k0 + 16);
                        int16x8_t _w3 = vld1q_s16(k0 + 24);
                        int16x8_t _w4 = vld1q_s16(k0 + 32);
                        int16x8_t _w5 = vld1q_s16(k0 + 40);
                        int16x8_t _w6 = vld1q_s16(k0 + 48);
                        int16x8_t _w7 = vld1q_s16(k0 + 56);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w0), vget_low_s16(_val0), 0);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w0), vget_low_s16(_val0), 0);
                        _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_w0), vget_low_s16(_val1), 0);
                        _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_w0), vget_low_s16(_val1), 0);
                        _sum4 = vmlal_lane_s16(_sum4, vget_low_s16(_w0), vget_low_s16(_val2), 0);
                        _sum5 = vmlal_lane_s16(_sum5, vget_high_s16(_w0), vget_low_s16(_val2), 0);
                        _sum6 = vmlal_lane_s16(_sum6, vget_low_s16(_w0), vget_low_s16(_val3), 0);
                        _sum7 = vmlal_lane_s16(_sum7, vget_high_s16(_w0), vget_low_s16(_val3), 0);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w1), vget_low_s16(_val0), 1);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w1), vget_low_s16(_val0), 1);
                        _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_w1), vget_low_s16(_val1), 1);
                        _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_w1), vget_low_s16(_val1), 1);
                        _sum4 = vmlal_lane_s16(_sum4, vget_low_s16(_w1), vget_low_s16(_val2), 1);
                        _sum5 = vmlal_lane_s16(_sum5, vget_high_s16(_w1), vget_low_s16(_val2), 1);
                        _sum6 = vmlal_lane_s16(_sum6, vget_low_s16(_w1), vget_low_s16(_val3), 1);
                        _sum7 = vmlal_lane_s16(_sum7, vget_high_s16(_w1), vget_low_s16(_val3), 1);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w2), vget_low_s16(_val0), 2);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w2), vget_low_s16(_val0), 2);
                        _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_w2), vget_low_s16(_val1), 2);
                        _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_w2), vget_low_s16(_val1), 2);
                        _sum4 = vmlal_lane_s16(_sum4, vget_low_s16(_w2), vget_low_s16(_val2), 2);
                        _sum5 = vmlal_lane_s16(_sum5, vget_high_s16(_w2), vget_low_s16(_val2), 2);
                        _sum6 = vmlal_lane_s16(_sum6, vget_low_s16(_w2), vget_low_s16(_val3), 2);
                        _sum7 = vmlal_lane_s16(_sum7, vget_high_s16(_w2), vget_low_s16(_val3), 2);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w3), vget_low_s16(_val0), 3);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w3), vget_low_s16(_val0), 3);
                        _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_w3), vget_low_s16(_val1), 3);
                        _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_w3), vget_low_s16(_val1), 3);
                        _sum4 = vmlal_lane_s16(_sum4, vget_low_s16(_w3), vget_low_s16(_val2), 3);
                        _sum5 = vmlal_lane_s16(_sum5, vget_high_s16(_w3), vget_low_s16(_val2), 3);
                        _sum6 = vmlal_lane_s16(_sum6, vget_low_s16(_w3), vget_low_s16(_val3), 3);
                        _sum7 = vmlal_lane_s16(_sum7, vget_high_s16(_w3), vget_low_s16(_val3), 3);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w4), vget_high_s16(_val0), 0);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w4), vget_high_s16(_val0), 0);
                        _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_w4), vget_high_s16(_val1), 0);
                        _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_w4), vget_high_s16(_val1), 0);
                        _sum4 = vmlal_lane_s16(_sum4, vget_low_s16(_w4), vget_high_s16(_val2), 0);
                        _sum5 = vmlal_lane_s16(_sum5, vget_high_s16(_w4), vget_high_s16(_val2), 0);
                        _sum6 = vmlal_lane_s16(_sum6, vget_low_s16(_w4), vget_high_s16(_val3), 0);
                        _sum7 = vmlal_lane_s16(_sum7, vget_high_s16(_w4), vget_high_s16(_val3), 0);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w5), vget_high_s16(_val0), 1);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w5), vget_high_s16(_val0), 1);
                        _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_w5), vget_high_s16(_val1), 1);
                        _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_w5), vget_high_s16(_val1), 1);
                        _sum4 = vmlal_lane_s16(_sum4, vget_low_s16(_w5), vget_high_s16(_val2), 1);
                        _sum5 = vmlal_lane_s16(_sum5, vget_high_s16(_w5), vget_high_s16(_val2), 1);
                        _sum6 = vmlal_lane_s16(_sum6, vget_low_s16(_w5), vget_high_s16(_val3), 1);
                        _sum7 = vmlal_lane_s16(_sum7, vget_high_s16(_w5), vget_high_s16(_val3), 1);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w6), vget_high_s16(_val0), 2);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w6), vget_high_s16(_val0), 2);
                        _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_w6), vget_high_s16(_val1), 2);
                        _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_w6), vget_high_s16(_val1), 2);
                        _sum4 = vmlal_lane_s16(_sum4, vget_low_s16(_w6), vget_high_s16(_val2), 2);
                        _sum5 = vmlal_lane_s16(_sum5, vget_high_s16(_w6), vget_high_s16(_val2), 2);
                        _sum6 = vmlal_lane_s16(_sum6, vget_low_s16(_w6), vget_high_s16(_val3), 2);
                        _sum7 = vmlal_lane_s16(_sum7, vget_high_s16(_w6), vget_high_s16(_val3), 2);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w7), vget_high_s16(_val0), 3);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w7), vget_high_s16(_val0), 3);
                        _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_w7), vget_high_s16(_val1), 3);
                        _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_w7), vget_high_s16(_val1), 3);
                        _sum4 = vmlal_lane_s16(_sum4, vget_low_s16(_w7), vget_high_s16(_val2), 3);
                        _sum5 = vmlal_lane_s16(_sum5, vget_high_s16(_w7), vget_high_s16(_val2), 3);
                        _sum6 = vmlal_lane_s16(_sum6, vget_low_s16(_w7), vget_high_s16(_val3), 3);
                        _sum7 = vmlal_lane_s16(_sum7, vget_high_s16(_w7), vget_high_s16(_val3), 3);

                        r0 += 32;
                        k0 += 64;
                    }

                    vst1q_s32(output0_tm, _sum0);
                    vst1q_s32(output0_tm + 4, _sum1);
                    vst1q_s32(output0_tm + 8, _sum2);
                    vst1q_s32(output0_tm + 12, _sum3);
                    vst1q_s32(output0_tm + 16, _sum4);
                    vst1q_s32(output0_tm + 20, _sum5);
                    vst1q_s32(output0_tm + 24, _sum6);
                    vst1q_s32(output0_tm + 28, _sum7);
                    output0_tm += 32;
                }
                for (; i + 1 < tiles; i += 2)
                {
//                     const __fp16* r0 = bb2.row<const __fp16>(i / 12 + (i % 12) / 8 + (i % 12 % 8) / 4 + (i % 12 % 4) / 2);
                    const short* r0 = bb2.row<const short>(i / 4 + (i % 4) / 2);
                    const short* k0 = kernel0_tm.row<const short>(r);

                    int nn = inch; // inch always > 0

                    int32x4_t _sum0 = vdupq_n_s32(0);
                    int32x4_t _sum1 = vdupq_n_s32(0);
                    int32x4_t _sum2 = vdupq_n_s32(0);
                    int32x4_t _sum3 = vdupq_n_s32(0);

                    for (int j = 0; j < nn; j++)
                    {
                        int16x8_t _val0 = vld1q_s16(r0);
                        int16x8_t _val1 = vld1q_s16(r0 + 8);

                        int16x8_t _w0 = vld1q_s16(k0);
                        int16x8_t _w1 = vld1q_s16(k0 + 8);
                        int16x8_t _w2 = vld1q_s16(k0 + 16);
                        int16x8_t _w3 = vld1q_s16(k0 + 24);
                        int16x8_t _w4 = vld1q_s16(k0 + 32);
                        int16x8_t _w5 = vld1q_s16(k0 + 40);
                        int16x8_t _w6 = vld1q_s16(k0 + 48);
                        int16x8_t _w7 = vld1q_s16(k0 + 56);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w0), vget_low_s16(_val0), 0);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w0), vget_low_s16(_val0), 0);
                        _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_w0), vget_low_s16(_val1), 0);
                        _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_w0), vget_low_s16(_val1), 0);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w1), vget_low_s16(_val0), 1);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w1), vget_low_s16(_val0), 1);
                        _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_w1), vget_low_s16(_val1), 1);
                        _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_w1), vget_low_s16(_val1), 1);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w2), vget_low_s16(_val0), 2);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w2), vget_low_s16(_val0), 2);
                        _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_w2), vget_low_s16(_val1), 2);
                        _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_w2), vget_low_s16(_val1), 2);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w3), vget_low_s16(_val0), 3);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w3), vget_low_s16(_val0), 3);
                        _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_w3), vget_low_s16(_val1), 3);
                        _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_w3), vget_low_s16(_val1), 3);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w4), vget_high_s16(_val0), 0);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w4), vget_high_s16(_val0), 0);
                        _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_w4), vget_high_s16(_val1), 0);
                        _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_w4), vget_high_s16(_val1), 0);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w5), vget_high_s16(_val0), 1);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w5), vget_high_s16(_val0), 1);
                        _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_w5), vget_high_s16(_val1), 1);
                        _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_w5), vget_high_s16(_val1), 1);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w6), vget_high_s16(_val0), 2);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w6), vget_high_s16(_val0), 2);
                        _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_w6), vget_high_s16(_val1), 2);
                        _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_w6), vget_high_s16(_val1), 2);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w7), vget_high_s16(_val0), 3);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w7), vget_high_s16(_val0), 3);
                        _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_w7), vget_high_s16(_val1), 3);
                        _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_w7), vget_high_s16(_val1), 3);

                        r0 += 16;
                        k0 += 64;
                    }

                    vst1q_s32(output0_tm, _sum0);
                    vst1q_s32(output0_tm + 4, _sum1);
                    vst1q_s32(output0_tm + 8, _sum2);
                    vst1q_s32(output0_tm + 12, _sum3);
                    output0_tm += 16;
                }
                for (; i < tiles; i++)
                {
//                     const __fp16* r0 = bb2.row<const __fp16>(i / 12 + (i % 12) / 8 + (i % 12 % 8) / 4 + (i % 12 % 4) / 2 + i % 12 % 2);
                    const short* r0 = bb2.row<const short>(i / 4 + (i % 4) / 2 + i % 2);
                    const short* k0 = kernel0_tm.row<const short>(r);

                    int nn = inch; // inch always > 0

                    int32x4_t _sum0 = vdupq_n_s32(0);
                    int32x4_t _sum1 = vdupq_n_s32(0);

                    for (int j = 0; j < nn; j++)
                    {
                        int16x8_t _val0 = vld1q_s16(r0);

                        int16x8_t _w0 = vld1q_s16(k0);
                        int16x8_t _w1 = vld1q_s16(k0 + 8);
                        int16x8_t _w2 = vld1q_s16(k0 + 16);
                        int16x8_t _w3 = vld1q_s16(k0 + 24);
                        int16x8_t _w4 = vld1q_s16(k0 + 32);
                        int16x8_t _w5 = vld1q_s16(k0 + 40);
                        int16x8_t _w6 = vld1q_s16(k0 + 48);
                        int16x8_t _w7 = vld1q_s16(k0 + 56);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w0), vget_low_s16(_val0), 0);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w0), vget_low_s16(_val0), 0);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w1), vget_low_s16(_val0), 1);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w1), vget_low_s16(_val0), 1);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w2), vget_low_s16(_val0), 2);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w2), vget_low_s16(_val0), 2);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w3), vget_low_s16(_val0), 3);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w3), vget_low_s16(_val0), 3);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w4), vget_high_s16(_val0), 0);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w4), vget_high_s16(_val0), 0);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w5), vget_high_s16(_val0), 1);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w5), vget_high_s16(_val0), 1);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w6), vget_high_s16(_val0), 2);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w6), vget_high_s16(_val0), 2);

                        _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_w7), vget_high_s16(_val0), 3);
                        _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_w7), vget_high_s16(_val0), 3);

                        r0 += 8;
                        k0 += 64;
                    }

                    vst1q_s32(output0_tm, _sum0);
                    vst1q_s32(output0_tm + 4, _sum1);
                    output0_tm += 8;
                }
            }
        }
    }
    bottom_blob_tm = Mat();
    // END dot

    // BEGIN transform output
    Mat top_blob_bordered;
    if (outw == top_blob.w && outh == top_blob.h)
    {
        top_blob_bordered = top_blob;
    }
    else
    {
        top_blob_bordered.create(outw, outh, outch, 4u * elempack, elempack, opt.workspace_allocator);
    }
    {
        // const float otm[4][6] = {
        //     {1.0f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f},
        //     {0.0f, 1.0f, -1.0f, 2.0f, -2.0f, 0.0f},
        //     {0.0f, 1.0f,  1.0f, 4.0f,  4.0f, 0.0f},
        //     {0.0f, 1.0f, -1.0f, 8.0f, -8.0f, 1.0f}
        // };

        // 0 = r00 + (r01 + r02) + (r03 + r04)
        // 1 =       (r01 - r02) + (r03 - r04) * 2
        // 2 =       (r01 + r02) + (r03 + r04) * 4
        // 3 = r05 + (r01 - r02) + (r03 - r04) * 8

        int w_tm = outw / 4 * 6;
        int h_tm = outh / 4 * 6;
        const int tiles = w_tm / 6 * h_tm / 6;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int p = 0; p < outch; p++)
        {
            const Mat out0_tm = top_blob_tm.channel(p);
            Mat out0 = top_blob_bordered.channel(p);

            int tmp[4][6][8];

            // tile
            for (int i = 0; i < outh / 4; i++)
            {
                for (int j = 0; j < outw / 4; j++)
                {
                    // top_blob_tm.create(tiles, 36, outch, elemsize, elempack);

                    const int* output0_tm_0 = (const int*)out0_tm + (i * w_tm / 6 + j) * 8;
                    const int* output0_tm_1 = output0_tm_0 + tiles * 8;
                    const int* output0_tm_2 = output0_tm_0 + tiles * 16;
                    const int* output0_tm_3 = output0_tm_0 + tiles * 24;
                    const int* output0_tm_4 = output0_tm_0 + tiles * 32;
                    const int* output0_tm_5 = output0_tm_0 + tiles * 40;

                    int* output0 = out0.row<int>(i * 4) + (j * 4) * 8;

                    // TODO neon optimize
                    for (int m = 0; m < 5; m++)
                    {
                        int32x4_t _out0tm0_low  = vld1q_s32(output0_tm_0);
                        int32x4_t _out0tm0_high = vld1q_s32(output0_tm_0 + 4);
                        int32x4_t _out0tm1_low  = vld1q_s32(output0_tm_1);
                        int32x4_t _out0tm1_high = vld1q_s32(output0_tm_1 + 4);
                        int32x4_t _out0tm2_low  = vld1q_s32(output0_tm_2);
                        int32x4_t _out0tm2_high = vld1q_s32(output0_tm_2 + 4);
                        int32x4_t _out0tm3_low  = vld1q_s32(output0_tm_3);
                        int32x4_t _out0tm3_high = vld1q_s32(output0_tm_3 + 4);
                        int32x4_t _out0tm4_low  = vld1q_s32(output0_tm_4);
                        int32x4_t _out0tm4_high = vld1q_s32(output0_tm_4 + 4);
                        int32x4_t _out0tm5_low  = vld1q_s32(output0_tm_5);
                        int32x4_t _out0tm5_high = vld1q_s32(output0_tm_5 + 4);

                        int32x4_t _tmp02a_low  = vaddq_s32(_out0tm1_low , _out0tm2_low );
                        int32x4_t _tmp02a_high = vaddq_s32(_out0tm1_high, _out0tm2_high);
                        int32x4_t _tmp13a_low  = vsubq_s32(_out0tm1_low , _out0tm2_low );
                        int32x4_t _tmp13a_high = vsubq_s32(_out0tm1_high, _out0tm2_high);

                        int32x4_t _tmp02b_low  = vaddq_s32(_out0tm3_low , _out0tm4_low );
                        int32x4_t _tmp02b_high = vaddq_s32(_out0tm3_high, _out0tm4_high);
                        int32x4_t _tmp13b_low  = vsubq_s32(_out0tm3_low , _out0tm4_low );
                        int32x4_t _tmp13b_high = vsubq_s32(_out0tm3_high, _out0tm4_high);

                        int32x4_t _v2 = vdupq_n_s32(2);
                        int32x4_t _v4 = vdupq_n_s32(4);
                        int32x4_t _v8 = vdupq_n_s32(8);

                        int32x4_t _tmp0m_low  = vaddq_s32(vaddq_s32(_out0tm0_low , _tmp02a_low ), _tmp02b_low );
                        int32x4_t _tmp0m_high = vaddq_s32(vaddq_s32(_out0tm0_high, _tmp02a_high), _tmp02b_high);
                        int32x4_t _tmp1m_low  = vmlaq_s32(_tmp13a_low , _tmp13b_low , _v2);
                        int32x4_t _tmp1m_high = vmlaq_s32(_tmp13a_high, _tmp13b_high, _v2);
                        int32x4_t _tmp2m_low  = vmlaq_s32(_tmp02a_low , _tmp02b_low , _v4);
                        int32x4_t _tmp2m_high = vmlaq_s32(_tmp02a_high, _tmp02b_high, _v4);
                        int32x4_t _tmp3m_low  = vmlaq_s32(vmlaq_s32(_tmp13a_low , _out0tm5_low , _v4), _tmp13b_low , _v8);
                        int32x4_t _tmp3m_high = vmlaq_s32(vmlaq_s32(_tmp13a_high, _out0tm5_high, _v4), _tmp13b_high, _v8);

                        vst1q_s32(tmp[0][m], _tmp0m_low );
                        vst1q_s32(tmp[0][m] + 4, _tmp0m_high);
                        vst1q_s32(tmp[1][m], _tmp1m_low );
                        vst1q_s32(tmp[1][m] + 4, _tmp1m_high);
                        vst1q_s32(tmp[2][m], _tmp2m_low );
                        vst1q_s32(tmp[2][m] + 4, _tmp2m_high);
                        vst1q_s32(tmp[3][m], _tmp3m_low );
                        vst1q_s32(tmp[3][m] + 4, _tmp3m_high);

                        output0_tm_0 += tiles * 48;
                        output0_tm_1 += tiles * 48;
                        output0_tm_2 += tiles * 48;
                        output0_tm_3 += tiles * 48;
                        output0_tm_4 += tiles * 48;
                        output0_tm_5 += tiles * 48;
                    }
                    for (int m = 5; m < 6; m++)
                    {
                        int32x4_t _out0tm0_low  = vld1q_s32(output0_tm_0);
                        int32x4_t _out0tm0_high = vld1q_s32(output0_tm_0 + 4);
                        int32x4_t _out0tm1_low  = vld1q_s32(output0_tm_1);
                        int32x4_t _out0tm1_high = vld1q_s32(output0_tm_1 + 4);
                        int32x4_t _out0tm2_low  = vld1q_s32(output0_tm_2);
                        int32x4_t _out0tm2_high = vld1q_s32(output0_tm_2 + 4);
                        int32x4_t _out0tm3_low  = vld1q_s32(output0_tm_3);
                        int32x4_t _out0tm3_high = vld1q_s32(output0_tm_3 + 4);
                        int32x4_t _out0tm4_low  = vld1q_s32(output0_tm_4);
                        int32x4_t _out0tm4_high = vld1q_s32(output0_tm_4 + 4);
                        int32x4_t _out0tm5_low  = vld1q_s32(output0_tm_5);
                        int32x4_t _out0tm5_high = vld1q_s32(output0_tm_5 + 4);

                        int32x4_t _tmp02a_low  = vaddq_s32(_out0tm1_low , _out0tm2_low );
                        int32x4_t _tmp02a_high = vaddq_s32(_out0tm1_high, _out0tm2_high);
                        int32x4_t _tmp13a_low  = vsubq_s32(_out0tm1_low , _out0tm2_low );
                        int32x4_t _tmp13a_high = vsubq_s32(_out0tm1_high, _out0tm2_high);

                        int32x4_t _tmp02b_low  = vaddq_s32(_out0tm3_low , _out0tm4_low );
                        int32x4_t _tmp02b_high = vaddq_s32(_out0tm3_high, _out0tm4_high);
                        int32x4_t _tmp13b_low  = vsubq_s32(_out0tm3_low , _out0tm4_low );
                        int32x4_t _tmp13b_high = vsubq_s32(_out0tm3_high, _out0tm4_high);

                        int32x4_t _v2 = vdupq_n_s32(2);
                        int32x4_t _v4 = vdupq_n_s32(4);
                        int32x4_t _v8 = vdupq_n_s32(8);

                        int32x4_t _tmp0m_low  = vaddq_s32(vaddq_s32(_out0tm0_low , _tmp02a_low ), _tmp02b_low );
                        int32x4_t _tmp0m_high = vaddq_s32(vaddq_s32(_out0tm0_high, _tmp02a_high), _tmp02b_high);
                        int32x4_t _tmp1m_low  = vmlaq_s32(_tmp13a_low , _tmp13b_low , _v2);
                        int32x4_t _tmp1m_high = vmlaq_s32(_tmp13a_high, _tmp13b_high, _v2);
                        int32x4_t _tmp2m_low  = vmlaq_s32(_tmp02a_low , _tmp02b_low , _v4);
                        int32x4_t _tmp2m_high = vmlaq_s32(_tmp02a_high, _tmp02b_high, _v4);
                        int32x4_t _tmp3m_low  = vmlaq_s32(vmlaq_s32(_tmp13a_low , _out0tm5_low , _v4), _tmp13b_low , _v8);
                        int32x4_t _tmp3m_high = vmlaq_s32(vmlaq_s32(_tmp13a_high, _out0tm5_high, _v4), _tmp13b_high, _v8);

                        _tmp0m_low  = vmulq_s32(_tmp0m_low , _v4);
                        _tmp0m_high = vmulq_s32(_tmp0m_high, _v4);
                        _tmp1m_low  = vmulq_s32(_tmp1m_low , _v4);
                        _tmp1m_high = vmulq_s32(_tmp1m_high, _v4);
                        _tmp2m_low  = vmulq_s32(_tmp2m_low , _v4);
                        _tmp2m_high = vmulq_s32(_tmp2m_high, _v4);
                        _tmp3m_low  = vmulq_s32(_tmp3m_low , _v4);
                        _tmp3m_high = vmulq_s32(_tmp3m_high, _v4);

                        vst1q_s32(tmp[0][m], _tmp0m_low );
                        vst1q_s32(tmp[0][m] + 4, _tmp0m_high);
                        vst1q_s32(tmp[1][m], _tmp1m_low );
                        vst1q_s32(tmp[1][m] + 4, _tmp1m_high);
                        vst1q_s32(tmp[2][m], _tmp2m_low );
                        vst1q_s32(tmp[2][m] + 4, _tmp2m_high);
                        vst1q_s32(tmp[3][m], _tmp3m_low );
                        vst1q_s32(tmp[3][m] + 4, _tmp3m_high);

                        output0_tm_0 += tiles * 48;
                        output0_tm_1 += tiles * 48;
                        output0_tm_2 += tiles * 48;
                        output0_tm_3 += tiles * 48;
                        output0_tm_4 += tiles * 48;
                        output0_tm_5 += tiles * 48;
                    }

                    for (int m = 0; m < 4; m++)
                    {
                        int32x4_t _tmp00_low  = vld1q_s32(tmp[m][0]);
                        int32x4_t _tmp00_high = vld1q_s32(tmp[m][0] + 4);
                        int32x4_t _tmp01_low  = vld1q_s32(tmp[m][1]);
                        int32x4_t _tmp01_high = vld1q_s32(tmp[m][1] + 4);
                        int32x4_t _tmp02_low  = vld1q_s32(tmp[m][2]);
                        int32x4_t _tmp02_high = vld1q_s32(tmp[m][2] + 4);
                        int32x4_t _tmp03_low  = vld1q_s32(tmp[m][3]);
                        int32x4_t _tmp03_high = vld1q_s32(tmp[m][3] + 4);
                        int32x4_t _tmp04_low  = vld1q_s32(tmp[m][4]);
                        int32x4_t _tmp04_high = vld1q_s32(tmp[m][4] + 4);
                        int32x4_t _tmp05_low  = vld1q_s32(tmp[m][5]);
                        int32x4_t _tmp05_high = vld1q_s32(tmp[m][5] + 4);

                        int32x4_t _tmp02a_low  = vaddq_s32(_tmp01_low , _tmp02_low );
                        int32x4_t _tmp02a_high = vaddq_s32(_tmp01_high, _tmp02_high);
                        int32x4_t _tmp13a_low  = vsubq_s32(_tmp01_low , _tmp02_low );
                        int32x4_t _tmp13a_high = vsubq_s32(_tmp01_high, _tmp02_high);

                        int32x4_t _tmp02b_low  = vaddq_s32(_tmp03_low , _tmp04_low );
                        int32x4_t _tmp02b_high = vaddq_s32(_tmp03_high, _tmp04_high);
                        int32x4_t _tmp13b_low  = vsubq_s32(_tmp03_low , _tmp04_low );
                        int32x4_t _tmp13b_high = vsubq_s32(_tmp03_high, _tmp04_high);

                        int32x4_t _v2 = vdupq_n_s32(2);
                        int32x4_t _v4 = vdupq_n_s32(4);
                        int32x4_t _v8 = vdupq_n_s32(8);

                        int32x4_t _out00_low  = vaddq_s32(vaddq_s32(_tmp00_low , _tmp02a_low ), _tmp02b_low );
                        int32x4_t _out00_high = vaddq_s32(vaddq_s32(_tmp00_high, _tmp02a_high), _tmp02b_high);
                        int32x4_t _out01_low  = vmlaq_s32(_tmp13a_low , _tmp13b_low , _v2);
                        int32x4_t _out01_high = vmlaq_s32(_tmp13a_high, _tmp13b_high, _v2);
                        int32x4_t _out02_low  = vmlaq_s32(_tmp02a_low , _tmp02b_low , _v4);
                        int32x4_t _out02_high = vmlaq_s32(_tmp02a_high, _tmp02b_high, _v4);
                        int32x4_t _out03_low  = vmlaq_s32(vaddq_s32(_tmp05_low , _tmp13a_low ), _tmp13b_low , _v8);
                        int32x4_t _out03_high = vmlaq_s32(vaddq_s32(_tmp05_high, _tmp13a_high), _tmp13b_high, _v8);

                        // TODO use integer trick for division by 576
                        float32x4_t _v576 = vdupq_n_f32(1.0 / 576);
                        _out00_low  = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_out00_low ), _v576));
                        _out00_high = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_out00_high), _v576));
                        _out01_low  = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_out01_low ), _v576));
                        _out01_high = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_out01_high), _v576));
                        _out02_low  = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_out02_low ), _v576));
                        _out02_high = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_out02_high), _v576));
                        _out03_low  = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_out03_low ), _v576));
                        _out03_high = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_out03_high), _v576));

                        vst1q_s32(output0, _out00_low);
                        vst1q_s32(output0 + 4, _out00_high);
                        vst1q_s32(output0 + 8, _out01_low);
                        vst1q_s32(output0 + 12, _out01_high);
                        vst1q_s32(output0 + 16, _out02_low );
                        vst1q_s32(output0 + 20, _out02_high);
                        vst1q_s32(output0 + 24, _out03_low );
                        vst1q_s32(output0 + 28, _out03_high);

                        output0 += outw * 8;
                    }
                }
            }
        }
    }
    // END transform output

    // cut result pad
    copy_cut_border(top_blob_bordered, top_blob, 0, top_blob_bordered.h - top_blob.h, 0, top_blob_bordered.w - top_blob.w, opt);
}
