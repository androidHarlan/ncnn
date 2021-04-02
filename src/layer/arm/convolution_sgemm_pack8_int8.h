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

static void im2col_sgemm_pack8_int8_neon(const Mat& bottom_im2col, Mat& top_blob, const Mat& kernel, const Option& opt)
{
    // Mat bottom_im2col(size, maxk, inch, 8u, 8, opt.workspace_allocator);

    const int size = bottom_im2col.w;
    const int maxk = bottom_im2col.h;
    const int inch = bottom_im2col.c;

    const int outch = top_blob.c;

    // permute
    Mat tmp;
#if __aarch64__
#if __ARM_FEATURE_DOTPROD
    if (size >= 12)
        tmp.create(12 * maxk, inch, size / 12 + (size % 12) / 8 + (size % 12 % 8) / 4 + (size % 12 % 4) / 2 + size % 12 % 2, 8u, 8, opt.workspace_allocator);
    else if (size >= 8)
        tmp.create(8 * maxk, inch, size / 8 + (size % 8) / 4 + (size % 4) / 2 + size % 2, 8u, 8, opt.workspace_allocator);
    else if (size >= 4)
        tmp.create(4 * maxk, inch, size / 4 + (size % 4) / 2 + size % 2, 8u, 8, opt.workspace_allocator);
    else if (size >= 2)
        tmp.create(2 * maxk, inch, size / 2 + size % 2, 8u, 8, opt.workspace_allocator);
    else
        tmp.create(maxk, inch, size, 8u, 8, opt.workspace_allocator);
#else  // __ARM_FEATURE_DOTPROD
    if (size >= 4)
        tmp.create(4 * maxk, inch, size / 4 + (size % 4) / 2 + size % 2, 8u, 8, opt.workspace_allocator);
    else if (size >= 2)
        tmp.create(2 * maxk, inch, size / 2 + size % 2, 8u, 8, opt.workspace_allocator);
    else
        tmp.create(maxk, inch, size, 8u, 8, opt.workspace_allocator);
#endif // __ARM_FEATURE_DOTPROD
#else  // __aarch64__
    if (size >= 2)
        tmp.create(2 * maxk, inch, size / 2 + size % 2, 8u, 8, opt.workspace_allocator);
    else
        tmp.create(maxk, inch, size, 8u, 8, opt.workspace_allocator);
#endif // __aarch64__
    {
#if __aarch64__
#if __ARM_FEATURE_DOTPROD
        int nn_size = size / 12;
        int remain_size_start = 0;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int ii = 0; ii < nn_size; ii++)
        {
            int i = remain_size_start + ii * 12;

            signed char* tmpptr = tmp.channel(i / 12);

            for (int q = 0; q < inch; q++)
            {
                const signed char* img0 = (const signed char*)bottom_im2col.channel(q) + i * 8;

                for (int k = 0; k < maxk; k++)
                {
                    asm volatile(
                        "prfm   pldl1keep, [%0, #512]       \n"
                        "ld1    {v0.16b, v1.16b, v2.16b, v3.16b}, [%0], #64 \n"
                        "ld1    {v4.16b, v5.16b}, [%0]      \n"
                        "sub    %0, %0, #64                 \n"
                        "st1    {v0.16b, v1.16b, v2.16b, v3.16b}, [%1], #64 \n"
                        "st1    {v4.16b, v5.16b}, [%1], #32 \n"
                        : "=r"(img0),  // %0
                        "=r"(tmpptr) // %1
                        : "0"(img0),
                        "1"(tmpptr)
                        : "memory", "v0", "v1", "v2", "v3", "v4", "v5");
                    img0 += size * 8;
                }
            }
        }

        remain_size_start += nn_size * 12;
        nn_size = (size - remain_size_start) >> 3;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int ii = 0; ii < nn_size; ii++)
        {
            int i = remain_size_start + ii * 8;

            signed char* tmpptr = tmp.channel(i / 12 + (i % 12) / 8);

            for (int q = 0; q < inch; q++)
            {
                const signed char* img0 = (const signed char*)bottom_im2col.channel(q) + i * 8;

                for (int k = 0; k < maxk; k++)
                {
                    asm volatile(
                        "prfm   pldl1keep, [%0, #512]       \n"
                        "ld1    {v0.16b, v1.16b, v2.16b, v3.16b}, [%0]      \n"
                        "st1    {v0.16b, v1.16b, v2.16b, v3.16b}, [%1], #64 \n"
                        : "=r"(img0),  // %0
                        "=r"(tmpptr) // %1
                        : "0"(img0),
                        "1"(tmpptr)
                        : "memory", "v0", "v1", "v2", "v3");
                    img0 += size * 8;
                }
            }
        }

        remain_size_start += nn_size << 3;
        nn_size = (size - remain_size_start) >> 2;
#else  // __ARM_FEATURE_DOTPROD
        int remain_size_start = 0;
        int nn_size = (size - remain_size_start) >> 2;
#endif // __ARM_FEATURE_DOTPROD

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int ii = 0; ii < nn_size; ii++)
        {
            int i = remain_size_start + ii * 4;

#if __ARM_FEATURE_DOTPROD
            signed char* tmpptr = tmp.channel(i / 12 + (i % 12) / 8 + (i % 12 % 8) / 4);
#else
            signed char* tmpptr = tmp.channel(i / 4);
#endif

            for (int q = 0; q < inch; q++)
            {
                const signed char* img0 = (const signed char*)bottom_im2col.channel(q) + i * 8;

                for (int k = 0; k < maxk; k++)
                {
                    asm volatile(
                        "prfm   pldl1keep, [%0, #256]       \n"
                        "ld1    {v0.16b, v1.16b}, [%0]      \n"
                        "st1    {v0.16b, v1.16b}, [%1], #32 \n"
                        : "=r"(img0),  // %0
                        "=r"(tmpptr) // %1
                        : "0"(img0),
                        "1"(tmpptr)
                        : "memory", "v0", "v1");
                    img0 += size * 8;
                }
            }
        }

        remain_size_start += nn_size << 2;
        nn_size = (size - remain_size_start) >> 1;
#else
        int remain_size_start = 0;
        int nn_size = (size - remain_size_start) >> 1;
#endif

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int ii = 0; ii < nn_size; ii++)
        {
            int i = remain_size_start + ii * 2;

#if __aarch64__
#if __ARM_FEATURE_DOTPROD
            signed char* tmpptr = tmp.channel(i / 12 + (i % 12) / 8 + (i % 12 % 8) / 4 + (i % 12 % 4) / 2);
#else
            signed char* tmpptr = tmp.channel(i / 4 + (i % 4) / 2);
#endif
#else
            signed char* tmpptr = tmp.channel(i / 2);
#endif

            for (int q = 0; q < inch; q++)
            {
                const signed char* img0 = (const signed char*)bottom_im2col.channel(q) + i * 8;

                for (int k = 0; k < maxk; k++)
                {
#if __aarch64__
                    asm volatile(
                        "prfm   pldl1keep, [%0, #128]   \n"
                        "ld1    {v0.16b}, [%0]          \n"
                        "st1    {v0.16b}, [%1], #16     \n"
                        : "=r"(img0),  // %0
                        "=r"(tmpptr) // %1
                        : "0"(img0),
                        "1"(tmpptr)
                        : "memory", "v0");
#else
                    asm volatile(
                        "pld        [%0, #128]          \n"
                        "vld1.s8    {d0-d1}, [%0 :64]   \n"
                        "vst1.s8    {d0-d1}, [%1 :64]!  \n"
                        : "=r"(img0),  // %0
                        "=r"(tmpptr) // %1
                        : "0"(img0),
                        "1"(tmpptr)
                        : "memory", "q0");
#endif
                    img0 += size * 8;
                }
            }
        }

        remain_size_start += nn_size << 1;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int i = remain_size_start; i < size; i++)
        {
#if __aarch64__
#if __ARM_FEATURE_DOTPROD
            signed char* tmpptr = tmp.channel(i / 12 + (i % 12) / 8 + (i % 12 % 8) / 4 + (i % 12 % 4) / 2 + i % 12 % 2);
#else
            signed char* tmpptr = tmp.channel(i / 4 + (i % 4) / 2 + i % 2);
#endif
#else
            signed char* tmpptr = tmp.channel(i / 2 + i % 2);
#endif

            for (int q = 0; q < inch; q++)
            {
                const signed char* img0 = (const signed char*)bottom_im2col.channel(q) + i * 8;

                for (int k = 0; k < maxk; k++)
                {
#if __aarch64__
                    asm volatile(
                        "prfm   pldl1keep, [%0, #64]    \n"
                        "ld1    {v0.8b}, [%0]           \n"
                        "st1    {v0.8b}, [%1], #8       \n"
                        : "=r"(img0),  // %0
                        "=r"(tmpptr) // %1
                        : "0"(img0),
                        "1"(tmpptr)
                        : "memory", "v0");
#else
                    asm volatile(
                        "pld        [%0, #64]           \n"
                        "vld1.s8    {d0}, [%0 :64]      \n"
                        "vst1.s8    {d0}, [%1 :64]!     \n"
                        : "=r"(img0),  // %0
                        "=r"(tmpptr) // %1
                        : "0"(img0),
                        "1"(tmpptr)
                        : "memory", "d0");
#endif
                    img0 += size * 8;
                }
            }
        }
    }

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int p = 0; p < outch; p++)
    {
        int* outptr0 = top_blob.channel(p);

        int i = 0;
#if __aarch64__
#if __ARM_FEATURE_DOTPROD
        for (; i + 11 < size; i += 12)
        {
            const signed char* tmpptr = tmp.channel(i / 12);
            const signed char* kptr0 = kernel.channel(p);

            int nn = inch * maxk; // inch always > 0

            asm volatile(
                "prfm   pldl1keep, [%2, #128]       \n"

                "eor    v0.16b, v0.16b, v0.16b      \n"
                "eor    v1.16b, v1.16b, v1.16b      \n"
                "eor    v2.16b, v2.16b, v2.16b      \n"
                "eor    v3.16b, v3.16b, v3.16b      \n"

                "prfm   pldl1keep, [%3, #256]       \n"

                "eor    v4.16b, v4.16b, v4.16b      \n"
                "eor    v5.16b, v5.16b, v5.16b      \n"
                "eor    v6.16b, v6.16b, v6.16b      \n"
                "eor    v7.16b, v7.16b, v7.16b      \n"
                "eor    v8.16b, v8.16b, v8.16b      \n"
                "eor    v9.16b, v9.16b, v9.16b      \n"
                "eor    v10.16b, v10.16b, v10.16b   \n"
                "eor    v11.16b, v11.16b, v11.16b   \n"
                "eor    v12.16b, v12.16b, v12.16b   \n"
                "eor    v13.16b, v13.16b, v13.16b   \n"
                "eor    v14.16b, v14.16b, v14.16b   \n"
                "eor    v15.16b, v15.16b, v15.16b   \n"
                "eor    v16.16b, v16.16b, v16.16b   \n"
                "eor    v17.16b, v17.16b, v17.16b   \n"
                "eor    v18.16b, v18.16b, v18.16b   \n"
                "eor    v19.16b, v19.16b, v19.16b   \n"

                "ld1    {v28.16b}, [%2], #16        \n" // _val0

                "eor    v20.16b, v20.16b, v20.16b   \n"
                "eor    v21.16b, v21.16b, v21.16b   \n"
                "eor    v22.16b, v22.16b, v22.16b   \n"
                "eor    v23.16b, v23.16b, v23.16b   \n"

                "0:                                 \n"

                "ld1    {v24.16b, v25.16b}, [%3], #32 \n" // _w01 _w23

                "ld1    {v29.16b}, [%2], #16        \n" // _val1

                "ext    v26.16b, v24.16b, v25.16b, #8 \n" // _w12

                "prfm   pldl1keep, [%2, #128]       \n"

                "sdot   v0.4s, v28.16b, v24.16b     \n"
                "sdot   v1.4s, v28.16b, v26.16b     \n"

                "ext    v27.16b, v25.16b, v24.16b, #8 \n" // _w30

                "sdot   v2.4s, v28.16b, v25.16b     \n"
                "sdot   v3.4s, v28.16b, v27.16b     \n"

                "ld1    {v30.16b, v31.16b}, [%2], #32 \n" // _val0 _val1

                "sdot   v4.4s, v29.16b, v24.16b     \n"
                "sdot   v5.4s, v29.16b, v26.16b     \n"

                "prfm   pldl1keep, [%2, #128]       \n"

                "sdot   v6.4s, v29.16b, v25.16b     \n"
                "sdot   v7.4s, v29.16b, v27.16b     \n"

                "ld1    {v28.16b}, [%2], #16        \n" // _val0

                "sdot   v8.4s, v30.16b, v24.16b     \n"
                "sdot   v9.4s, v30.16b, v26.16b     \n"

                "prfm   pldl1keep, [%2, #128]       \n"

                "sdot   v10.4s, v30.16b, v25.16b    \n"
                "sdot   v11.4s, v30.16b, v27.16b    \n"

                "ld1    {v29.16b}, [%2], #16        \n" // _val1

                "sdot   v12.4s, v31.16b, v24.16b    \n"
                "sdot   v13.4s, v31.16b, v26.16b    \n"

                "prfm   pldl1keep, [%2, #128]       \n"

                "sdot   v14.4s, v31.16b, v25.16b    \n"
                "sdot   v15.4s, v31.16b, v27.16b    \n"

                "prfm   pldl1keep, [%3, #256]       \n"

                "sdot   v16.4s, v28.16b, v24.16b    \n"
                "sdot   v17.4s, v28.16b, v26.16b    \n"

                "prfm   pldl1keep, [%2, #256]       \n"

                "sdot   v18.4s, v28.16b, v25.16b    \n"
                "sdot   v19.4s, v28.16b, v27.16b    \n"

                "ld1    {v28.16b}, [%2], #16        \n" // _val0

                "sdot   v20.4s, v29.16b, v24.16b    \n"
                "sdot   v21.4s, v29.16b, v26.16b    \n"

                "prfm   pldl1keep, [%2, #256]       \n"

                "sdot   v22.4s, v29.16b, v25.16b    \n"

                "subs   %w1, %w1, #1                \n"

                "sdot   v23.4s, v29.16b, v27.16b    \n"

                "bne    0b                          \n"

                "sub    %2, %2, #16                 \n"

                "addp   v0.4s, v0.4s, v1.4s         \n"
                "addp   v2.4s, v2.4s, v3.4s         \n"
                "addp   v4.4s, v4.4s, v5.4s         \n"
                "addp   v6.4s, v6.4s, v7.4s         \n"
                "addp   v8.4s, v8.4s, v9.4s         \n"
                "addp   v10.4s, v10.4s, v11.4s      \n"
                "addp   v12.4s, v12.4s, v13.4s      \n"
                "addp   v14.4s, v14.4s, v15.4s      \n"
                "addp   v16.4s, v16.4s, v17.4s      \n"
                "addp   v18.4s, v18.4s, v19.4s      \n"
                "addp   v20.4s, v20.4s, v21.4s      \n"
                "addp   v22.4s, v22.4s, v23.4s      \n"

                "uzp1   v24.4s, v0.4s, v2.4s        \n"
                "uzp2   v25.4s, v0.4s, v2.4s        \n"
                "uzp1   v26.4s, v4.4s, v6.4s        \n"
                "uzp2   v27.4s, v4.4s, v6.4s        \n"
                "uzp1   v28.4s, v8.4s, v10.4s       \n"
                "uzp2   v29.4s, v8.4s, v10.4s       \n"
                "uzp1   v30.4s, v12.4s, v14.4s      \n"
                "uzp2   v31.4s, v12.4s, v14.4s      \n"
                "uzp1   v0.4s, v16.4s, v18.4s       \n"
                "uzp2   v1.4s, v16.4s, v18.4s       \n"
                "uzp1   v2.4s, v20.4s, v22.4s       \n"
                "uzp2   v3.4s, v20.4s, v22.4s       \n"

                "ext    v25.16b, v25.16b, v25.16b, #12 \n"
                "ext    v27.16b, v27.16b, v27.16b, #12 \n"
                "ext    v29.16b, v29.16b, v29.16b, #12 \n"
                "ext    v31.16b, v31.16b, v31.16b, #12 \n"
                "ext    v1.16b, v1.16b, v1.16b, #12 \n"
                "ext    v3.16b, v3.16b, v3.16b, #12 \n"

                "st1    {v24.4s, v25.4s, v26.4s, v27.4s}, [%0], #64 \n"
                "st1    {v28.4s, v29.4s, v30.4s, v31.4s}, [%0], #64 \n"
                "st1    {v0.4s, v1.4s, v2.4s, v3.4s}, [%0], #64 \n"

                : "=r"(outptr0),
                "=r"(nn),
                "=r"(tmpptr),
                "=r"(kptr0)
                : "0"(outptr0),
                "1"(nn),
                "2"(tmpptr),
                "3"(kptr0)
                : "memory", "x4", "x5", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31");
        }
        for (; i + 7 < size; i += 8)
        {
            const signed char* tmpptr = tmp.channel(i / 12 + (i % 12) / 8);
            const signed char* kptr0 = kernel.channel(p);

            int nn = inch * maxk; // inch always > 0

            int32x4_t _sum01_01 = vdupq_n_s32(0);
            int32x4_t _sum01_12 = vdupq_n_s32(0);
            int32x4_t _sum01_23 = vdupq_n_s32(0);
            int32x4_t _sum01_30 = vdupq_n_s32(0);
            int32x4_t _sum23_01 = vdupq_n_s32(0);
            int32x4_t _sum23_12 = vdupq_n_s32(0);
            int32x4_t _sum23_23 = vdupq_n_s32(0);
            int32x4_t _sum23_30 = vdupq_n_s32(0);
            int32x4_t _sum45_01 = vdupq_n_s32(0);
            int32x4_t _sum45_12 = vdupq_n_s32(0);
            int32x4_t _sum45_23 = vdupq_n_s32(0);
            int32x4_t _sum45_30 = vdupq_n_s32(0);
            int32x4_t _sum67_01 = vdupq_n_s32(0);
            int32x4_t _sum67_12 = vdupq_n_s32(0);
            int32x4_t _sum67_23 = vdupq_n_s32(0);
            int32x4_t _sum67_30 = vdupq_n_s32(0);

            for (int j = 0; j < nn; j++)
            {
                int8x16_t _val0 = vld1q_s8(tmpptr);
                int8x16_t _val1 = vld1q_s8(tmpptr + 16);
                int8x16_t _val2 = vld1q_s8(tmpptr + 32);
                int8x16_t _val3 = vld1q_s8(tmpptr + 48);

                int8x16_t _w01 = vld1q_s8(kptr0);
                int8x16_t _w23 = vld1q_s8(kptr0 + 16);

                int8x16_t _w12 = vextq_s8(_w01, _w23, 8);
                int8x16_t _w30 = vextq_s8(_w23, _w01, 8);

                _sum01_01 = vdotq_s32(_sum01_01, _val0, _w01);
                _sum01_12 = vdotq_s32(_sum01_12, _val0, _w12);
                _sum01_23 = vdotq_s32(_sum01_23, _val0, _w23);
                _sum01_30 = vdotq_s32(_sum01_30, _val0, _w30);
                _sum23_01 = vdotq_s32(_sum23_01, _val1, _w01);
                _sum23_12 = vdotq_s32(_sum23_12, _val1, _w12);
                _sum23_23 = vdotq_s32(_sum23_23, _val1, _w23);
                _sum23_30 = vdotq_s32(_sum23_30, _val1, _w30);
                _sum45_01 = vdotq_s32(_sum45_01, _val2, _w01);
                _sum45_12 = vdotq_s32(_sum45_12, _val2, _w12);
                _sum45_23 = vdotq_s32(_sum45_23, _val2, _w23);
                _sum45_30 = vdotq_s32(_sum45_30, _val2, _w30);
                _sum67_01 = vdotq_s32(_sum67_01, _val3, _w01);
                _sum67_12 = vdotq_s32(_sum67_12, _val3, _w12);
                _sum67_23 = vdotq_s32(_sum67_23, _val3, _w23);
                _sum67_30 = vdotq_s32(_sum67_30, _val3, _w30);

                tmpptr += 64;
                kptr0 += 32;
            }

            int32x4_t _s01_01_12 = vpaddq_s32(_sum01_01, _sum01_12);
            int32x4_t _s01_23_30 = vpaddq_s32(_sum01_23, _sum01_30);
            int32x4_t _s23_01_12 = vpaddq_s32(_sum23_01, _sum23_12);
            int32x4_t _s23_23_30 = vpaddq_s32(_sum23_23, _sum23_30);
            int32x4_t _s45_01_12 = vpaddq_s32(_sum45_01, _sum45_12);
            int32x4_t _s45_23_30 = vpaddq_s32(_sum45_23, _sum45_30);
            int32x4_t _s67_01_12 = vpaddq_s32(_sum67_01, _sum67_12);
            int32x4_t _s67_23_30 = vpaddq_s32(_sum67_23, _sum67_30);

            int32x4x2_t _sum01_0123_1230 = vuzpq_s32(_s01_01_12, _s01_23_30);
            int32x4x2_t _sum23_0123_1230 = vuzpq_s32(_s23_01_12, _s23_23_30);
            int32x4x2_t _sum45_0123_1230 = vuzpq_s32(_s45_01_12, _s45_23_30);
            int32x4x2_t _sum67_0123_1230 = vuzpq_s32(_s67_01_12, _s67_23_30);

            vst1q_s32(outptr0, _sum01_0123_1230.val[0]);
            vst1q_s32(outptr0 + 4, vextq_s32(_sum01_0123_1230.val[1], _sum01_0123_1230.val[1], 3));
            vst1q_s32(outptr0 + 8, _sum23_0123_1230.val[0]);
            vst1q_s32(outptr0 + 12, vextq_s32(_sum23_0123_1230.val[1], _sum23_0123_1230.val[1], 3));
            vst1q_s32(outptr0 + 16, _sum45_0123_1230.val[0]);
            vst1q_s32(outptr0 + 20, vextq_s32(_sum45_0123_1230.val[1], _sum45_0123_1230.val[1], 3));
            vst1q_s32(outptr0 + 24, _sum67_0123_1230.val[0]);
            vst1q_s32(outptr0 + 28, vextq_s32(_sum67_0123_1230.val[1], _sum67_0123_1230.val[1], 3));
            outptr0 += 32;
        }
#endif
        for (; i + 3 < size; i += 4)
        {
#if __ARM_FEATURE_DOTPROD
            const signed char* tmpptr = tmp.channel(i / 12 + (i % 12) / 8 + (i % 12 % 8) / 4);
#else
            const signed char* tmpptr = tmp.channel(i / 4);
#endif
            const signed char* kptr0 = kernel.channel(p);

            int nn = inch * maxk; // inch always > 0

#if __ARM_FEATURE_DOTPROD
            int32x4_t _sum01_01 = vdupq_n_s32(0);
            int32x4_t _sum01_12 = vdupq_n_s32(0);
            int32x4_t _sum01_23 = vdupq_n_s32(0);
            int32x4_t _sum01_30 = vdupq_n_s32(0);
            int32x4_t _sum23_01 = vdupq_n_s32(0);
            int32x4_t _sum23_12 = vdupq_n_s32(0);
            int32x4_t _sum23_23 = vdupq_n_s32(0);
            int32x4_t _sum23_30 = vdupq_n_s32(0);

            for (int j = 0; j < nn; j++)
            {
                int8x16_t _val0 = vld1q_s8(tmpptr);
                int8x16_t _val1 = vld1q_s8(tmpptr + 16);

                int8x16_t _w01 = vld1q_s8(kptr0);
                int8x16_t _w23 = vld1q_s8(kptr0 + 16);

                int8x16_t _w12 = vextq_s8(_w01, _w23, 8);
                int8x16_t _w30 = vextq_s8(_w23, _w01, 8);

                _sum01_01 = vdotq_s32(_sum01_01, _val0, _w01);
                _sum01_12 = vdotq_s32(_sum01_12, _val0, _w12);
                _sum01_23 = vdotq_s32(_sum01_23, _val0, _w23);
                _sum01_30 = vdotq_s32(_sum01_30, _val0, _w30);
                _sum23_01 = vdotq_s32(_sum23_01, _val1, _w01);
                _sum23_12 = vdotq_s32(_sum23_12, _val1, _w12);
                _sum23_23 = vdotq_s32(_sum23_23, _val1, _w23);
                _sum23_30 = vdotq_s32(_sum23_30, _val1, _w30);

                tmpptr += 32;
                kptr0 += 32;
            }

            int32x4_t _s01_01_12 = vpaddq_s32(_sum01_01, _sum01_12);
            int32x4_t _s01_23_30 = vpaddq_s32(_sum01_23, _sum01_30);
            int32x4_t _s23_01_12 = vpaddq_s32(_sum23_01, _sum23_12);
            int32x4_t _s23_23_30 = vpaddq_s32(_sum23_23, _sum23_30);

            int32x4x2_t _sum01_0123_1230 = vuzpq_s32(_s01_01_12, _s01_23_30);
            int32x4x2_t _sum23_0123_1230 = vuzpq_s32(_s23_01_12, _s23_23_30);

            vst1q_s32(outptr0, _sum01_0123_1230.val[0]);
            vst1q_s32(outptr0 + 4, vextq_s32(_sum01_0123_1230.val[1], _sum01_0123_1230.val[1], 3));
            vst1q_s32(outptr0 + 8, _sum23_0123_1230.val[0]);
            vst1q_s32(outptr0 + 12, vextq_s32(_sum23_0123_1230.val[1], _sum23_0123_1230.val[1], 3));
            outptr0 += 16;
#else  // __ARM_FEATURE_DOTPROD
            asm volatile(
                "eor    v0.16b, v0.16b, v0.16b      \n"
                "eor    v1.16b, v1.16b, v1.16b      \n"
                "eor    v2.16b, v2.16b, v2.16b      \n"
                "eor    v3.16b, v3.16b, v3.16b      \n"
                "eor    v4.16b, v4.16b, v4.16b      \n"
                "eor    v5.16b, v5.16b, v5.16b      \n"
                "eor    v6.16b, v6.16b, v6.16b      \n"
                "eor    v7.16b, v7.16b, v7.16b      \n"
                "eor    v8.16b, v8.16b, v8.16b      \n"
                "eor    v9.16b, v9.16b, v9.16b      \n"
                "eor    v10.16b, v10.16b, v10.16b   \n"
                "eor    v11.16b, v11.16b, v11.16b   \n"
                "eor    v12.16b, v12.16b, v12.16b   \n"
                "eor    v13.16b, v13.16b, v13.16b   \n"
                "eor    v14.16b, v14.16b, v14.16b   \n"
                "eor    v15.16b, v15.16b, v15.16b   \n"

                "prfm   pldl1keep, [%2, #128]       \n"

                "prfm   pldl1keep, [%3, #256]       \n"

                "lsr    w4, %w1, #1                 \n" // w4 = nn >> 1
                "cmp    w4, #0                      \n"
                "beq    1f                          \n"

                "prfm   pldl1keep, [%3, #512]       \n"

                "add    x5, %2, #16                 \n"

                "prfm   pldl1keep, [x5, #128]       \n"

                "ld1    {v16.16b}, [%2]             \n" // val L H
                "ld1    {v20.16b, v21.16b, v22.16b, v23.16b}, [%3], #64 \n"
                "add    %2, %2, #32                 \n"
                "ext    v17.16b, v16.16b, v16.16b, #8 \n" // val H L

                "ld1    {v18.16b}, [%2]             \n"
                "add    %2, %2, #32                 \n"

                "0:                                 \n"

                "smull  v24.8h, v16.8b,  v20.8b     \n"
                "prfm   pldl1keep, [%3, #256]       \n"
                "smull2 v25.8h, v17.16b, v20.16b    \n"
                "prfm   pldl1keep, [%3, #512]       \n"
                "smull  v26.8h, v16.8b,  v21.8b     \n"
                "subs   w4, w4, #1                  \n"
                "smull2 v27.8h, v17.16b, v21.16b    \n"
                "ext    v19.16b, v18.16b, v18.16b, #8 \n" // val H L

                "smlal  v24.8h, v18.8b,  v22.8b     \n"
                "smlal2 v25.8h, v19.16b, v22.16b    \n"
                "smlal  v26.8h, v18.8b,  v23.8b     \n"
                "smlal2 v27.8h, v19.16b, v23.16b    \n"

                "smull2 v29.8h, v16.16b, v20.16b    \n"
                "sadalp v0.4s, v24.8h               \n"
                "smull  v28.8h, v17.8b,  v20.8b     \n"
                "sadalp v1.4s, v25.8h               \n"
                "smull2 v31.8h, v16.16b, v21.16b    \n"
                "ld1    {v16.16b}, [x5]             \n" // val L H
                "smull  v30.8h, v17.8b,  v21.8b     \n"
                "add    x5, x5, #32                 \n"
                "smlal2 v29.8h, v18.16b, v22.16b    \n"
                "sadalp v2.4s, v26.8h               \n"
                "smlal  v28.8h, v19.8b,  v22.8b     \n"
                "sadalp v3.4s, v27.8h               \n"
                "smlal2 v31.8h, v18.16b, v23.16b    \n"
                "ld1    {v18.16b}, [x5]             \n"
                "smlal  v30.8h, v19.8b,  v23.8b     \n"
                "ext    v17.16b, v16.16b, v16.16b, #8 \n" // val H L

                "smull  v24.8h, v16.8b,  v20.8b     \n"
                "add    x5, x5, #32                 \n"
                "smull2 v25.8h, v17.16b, v20.16b    \n"
                "prfm   pldl1keep, [x5, #128]       \n"
                "smull  v26.8h, v16.8b,  v21.8b     \n"
                "prfm   pldl1keep, [x5, #384]       \n"
                "smull2 v27.8h, v17.16b, v21.16b    \n"
                "ext    v19.16b, v18.16b, v18.16b, #8 \n" // val H L

                "smlal  v24.8h, v18.8b,  v22.8b     \n"
                "sadalp v5.4s, v29.8h               \n"
                "smlal2 v25.8h, v19.16b, v22.16b    \n"
                "sadalp v4.4s, v28.8h               \n"
                "smlal  v26.8h, v18.8b,  v23.8b     \n"
                "sadalp v7.4s, v31.8h               \n"
                "smlal2 v27.8h, v19.16b, v23.16b    \n"
                "sadalp v6.4s, v30.8h               \n"

                "smull2 v29.8h, v16.16b, v20.16b    \n"
                "sadalp v8.4s, v24.8h               \n"
                "smull  v28.8h, v17.8b,  v20.8b     \n"
                "sadalp v9.4s, v25.8h               \n"
                "smull2 v31.8h, v16.16b, v21.16b    \n"
                "ld1    {v16.16b}, [%2]             \n" // val L H
                "smull  v30.8h, v17.8b,  v21.8b     \n"
                "add    %2, %2, #32                 \n"
                "smlal2 v29.8h, v18.16b, v22.16b    \n"
                "sadalp v10.4s, v26.8h              \n"
                "smlal  v28.8h, v19.8b,  v22.8b     \n"
                "sadalp v11.4s, v27.8h              \n"
                "smlal2 v31.8h, v18.16b, v23.16b    \n"
                "ld1    {v18.16b}, [%2]             \n"
                "smlal  v30.8h, v19.8b,  v23.8b     \n"
                "add    %2, %2, #32                 \n"
                "ld1    {v20.16b, v21.16b, v22.16b, v23.16b}, [%3], #64 \n"

                "sadalp v13.4s, v29.8h              \n"
                "prfm   pldl1keep, [%2, #128]       \n"
                "sadalp v12.4s, v28.8h              \n"
                "prfm   pldl1keep, [%2, #384]       \n"
                "sadalp v15.4s, v31.8h              \n"
                "ext    v17.16b, v16.16b, v16.16b, #8 \n" // val H L

                "sadalp v14.4s, v30.8h              \n"

                "bne    0b                          \n"

                "sub    %2, %2, #64                 \n"
                "sub    %3, %3, #64                 \n"

                "1:                                 \n"
                "and    w4, %w1, #1                 \n" // w4 = remain = nn & 1
                "cmp    w4, #0                      \n" // w4 > 0
                "beq    2f                          \n"

                "ld1    {v16.8b, v17.8b}, [%2], #16 \n"
                "ld1    {v20.8b, v21.8b, v22.8b, v23.8b}, [%3], #32 \n"

                "smull  v24.8h, v16.8b, v20.8b      \n"
                "smull  v25.8h, v16.8b, v21.8b      \n"
                "smull  v26.8h, v16.8b, v22.8b      \n"
                "ld1    {v18.8b, v19.8b}, [%2], #16 \n"
                "smull  v27.8h, v16.8b, v23.8b      \n"
                "sadalp v0.4s, v24.8h               \n"
                "smull  v28.8h, v17.8b, v20.8b      \n"
                "sadalp v1.4s, v25.8h               \n"
                "smull  v29.8h, v17.8b, v21.8b      \n"
                "sadalp v2.4s, v26.8h               \n"
                "smull  v30.8h, v17.8b, v22.8b      \n"
                "sadalp v3.4s, v27.8h               \n"
                "smull  v31.8h, v17.8b, v23.8b      \n"
                "sadalp v4.4s, v28.8h               \n"
                "smull  v24.8h, v18.8b, v20.8b      \n"
                "sadalp v5.4s, v29.8h               \n"
                "smull  v25.8h, v18.8b, v21.8b      \n"
                "sadalp v6.4s, v30.8h               \n"
                "smull  v26.8h, v18.8b, v22.8b      \n"
                "sadalp v7.4s, v31.8h               \n"
                "smull  v27.8h, v18.8b, v23.8b      \n"
                "sadalp v8.4s, v24.8h               \n"
                "smull  v28.8h, v19.8b, v20.8b      \n"
                "sadalp v9.4s, v25.8h               \n"
                "smull  v29.8h, v19.8b, v21.8b      \n"
                "sadalp v10.4s, v26.8h              \n"
                "smull  v30.8h, v19.8b, v22.8b      \n"
                "sadalp v11.4s, v27.8h              \n"
                "smull  v31.8h, v19.8b, v23.8b      \n"

                "sadalp v12.4s, v28.8h              \n"
                "sadalp v13.4s, v29.8h              \n"
                "sadalp v14.4s, v30.8h              \n"
                "sadalp v15.4s, v31.8h              \n"

                "2:                                 \n"

                "addp   v0.4s, v0.4s, v1.4s         \n"
                "addp   v2.4s, v2.4s, v3.4s         \n"
                "addp   v4.4s, v4.4s, v5.4s         \n"
                "addp   v6.4s, v6.4s, v7.4s         \n"
                "addp   v8.4s, v8.4s, v9.4s         \n"
                "addp   v10.4s, v10.4s, v11.4s      \n"
                "addp   v12.4s, v12.4s, v13.4s      \n"
                "addp   v14.4s, v14.4s, v15.4s      \n"

                "addp   v0.4s, v0.4s, v2.4s         \n"
                "addp   v1.4s, v4.4s, v6.4s         \n"
                "addp   v2.4s, v8.4s, v10.4s        \n"
                "addp   v3.4s, v12.4s, v14.4s       \n"

                "st1    {v0.4s, v1.4s, v2.4s, v3.4s}, [%0], #64 \n"

                : "=r"(outptr0),
                "=r"(nn),
                "=r"(tmpptr),
                "=r"(kptr0)
                : "0"(outptr0),
                "1"(nn),
                "2"(tmpptr),
                "3"(kptr0)
                : "memory", "x4", "x5", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31");
#endif // __ARM_FEATURE_DOTPROD
        }
#endif // __aarch64__
        for (; i + 1 < size; i += 2)
        {
#if __aarch64__
#if __ARM_FEATURE_DOTPROD
            const signed char* tmpptr = tmp.channel(i / 12 + (i % 12) / 8 + (i % 12 % 8) / 4 + (i % 12 % 4) / 2);
#else
            const signed char* tmpptr = tmp.channel(i / 4 + (i % 4) / 2);
#endif
#else
            const signed char* tmpptr = tmp.channel(i / 2);
#endif
            const signed char* kptr0 = kernel.channel(p);

            int nn = inch * maxk; // inch always > 0

#if __aarch64__
#if __ARM_FEATURE_DOTPROD
            int32x4_t _sum01_01 = vdupq_n_s32(0);
            int32x4_t _sum01_12 = vdupq_n_s32(0);
            int32x4_t _sum01_23 = vdupq_n_s32(0);
            int32x4_t _sum01_30 = vdupq_n_s32(0);

            for (int j = 0; j < nn; j++)
            {
                int8x16_t _val = vld1q_s8(tmpptr);

                int8x16_t _w01 = vld1q_s8(kptr0);
                int8x16_t _w23 = vld1q_s8(kptr0 + 16);

                int8x16_t _w12 = vextq_s8(_w01, _w23, 8);
                int8x16_t _w30 = vextq_s8(_w23, _w01, 8);

                _sum01_01 = vdotq_s32(_sum01_01, _val, _w01);
                _sum01_12 = vdotq_s32(_sum01_12, _val, _w12);
                _sum01_23 = vdotq_s32(_sum01_23, _val, _w23);
                _sum01_30 = vdotq_s32(_sum01_30, _val, _w30);

                tmpptr += 16;
                kptr0 += 32;
            }

            int32x4_t _s01_01_12 = vpaddq_s32(_sum01_01, _sum01_12);
            int32x4_t _s01_23_30 = vpaddq_s32(_sum01_23, _sum01_30);

            int32x4x2_t _sum01_0123_1230 = vuzpq_s32(_s01_01_12, _s01_23_30);

            vst1q_s32(outptr0, _sum01_0123_1230.val[0]);
            vst1q_s32(outptr0 + 4, vextq_s32(_sum01_0123_1230.val[1], _sum01_0123_1230.val[1], 3));
            outptr0 += 8;
#else  // __ARM_FEATURE_DOTPROD
            int32x4_t _sum00 = vdupq_n_s32(0);
            int32x4_t _sum01 = vdupq_n_s32(0);
            int32x4_t _sum02 = vdupq_n_s32(0);
            int32x4_t _sum03 = vdupq_n_s32(0);
            int32x4_t _sum10 = vdupq_n_s32(0);
            int32x4_t _sum11 = vdupq_n_s32(0);
            int32x4_t _sum12 = vdupq_n_s32(0);
            int32x4_t _sum13 = vdupq_n_s32(0);

            int j = 0;
            for (; j + 1 < nn; j += 2)
            {
                int8x16_t _val0 = vld1q_s8(tmpptr);
                int8x16_t _val1 = vld1q_s8(tmpptr + 16);

                int8x16_t _w01 = vld1q_s8(kptr0);
                int8x16_t _w23 = vld1q_s8(kptr0 + 16);

                int16x8_t _wv00 = vmull_s8(vget_low_s8(_val0), vget_low_s8(_w01));
                int16x8_t _wv01 = vmull_s8(vget_low_s8(_val0), vget_high_s8(_w01));
                int16x8_t _wv02 = vmull_s8(vget_low_s8(_val0), vget_low_s8(_w23));
                int16x8_t _wv03 = vmull_s8(vget_low_s8(_val0), vget_high_s8(_w23));

                int16x8_t _wv10 = vmull_s8(vget_high_s8(_val0), vget_low_s8(_w01));
                int16x8_t _wv11 = vmull_s8(vget_high_s8(_val0), vget_high_s8(_w01));
                int16x8_t _wv12 = vmull_s8(vget_high_s8(_val0), vget_low_s8(_w23));
                int16x8_t _wv13 = vmull_s8(vget_high_s8(_val0), vget_high_s8(_w23));

                int8x16_t _w45 = vld1q_s8(kptr0 + 32);
                int8x16_t _w67 = vld1q_s8(kptr0 + 48);

                _wv00 = vmlal_s8(_wv00, vget_low_s8(_val1), vget_low_s8(_w45));
                _wv01 = vmlal_s8(_wv01, vget_low_s8(_val1), vget_high_s8(_w45));
                _wv02 = vmlal_s8(_wv02, vget_low_s8(_val1), vget_low_s8(_w67));
                _wv03 = vmlal_s8(_wv03, vget_low_s8(_val1), vget_high_s8(_w67));

                _wv10 = vmlal_s8(_wv10, vget_high_s8(_val1), vget_low_s8(_w45));
                _wv11 = vmlal_s8(_wv11, vget_high_s8(_val1), vget_high_s8(_w45));
                _wv12 = vmlal_s8(_wv12, vget_high_s8(_val1), vget_low_s8(_w67));
                _wv13 = vmlal_s8(_wv13, vget_high_s8(_val1), vget_high_s8(_w67));

                _sum00 = vpadalq_s16(_sum00, _wv00);
                _sum01 = vpadalq_s16(_sum01, _wv01);
                _sum02 = vpadalq_s16(_sum02, _wv02);
                _sum03 = vpadalq_s16(_sum03, _wv03);
                _sum10 = vpadalq_s16(_sum10, _wv10);
                _sum11 = vpadalq_s16(_sum11, _wv11);
                _sum12 = vpadalq_s16(_sum12, _wv12);
                _sum13 = vpadalq_s16(_sum13, _wv13);

                tmpptr += 32;
                kptr0 += 64;
            }
            for (; j < nn; j++)
            {
                int8x16_t _val = vld1q_s8(tmpptr);

                int8x16_t _w01 = vld1q_s8(kptr0);
                int8x16_t _w23 = vld1q_s8(kptr0 + 16);

                int16x8_t _wv00 = vmull_s8(vget_low_s8(_val), vget_low_s8(_w01));
                int16x8_t _wv01 = vmull_s8(vget_low_s8(_val), vget_high_s8(_w01));
                int16x8_t _wv02 = vmull_s8(vget_low_s8(_val), vget_low_s8(_w23));
                int16x8_t _wv03 = vmull_s8(vget_low_s8(_val), vget_high_s8(_w23));
                int16x8_t _wv10 = vmull_s8(vget_high_s8(_val), vget_low_s8(_w01));
                int16x8_t _wv11 = vmull_s8(vget_high_s8(_val), vget_high_s8(_w01));
                int16x8_t _wv12 = vmull_s8(vget_high_s8(_val), vget_low_s8(_w23));
                int16x8_t _wv13 = vmull_s8(vget_high_s8(_val), vget_high_s8(_w23));

                _sum00 = vpadalq_s16(_sum00, _wv00);
                _sum01 = vpadalq_s16(_sum01, _wv01);
                _sum02 = vpadalq_s16(_sum02, _wv02);
                _sum03 = vpadalq_s16(_sum03, _wv03);
                _sum10 = vpadalq_s16(_sum10, _wv10);
                _sum11 = vpadalq_s16(_sum11, _wv11);
                _sum12 = vpadalq_s16(_sum12, _wv12);
                _sum13 = vpadalq_s16(_sum13, _wv13);

                tmpptr += 16;
                kptr0 += 32;
            }

            int32x4_t _s001 = vpaddq_s32(_sum00, _sum01);
            int32x4_t _s023 = vpaddq_s32(_sum02, _sum03);
            int32x4_t _s101 = vpaddq_s32(_sum10, _sum11);
            int32x4_t _s123 = vpaddq_s32(_sum12, _sum13);

            int32x4_t _s00123 = vpaddq_s32(_s001, _s023);
            int32x4_t _s10123 = vpaddq_s32(_s101, _s123);

            vst1q_s32(outptr0, _s00123);
            vst1q_s32(outptr0 + 4, _s10123);
            outptr0 += 8;
#endif // __ARM_FEATURE_DOTPROD
#else  // __aarch64__
            asm volatile(
                "veor       q0, q0              \n"
                "veor       q1, q1              \n"
                "veor       q2, q2              \n"
                "veor       q3, q3              \n"
                "veor       q4, q4              \n"
                "veor       q5, q5              \n"
                "veor       q6, q6              \n"
                "veor       q7, q7              \n"

                "pld        [%2, #256]          \n"

                "lsr        r4, %1, #1          \n" // r4 = nn = size >> 1
                "cmp        r4, #0              \n"
                "beq        1f                  \n"

                "add        r5, %3, #16         \n"
                "pld        [%3, #128]          \n"
                "mov        r6, #32             \n"
                "pld        [%3, #384]          \n"

                "vld1.s8    {d20-d21}, [%3], r6 \n" // _w01

                "vld1.s8    {d16-d19}, [%2]!    \n" // _val0 _val1

                "vld1.s8    {d22-d23}, [%3], r6 \n" // _w45

                "0:                             \n"

                "vmull.s8   q12, d16, d20       \n"
                "pld        [%2, #256]          \n"
                "vmull.s8   q13, d16, d21       \n"
                "pld        [%3, #384]          \n"
                "vmull.s8   q14, d17, d20       \n"
                "vmull.s8   q15, d17, d21       \n"
                "vld1.s8    {d20-d21}, [r5], r6 \n" // _w23

                "vmlal.s8   q12, d18, d22       \n"
                "vmlal.s8   q13, d18, d23       \n"
                "subs       r4, r4, #1          \n"
                "vmlal.s8   q14, d19, d22       \n"
                "vmlal.s8   q15, d19, d23       \n"
                "vld1.s8    {d22-d23}, [r5], r6 \n" // _w67

                "vpadal.s16 q0, q12             \n"
                "vmull.s8   q12, d16, d20       \n"
                "vpadal.s16 q1, q13             \n"
                "vmull.s8   q13, d16, d21       \n"
                "vpadal.s16 q4, q14             \n"
                "vmull.s8   q14, d17, d20       \n"
                "vpadal.s16 q5, q15             \n"
                "vmull.s8   q15, d17, d21       \n"
                "vld1.s8    {d16-d17}, [%2]!    \n" // _val0

                "vmlal.s8   q12, d18, d22       \n"
                "vld1.s8    {d20-d21}, [%3], r6 \n" // _w01
                "vmlal.s8   q13, d18, d23       \n"
                "pld        [r5, #128]          \n"
                "vmlal.s8   q14, d19, d22       \n"
                "pld        [r5, #384]          \n"
                "vmlal.s8   q15, d19, d23       \n"
                "vld1.s8    {d18-d19}, [%2]!    \n" // _val1

                "vpadal.s16 q2, q12             \n"
                "vld1.s8    {d22-d23}, [%3], r6 \n" // _w45
                "vpadal.s16 q3, q13             \n"
                "pld        [%2, #128]          \n"
                "vpadal.s16 q6, q14             \n"
                "pld        [%3, #128]          \n"
                "vpadal.s16 q7, q15             \n"

                "bne        0b                  \n"

                "sub        %2, %2, #32         \n"
                "sub        %3, %3, #64         \n"

                "1:                             \n"
                "and        r4, %1, #1          \n" // r4 = remain = size & 1
                "cmp        r4, #0              \n" // r4 > 0
                "beq        2f                  \n"

                "vld1.s8    {d16-d17}, [%2]!    \n" // _val
                "vld1.s8    {d20-d21}, [%3]!    \n" // _w01

                "vmull.s8   q12, d16, d20       \n"

                "vld1.s8    {d22-d23}, [%3]!    \n" // _w23
                "vmull.s8   q13, d16, d21       \n"
                "vmull.s8   q14, d17, d20       \n"
                "vmull.s8   q15, d17, d21       \n"

                "vpadal.s16 q0, q12             \n"
                "vmull.s8   q12, d16, d22       \n"
                "vpadal.s16 q1, q13             \n"
                "vmull.s8   q13, d16, d23       \n"
                "vpadal.s16 q4, q14             \n"
                "vmull.s8   q14, d17, d22       \n"
                "vpadal.s16 q5, q15             \n"
                "vmull.s8   q15, d17, d23       \n"

                "vpadal.s16 q2, q12             \n"
                "vpadal.s16 q3, q13             \n"
                "vpadal.s16 q6, q14             \n"
                "vpadal.s16 q7, q15             \n"

                "2:                             \n"

                "vpadd.s32  d16, d0, d1         \n"
                "vpadd.s32  d17, d2, d3         \n"
                "vpadd.s32  d18, d4, d5         \n"
                "vpadd.s32  d19, d6, d7         \n"
                "vpadd.s32  d20, d8, d9         \n"
                "vpadd.s32  d21, d10, d11       \n"
                "vpadd.s32  d22, d12, d13       \n"
                "vpadd.s32  d23, d14, d15       \n"

                "vpadd.s32  d0, d16, d17        \n"
                "vpadd.s32  d1, d18, d19        \n"
                "vpadd.s32  d2, d20, d21        \n"
                "vpadd.s32  d3, d22, d23        \n"

                "vst1.f32   {d0-d3}, [%0 :128]! \n"

                : "=r"(outptr0),
                "=r"(nn),
                "=r"(tmpptr),
                "=r"(kptr0)
                : "0"(outptr0),
                "1"(nn),
                "2"(tmpptr),
                "3"(kptr0)
                : "memory", "r4", "r5", "r6", "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15");
#endif // __aarch64__
        }
        for (; i < size; i++)
        {
#if __aarch64__
#if __ARM_FEATURE_DOTPROD
            const signed char* tmpptr = tmp.channel(i / 12 + (i % 12) / 8 + (i % 12 % 8) / 4 + (i % 12 % 4) / 2 + i % 12 % 2);
#else
            const signed char* tmpptr = tmp.channel(i / 4 + (i % 4) / 2 + i % 2);
#endif
#else
            const signed char* tmpptr = tmp.channel(i / 2 + i % 2);
#endif
            const signed char* kptr0 = kernel.channel(p);

            int nn = inch * maxk; // inch always > 0

#if __ARM_FEATURE_DOTPROD
            int32x4_t _sum01 = vdupq_n_s32(0);
            int32x4_t _sum23 = vdupq_n_s32(0);

            for (int j = 0; j < nn; j++)
            {
                int8x8_t _val = vld1_s8(tmpptr);

                int8x16_t _w01 = vld1q_s8(kptr0);
                int8x16_t _w23 = vld1q_s8(kptr0 + 16);

                int8x16_t _valval = vcombine_s8(_val, _val);

                _sum01 = vdotq_s32(_sum01, _valval, _w01);
                _sum23 = vdotq_s32(_sum23, _valval, _w23);

                tmpptr += 8;
                kptr0 += 32;
            }

            int32x4_t _s0123 = vpaddq_s32(_sum01, _sum23);

            vst1q_s32(outptr0, _s0123);
            outptr0 += 4;
#else // __ARM_FEATURE_DOTPROD
            int32x4_t _sum0 = vdupq_n_s32(0);
            int32x4_t _sum1 = vdupq_n_s32(0);
            int32x4_t _sum2 = vdupq_n_s32(0);
            int32x4_t _sum3 = vdupq_n_s32(0);

            int j = 0;
            for (; j + 1 < nn; j += 2)
            {
                int8x16_t _val = vld1q_s8(tmpptr);

                int8x16_t _w01 = vld1q_s8(kptr0);
                int8x16_t _w23 = vld1q_s8(kptr0 + 16);

                int16x8_t _wv0 = vmull_s8(vget_low_s8(_val), vget_low_s8(_w01));
                int16x8_t _wv1 = vmull_s8(vget_low_s8(_val), vget_high_s8(_w01));
                int16x8_t _wv2 = vmull_s8(vget_low_s8(_val), vget_low_s8(_w23));
                int16x8_t _wv3 = vmull_s8(vget_low_s8(_val), vget_high_s8(_w23));

                int8x16_t _w45 = vld1q_s8(kptr0 + 32);
                int8x16_t _w67 = vld1q_s8(kptr0 + 48);

                _wv0 = vmlal_s8(_wv0, vget_high_s8(_val), vget_low_s8(_w45));
                _wv1 = vmlal_s8(_wv1, vget_high_s8(_val), vget_high_s8(_w45));
                _wv2 = vmlal_s8(_wv2, vget_high_s8(_val), vget_low_s8(_w67));
                _wv3 = vmlal_s8(_wv3, vget_high_s8(_val), vget_high_s8(_w67));

                _sum0 = vpadalq_s16(_sum0, _wv0);
                _sum1 = vpadalq_s16(_sum1, _wv1);
                _sum2 = vpadalq_s16(_sum2, _wv2);
                _sum3 = vpadalq_s16(_sum3, _wv3);

                tmpptr += 16;
                kptr0 += 64;
            }
            for (; j < nn; j++)
            {
                int8x8_t _val = vld1_s8(tmpptr);

                int8x16_t _w01 = vld1q_s8(kptr0);
                int8x16_t _w23 = vld1q_s8(kptr0 + 16);

                int16x8_t _wv0 = vmull_s8(_val, vget_low_s8(_w01));
                int16x8_t _wv1 = vmull_s8(_val, vget_high_s8(_w01));
                int16x8_t _wv2 = vmull_s8(_val, vget_low_s8(_w23));
                int16x8_t _wv3 = vmull_s8(_val, vget_high_s8(_w23));

                _sum0 = vpadalq_s16(_sum0, _wv0);
                _sum1 = vpadalq_s16(_sum1, _wv1);
                _sum2 = vpadalq_s16(_sum2, _wv2);
                _sum3 = vpadalq_s16(_sum3, _wv3);

                tmpptr += 8;
                kptr0 += 32;
            }

#if __aarch64__
            int32x4_t _s01 = vpaddq_s32(_sum0, _sum1);
            int32x4_t _s23 = vpaddq_s32(_sum2, _sum3);

            int32x4_t _s0123 = vpaddq_s32(_s01, _s23);
#else
            int32x2_t _s01_low = vpadd_s32(vget_low_s32(_sum0), vget_high_s32(_sum0));
            int32x2_t _s01_high = vpadd_s32(vget_low_s32(_sum1), vget_high_s32(_sum1));
            int32x2_t _s23_low = vpadd_s32(vget_low_s32(_sum2), vget_high_s32(_sum2));
            int32x2_t _s23_high = vpadd_s32(vget_low_s32(_sum3), vget_high_s32(_sum3));

            int32x4_t _s0123 = vcombine_s32(vpadd_s32(_s01_low, _s01_high), vpadd_s32(_s23_low, _s23_high));
#endif

            vst1q_s32(outptr0, _s0123);
            outptr0 += 4;
#endif // __ARM_FEATURE_DOTPROD
        }
    }
}

static void convolution_im2col_sgemm_transform_kernel_pack8_int8_neon(const Mat& _kernel, Mat& kernel_tm, int inch, int outch, int kernel_w, int kernel_h)
{
    const int maxk = kernel_w * kernel_h;

    // interleave
    // src = maxk-inch-outch
    // dst = 8a-4b-maxk-inch/8a-outch/4b
    Mat kernel = _kernel.reshape(maxk, inch, outch);
    kernel_tm.create(32 * maxk, inch / 8, outch / 4, 1u);

    for (int q = 0; q + 3 < outch; q += 4)
    {
        Mat g0 = kernel_tm.channel(q / 4);

        for (int p = 0; p + 7 < inch; p += 8)
        {
            signed char* g00 = g0.row<signed char>(p / 8);

            for (int k = 0; k < maxk; k++)
            {
                for (int i = 0; i < 4; i++)
                {
                    for (int j = 0; j < 8; j++)
                    {
                        const signed char* k00 = kernel.channel(q + i).row<const signed char>(p + j);

                        g00[0] = k00[k];

                        g00++;
                    }
                }
            }
        }
    }
}

static void convolution_im2col_sgemm_pack8_int8_neon(const Mat& bottom_blob, Mat& top_blob, const Mat& kernel, int kernel_w, int kernel_h, int dilation_w, int dilation_h, int stride_w, int stride_h, const Option& opt)
{
    int w = bottom_blob.w;
    int inch = bottom_blob.c;

    int outw = top_blob.w;
    int outh = top_blob.h;
    const int size = outw * outh;

    const int maxk = kernel_w * kernel_h;

    // im2col
    Mat bottom_im2col(size, maxk, inch, 8u, 8, opt.workspace_allocator);
    {
        const int gap = (w * stride_h - outw * stride_w) * 8;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int p = 0; p < inch; p++)
        {
            const Mat img = bottom_blob.channel(p);
            signed char* ptr = bottom_im2col.channel(p);

            for (int u = 0; u < kernel_h; u++)
            {
                for (int v = 0; v < kernel_w; v++)
                {
                    const signed char* sptr = img.row<const signed char>(dilation_h * u) + dilation_w * v * 8;

                    for (int i = 0; i < outh; i++)
                    {
                        int j = 0;
                        for (; j + 3 < outw; j += 4)
                        {
                            int8x8_t _val0 = vld1_s8(sptr);
                            int8x8_t _val1 = vld1_s8(sptr + stride_w * 8);
                            int8x8_t _val2 = vld1_s8(sptr + stride_w * 16);
                            int8x8_t _val3 = vld1_s8(sptr + stride_w * 24);
                            vst1_s8(ptr, _val0);
                            vst1_s8(ptr + 8, _val1);
                            vst1_s8(ptr + 16, _val2);
                            vst1_s8(ptr + 24, _val3);

                            sptr += stride_w * 32;
                            ptr += 32;
                        }
                        for (; j + 1 < outw; j += 2)
                        {
                            int8x8_t _val0 = vld1_s8(sptr);
                            int8x8_t _val1 = vld1_s8(sptr + stride_w * 8);
                            vst1_s8(ptr, _val0);
                            vst1_s8(ptr + 8, _val1);

                            sptr += stride_w * 16;
                            ptr += 16;
                        }
                        for (; j < outw; j++)
                        {
                            int8x8_t _val = vld1_s8(sptr);
                            vst1_s8(ptr, _val);

                            sptr += stride_w * 8;
                            ptr += 8;
                        }

                        sptr += gap;
                    }
                }
            }
        }
    }

    im2col_sgemm_pack8_int8_neon(bottom_im2col, top_blob, kernel, opt);
}
