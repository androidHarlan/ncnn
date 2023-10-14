// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

static void pack_A_tile_int8(const Mat& A, Mat& AT, int batch, int max_ii, int max_kk)
{
    const int N = max_kk * batch;

    for (int b = 0; b < batch; b++)
    {
        short* pp = AT.row<short>(b);

        int ii = 0;
#if __ARM_NEON
        for (; ii + 7 < max_ii; ii += 8)
        {
            const short* p0 = (const short*)A + ii * N + b;

            int kk = 0;
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[N];
                pp[2] = p0[N * 2];
                pp[3] = p0[N * 3];
                pp[4] = p0[N * 4];
                pp[5] = p0[N * 5];
                pp[6] = p0[N * 6];
                pp[7] = p0[N * 7];
                p0 += batch;
                pp += 8;
            }
        }
        for (; ii + 3 < max_ii; ii += 4)
        {
            const short* p0 = (const short*)A + ii * N + b;

            int kk = 0;
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[N];
                pp[2] = p0[N * 2];
                pp[3] = p0[N * 3];
                p0 += batch;
                pp += 4;
            }
        }
#endif // __ARM_NEON
        for (; ii + 1 < max_ii; ii += 2)
        {
            const short* p0 = (const short*)A + ii * N + b;

            int kk = 0;
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[N];
                p0 += batch;
                pp += 2;
            }
        }
        for (; ii < max_ii; ii++)
        {
            const short* p0 = (const short*)A + ii * N + b;

            int kk = 0;
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                p0 += batch;
                pp += 1;
            }
        }
    }
}

static void transpose_pack_B_tile_int8(const Mat& B, Mat& BT, int batch, int max_jj, int max_kk, int nT)
{
    // NCNN_LOGE("transpose_pack_B_tile_int8 %d %d", max_jj, max_kk);

    #pragma omp parallel for num_threads(nT)
    for (int b = 0; b < batch; b++)
    {
        short* pp = BT.row<short>(b);

        int jj = 0;
#if __ARM_NEON
#if __aarch64__
        for (; jj + 7 < max_jj; jj += 8)
        {
            const short* p0 = B;

            int kk = 0;
            p0 += (b * max_jj + jj) * 8;
            for (; kk + 7 < max_kk; kk += 8)
            {
                // transpose 8x8
                int16x8x4_t _r0123 = vld4q_s16(p0);
                int16x8x4_t _r4567 = vld4q_s16(p0 + 32);
                int16x8x2_t _r04 = vuzpq_s16(_r0123.val[0], _r4567.val[0]);
                int16x8x2_t _r15 = vuzpq_s16(_r0123.val[1], _r4567.val[1]);
                int16x8x2_t _r26 = vuzpq_s16(_r0123.val[2], _r4567.val[2]);
                int16x8x2_t _r37 = vuzpq_s16(_r0123.val[3], _r4567.val[3]);
                vst1q_s16(pp, _r04.val[0]);
                vst1q_s16(pp + 8, _r15.val[0]);
                vst1q_s16(pp + 16, _r26.val[0]);
                vst1q_s16(pp + 24, _r37.val[0]);
                vst1q_s16(pp + 32, _r04.val[1]);
                vst1q_s16(pp + 40, _r15.val[1]);
                vst1q_s16(pp + 48, _r26.val[1]);
                vst1q_s16(pp + 56, _r37.val[1]);
                p0 += max_jj * batch * 8;
                pp += 64;
            }
            p0 -= (b * max_jj + jj) * 8;
            p0 += (b * max_jj + jj) * 2;
            for (; kk + 1 < max_kk; kk += 2)
            {
                int16x8x2_t _r01 = vld2q_s16(p0);
                vst1q_s16(pp, _r01.val[0]);
                vst1q_s16(pp + 8, _r01.val[1]);
                p0 += max_jj * batch * 2;
                pp += 16;
            }
            p0 -= (b * max_jj + jj) * 2;
            p0 += (b * max_jj + jj);
            for (; kk < max_kk; kk++)
            {
                int16x8_t _r0 = vld1q_s16(p0);
                vst1q_s16(pp, _r0);
                p0 += max_jj * batch;
                pp += 8;
            }
        }
#endif // __aarch64__
        for (; jj + 5 < max_jj; jj += 6)
        {
            const short* p0 = B;

            int kk = 0;
            p0 += (b * max_jj + jj) * 8;
            for (; kk + 7 < max_kk; kk += 8)
            {
                int16x8_t _r0 = vld1q_s16(p0);
                int16x8_t _r1 = vld1q_s16(p0 + 8);
                int16x8_t _r2 = vld1q_s16(p0 + 16);
                int16x8_t _r3 = vld1q_s16(p0 + 24);
                int16x8_t _r4 = vld1q_s16(p0 + 32);
                int16x8_t _r5 = vld1q_s16(p0 + 40);
                int16x8x2_t _r01 = vzipq_s16(_r0, _r1);
                int16x8x2_t _r23 = vzipq_s16(_r2, _r3);
                int16x8x2_t _r45 = vzipq_s16(_r4, _r5);
                int32x4x3_t _r012;
                _r012.val[0] = vreinterpretq_s32_s16(_r01.val[0]);
                _r012.val[1] = vreinterpretq_s32_s16(_r23.val[0]);
                _r012.val[2] = vreinterpretq_s32_s16(_r45.val[0]);
                int32x4x3_t _r345;
                _r345.val[0] = vreinterpretq_s32_s16(_r01.val[1]);
                _r345.val[1] = vreinterpretq_s32_s16(_r23.val[1]);
                _r345.val[2] = vreinterpretq_s32_s16(_r45.val[1]);
                vst3q_s32((int*)pp, _r012);
                vst3q_s32((int*)(pp + 24), _r345);
                p0 += max_jj * batch * 8;
                pp += 48;
            }
            p0 -= (b * max_jj + jj) * 8;
            p0 += (b * max_jj + jj) * 2;
            for (; kk + 1 < max_kk; kk += 2)
            {
                int16x8x2_t _r01 = vld2q_s16(p0);
                int32x4x2_t _r01x = vtrnq_s32(vreinterpretq_s32_s16(_r01.val[0]), vreinterpretq_s32_s16(_r01.val[1]));
                int32x2x3_t _r012;
                _r012.val[0] = vget_low_s32(_r01x.val[0]);
                _r012.val[1] = vget_low_s32(_r01x.val[1]);
                _r012.val[2] = vget_high_s32(_r01x.val[0]);
                vst3_s32((int*)pp, _r012);
                p0 += max_jj * batch * 2;
                pp += 12;
            }
            p0 -= (b * max_jj + jj) * 2;
            p0 += (b * max_jj + jj);
            for (; kk < max_kk; kk++)
            {
                int16x4_t _r0 = vld1_s16(p0);
                vst1_s16(pp, _r0);
                pp[4] = p0[4];
                pp[5] = p0[5];
                p0 += max_jj * batch;
                pp += 6;
            }
        }
        for (; jj + 3 < max_jj; jj += 4)
        {
            const short* p0 = B;

            int kk = 0;
            p0 += (b * max_jj + jj) * 8;
            for (; kk + 7 < max_kk; kk += 8)
            {
                int16x8x4_t _r0123;
                _r0123.val[0] = vld1q_s16(p0);
                _r0123.val[1] = vld1q_s16(p0 + 8);
                _r0123.val[2] = vld1q_s16(p0 + 16);
                _r0123.val[3] = vld1q_s16(p0 + 24);
                vst4q_s16(pp, _r0123);
                p0 += max_jj * batch * 8;
                pp += 32;
            }
            p0 -= (b * max_jj + jj) * 8;
            p0 += (b * max_jj + jj) * 2;
            for (; kk + 1 < max_kk; kk += 2)
            {
                int16x4x2_t _r01 = vld2_s16(p0);
                vst1_s16(pp, _r01.val[0]);
                vst1_s16(pp + 4, _r01.val[1]);
                p0 += max_jj * batch * 2;
                pp += 8;
            }
            p0 -= (b * max_jj + jj) * 2;
            p0 += (b * max_jj + jj);
            for (; kk < max_kk; kk++)
            {
                int16x4_t _r0 = vld1_s16(p0);
                vst1_s16(pp, _r0);
                p0 += max_jj * batch;
                pp += 4;
            }
        }
#endif // __ARM_NEON
        for (; jj + 1 < max_jj; jj += 2)
        {
            const short* p0 = B;

            int kk = 0;
#if __ARM_NEON
            p0 += (b * max_jj + jj) * 8;
            for (; kk + 7 < max_kk; kk += 8)
            {
                int16x8x2_t _r01;
                _r01.val[0] = vld1q_s16(p0);
                _r01.val[1] = vld1q_s16(p0 + 8);
                vst2q_s16(pp, _r01);
                p0 += max_jj * batch * 8;
                pp += 16;
            }
            p0 -= (b * max_jj + jj) * 8;
#endif // __ARM_NEON
            p0 += (b * max_jj + jj) * 2;
            for (; kk + 1 < max_kk; kk += 2)
            {
                pp[0] = p0[0];
                pp[1] = p0[2];
                pp[2] = p0[1];
                pp[3] = p0[3];
                p0 += max_jj * batch * 2;
                pp += 4;
            }
            p0 -= (b * max_jj + jj) * 2;
            p0 += (b * max_jj + jj);
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                pp[1] = p0[1];
                p0 += max_jj * batch;
                pp += 2;
            }
        }
        for (; jj < max_jj; jj++)
        {
            const short* p0 = B;

            int kk = 0;
#if __ARM_NEON
            p0 += (b * max_jj + jj) * 8;
            for (; kk + 7 < max_kk; kk += 8)
            {
                int16x8_t _r0 = vld1q_s16(p0);
                vst1q_s16(pp, _r0);
                p0 += max_jj * batch * 8;
                pp += 8;
            }
            p0 -= (b * max_jj + jj) * 8;
#endif // __ARM_NEON
            p0 += (b * max_jj + jj) * 2;
            for (; kk + 1 < max_kk; kk += 2)
            {
                pp[0] = p0[0];
                pp[1] = p0[1];
                p0 += max_jj * batch * 2;
                pp += 2;
            }
            p0 -= (b * max_jj + jj) * 2;
            p0 += (b * max_jj + jj);
            for (; kk < max_kk; kk++)
            {
                pp[0] = p0[0];
                p0 += max_jj * batch;
                pp += 1;
            }
        }
    }
}

static void gemm_transB_packed_tile_int8(const Mat& AT_tile, const Mat& BT_tile, Mat& top_blob, int batch, int max_ii, int max_jj, int k, int max_kk)
{
    // NCNN_LOGE("gemm_transB_packed_tile_int8 %d %d %d", max_ii, max_jj, max_kk);

    int* outptr = top_blob;

    int ii = 0;
#if __ARM_NEON
    for (; ii + 7 < max_ii; ii += 8)
    {
        for (int b = 0; b < batch; b++)
        {
            const short* pAT = AT_tile.row<const short>(b) + max_kk * ii;
            const short* pB = BT_tile.row<const short>(b);

            int jj = 0;
#if __aarch64__
            for (; jj + 7 < max_jj; jj += 8)
            {
                const short* pA = pAT;

                int32x4_t _sum0;
                int32x4_t _sum1;
                int32x4_t _sum2;
                int32x4_t _sum3;
                int32x4_t _sum4;
                int32x4_t _sum5;
                int32x4_t _sum6;
                int32x4_t _sum7;
                int32x4_t _sum8;
                int32x4_t _sum9;
                int32x4_t _suma;
                int32x4_t _sumb;
                int32x4_t _sumc;
                int32x4_t _sumd;
                int32x4_t _sume;
                int32x4_t _sumf;

                if (k == 0)
                {
                    _sum0 = vdupq_n_s32(0);
                    _sum1 = vdupq_n_s32(0);
                    _sum2 = vdupq_n_s32(0);
                    _sum3 = vdupq_n_s32(0);
                    _sum4 = vdupq_n_s32(0);
                    _sum5 = vdupq_n_s32(0);
                    _sum6 = vdupq_n_s32(0);
                    _sum7 = vdupq_n_s32(0);
                    _sum8 = vdupq_n_s32(0);
                    _sum9 = vdupq_n_s32(0);
                    _suma = vdupq_n_s32(0);
                    _sumb = vdupq_n_s32(0);
                    _sumc = vdupq_n_s32(0);
                    _sumd = vdupq_n_s32(0);
                    _sume = vdupq_n_s32(0);
                    _sumf = vdupq_n_s32(0);
                }
                else
                {
                    _sum0 = vld1q_s32(outptr);
                    _sum1 = vld1q_s32(outptr + 4);
                    _sum2 = vld1q_s32(outptr + 8);
                    _sum3 = vld1q_s32(outptr + 12);
                    _sum4 = vld1q_s32(outptr + 16);
                    _sum5 = vld1q_s32(outptr + 20);
                    _sum6 = vld1q_s32(outptr + 24);
                    _sum7 = vld1q_s32(outptr + 28);
                    _sum8 = vld1q_s32(outptr + 32);
                    _sum9 = vld1q_s32(outptr + 36);
                    _suma = vld1q_s32(outptr + 40);
                    _sumb = vld1q_s32(outptr + 44);
                    _sumc = vld1q_s32(outptr + 48);
                    _sumd = vld1q_s32(outptr + 52);
                    _sume = vld1q_s32(outptr + 56);
                    _sumf = vld1q_s32(outptr + 60);
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    int16x8_t _pA = vld1q_s16(pA);
                    int16x8_t _pB = vld1q_s16(pB);
                    _sum0 = vmlal_laneq_s16(_sum0, vget_low_s16(_pA), _pB, 0);
                    _sum1 = vmlal_laneq_s16(_sum1, vget_high_s16(_pA), _pB, 0);
                    _sum2 = vmlal_laneq_s16(_sum2, vget_low_s16(_pA), _pB, 1);
                    _sum3 = vmlal_laneq_s16(_sum3, vget_high_s16(_pA), _pB, 1);
                    _sum4 = vmlal_laneq_s16(_sum4, vget_low_s16(_pA), _pB, 2);
                    _sum5 = vmlal_laneq_s16(_sum5, vget_high_s16(_pA), _pB, 2);
                    _sum6 = vmlal_laneq_s16(_sum6, vget_low_s16(_pA), _pB, 3);
                    _sum7 = vmlal_laneq_s16(_sum7, vget_high_s16(_pA), _pB, 3);
                    _sum8 = vmlal_laneq_s16(_sum8, vget_low_s16(_pA), _pB, 4);
                    _sum9 = vmlal_laneq_s16(_sum9, vget_high_s16(_pA), _pB, 4);
                    _suma = vmlal_laneq_s16(_suma, vget_low_s16(_pA), _pB, 5);
                    _sumb = vmlal_laneq_s16(_sumb, vget_high_s16(_pA), _pB, 5);
                    _sumc = vmlal_laneq_s16(_sumc, vget_low_s16(_pA), _pB, 6);
                    _sumd = vmlal_laneq_s16(_sumd, vget_high_s16(_pA), _pB, 6);
                    _sume = vmlal_laneq_s16(_sume, vget_low_s16(_pA), _pB, 7);
                    _sumf = vmlal_laneq_s16(_sumf, vget_high_s16(_pA), _pB, 7);
                    pA += 8;
                    pB += 8;
                }

                vst1q_s32(outptr, _sum0);
                vst1q_s32(outptr + 4, _sum1);
                vst1q_s32(outptr + 8, _sum2);
                vst1q_s32(outptr + 12, _sum3);
                vst1q_s32(outptr + 16, _sum4);
                vst1q_s32(outptr + 20, _sum5);
                vst1q_s32(outptr + 24, _sum6);
                vst1q_s32(outptr + 28, _sum7);
                vst1q_s32(outptr + 32, _sum8);
                vst1q_s32(outptr + 36, _sum9);
                vst1q_s32(outptr + 40, _suma);
                vst1q_s32(outptr + 44, _sumb);
                vst1q_s32(outptr + 48, _sumc);
                vst1q_s32(outptr + 52, _sumd);
                vst1q_s32(outptr + 56, _sume);
                vst1q_s32(outptr + 60, _sumf);
                outptr += 64;
            }
#endif // __aarch64__
            for (; jj + 5 < max_jj; jj += 6)
            {
                const short* pA = pAT;

                int32x4_t _sum0;
                int32x4_t _sum1;
                int32x4_t _sum2;
                int32x4_t _sum3;
                int32x4_t _sum4;
                int32x4_t _sum5;
                int32x4_t _sum6;
                int32x4_t _sum7;
                int32x4_t _sum8;
                int32x4_t _sum9;
                int32x4_t _suma;
                int32x4_t _sumb;

                if (k == 0)
                {
                    _sum0 = vdupq_n_s32(0);
                    _sum1 = vdupq_n_s32(0);
                    _sum2 = vdupq_n_s32(0);
                    _sum3 = vdupq_n_s32(0);
                    _sum4 = vdupq_n_s32(0);
                    _sum5 = vdupq_n_s32(0);
                    _sum6 = vdupq_n_s32(0);
                    _sum7 = vdupq_n_s32(0);
                    _sum8 = vdupq_n_s32(0);
                    _sum9 = vdupq_n_s32(0);
                    _suma = vdupq_n_s32(0);
                    _sumb = vdupq_n_s32(0);
                }
                else
                {
                    _sum0 = vld1q_s32(outptr);
                    _sum1 = vld1q_s32(outptr + 4);
                    _sum2 = vld1q_s32(outptr + 8);
                    _sum3 = vld1q_s32(outptr + 12);
                    _sum4 = vld1q_s32(outptr + 16);
                    _sum5 = vld1q_s32(outptr + 20);
                    _sum6 = vld1q_s32(outptr + 24);
                    _sum7 = vld1q_s32(outptr + 28);
                    _sum8 = vld1q_s32(outptr + 32);
                    _sum9 = vld1q_s32(outptr + 36);
                    _suma = vld1q_s32(outptr + 40);
                    _sumb = vld1q_s32(outptr + 44);
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    int16x8_t _pA = vld1q_s16(pA);
                    int16x8_t _pB = vld1q_s16(pB);
                    _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_pA), vget_low_s16(_pB), 0);
                    _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_pA), vget_low_s16(_pB), 0);
                    _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_pA), vget_low_s16(_pB), 1);
                    _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_pA), vget_low_s16(_pB), 1);
                    _sum4 = vmlal_lane_s16(_sum4, vget_low_s16(_pA), vget_low_s16(_pB), 2);
                    _sum5 = vmlal_lane_s16(_sum5, vget_high_s16(_pA), vget_low_s16(_pB), 2);
                    _sum6 = vmlal_lane_s16(_sum6, vget_low_s16(_pA), vget_low_s16(_pB), 3);
                    _sum7 = vmlal_lane_s16(_sum7, vget_high_s16(_pA), vget_low_s16(_pB), 3);
                    _sum8 = vmlal_lane_s16(_sum8, vget_low_s16(_pA), vget_high_s16(_pB), 0);
                    _sum9 = vmlal_lane_s16(_sum9, vget_high_s16(_pA), vget_high_s16(_pB), 0);
                    _suma = vmlal_lane_s16(_suma, vget_low_s16(_pA), vget_high_s16(_pB), 1);
                    _sumb = vmlal_lane_s16(_sumb, vget_high_s16(_pA), vget_high_s16(_pB), 1);
                    pA += 8;
                    pB += 6;
                }

                vst1q_s32(outptr, _sum0);
                vst1q_s32(outptr + 4, _sum1);
                vst1q_s32(outptr + 8, _sum2);
                vst1q_s32(outptr + 12, _sum3);
                vst1q_s32(outptr + 16, _sum4);
                vst1q_s32(outptr + 20, _sum5);
                vst1q_s32(outptr + 24, _sum6);
                vst1q_s32(outptr + 28, _sum7);
                vst1q_s32(outptr + 32, _sum8);
                vst1q_s32(outptr + 36, _sum9);
                vst1q_s32(outptr + 40, _suma);
                vst1q_s32(outptr + 44, _sumb);
                outptr += 48;
            }
            for (; jj + 3 < max_jj; jj += 4)
            {
                const short* pA = pAT;

                int32x4_t _sum0;
                int32x4_t _sum1;
                int32x4_t _sum2;
                int32x4_t _sum3;
                int32x4_t _sum4;
                int32x4_t _sum5;
                int32x4_t _sum6;
                int32x4_t _sum7;

                if (k == 0)
                {
                    _sum0 = vdupq_n_s32(0);
                    _sum1 = vdupq_n_s32(0);
                    _sum2 = vdupq_n_s32(0);
                    _sum3 = vdupq_n_s32(0);
                    _sum4 = vdupq_n_s32(0);
                    _sum5 = vdupq_n_s32(0);
                    _sum6 = vdupq_n_s32(0);
                    _sum7 = vdupq_n_s32(0);
                }
                else
                {
                    _sum0 = vld1q_s32(outptr);
                    _sum1 = vld1q_s32(outptr + 4);
                    _sum2 = vld1q_s32(outptr + 8);
                    _sum3 = vld1q_s32(outptr + 12);
                    _sum4 = vld1q_s32(outptr + 16);
                    _sum5 = vld1q_s32(outptr + 20);
                    _sum6 = vld1q_s32(outptr + 24);
                    _sum7 = vld1q_s32(outptr + 28);
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    int16x8_t _pA = vld1q_s16(pA);
                    int16x4_t _pB = vld1_s16(pB);
                    _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_pA), _pB, 0);
                    _sum1 = vmlal_lane_s16(_sum1, vget_high_s16(_pA), _pB, 0);
                    _sum2 = vmlal_lane_s16(_sum2, vget_low_s16(_pA), _pB, 1);
                    _sum3 = vmlal_lane_s16(_sum3, vget_high_s16(_pA), _pB, 1);
                    _sum4 = vmlal_lane_s16(_sum4, vget_low_s16(_pA), _pB, 2);
                    _sum5 = vmlal_lane_s16(_sum5, vget_high_s16(_pA), _pB, 2);
                    _sum6 = vmlal_lane_s16(_sum6, vget_low_s16(_pA), _pB, 3);
                    _sum7 = vmlal_lane_s16(_sum7, vget_high_s16(_pA), _pB, 3);
                    pA += 8;
                    pB += 4;
                }

                vst1q_s32(outptr, _sum0);
                vst1q_s32(outptr + 4, _sum1);
                vst1q_s32(outptr + 8, _sum2);
                vst1q_s32(outptr + 12, _sum3);
                vst1q_s32(outptr + 16, _sum4);
                vst1q_s32(outptr + 20, _sum5);
                vst1q_s32(outptr + 24, _sum6);
                vst1q_s32(outptr + 28, _sum7);
                outptr += 32;
            }
            for (; jj + 1 < max_jj; jj += 2)
            {
                const short* pA = pAT;

                int32x4_t _sum0;
                int32x4_t _sum1;
                int32x4_t _sum2;
                int32x4_t _sum3;

                if (k == 0)
                {
                    _sum0 = vdupq_n_s32(0);
                    _sum1 = vdupq_n_s32(0);
                    _sum2 = vdupq_n_s32(0);
                    _sum3 = vdupq_n_s32(0);
                }
                else
                {
                    _sum0 = vld1q_s32(outptr);
                    _sum1 = vld1q_s32(outptr + 4);
                    _sum2 = vld1q_s32(outptr + 8);
                    _sum3 = vld1q_s32(outptr + 12);
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    int16x8_t _pA = vld1q_s16(pA);
                    int16x4_t _pB0 = vdup_n_s16(pB[0]);
                    int16x4_t _pB1 = vdup_n_s16(pB[1]);
                    _sum0 = vmlal_s16(_sum0, vget_low_s16(_pA), _pB0);
                    _sum1 = vmlal_s16(_sum1, vget_high_s16(_pA), _pB0);
                    _sum2 = vmlal_s16(_sum2, vget_low_s16(_pA), _pB1);
                    _sum3 = vmlal_s16(_sum3, vget_high_s16(_pA), _pB1);
                    pA += 8;
                    pB += 2;
                }

                vst1q_s32(outptr, _sum0);
                vst1q_s32(outptr + 4, _sum1);
                vst1q_s32(outptr + 8, _sum2);
                vst1q_s32(outptr + 12, _sum3);
                outptr += 16;
            }
            for (; jj < max_jj; jj++)
            {
                const short* pA = pAT;

                int32x4_t _sum0;
                int32x4_t _sum1;

                if (k == 0)
                {
                    _sum0 = vdupq_n_s32(0);
                    _sum1 = vdupq_n_s32(0);
                }
                else
                {
                    _sum0 = vld1q_s32(outptr);
                    _sum1 = vld1q_s32(outptr + 4);
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    int16x8_t _pA = vld1q_s16(pA);
                    int16x4_t _pB = vld1_dup_s16(pB);
                    _sum0 = vmlal_s16(_sum0, vget_low_s16(_pA), _pB);
                    _sum1 = vmlal_s16(_sum1, vget_high_s16(_pA), _pB);
                    pA += 8;
                    pB += 1;
                }

                vst1q_s32(outptr, _sum0);
                vst1q_s32(outptr + 4, _sum1);
                outptr += 8;
            }
        }
    }
    for (; ii + 3 < max_ii; ii += 4)
    {
        for (int b = 0; b < batch; b++)
        {
            const short* pAT = AT_tile.row<const short>(b) + max_kk * ii;
            const short* pB = BT_tile.row<const short>(b);

            int jj = 0;
#if __aarch64__
            for (; jj + 7 < max_jj; jj += 8)
            {
                const short* pA = pAT;

                int32x4_t _sum0;
                int32x4_t _sum1;
                int32x4_t _sum2;
                int32x4_t _sum3;
                int32x4_t _sum4;
                int32x4_t _sum5;
                int32x4_t _sum6;
                int32x4_t _sum7;

                if (k == 0)
                {
                    _sum0 = vdupq_n_s32(0);
                    _sum1 = vdupq_n_s32(0);
                    _sum2 = vdupq_n_s32(0);
                    _sum3 = vdupq_n_s32(0);
                    _sum4 = vdupq_n_s32(0);
                    _sum5 = vdupq_n_s32(0);
                    _sum6 = vdupq_n_s32(0);
                    _sum7 = vdupq_n_s32(0);
                }
                else
                {
                    _sum0 = vld1q_s32(outptr);
                    _sum1 = vld1q_s32(outptr + 4);
                    _sum2 = vld1q_s32(outptr + 8);
                    _sum3 = vld1q_s32(outptr + 12);
                    _sum4 = vld1q_s32(outptr + 16);
                    _sum5 = vld1q_s32(outptr + 20);
                    _sum6 = vld1q_s32(outptr + 24);
                    _sum7 = vld1q_s32(outptr + 28);
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    int16x4_t _pA = vld1_s16(pA);
                    int16x8_t _pB = vld1q_s16(pB);
                    _sum0 = vmlal_laneq_s16(_sum0, _pA, _pB, 0);
                    _sum1 = vmlal_laneq_s16(_sum1, _pA, _pB, 1);
                    _sum2 = vmlal_laneq_s16(_sum2, _pA, _pB, 2);
                    _sum3 = vmlal_laneq_s16(_sum3, _pA, _pB, 3);
                    _sum4 = vmlal_laneq_s16(_sum4, _pA, _pB, 4);
                    _sum5 = vmlal_laneq_s16(_sum5, _pA, _pB, 5);
                    _sum6 = vmlal_laneq_s16(_sum6, _pA, _pB, 6);
                    _sum7 = vmlal_laneq_s16(_sum7, _pA, _pB, 7);
                    pA += 4;
                    pB += 8;
                }

                vst1q_s32(outptr, _sum0);
                vst1q_s32(outptr + 4, _sum1);
                vst1q_s32(outptr + 8, _sum2);
                vst1q_s32(outptr + 12, _sum3);
                vst1q_s32(outptr + 16, _sum4);
                vst1q_s32(outptr + 20, _sum5);
                vst1q_s32(outptr + 24, _sum6);
                vst1q_s32(outptr + 28, _sum7);
                outptr += 32;
            }
#endif // __aarch64__
            for (; jj + 5 < max_jj; jj += 6)
            {
                const short* pA = pAT;

                int32x4_t _sum0;
                int32x4_t _sum1;
                int32x4_t _sum2;
                int32x4_t _sum3;
                int32x4_t _sum4;
                int32x4_t _sum5;

                if (k == 0)
                {
                    _sum0 = vdupq_n_s32(0);
                    _sum1 = vdupq_n_s32(0);
                    _sum2 = vdupq_n_s32(0);
                    _sum3 = vdupq_n_s32(0);
                    _sum4 = vdupq_n_s32(0);
                    _sum5 = vdupq_n_s32(0);
                }
                else
                {
                    _sum0 = vld1q_s32(outptr);
                    _sum1 = vld1q_s32(outptr + 4);
                    _sum2 = vld1q_s32(outptr + 8);
                    _sum3 = vld1q_s32(outptr + 12);
                    _sum4 = vld1q_s32(outptr + 16);
                    _sum5 = vld1q_s32(outptr + 20);
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    int16x4_t _pA = vld1_s16(pA);
                    int16x8_t _pB = vld1q_s16(pB);
                    _sum0 = vmlal_lane_s16(_sum0, _pA, vget_low_s16(_pB), 0);
                    _sum1 = vmlal_lane_s16(_sum1, _pA, vget_low_s16(_pB), 1);
                    _sum2 = vmlal_lane_s16(_sum2, _pA, vget_low_s16(_pB), 2);
                    _sum3 = vmlal_lane_s16(_sum3, _pA, vget_low_s16(_pB), 3);
                    _sum4 = vmlal_lane_s16(_sum4, _pA, vget_high_s16(_pB), 0);
                    _sum5 = vmlal_lane_s16(_sum5, _pA, vget_high_s16(_pB), 1);
                    pA += 4;
                    pB += 6;
                }

                vst1q_s32(outptr, _sum0);
                vst1q_s32(outptr + 4, _sum1);
                vst1q_s32(outptr + 8, _sum2);
                vst1q_s32(outptr + 12, _sum3);
                vst1q_s32(outptr + 16, _sum4);
                vst1q_s32(outptr + 20, _sum5);
                outptr += 24;
            }
            for (; jj + 3 < max_jj; jj += 4)
            {
                const short* pA = pAT;

                int32x4_t _sum0;
                int32x4_t _sum1;
                int32x4_t _sum2;
                int32x4_t _sum3;

                if (k == 0)
                {
                    _sum0 = vdupq_n_s32(0);
                    _sum1 = vdupq_n_s32(0);
                    _sum2 = vdupq_n_s32(0);
                    _sum3 = vdupq_n_s32(0);
                }
                else
                {
                    _sum0 = vld1q_s32(outptr);
                    _sum1 = vld1q_s32(outptr + 4);
                    _sum2 = vld1q_s32(outptr + 8);
                    _sum3 = vld1q_s32(outptr + 12);
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    int16x4_t _pA = vld1_s16(pA);
                    int16x4_t _pB = vld1_s16(pB);
                    _sum0 = vmlal_lane_s16(_sum0, _pA, _pB, 0);
                    _sum1 = vmlal_lane_s16(_sum1, _pA, _pB, 1);
                    _sum2 = vmlal_lane_s16(_sum2, _pA, _pB, 2);
                    _sum3 = vmlal_lane_s16(_sum3, _pA, _pB, 3);
                    pA += 4;
                    pB += 4;
                }

                vst1q_s32(outptr, _sum0);
                vst1q_s32(outptr + 4, _sum1);
                vst1q_s32(outptr + 8, _sum2);
                vst1q_s32(outptr + 12, _sum3);
                outptr += 16;
            }
            for (; jj + 1 < max_jj; jj += 2)
            {
                const short* pA = pAT;

                int32x4_t _sum0;
                int32x4_t _sum1;

                if (k == 0)
                {
                    _sum0 = vdupq_n_s32(0);
                    _sum1 = vdupq_n_s32(0);
                }
                else
                {
                    _sum0 = vld1q_s32(outptr);
                    _sum1 = vld1q_s32(outptr + 4);
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    int16x4_t _pA = vld1_s16(pA);
                    int16x4_t _pB0 = vdup_n_s16(pB[0]);
                    int16x4_t _pB1 = vdup_n_s16(pB[1]);
                    _sum0 = vmlal_s16(_sum0, _pA, _pB0);
                    _sum1 = vmlal_s16(_sum1, _pA, _pB1);
                    pA += 4;
                    pB += 2;
                }

                vst1q_s32(outptr, _sum0);
                vst1q_s32(outptr + 4, _sum1);
                outptr += 8;
            }
            for (; jj < max_jj; jj++)
            {
                const short* pA = pAT;

                int32x4_t _sum0;

                if (k == 0)
                {
                    _sum0 = vdupq_n_s32(0);
                }
                else
                {
                    _sum0 = vld1q_s32(outptr);
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    int16x4_t _pA = vld1_s16(pA);
                    int16x4_t _pB = vld1_dup_s16(pB);
                    _sum0 = vmlal_s16(_sum0, _pA, _pB);
                    pA += 4;
                    pB += 1;
                }

                vst1q_s32(outptr, _sum0);
                outptr += 4;
            }
        }
    }
#endif // __ARM_NEON
    for (; ii + 1 < max_ii; ii += 2)
    {
        for (int b = 0; b < batch; b++)
        {
            const short* pAT = AT_tile.row<const short>(b) + max_kk * ii;
            const short* pB = BT_tile.row<const short>(b);

            int jj = 0;
#if __ARM_NEON
#if __aarch64__
            for (; jj + 7 < max_jj; jj += 8)
            {
                const short* pA = pAT;

                int32x4_t _sum0;
                int32x4_t _sum1;
                int32x4_t _sum2;
                int32x4_t _sum3;

                if (k == 0)
                {
                    _sum0 = vdupq_n_s32(0);
                    _sum1 = vdupq_n_s32(0);
                    _sum2 = vdupq_n_s32(0);
                    _sum3 = vdupq_n_s32(0);
                }
                else
                {
                    int32x4x2_t _s01 = vld2q_s32(outptr);
                    int32x4x2_t _s23 = vld2q_s32(outptr + 8);
                    _sum0 = _s01.val[0];
                    _sum2 = _s01.val[1];
                    _sum1 = _s23.val[0];
                    _sum3 = _s23.val[1];
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    int16x4_t _pA0 = vdup_n_s16(pA[0]);
                    int16x4_t _pA1 = vdup_n_s16(pA[1]);
                    int16x8_t _pB = vld1q_s16(pB);
                    _sum0 = vmlal_s16(_sum0, _pA0, vget_low_s16(_pB));
                    _sum1 = vmlal_s16(_sum1, _pA0, vget_high_s16(_pB));
                    _sum2 = vmlal_s16(_sum2, _pA1, vget_low_s16(_pB));
                    _sum3 = vmlal_s16(_sum3, _pA1, vget_high_s16(_pB));
                    pA += 2;
                    pB += 8;
                }

                int32x4x2_t _s01;
                _s01.val[0] = _sum0;
                _s01.val[1] = _sum2;
                int32x4x2_t _s23;
                _s23.val[0] = _sum1;
                _s23.val[1] = _sum3;
                vst2q_s32(outptr, _s01);
                vst2q_s32(outptr + 8, _s23);
                outptr += 16;
            }
#endif // __aarch64__
            for (; jj + 5 < max_jj; jj += 6)
            {
                const short* pA = pAT;

                int32x4_t _sum0;
                int32x4_t _sum1;
                int32x4_t _sum2;

                if (k == 0)
                {
                    _sum0 = vdupq_n_s32(0);
                    _sum1 = vdupq_n_s32(0);
                    _sum2 = vdupq_n_s32(0);
                }
                else
                {
                    int32x4x2_t _s01 = vld2q_s32(outptr);
                    _sum0 = _s01.val[0];
                    _sum1 = _s01.val[1];
                    _sum2 = vld1q_s32(outptr + 8);
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    int16x4_t _pA = vreinterpret_s16_s32(vld1_dup_s32((const int*)pA));
                    int16x8_t _pB = vld1q_s16(pB);
                    int16x4_t _pB2 = vzip_s16(vget_high_s16(_pB), vget_high_s16(_pB)).val[0];
                    _sum0 = vmlal_lane_s16(_sum0, vget_low_s16(_pB), _pA, 0);
                    _sum1 = vmlal_lane_s16(_sum1, vget_low_s16(_pB), _pA, 1);
                    _sum2 = vmlal_s16(_sum2, _pA, vreinterpret_s16_s32(_pB2));
                    pA += 2;
                    pB += 6;
                }

                int32x4x2_t _s01;
                _s01.val[0] = _sum0;
                _s01.val[1] = _sum1;
                vst2q_s32(outptr, _s01);
                vst1q_s32(outptr + 8, _sum2);
                outptr += 12;
            }
            for (; jj + 3 < max_jj; jj += 4)
            {
                const short* pA = pAT;

                int32x4_t _sum0;
                int32x4_t _sum1;

                if (k == 0)
                {
                    _sum0 = vdupq_n_s32(0);
                    _sum1 = vdupq_n_s32(0);
                }
                else
                {
                    int32x4x2_t _s01 = vld2q_s32(outptr);
                    _sum0 = _s01.val[0];
                    _sum1 = _s01.val[1];
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    int16x4_t _pA0 = vdup_n_s16(pA[0]);
                    int16x4_t _pA1 = vdup_n_s16(pA[1]);
                    int16x4_t _pB = vld1_s16(pB);
                    _sum0 = vmlal_s16(_sum0, _pA0, _pB);
                    _sum1 = vmlal_s16(_sum1, _pA1, _pB);
                    pA += 2;
                    pB += 4;
                }

                int32x4x2_t _s01;
                _s01.val[0] = _sum0;
                _s01.val[1] = _sum1;
                vst2q_s32(outptr, _s01);
                outptr += 8;
            }
#endif // __ARM_NEON
            for (; jj + 1 < max_jj; jj += 2)
            {
                const short* pA = pAT;

                int sum00 = 0;
                int sum01 = 0;
                int sum10 = 0;
                int sum11 = 0;

                if (k == 0)
                {
                    sum00 = 0;
                    sum01 = 0;
                    sum10 = 0;
                    sum11 = 0;
                }
                else
                {
                    sum00 = outptr[0];
                    sum01 = outptr[1];
                    sum10 = outptr[2];
                    sum11 = outptr[3];
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    sum00 += pA[0] * pB[0];
                    sum01 += pA[1] * pB[0];
                    sum10 += pA[0] * pB[1];
                    sum11 += pA[1] * pB[1];
                    pA += 2;
                    pB += 2;
                }

                outptr[0] = sum00;
                outptr[1] = sum01;
                outptr[2] = sum10;
                outptr[3] = sum11;
                outptr += 2 * 2;
            }
            for (; jj < max_jj; jj++)
            {
                const short* pA = pAT;

                int sum0 = 0;
                int sum1 = 0;

                if (k == 0)
                {
                    sum0 = 0;
                    sum1 = 0;
                }
                else
                {
                    sum0 = outptr[0];
                    sum1 = outptr[1];
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    sum0 += pA[0] * pB[0];
                    sum1 += pA[1] * pB[0];
                    pA += 2;
                    pB += 1;
                }

                outptr[0] = sum0;
                outptr[1] = sum1;
                outptr += 2;
            }
        }
    }
    for (; ii < max_ii; ii++)
    {
        for (int b = 0; b < batch; b++)
        {
            const short* pAT = AT_tile.row<const short>(b) + max_kk * ii;
            const short* pB = BT_tile.row<const short>(b);

            int jj = 0;
#if __ARM_NEON
#if __aarch64__
            for (; jj + 7 < max_jj; jj += 8)
            {
                const short* pA = pAT;

                int32x4_t _sum0;
                int32x4_t _sum1;

                if (k == 0)
                {
                    _sum0 = vdupq_n_s32(0);
                    _sum1 = vdupq_n_s32(0);
                }
                else
                {
                    _sum0 = vld1q_s32(outptr);
                    _sum1 = vld1q_s32(outptr + 4);
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    int16x4_t _pA = vld1_dup_s16(pA);
                    int16x8_t _pB = vld1q_s16(pB);
                    _sum0 = vmlal_s16(_sum0, _pA, vget_low_s16(_pB));
                    _sum1 = vmlal_s16(_sum1, _pA, vget_high_s16(_pB));
                    pA += 1;
                    pB += 8;
                }

                vst1q_s32(outptr, _sum0);
                vst1q_s32(outptr + 4, _sum1);
                outptr += 8;
            }
#endif // __aarch64__
            for (; jj + 5 < max_jj; jj += 6)
            {
                const short* pA = pAT;

                int32x4_t _sum0;
                int32x4_t _sum1;

                if (k == 0)
                {
                    _sum0 = vdupq_n_s32(0);
                    _sum1 = vdupq_n_s32(0);
                }
                else
                {
                    _sum0 = vld1q_s32(outptr);
                    _sum1 = vld1q_s32(outptr + 4);
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    int16x4_t _pA = vld1_dup_s16(pA);
                    int16x8_t _pB = vld1q_s16(pB);
                    _sum0 = vmlal_s16(_sum0, _pA, vget_low_s16(_pB));
                    _sum1 = vmlal_s16(_sum1, _pA, vget_high_s16(_pB));
                    pA += 1;
                    pB += 6;
                }

                vst1q_s32(outptr, _sum0);
                vst1_s32(outptr + 4, vget_low_s32(_sum1));
                outptr += 6;
            }
            for (; jj + 3 < max_jj; jj += 4)
            {
                const short* pA = pAT;

                int32x4_t _sum0;

                if (k == 0)
                {
                    _sum0 = vdupq_n_s32(0);
                }
                else
                {
                    _sum0 = vld1q_s32(outptr);
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    int16x4_t _pA = vld1_dup_s16(pA);
                    int16x4_t _pB = vld1_s16(pB);
                    _sum0 = vmlal_s16(_sum0, _pA, _pB);
                    pA += 1;
                    pB += 4;
                }

                vst1q_s32(outptr, _sum0);
                outptr += 4;
            }
#endif // __ARM_NEON
            for (; jj + 1 < max_jj; jj += 2)
            {
                const short* pA = pAT;

                int sum0 = 0;
                int sum1 = 0;

                if (k == 0)
                {
                    sum0 = 0;
                    sum1 = 0;
                }
                else
                {
                    sum0 = outptr[0];
                    sum1 = outptr[1];
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    sum0 += pA[0] * pB[0];
                    sum1 += pA[0] * pB[1];
                    pA += 1;
                    pB += 2;
                }

                outptr[0] = sum0;
                outptr[1] = sum1;
                outptr += 2;
            }
            for (; jj < max_jj; jj++)
            {
                const short* pA = pAT;

                int sum = 0;

                if (k == 0)
                {
                    sum = 0;
                }
                else
                {
                    sum = outptr[0];
                }

                int kk = 0;
                for (; kk < max_kk; kk++)
                {
                    sum += pA[0] * pB[0];
                    pA += 1;
                    pB += 1;
                }

                outptr[0] = sum;
                outptr += 1;
            }
        }
    }
}

static void get_optimal_tile_mnk_int8(int M, int N, int K, int& TILE_M, int& TILE_N, int& TILE_K, int nT)
{
    // resolve optimal tile size from cache size
    const size_t l2_cache_size_int8 = (int)(get_cpu_level2_cache_size() / sizeof(short));

    if (nT == 0)
        nT = get_physical_big_cpu_count();

    // solve M
    {
        int tile_size = (int)sqrt((float)l2_cache_size_int8 / 3);

#if __ARM_NEON
        TILE_M = std::max(8, tile_size / 8 * 8);
#else
        TILE_M = std::max(2, tile_size / 2 * 2);
#endif

        TILE_M *= std::min(nT, get_physical_cpu_count());

        int nn_M = (M + TILE_M - 1) / TILE_M;
#if __ARM_NEON
        TILE_M = std::min(TILE_M, ((M + nn_M - 1) / nn_M + 7) / 8 * 8);
#else
        TILE_M = std::min(TILE_M, ((M + nn_M - 1) / nn_M + 1) / 2 * 2);
#endif

        if (nT > 1)
        {
#if __ARM_NEON
            TILE_M = std::min(TILE_M, (std::max(1, TILE_M / nT) + 7) / 8 * 8);
#else
            TILE_M = std::min(TILE_M, (std::max(1, TILE_M / nT) + 1) / 2 * 2);
#endif
        }
    }

    // solve K
    {
        int tile_size = (int)(sqrt((float)l2_cache_size_int8) - TILE_M);

#if __aarch64__
        TILE_K = std::max(8, tile_size / 8 * 8);
#elif __ARM_NEON
        TILE_K = std::max(4, tile_size / 4 * 4);
#else
        TILE_K = std::max(2, tile_size / 2 * 2);
#endif

        int nn_K = (K + TILE_K - 1) / TILE_K;
#if __aarch64__
        TILE_K = std::min(TILE_K, ((K + nn_K - 1) / nn_K + 7) / 8 * 8);
#elif __ARM_NEON
        TILE_K = std::min(TILE_K, ((K + nn_K - 1) / nn_K + 3) / 4 * 4);
#else
        TILE_K = std::min(TILE_K, ((K + nn_K - 1) / nn_K + 1) / 2 * 2);
#endif
    }

    if (N > 0)
    {
        int tile_size = (int)((l2_cache_size_int8 - TILE_M * TILE_K) / (TILE_M * 2 + TILE_K));

#if __ARM_NEON
        TILE_N = std::max(4, tile_size / 4 * 4);
#else
        TILE_N = std::max(1, tile_size);
#endif

        int nn_N = (N + TILE_N - 1) / TILE_N;
#if __ARM_NEON
        TILE_N = std::min(TILE_N, ((N + nn_N - 1) / nn_N + 3) / 4 * 4);
#else
        TILE_N = std::min(TILE_N, (N + nn_N - 1) / nn_N);
#endif
    }
}

static inline void conv3x3s1_winograd23_transform_kernel_tile_int8(const Mat& kernel, Mat& A, int inch, int i, int max_ii, int k, int max_kk)
{
    // const signed char ktm[4][3] = {
    //     {2, 0, 0},
    //     {1, 1, 1},
    //     {1, -1, 1},
    //     {0, 0, 2}
    // };

    short* ptmp = A;

    int ii = 0;
    for (; ii < max_ii; ii++)
    {
        int kk = 0;
        for (; kk < max_kk; kk++)
        {
            short tmp[4][3];

            const signed char* k0 = (const signed char*)kernel + (i + ii) * inch * 9 + (k + kk) * 9;

            for (int m = 0; m < 3; m++)
            {
                signed char r0 = k0[0];
                signed char r1 = k0[1];
                signed char r2 = k0[2];

                tmp[0][m] = r0 * 2;
                tmp[1][m] = r0 + r1 + r2;
                tmp[2][m] = r0 - r1 + r2;
                tmp[3][m] = r2 * 2;

                k0 += 3;
            }

            for (int m = 0; m < 4; m++)
            {
                short r0 = tmp[m][0];
                short r1 = tmp[m][1];
                short r2 = tmp[m][2];

                short z0 = r0 * 2;
                short z1 = r0 + r1 + r2;
                short z2 = r0 - r1 + r2;
                short z3 = r2 * 2;

                ptmp[0] = z0;
                ptmp[1] = z1;
                ptmp[2] = z2;
                ptmp[3] = z3;
                ptmp += 4;
            }
        }
    }
}

static void conv3x3s1_winograd23_transform_kernel_int8(const Mat& kernel, Mat& AT, int inch, int outch, const Option& opt)
{
    const int M = outch;
    const int K = inch;
    const int B = 16;

    int TILE_M, TILE_N, TILE_K;
    get_optimal_tile_mnk_int8(M, 0, K, TILE_M, TILE_N, TILE_K, opt.num_threads);

    const int nn_M = (M + TILE_M - 1) / TILE_M;

    Mat A_tileX(B * TILE_M * TILE_K, 1, opt.num_threads, 2u, (Allocator*)0);

    AT.create(TILE_K * TILE_M, B, (K + TILE_K - 1) / TILE_K, (M + TILE_M - 1) / TILE_M, 2u, (Allocator*)0);

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int ppj = 0; ppj < nn_M; ppj++)
    {
        const int i = ppj * TILE_M;

        Mat A_tile = A_tileX.channel(get_omp_thread_num());

        for (int k = 0; k < K; k += TILE_K)
        {
            const int max_ii = std::min((M - i), TILE_M);
            const int max_kk = std::min((K - k), TILE_K);

            conv3x3s1_winograd23_transform_kernel_tile_int8(kernel, A_tile, inch, i, max_ii, k, max_kk);

            Mat AT_tile = AT.channel(i / TILE_M).depth(k / TILE_K);

            pack_A_tile_int8(A_tile, AT_tile, B, max_ii, max_kk);
        }
    }
}

static inline void conv3x3s1_winograd23_transform_input_tile_int8(const Mat& bottom_blob, Mat& B, int j, int max_jj, int k, int max_kk, int nT)
{
    // const signed char itm[4][4] = {
    //     {1,  0, -1,  0},
    //     {0,  1,  1,  0},
    //     {0, -1,  1,  0},
    //     {0, -1,  0,  1}
    // };

    const int w = bottom_blob.w;
    const int h = bottom_blob.h;
    const int elempack = bottom_blob.elempack;
    const int N = bottom_blob.cstep * elempack;

    const int w_tiles = (w - 1) / 2;

    int nn_max_kk = 0;
    int remain_max_kk_start = 0;
#if __ARM_NEON
    nn_max_kk = max_kk / 8;
    #pragma omp parallel for num_threads(nT)
    for (int ppkk = 0; ppkk < nn_max_kk; ppkk++)
    {
        const int kk = remain_max_kk_start + ppkk * 8;

        short tmp[4][4][8];

        int jj = 0;
        for (; jj < max_jj; jj++)
        {
            int ti = (j + jj) / w_tiles;
            int tj = (j + jj) % w_tiles;

            const signed char* r0 = bottom_blob.channel((k + kk) / elempack).row<const signed char>(ti * 2) + (tj * 2) * elempack;

            for (int m = 0; m < 4; m++)
            {
                int8x8_t _r0 = vdup_n_s8(0);
                int8x8_t _r1 = vdup_n_s8(0);
                int8x8_t _r2 = vdup_n_s8(0);
                int8x8_t _r3 = vdup_n_s8(0);

                if (ti * 2 + m < h)
                {
                    if (elempack == 8)
                    {
                        _r0 = vld1_s8(r0);
                        if (tj * 2 + 1 < w) _r1 = vld1_s8(r0 + 8);
                        if (tj * 2 + 2 < w) _r2 = vld1_s8(r0 + 16);
                        if (tj * 2 + 3 < w) _r3 = vld1_s8(r0 + 24);
                    }
                    if (elempack == 1)
                    {
                        const signed char* r1 = r0 + N;
                        const signed char* r2 = r0 + N * 2;
                        const signed char* r3 = r0 + N * 3;
                        const signed char* r4 = r0 + N * 4;
                        const signed char* r5 = r0 + N * 5;
                        const signed char* r6 = r0 + N * 6;
                        const signed char* r7 = r0 + N * 7;

                        int8x8_t _t0 = vld1_s8(r0);
                        int8x8_t _t1 = vld1_s8(r1);
                        int8x8_t _t2 = vld1_s8(r2);
                        int8x8_t _t3 = vld1_s8(r3);
                        int8x8_t _t4 = vld1_s8(r4);
                        int8x8_t _t5 = vld1_s8(r5);
                        int8x8_t _t6 = vld1_s8(r6);
                        int8x8_t _t7 = vld1_s8(r7);

                        int8x8_t _t01 = vzip_s8(_t0, _t1).val[0];
                        int8x8_t _t23 = vzip_s8(_t2, _t3).val[0];
                        int8x8_t _t45 = vzip_s8(_t4, _t5).val[0];
                        int8x8_t _t67 = vzip_s8(_t6, _t7).val[0];
                        int16x4x2_t _t0123 = vzip_s16(vreinterpret_s16_s8(_t01), vreinterpret_s16_s8(_t23));
                        int16x4x2_t _t4567 = vzip_s16(vreinterpret_s16_s8(_t45), vreinterpret_s16_s8(_t67));
                        int16x8_t _ta = vcombine_s16(_t0123.val[0], _t0123.val[1]);
                        int16x8_t _tb = vcombine_s16(_t4567.val[0], _t4567.val[1]);
                        int32x4x2_t _tab = vzipq_s32(vreinterpretq_s32_s16(_ta), vreinterpretq_s32_s16(_tb));

                        _r0 = vreinterpret_s8_s32(vget_low_s32(_tab.val[0]));
                        if (tj * 2 + 1 < w) _r1 = vreinterpret_s8_s32(vget_high_s32(_tab.val[0]));
                        if (tj * 2 + 2 < w) _r2 = vreinterpret_s8_s32(vget_low_s32(_tab.val[1]));
                        if (tj * 2 + 3 < w) _r3 = vreinterpret_s8_s32(vget_high_s32(_tab.val[1]));
                    }
                }

                int16x8_t _tmp0 = vsubl_s8(_r0, _r2);
                int16x8_t _tmp1 = vaddl_s8(_r1, _r2);
                int16x8_t _tmp2 = vsubl_s8(_r2, _r1);
                int16x8_t _tmp3 = vsubl_s8(_r3, _r1);

                vst1q_s16(tmp[0][m], _tmp0);
                vst1q_s16(tmp[1][m], _tmp1);
                vst1q_s16(tmp[2][m], _tmp2);
                vst1q_s16(tmp[3][m], _tmp3);

                r0 += w * elempack;
            }

            short* p0 = (short*)B + kk * max_jj * 16 + jj * 8;
            short* p1 = p0 + max_jj * 8;
            short* p2 = p0 + max_jj * 8 * 2;
            short* p3 = p0 + max_jj * 8 * 3;

            for (int m = 0; m < 4; m++)
            {
                int16x8_t _r0 = vld1q_s16(tmp[m][0]);
                int16x8_t _r1 = vld1q_s16(tmp[m][1]);
                int16x8_t _r2 = vld1q_s16(tmp[m][2]);
                int16x8_t _r3 = vld1q_s16(tmp[m][3]);

                int16x8_t _tmp0 = vsubq_s16(_r0, _r2);
                int16x8_t _tmp1 = vaddq_s16(_r1, _r2);
                int16x8_t _tmp2 = vsubq_s16(_r2, _r1);
                int16x8_t _tmp3 = vsubq_s16(_r3, _r1);

                vst1q_s16(p0, _tmp0);
                vst1q_s16(p1, _tmp1);
                vst1q_s16(p2, _tmp2);
                vst1q_s16(p3, _tmp3);

                p0 += max_jj * 4 * 8;
                p1 += max_jj * 4 * 8;
                p2 += max_jj * 4 * 8;
                p3 += max_jj * 4 * 8;
            }
        }
    }
    remain_max_kk_start += nn_max_kk * 8;
    nn_max_kk = (max_kk - remain_max_kk_start) / 2;
#else // __ARM_NEON
    nn_max_kk = (max_kk - remain_max_kk_start) / 2;
    #pragma omp parallel for num_threads(nT)
#endif // __ARM_NEON
    for (int ppkk = 0; ppkk < nn_max_kk; ppkk++)
    {
        const int kk = remain_max_kk_start + ppkk * 2;

        short tmp[4][4][2];

        int jj = 0;
        for (; jj < max_jj; jj++)
        {
            int ti = (j + jj) / w_tiles;
            int tj = (j + jj) % w_tiles;

            const signed char* r0 = bottom_blob.channel(k + kk).row<const signed char>(ti * 2) + (tj * 2);

            for (int m = 0; m < 4; m++)
            {
                signed char r00 = 0;
                signed char r01 = 0;
                signed char r10 = 0;
                signed char r11 = 0;
                signed char r20 = 0;
                signed char r21 = 0;
                signed char r30 = 0;
                signed char r31 = 0;

                if (ti * 2 + m < h)
                {
                    // if (elempack == 1)
                    {
                        const signed char* r1 = r0 + N;

                        r00 = r0[0];
                        r01 = r1[0];
                        if (tj * 2 + 1 < w)
                        {
                            r10 = r0[1];
                            r11 = r1[1];
                        }
                        if (tj * 2 + 2 < w)
                        {
                            r20 = r0[2];
                            r21 = r1[2];
                        }
                        if (tj * 2 + 3 < w)
                        {
                            r30 = r0[3];
                            r31 = r1[3];
                        }
                    }
                }

                tmp[0][m][0] = r00 - r20;
                tmp[0][m][1] = r01 - r21;
                tmp[1][m][0] = r10 + r20;
                tmp[1][m][1] = r11 + r21;
                tmp[2][m][0] = r20 - r10;
                tmp[2][m][1] = r21 - r11;
                tmp[3][m][0] = r30 - r10;
                tmp[3][m][1] = r31 - r11;

                r0 += w;
            }

            short* p0 = (short*)B + kk * max_jj * 16 + jj * 2;
            short* p1 = p0 + max_jj * 2;
            short* p2 = p0 + max_jj * 2 * 2;
            short* p3 = p0 + max_jj * 2 * 3;

            for (int m = 0; m < 4; m++)
            {
                short r00 = tmp[m][0][0];
                short r01 = tmp[m][0][1];
                short r10 = tmp[m][1][0];
                short r11 = tmp[m][1][1];
                short r20 = tmp[m][2][0];
                short r21 = tmp[m][2][1];
                short r30 = tmp[m][3][0];
                short r31 = tmp[m][3][1];

                p0[0] = r00 - r20;
                p0[1] = r01 - r21;
                p1[0] = r10 + r20;
                p1[1] = r11 + r21;
                p2[0] = r20 - r10;
                p2[1] = r21 - r11;
                p3[0] = r30 - r10;
                p3[1] = r31 - r11;

                p0 += max_jj * 4 * 2;
                p1 += max_jj * 4 * 2;
                p2 += max_jj * 4 * 2;
                p3 += max_jj * 4 * 2;
            }
        }
    }
    remain_max_kk_start += nn_max_kk * 2;
    for (int kk = remain_max_kk_start; kk < max_kk; kk++)
    {
        short tmp[4][4];

        int jj = 0;
        for (; jj < max_jj; jj++)
        {
            int ti = (j + jj) / w_tiles;
            int tj = (j + jj) % w_tiles;

            const signed char* r0123 = bottom_blob.channel(k + kk).row<const signed char>(ti * 2) + (tj * 2);

            for (int m = 0; m < 4; m++)
            {
                signed char r0 = 0;
                signed char r1 = 0;
                signed char r2 = 0;
                signed char r3 = 0;

                if (ti * 2 + m < h)
                {
                    // if (elempack == 1)
                    {
                        r0 = r0123[0];
                        if (tj * 2 + 1 < w) r1 = r0123[1];
                        if (tj * 2 + 2 < w) r2 = r0123[2];
                        if (tj * 2 + 3 < w) r3 = r0123[3];
                    }
                }

                tmp[0][m] = r0 - r2;
                tmp[1][m] = r1 + r2;
                tmp[2][m] = r2 - r1;
                tmp[3][m] = r3 - r1;

                r0123 += w;
            }

            short* p0 = (short*)B + kk * max_jj * 16 + jj;
            short* p1 = p0 + max_jj;
            short* p2 = p0 + max_jj * 2;
            short* p3 = p0 + max_jj * 3;

            for (int m = 0; m < 4; m++)
            {
                short r0 = tmp[m][0];
                short r1 = tmp[m][1];
                short r2 = tmp[m][2];
                short r3 = tmp[m][3];

                p0[0] = r0 - r2;
                p1[0] = r1 + r2;
                p2[0] = r2 - r1;
                p3[0] = r3 - r1;

                p0 += max_jj * 4;
                p1 += max_jj * 4;
                p2 += max_jj * 4;
                p3 += max_jj * 4;
            }
        }
    }
}

static inline void conv3x3s1_winograd23_transform_output_tile_int8(const Mat& top_tile, Mat& top_blob, int i, int max_ii, int j, int max_jj)
{
    // const int otm[2][4] = {
    //     {1,  1,  1,  0},
    //     {0,  1, -1,  1}
    // };

    const int outw = top_blob.w;
    const int outh = top_blob.h;
    const int out_elempack = top_blob.elempack;
    const int N = top_blob.cstep * out_elempack;

    const int w_tiles = (outw + 1) / 2;

    int ii = 0;
#if __ARM_NEON
    for (; ii + 7 < max_ii; ii += 8)
    {
        int tmp[2][4][8];

        int jj = 0;
        for (; jj < max_jj; jj++)
        {
            int ti = (j + jj) / w_tiles;
            int tj = (j + jj) % w_tiles;

            const int* r0 = (const int*)top_tile + ii * max_jj * 16 + jj * 8;
            const int* r1 = r0 + max_jj * 8;
            const int* r2 = r0 + max_jj * 8 * 2;
            const int* r3 = r0 + max_jj * 8 * 3;

            for (int m = 0; m < 4; m++)
            {
                int32x4_t _r00 = vld1q_s32(r0);
                int32x4_t _r01 = vld1q_s32(r0 + 4);
                int32x4_t _r10 = vld1q_s32(r1);
                int32x4_t _r11 = vld1q_s32(r1 + 4);
                int32x4_t _r20 = vld1q_s32(r2);
                int32x4_t _r21 = vld1q_s32(r2 + 4);
                int32x4_t _r30 = vld1q_s32(r3);
                int32x4_t _r31 = vld1q_s32(r3 + 4);

                int32x4_t _tmp00 = vaddq_s32(vaddq_s32(_r00, _r10), _r20);
                int32x4_t _tmp01 = vaddq_s32(vaddq_s32(_r01, _r11), _r21);
                int32x4_t _tmp10 = vaddq_s32(vsubq_s32(_r10, _r20), _r30);
                int32x4_t _tmp11 = vaddq_s32(vsubq_s32(_r11, _r21), _r31);

                vst1q_s32(tmp[0][m], _tmp00);
                vst1q_s32(tmp[0][m] + 4, _tmp01);
                vst1q_s32(tmp[1][m], _tmp10);
                vst1q_s32(tmp[1][m] + 4, _tmp11);

                r0 += max_jj * 4 * 8;
                r1 += max_jj * 4 * 8;
                r2 += max_jj * 4 * 8;
                r3 += max_jj * 4 * 8;
            }

            int* outptr0 = top_blob.channel((i + ii) / out_elempack).row<int>(ti * 2) + (tj * 2) * out_elempack;

            for (int m = 0; m < 2; m++)
            {
                if (ti * 2 + m >= outh)
                    continue;

                int32x4_t _r00 = vld1q_s32(tmp[m][0]);
                int32x4_t _r01 = vld1q_s32(tmp[m][0] + 4);
                int32x4_t _r10 = vld1q_s32(tmp[m][1]);
                int32x4_t _r11 = vld1q_s32(tmp[m][1] + 4);
                int32x4_t _r20 = vld1q_s32(tmp[m][2]);
                int32x4_t _r21 = vld1q_s32(tmp[m][2] + 4);
                int32x4_t _r30 = vld1q_s32(tmp[m][3]);
                int32x4_t _r31 = vld1q_s32(tmp[m][3] + 4);

                int32x4_t _tmp00 = vaddq_s32(vaddq_s32(_r00, _r10), _r20);
                int32x4_t _tmp01 = vaddq_s32(vaddq_s32(_r01, _r11), _r21);
                int32x4_t _tmp10 = vaddq_s32(vsubq_s32(_r10, _r20), _r30);
                int32x4_t _tmp11 = vaddq_s32(vsubq_s32(_r11, _r21), _r31);

                _tmp00 = vshrq_n_s32(_tmp00, 2);
                _tmp01 = vshrq_n_s32(_tmp01, 2);
                _tmp10 = vshrq_n_s32(_tmp10, 2);
                _tmp11 = vshrq_n_s32(_tmp11, 2);

                if (out_elempack == 8)
                {
                    vst1q_s32(outptr0, _tmp00);
                    vst1q_s32(outptr0 + 4, _tmp01);
                    if (tj * 2 + 1 < outw)
                    {
                        vst1q_s32(outptr0 + 8, _tmp10);
                        vst1q_s32(outptr0 + 12, _tmp11);
                    }
                }
                if (out_elempack == 4)
                {
                    int* outptr1 = outptr0 + N;

                    vst1q_s32(outptr0, _tmp00);
                    vst1q_s32(outptr1, _tmp01);
                    if (tj * 2 + 1 < outw)
                    {
                        vst1q_s32(outptr0 + 4, _tmp10);
                        vst1q_s32(outptr1 + 4, _tmp11);
                    }
                }
                if (out_elempack == 1)
                {
                    int* outptr1 = outptr0 + N;
                    int* outptr2 = outptr0 + N * 2;
                    int* outptr3 = outptr0 + N * 3;
                    int* outptr4 = outptr0 + N * 4;
                    int* outptr5 = outptr0 + N * 5;
                    int* outptr6 = outptr0 + N * 6;
                    int* outptr7 = outptr0 + N * 7;

                    outptr0[0] = vgetq_lane_s32(_tmp00, 0);
                    outptr1[0] = vgetq_lane_s32(_tmp00, 1);
                    outptr2[0] = vgetq_lane_s32(_tmp00, 2);
                    outptr3[0] = vgetq_lane_s32(_tmp00, 3);
                    outptr4[0] = vgetq_lane_s32(_tmp01, 0);
                    outptr5[0] = vgetq_lane_s32(_tmp01, 1);
                    outptr6[0] = vgetq_lane_s32(_tmp01, 2);
                    outptr7[0] = vgetq_lane_s32(_tmp01, 3);

                    if (tj * 2 + 1 < outw)
                    {
                        outptr0[1] = vgetq_lane_s32(_tmp10, 0);
                        outptr1[1] = vgetq_lane_s32(_tmp10, 1);
                        outptr2[1] = vgetq_lane_s32(_tmp10, 2);
                        outptr3[1] = vgetq_lane_s32(_tmp10, 3);
                        outptr4[1] = vgetq_lane_s32(_tmp11, 0);
                        outptr5[1] = vgetq_lane_s32(_tmp11, 1);
                        outptr6[1] = vgetq_lane_s32(_tmp11, 2);
                        outptr7[1] = vgetq_lane_s32(_tmp11, 3);
                    }
                }

                outptr0 += outw * out_elempack;
            }
        }
    }
    for (; ii + 3 < max_ii; ii += 4)
    {
        int tmp[2][4][4];

        int jj = 0;
        for (; jj < max_jj; jj++)
        {
            int ti = (j + jj) / w_tiles;
            int tj = (j + jj) % w_tiles;

            const int* r0 = (const int*)top_tile + ii * max_jj * 16 + jj * 4;
            const int* r1 = r0 + max_jj * 4;
            const int* r2 = r0 + max_jj * 4 * 2;
            const int* r3 = r0 + max_jj * 4 * 3;

            for (int m = 0; m < 4; m++)
            {
                int32x4_t _r0 = vld1q_s32(r0);
                int32x4_t _r1 = vld1q_s32(r1);
                int32x4_t _r2 = vld1q_s32(r2);
                int32x4_t _r3 = vld1q_s32(r3);

                int32x4_t _tmp0 = vaddq_s32(vaddq_s32(_r0, _r1), _r2);
                int32x4_t _tmp1 = vaddq_s32(vsubq_s32(_r1, _r2), _r3);

                vst1q_s32(tmp[0][m], _tmp0);
                vst1q_s32(tmp[1][m], _tmp1);

                r0 += max_jj * 4 * 4;
                r1 += max_jj * 4 * 4;
                r2 += max_jj * 4 * 4;
                r3 += max_jj * 4 * 4;
            }

            int* outptr0 = top_blob.channel((i + ii) / out_elempack).row<int>(ti * 2) + (tj * 2) * out_elempack;

            for (int m = 0; m < 2; m++)
            {
                if (ti * 2 + m >= outh)
                    continue;

                int32x4_t _r0 = vld1q_s32(tmp[m][0]);
                int32x4_t _r1 = vld1q_s32(tmp[m][1]);
                int32x4_t _r2 = vld1q_s32(tmp[m][2]);
                int32x4_t _r3 = vld1q_s32(tmp[m][3]);

                int32x4_t _tmp0 = vaddq_s32(vaddq_s32(_r0, _r1), _r2);
                int32x4_t _tmp1 = vaddq_s32(vsubq_s32(_r1, _r2), _r3);

                _tmp0 = vshrq_n_s32(_tmp0, 2);
                _tmp1 = vshrq_n_s32(_tmp1, 2);

                if (out_elempack == 4)
                {
                    vst1q_s32(outptr0, _tmp0);
                    if (tj * 2 + 1 < outw) vst1q_s32(outptr0 + 4, _tmp1);
                }
                if (out_elempack == 1)
                {
                    int* outptr1 = outptr0 + N;
                    int* outptr2 = outptr0 + N * 2;
                    int* outptr3 = outptr0 + N * 3;

                    outptr0[0] = vgetq_lane_s32(_tmp0, 0);
                    outptr1[0] = vgetq_lane_s32(_tmp0, 1);
                    outptr2[0] = vgetq_lane_s32(_tmp0, 2);
                    outptr3[0] = vgetq_lane_s32(_tmp0, 3);

                    if (tj * 2 + 1 < outw)
                    {
                        outptr0[1] = vgetq_lane_s32(_tmp1, 0);
                        outptr1[1] = vgetq_lane_s32(_tmp1, 1);
                        outptr2[1] = vgetq_lane_s32(_tmp1, 2);
                        outptr3[1] = vgetq_lane_s32(_tmp1, 3);
                    }
                }

                outptr0 += outw * out_elempack;
            }
        }
    }
#endif // __ARM_NEON
    for (; ii + 1 < max_ii; ii += 2)
    {
        int tmp[2][4][2];

        int jj = 0;
        for (; jj < max_jj; jj++)
        {
            int ti = (j + jj) / w_tiles;
            int tj = (j + jj) % w_tiles;

            const int* r0 = (const int*)top_tile + ii * max_jj * 16 + jj * 2;
            const int* r1 = r0 + max_jj * 2;
            const int* r2 = r0 + max_jj * 2 * 2;
            const int* r3 = r0 + max_jj * 2 * 3;

            for (int m = 0; m < 4; m++)
            {
                tmp[0][m][0] = r0[0] + r1[0] + r2[0];
                tmp[0][m][1] = r0[1] + r1[1] + r2[1];
                tmp[1][m][0] = r1[0] - r2[0] + r3[0];
                tmp[1][m][1] = r1[1] - r2[1] + r3[1];

                r0 += max_jj * 4 * 2;
                r1 += max_jj * 4 * 2;
                r2 += max_jj * 4 * 2;
                r3 += max_jj * 4 * 2;
            }

            int* outptr0 = top_blob.channel(i + ii).row<int>(ti * 2) + (tj * 2);

            for (int m = 0; m < 2; m++)
            {
                if (ti * 2 + m >= outh)
                    continue;

                int tmp00 = tmp[m][0][0] + tmp[m][1][0] + tmp[m][2][0];
                int tmp01 = tmp[m][0][1] + tmp[m][1][1] + tmp[m][2][1];
                int tmp10 = tmp[m][1][0] - tmp[m][2][0] + tmp[m][3][0];
                int tmp11 = tmp[m][1][1] - tmp[m][2][1] + tmp[m][3][1];

                tmp00 = tmp00 >> 2;
                tmp01 = tmp01 >> 2;
                tmp10 = tmp10 >> 2;
                tmp11 = tmp11 >> 2;

                // if (out_elempack == 1)
                {
                    int* outptr1 = outptr0 + N;

                    outptr0[0] = tmp00;
                    outptr1[0] = tmp01;
                    if (tj * 2 + 1 < outw)
                    {
                        outptr0[1] = tmp10;
                        outptr1[1] = tmp11;
                    }
                }

                outptr0 += outw;
            }
        }
    }
    for (; ii < max_ii; ii++)
    {
        int tmp[2][4];

        int jj = 0;
        for (; jj < max_jj; jj++)
        {
            int ti = (j + jj) / w_tiles;
            int tj = (j + jj) % w_tiles;

            const int* r0 = (const int*)top_tile + ii * max_jj * 16 + jj;
            const int* r1 = r0 + max_jj;
            const int* r2 = r0 + max_jj * 2;
            const int* r3 = r0 + max_jj * 3;

            for (int m = 0; m < 4; m++)
            {
                tmp[0][m] = r0[0] + r1[0] + r2[0];
                tmp[1][m] = r1[0] - r2[0] + r3[0];

                r0 += max_jj * 4;
                r1 += max_jj * 4;
                r2 += max_jj * 4;
                r3 += max_jj * 4;
            }

            int* outptr0 = top_blob.channel(i + ii).row<int>(ti * 2) + (tj * 2);

            for (int m = 0; m < 2; m++)
            {
                if (ti * 2 + m >= outh)
                    continue;

                int tmp0 = tmp[m][0] + tmp[m][1] + tmp[m][2];
                int tmp1 = tmp[m][1] - tmp[m][2] + tmp[m][3];

                tmp0 = tmp0 >> 2;
                tmp1 = tmp1 >> 2;

                // if (out_elempack == 1)
                {
                    outptr0[0] = tmp0;
                    if (tj * 2 + 1 < outw) outptr0[1] = tmp1;
                }

                outptr0 += outw;
            }
        }
    }
}

static void conv3x3s1_winograd23_int8(const Mat& bottom_blob, Mat& top_blob, const Mat& AT, int nT, const Option& opt)
{
    int outw = top_blob.w;
    int outh = top_blob.h;

    // pad to 2n+2, winograd F(2,3)
    int w_tiles = (outw + 1) / 2;
    int h_tiles = (outh + 1) / 2;
    int tiles = w_tiles * h_tiles;

    const int M = top_blob.c * top_blob.elempack;
    const int N = tiles;
    const int K = bottom_blob.c * bottom_blob.elempack;
    const int B = 16;

    // NCNN_LOGE("conv3x3s1_winograd23_int8 %d %d %d", M, N, K);

    int TILE_M, TILE_N, TILE_K;
    get_optimal_tile_mnk_int8(M, N, K, TILE_M, TILE_N, TILE_K, nT);

    const int nn_M = (M + TILE_M - 1) / TILE_M;
    const int nn_N = (N + TILE_N - 1) / TILE_N;
    const int nn_K = (K + TILE_K - 1) / TILE_K;

    // NCNN_LOGE("TILE M/N/K = %d %d %d -> %d %d %d", M, N, K, TILE_M, TILE_N, TILE_K);

    Mat BT(TILE_K * TILE_N, B, (K + TILE_K - 1) / TILE_K, (N + TILE_N - 1) / TILE_N, 2u, opt.workspace_allocator);

    const int nn_NK = nn_N * nn_K;

    if (nT > 1 && nn_NK < nT)
    {
        Mat B_tile(TILE_N * B * TILE_K, 2u, opt.workspace_allocator);

        for (int ppjk = 0; ppjk < nn_NK; ppjk++)
        {
            const int ppj = ppjk / nn_K;
            const int ppk = ppjk % nn_K;

            const int j = ppj * TILE_N;
            const int k = ppk * TILE_K;

            const int max_jj = std::min((N - j), TILE_N);
            const int max_kk = std::min((K - k), TILE_K);

            // transform input
            conv3x3s1_winograd23_transform_input_tile_int8(bottom_blob, B_tile, j, max_jj, k, max_kk, nT);

            Mat BT_tile = BT.channel(j / TILE_N).depth(k / TILE_K);

            transpose_pack_B_tile_int8(B_tile, BT_tile, B, max_jj, max_kk, nT);
        }
    }
    else
    {
        Mat B_tileX(TILE_N * B * TILE_K, 1, nT, 2u, opt.workspace_allocator);

        // #pragma omp parallel for num_threads(nT)
        for (int ppjk = 0; ppjk < nn_NK; ppjk++)
        {
            const int ppj = ppjk / nn_K;
            const int ppk = ppjk % nn_K;

            const int j = ppj * TILE_N;
            const int k = ppk * TILE_K;

            const int max_jj = std::min((N - j), TILE_N);
            const int max_kk = std::min((K - k), TILE_K);

            Mat B_tile = B_tileX.channel(get_omp_thread_num());

            // transform input
            conv3x3s1_winograd23_transform_input_tile_int8(bottom_blob, B_tile, j, max_jj, k, max_kk, 1);

            Mat BT_tile = BT.channel(j / TILE_N).depth(k / TILE_K);

            transpose_pack_B_tile_int8(B_tile, BT_tile, B, max_jj, max_kk, 1);
        }
    }

    Mat top_tileX(TILE_N * B * TILE_M, 1, nT, 4u, opt.workspace_allocator);

    #pragma omp parallel for num_threads(nT)
    for (int ppj = 0; ppj < nn_M; ppj++)
    {
        const int i = ppj * TILE_M;

        Mat top_tile = top_tileX.channel(get_omp_thread_num());

        const int max_ii = std::min((M - i), TILE_M);

        for (int j = 0; j < N; j += TILE_N)
        {
            const int max_jj = std::min((N - j), TILE_N);

            for (int k = 0; k < K; k += TILE_K)
            {
                const int max_kk = std::min((K - k), TILE_K);

                const Mat AT_tile = AT.channel(i / TILE_M).depth(k / TILE_K);

                const Mat BT_tile = BT.channel(j / TILE_N).depth(k / TILE_K);

                gemm_transB_packed_tile_int8(AT_tile, BT_tile, top_tile, B, max_ii, max_jj, k, max_kk);
            }

            // transform output
            conv3x3s1_winograd23_transform_output_tile_int8(top_tile, top_blob, i, max_ii, j, max_jj);
        }
    }
}

static inline void conv3x3s1_winograd43_transform_kernel_tile_int8(const Mat& kernel, Mat& A, int inch, int i, int max_ii, int k, int max_kk)
{
    // const short ktm[6][3] = {
    //     {6, 0, 0},
    //     {-4, -4, -4},
    //     {-4, 4, -4},
    //     {1, 2, 4},
    //     {1, -2, 4},
    //     {0, 0, 6}
    // };

    short* ptmp = A;

    int ii = 0;
    for (; ii < max_ii; ii++)
    {
        int kk = 0;
        for (; kk < max_kk; kk++)
        {
            short tmp[6][3];

            const signed char* k0 = (const signed char*)kernel + (i + ii) * inch * 9 + (k + kk) * 9;

            for (int m = 0; m < 3; m++)
            {
                signed char r0 = k0[0];
                signed char r1 = k0[1];
                signed char r2 = k0[2];

                tmp[0][m] = r0 * 6;
                tmp[1][m] = -r0 * 4 - r1 * 4 - r2 * 4;
                tmp[2][m] = -r0 * 4 + r1 * 4 - r2 * 4;
                tmp[3][m] = r0 + r1 * 2 + r2 * 4;
                tmp[4][m] = r0 - r1 * 2 + r2 * 4;
                tmp[5][m] = r2 * 6;

                k0 += 3;
            }

            for (int m = 0; m < 6; m++)
            {
                short r0 = tmp[m][0];
                short r1 = tmp[m][1];
                short r2 = tmp[m][2];

                short z0 = r0 * 6;
                short z1 = -r0 * 4 - r1 * 4 - r2 * 4;
                short z2 = -r0 * 4 + r1 * 4 - r2 * 4;
                short z3 = r0 + r1 * 2 + r2 * 4;
                short z4 = r0 - r1 * 2 + r2 * 4;
                short z5 = r2 * 6;

                ptmp[0] = z0;
                ptmp[1] = z1;
                ptmp[2] = z2;
                ptmp[3] = z3;
                ptmp[4] = z4;
                ptmp[5] = z5;
                ptmp += 6;
            }
        }
    }
}

static void conv3x3s1_winograd43_transform_kernel_int8(const Mat& kernel, Mat& AT, int inch, int outch, const Option& opt)
{
    const int M = outch;
    const int K = inch;
    const int B = 36;

    int TILE_M, TILE_N, TILE_K;
    get_optimal_tile_mnk_int8(M, 0, K, TILE_M, TILE_N, TILE_K, opt.num_threads);

    const int nn_M = (M + TILE_M - 1) / TILE_M;

    Mat A_tileX(B * TILE_M * TILE_K, 1, opt.num_threads, 4u, (Allocator*)0);

    AT.create(TILE_K * TILE_M, B, (K + TILE_K - 1) / TILE_K, (M + TILE_M - 1) / TILE_M, 4u, (Allocator*)0);

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int ppj = 0; ppj < nn_M; ppj++)
    {
        const int i = ppj * TILE_M;

        Mat A_tile = A_tileX.channel(get_omp_thread_num());

        for (int k = 0; k < K; k += TILE_K)
        {
            const int max_ii = std::min((M - i), TILE_M);
            const int max_kk = std::min((K - k), TILE_K);

            conv3x3s1_winograd43_transform_kernel_tile_int8(kernel, A_tile, inch, i, max_ii, k, max_kk);

            Mat AT_tile = AT.channel(i / TILE_M).depth(k / TILE_K);

            pack_A_tile_int8(A_tile, AT_tile, B, max_ii, max_kk);
        }
    }
}

static inline void conv3x3s1_winograd43_transform_input_tile_int8(const Mat& bottom_blob, Mat& B, int j, int max_jj, int k, int max_kk, int nT)
{
    // const float itm[4][4] = {
    //     {4,  0, -5,  0, 1, 0},
    //     {0, -4, -4,  1, 1, 0},
    //     {0,  4, -4, -1, 1, 0},
    //     {0, -2, -1,  2, 1, 0},
    //     {0,  2, -1, -2, 1, 0},
    //     {0,  4,  0, -5, 0, 1}
    // };

    const int w = bottom_blob.w;
    const int h = bottom_blob.h;
    const int elempack = bottom_blob.elempack;
    const int N = bottom_blob.cstep * elempack;

    const int w_tiles = (w + 1) / 4;

    int nn_max_kk = 0;
    int remain_max_kk_start = 0;
#if __ARM_NEON
    nn_max_kk = max_kk / 8;
    #pragma omp parallel for num_threads(nT)
    for (int ppkk = 0; ppkk < nn_max_kk; ppkk++)
    {
        const int kk = remain_max_kk_start + ppkk * 8;

        short tmp[6][6][8];

        int jj = 0;
        for (; jj < max_jj; jj++)
        {
            int ti = (j + jj) / w_tiles;
            int tj = (j + jj) % w_tiles;

            const signed char* r0 = bottom_blob.channel((k + kk) / elempack).row<const signed char>(ti * 4) + (tj * 4) * elempack;

            int8x8_t _v5 = vdup_n_s8(5);

            for (int m = 0; m < 6; m++)
            {
                int8x8_t _r0 = vdup_n_s8(0);
                int8x8_t _r1 = vdup_n_s8(0);
                int8x8_t _r2 = vdup_n_s8(0);
                int8x8_t _r3 = vdup_n_s8(0);
                int8x8_t _r4 = vdup_n_s8(0);
                int8x8_t _r5 = vdup_n_s8(0);

                if (ti * 4 + m < h)
                {
                    if (elempack == 8)
                    {
                        _r0 = vld1_s8(r0);
                        if (tj * 4 + 1 < w) _r1 = vld1_s8(r0 + 8);
                        if (tj * 4 + 2 < w) _r2 = vld1_s8(r0 + 16);
                        if (tj * 4 + 3 < w) _r3 = vld1_s8(r0 + 24);
                        if (tj * 4 + 4 < w) _r4 = vld1_s8(r0 + 32);
                        if (tj * 4 + 5 < w) _r5 = vld1_s8(r0 + 40);
                    }
                    if (elempack == 1)
                    {
                        const signed char* r1 = r0 + N;
                        const signed char* r2 = r0 + N * 2;
                        const signed char* r3 = r0 + N * 3;
                        const signed char* r4 = r0 + N * 4;
                        const signed char* r5 = r0 + N * 5;
                        const signed char* r6 = r0 + N * 6;
                        const signed char* r7 = r0 + N * 7;

                        int8x8_t _t0 = vld1_s8(r0);
                        int8x8_t _t1 = vld1_s8(r1);
                        int8x8_t _t2 = vld1_s8(r2);
                        int8x8_t _t3 = vld1_s8(r3);
                        int8x8_t _t4 = vld1_s8(r4);
                        int8x8_t _t5 = vld1_s8(r5);
                        int8x8_t _t6 = vld1_s8(r6);
                        int8x8_t _t7 = vld1_s8(r7);

                        int8x8_t _t01 = vzip_s8(_t0, _t1).val[0];
                        int8x8_t _t23 = vzip_s8(_t2, _t3).val[0];
                        int8x8_t _t45 = vzip_s8(_t4, _t5).val[0];
                        int8x8_t _t67 = vzip_s8(_t6, _t7).val[0];
                        int16x4x2_t _t0123 = vzip_s16(vreinterpret_s16_s8(_t01), vreinterpret_s16_s8(_t23));
                        int16x4x2_t _t4567 = vzip_s16(vreinterpret_s16_s8(_t45), vreinterpret_s16_s8(_t67));
                        int16x8_t _ta = vcombine_s16(_t0123.val[0], _t0123.val[1]);
                        int16x8_t _tb = vcombine_s16(_t4567.val[0], _t4567.val[1]);
                        int32x4x2_t _tab = vzipq_s32(vreinterpretq_s32_s16(_ta), vreinterpretq_s32_s16(_tb));

                        _r0 = vreinterpret_s8_s32(vget_low_s32(_tab.val[0]));
                        if (tj * 4 + 1 < w) _r1 = vreinterpret_s8_s32(vget_high_s32(_tab.val[0]));
                        if (tj * 4 + 2 < w) _r2 = vreinterpret_s8_s32(vget_low_s32(_tab.val[1]));
                        if (tj * 4 + 3 < w) _r3 = vreinterpret_s8_s32(vget_high_s32(_tab.val[1]));
                        if (tj * 4 + 4 < w)
                        {
                            _t01 = vzip_s8(_t0, _t1).val[1];
                            _t23 = vzip_s8(_t2, _t3).val[1];
                            _t45 = vzip_s8(_t4, _t5).val[1];
                            _t67 = vzip_s8(_t6, _t7).val[1];
                            int16x4_t _tc = vzip_s16(vreinterpret_s16_s8(_t01), vreinterpret_s16_s8(_t23)).val[0];
                            int16x4_t _td = vzip_s16(vreinterpret_s16_s8(_t45), vreinterpret_s16_s8(_t67)).val[0];
                            int32x2x2_t _tcd = vzip_s32(vreinterpret_s32_s16(_tc), vreinterpret_s32_s16(_td));

                            _r4 = vreinterpret_s8_s32(_tcd.val[0]);
                            if (tj * 4 + 5 < w) _r5 = vreinterpret_s8_s32(_tcd.val[1]);
                        }
                    }
                }

                int16x8_t _tmp12a = vsubw_s8(vshll_n_s8(_r1, 2), _r3);
                int16x8_t _tmp12b = vsubw_s8(vshll_n_s8(_r2, 2), _r4);
                int16x8_t _tmp34a = vshlq_n_s16(vsubl_s8(_r3, _r1), 1);
                int16x8_t _tmp34b = vsubl_s8(_r4, _r2);

                int16x8_t _tmp0 = vaddq_s16(vmovl_s8(_r4), vsubq_s16(vshll_n_s8(_r0, 2), vmull_s8(_r2, _v5)));
                int16x8_t _tmp1 = vnegq_s16(vaddq_s16(_tmp12a, _tmp12b));
                int16x8_t _tmp2 = vsubq_s16(_tmp12a, _tmp12b);
                int16x8_t _tmp3 = vaddq_s16(_tmp34b, _tmp34a);
                int16x8_t _tmp4 = vsubq_s16(_tmp34b, _tmp34a);
                int16x8_t _tmp5 = vaddq_s16(vmovl_s8(_r5), vsubq_s16(vshll_n_s8(_r1, 2), vmull_s8(_r3, _v5)));

                vst1q_s16(tmp[0][m], _tmp0);
                vst1q_s16(tmp[1][m], _tmp1);
                vst1q_s16(tmp[2][m], _tmp2);
                vst1q_s16(tmp[3][m], _tmp3);
                vst1q_s16(tmp[4][m], _tmp4);
                vst1q_s16(tmp[5][m], _tmp5);

                r0 += w * elempack;
            }

            int16x8_t _v5q = vdupq_n_s16(5);

            short* p0 = (short*)B + kk * max_jj * 36 + jj * 8;
            short* p1 = p0 + max_jj * 8;
            short* p2 = p0 + max_jj * 8 * 2;
            short* p3 = p0 + max_jj * 8 * 3;
            short* p4 = p0 + max_jj * 8 * 4;
            short* p5 = p0 + max_jj * 8 * 5;

            for (int m = 0; m < 6; m++)
            {
                int16x8_t _r0 = vld1q_s16(tmp[m][0]);
                int16x8_t _r1 = vld1q_s16(tmp[m][1]);
                int16x8_t _r2 = vld1q_s16(tmp[m][2]);
                int16x8_t _r3 = vld1q_s16(tmp[m][3]);
                int16x8_t _r4 = vld1q_s16(tmp[m][4]);
                int16x8_t _r5 = vld1q_s16(tmp[m][5]);

                int16x8_t _tmp12a = vsubq_s16(_r3, vshlq_n_s16(_r1, 2));
                int16x8_t _tmp12b = vsubq_s16(_r4, vshlq_n_s16(_r2, 2));
                int16x8_t _tmp34a = vshlq_n_s16(vsubq_s16(_r3, _r1), 1);
                int16x8_t _tmp34b = vsubq_s16(_r4, _r2);

                int16x8_t _tmp0 = vaddq_s16(_r4, vsubq_s16(vshlq_n_s16(_r0, 2), vmulq_s16(_r2, _v5q)));
                int16x8_t _tmp1 = vaddq_s16(_tmp12b, _tmp12a);
                int16x8_t _tmp2 = vsubq_s16(_tmp12b, _tmp12a);
                int16x8_t _tmp3 = vaddq_s16(_tmp34b, _tmp34a);
                int16x8_t _tmp4 = vsubq_s16(_tmp34b, _tmp34a);
                int16x8_t _tmp5 = vaddq_s16(_r5, vsubq_s16(vshlq_n_s16(_r1, 2), vmulq_s16(_r3, _v5q)));

                vst1q_s16(p0, _tmp0);
                vst1q_s16(p1, _tmp1);
                vst1q_s16(p2, _tmp2);
                vst1q_s16(p3, _tmp3);
                vst1q_s16(p4, _tmp4);
                vst1q_s16(p5, _tmp5);

                p0 += max_jj * 6 * 8;
                p1 += max_jj * 6 * 8;
                p2 += max_jj * 6 * 8;
                p3 += max_jj * 6 * 8;
                p4 += max_jj * 6 * 8;
                p5 += max_jj * 6 * 8;
            }
        }
    }
    remain_max_kk_start += nn_max_kk * 8;
    nn_max_kk = (max_kk - remain_max_kk_start) / 2;
#else // __ARM_NEON
    nn_max_kk = (max_kk - remain_max_kk_start) / 2;
    #pragma omp parallel for num_threads(nT)
#endif // __ARM_NEON
    for (int ppkk = 0; ppkk < nn_max_kk; ppkk++)
    {
        const int kk = remain_max_kk_start + ppkk * 2;

        short tmp[6][6][2];

        int jj = 0;
        for (; jj < max_jj; jj++)
        {
            int ti = (j + jj) / w_tiles;
            int tj = (j + jj) % w_tiles;

            const signed char* r0 = bottom_blob.channel(k + kk).row<const signed char>(ti * 4) + (tj * 4);

            for (int m = 0; m < 6; m++)
            {
                signed char r00 = 0;
                signed char r01 = 0;
                signed char r10 = 0;
                signed char r11 = 0;
                signed char r20 = 0;
                signed char r21 = 0;
                signed char r30 = 0;
                signed char r31 = 0;
                signed char r40 = 0;
                signed char r41 = 0;
                signed char r50 = 0;
                signed char r51 = 0;

                if (ti * 4 + m < h)
                {
                    // if (elempack == 1)
                    {
                        const signed char* r1 = r0 + N;

                        r00 = r0[0];
                        r01 = r1[0];
                        if (tj * 4 + 1 < w)
                        {
                            r10 = r0[1];
                            r11 = r1[1];
                        }
                        if (tj * 4 + 2 < w)
                        {
                            r20 = r0[2];
                            r21 = r1[2];
                        }
                        if (tj * 4 + 3 < w)
                        {
                            r30 = r0[3];
                            r31 = r1[3];
                        }
                        if (tj * 4 + 4 < w)
                        {
                            r40 = r0[4];
                            r41 = r1[4];
                        }
                        if (tj * 4 + 5 < w)
                        {
                            r50 = r0[5];
                            r51 = r1[5];
                        }
                    }
                }

                short tmp120a = r30 - r10 * 4;
                short tmp121a = r31 - r11 * 4;
                short tmp120b = r40 - r20 * 4;
                short tmp121b = r41 - r21 * 4;
                short tmp340a = (r30 - r10) * 2;
                short tmp341a = (r31 - r11) * 2;
                short tmp340b = r40 - r20;
                short tmp341b = r41 - r21;

                tmp[0][m][0] = r40 + r00 * 4 - r20 * 5;
                tmp[0][m][1] = r41 + r01 * 4 - r21 * 5;
                tmp[1][m][0] = tmp120b + tmp120a;
                tmp[1][m][1] = tmp121b + tmp121a;
                tmp[2][m][0] = tmp120b - tmp120a;
                tmp[2][m][1] = tmp121b - tmp121a;
                tmp[3][m][0] = tmp340b + tmp340a;
                tmp[3][m][1] = tmp341b + tmp341a;
                tmp[4][m][0] = tmp340b - tmp340a;
                tmp[4][m][1] = tmp341b - tmp341a;
                tmp[5][m][0] = r50 + r10 * 4 - r30 * 5;
                tmp[5][m][1] = r51 + r11 * 4 - r31 * 5;

                r0 += w;
            }

            short* p0 = (short*)B + kk * max_jj * 36 + jj * 2;
            short* p1 = p0 + max_jj * 2;
            short* p2 = p0 + max_jj * 2 * 2;
            short* p3 = p0 + max_jj * 2 * 3;
            short* p4 = p0 + max_jj * 2 * 4;
            short* p5 = p0 + max_jj * 2 * 5;

            for (int m = 0; m < 6; m++)
            {
                short r00 = tmp[m][0][0];
                short r01 = tmp[m][0][1];
                short r10 = tmp[m][1][0];
                short r11 = tmp[m][1][1];
                short r20 = tmp[m][2][0];
                short r21 = tmp[m][2][1];
                short r30 = tmp[m][3][0];
                short r31 = tmp[m][3][1];
                short r40 = tmp[m][4][0];
                short r41 = tmp[m][4][1];
                short r50 = tmp[m][5][0];
                short r51 = tmp[m][5][1];

                short tmp120a = r30 - r10 * 4;
                short tmp121a = r31 - r11 * 4;
                short tmp120b = r40 - r20 * 4;
                short tmp121b = r41 - r21 * 4;
                short tmp340a = (r30 - r10) * 2;
                short tmp341a = (r31 - r11) * 2;
                short tmp340b = r40 - r20;
                short tmp341b = r41 - r21;

                p0[0] = r40 + r00 * 4 - r20 * 5;
                p0[1] = r41 + r01 * 4 - r21 * 5;
                p1[0] = tmp120b + tmp120a;
                p1[1] = tmp121b + tmp121a;
                p2[0] = tmp120b - tmp120a;
                p2[1] = tmp121b - tmp121a;
                p3[0] = tmp340b + tmp340a;
                p3[1] = tmp341b + tmp341a;
                p4[0] = tmp340b - tmp340a;
                p4[1] = tmp341b - tmp341a;
                p5[0] = r50 + r10 * 4 - r30 * 5;
                p5[1] = r51 + r11 * 4 - r31 * 5;

                p0 += max_jj * 6 * 2;
                p1 += max_jj * 6 * 2;
                p2 += max_jj * 6 * 2;
                p3 += max_jj * 6 * 2;
                p4 += max_jj * 6 * 2;
                p5 += max_jj * 6 * 2;
            }
        }
    }
    remain_max_kk_start += nn_max_kk * 2;
    for (int kk = remain_max_kk_start; kk < max_kk; kk++)
    {
        short tmp[6][6];

        int jj = 0;
        for (; jj < max_jj; jj++)
        {
            int ti = (j + jj) / w_tiles;
            int tj = (j + jj) % w_tiles;

            const signed char* r0123 = bottom_blob.channel(k + kk).row<const signed char>(ti * 4) + (tj * 4);

            for (int m = 0; m < 6; m++)
            {
                signed char r0 = 0;
                signed char r1 = 0;
                signed char r2 = 0;
                signed char r3 = 0;
                signed char r4 = 0;
                signed char r5 = 0;

                if (ti * 4 + m < h)
                {
                    // if (elempack == 1)
                    {
                        r0 = r0123[0];
                        if (tj * 4 + 1 < w) r1 = r0123[1];
                        if (tj * 4 + 2 < w) r2 = r0123[2];
                        if (tj * 4 + 3 < w) r3 = r0123[3];
                        if (tj * 4 + 4 < w) r4 = r0123[4];
                        if (tj * 4 + 5 < w) r5 = r0123[5];
                    }
                }

                short tmp12a = r3 - r1 * 4;
                short tmp12b = r4 - r2 * 4;
                short tmp34a = (r3 - r1) * 2;
                short tmp34b = r4 - r2;

                tmp[0][m] = r4 + r0 * 4 - r2 * 5;
                tmp[1][m] = tmp12b + tmp12a;
                tmp[2][m] = tmp12b - tmp12a;
                tmp[3][m] = tmp34b + tmp34a;
                tmp[4][m] = tmp34b - tmp34a;
                tmp[5][m] = r5 + r1 * 4 - r3 * 5;

                r0123 += w;
            }

            short* p0 = (short*)B + kk * max_jj * 36 + jj;
            short* p1 = p0 + max_jj;
            short* p2 = p0 + max_jj * 2;
            short* p3 = p0 + max_jj * 3;
            short* p4 = p0 + max_jj * 4;
            short* p5 = p0 + max_jj * 5;

            for (int m = 0; m < 6; m++)
            {
                short r0 = tmp[m][0];
                short r1 = tmp[m][1];
                short r2 = tmp[m][2];
                short r3 = tmp[m][3];
                short r4 = tmp[m][4];
                short r5 = tmp[m][5];

                short tmp12a = r3 - r1 * 4;
                short tmp12b = r4 - r2 * 4;
                short tmp34a = (r3 - r1) * 2;
                short tmp34b = r4 - r2;

                p0[0] = r4 + r0 * 4 - r2 * 5;
                p1[0] = tmp12b + tmp12a;
                p2[0] = tmp12b - tmp12a;
                p3[0] = tmp34b + tmp34a;
                p4[0] = tmp34b - tmp34a;
                p5[0] = r5 + r1 * 4 - r3 * 5;

                p0 += max_jj * 6;
                p1 += max_jj * 6;
                p2 += max_jj * 6;
                p3 += max_jj * 6;
                p4 += max_jj * 6;
                p5 += max_jj * 6;
            }
        }
    }
}

static inline void conv3x3s1_winograd43_transform_output_tile_int8(const Mat& top_tile, Mat& top_blob, int i, int max_ii, int j, int max_jj)
{
    // const int otm[4][6] = {
    //     {1, 1,  1, 1,  1, 0},
    //     {0, 1, -1, 2, -2, 0},
    //     {0, 1,  1, 4,  4, 0},
    //     {0, 1, -1, 8, -8, 1}
    // };

    const int outw = top_blob.w;
    const int outh = top_blob.h;
    const int out_elempack = top_blob.elempack;
    const int N = top_blob.cstep * out_elempack;

    const int w_tiles = (outw + 3) / 4;

    int ii = 0;
#if __ARM_NEON
    for (; ii + 7 < max_ii; ii += 8)
    {
        int tmp[4][6][8];

        int jj = 0;
        for (; jj < max_jj; jj++)
        {
            int ti = (j + jj) / w_tiles;
            int tj = (j + jj) % w_tiles;

            const int* r0 = (const int*)top_tile + ii * max_jj * 36 + jj * 8;
            const int* r1 = r0 + max_jj * 8;
            const int* r2 = r0 + max_jj * 8 * 2;
            const int* r3 = r0 + max_jj * 8 * 3;
            const int* r4 = r0 + max_jj * 8 * 4;
            const int* r5 = r0 + max_jj * 8 * 5;

            for (int m = 0; m < 5; m++)
            {
                int32x4_t _r00 = vld1q_s32(r0);
                int32x4_t _r01 = vld1q_s32(r0 + 4);
                int32x4_t _r10 = vld1q_s32(r1);
                int32x4_t _r11 = vld1q_s32(r1 + 4);
                int32x4_t _r20 = vld1q_s32(r2);
                int32x4_t _r21 = vld1q_s32(r2 + 4);
                int32x4_t _r30 = vld1q_s32(r3);
                int32x4_t _r31 = vld1q_s32(r3 + 4);
                int32x4_t _r40 = vld1q_s32(r4);
                int32x4_t _r41 = vld1q_s32(r4 + 4);
                int32x4_t _r50 = vld1q_s32(r5);
                int32x4_t _r51 = vld1q_s32(r5 + 4);

                int32x4_t _tmp02a0 = vaddq_s32(_r10, _r20);
                int32x4_t _tmp02a1 = vaddq_s32(_r11, _r21);
                int32x4_t _tmp02b0 = vaddq_s32(_r30, _r40);
                int32x4_t _tmp02b1 = vaddq_s32(_r31, _r41);
                int32x4_t _tmp13a0 = vsubq_s32(_r10, _r20);
                int32x4_t _tmp13a1 = vsubq_s32(_r11, _r21);
                int32x4_t _tmp13b0 = vsubq_s32(_r30, _r40);
                int32x4_t _tmp13b1 = vsubq_s32(_r31, _r41);

                int32x4_t _tmp00 = vaddq_s32(vaddq_s32(_tmp02a0, _tmp02b0), _r00);
                int32x4_t _tmp01 = vaddq_s32(vaddq_s32(_tmp02a1, _tmp02b1), _r01);
                int32x4_t _tmp10 = vaddq_s32(_tmp13a0, vshlq_n_s32(_tmp13b0, 1));
                int32x4_t _tmp11 = vaddq_s32(_tmp13a1, vshlq_n_s32(_tmp13b1, 1));
                int32x4_t _tmp20 = vaddq_s32(_tmp02a0, vshlq_n_s32(_tmp02b0, 2));
                int32x4_t _tmp21 = vaddq_s32(_tmp02a1, vshlq_n_s32(_tmp02b1, 2));
                int32x4_t _tmp30 = vaddq_s32(vaddq_s32(_tmp13a0, vshlq_n_s32(_tmp13b0, 3)), vshlq_n_s32(_r50, 2));
                int32x4_t _tmp31 = vaddq_s32(vaddq_s32(_tmp13a1, vshlq_n_s32(_tmp13b1, 3)), vshlq_n_s32(_r51, 2));

                vst1q_s32(tmp[0][m], _tmp00);
                vst1q_s32(tmp[0][m] + 4, _tmp01);
                vst1q_s32(tmp[1][m], _tmp10);
                vst1q_s32(tmp[1][m] + 4, _tmp11);
                vst1q_s32(tmp[2][m], _tmp20);
                vst1q_s32(tmp[2][m] + 4, _tmp21);
                vst1q_s32(tmp[3][m], _tmp30);
                vst1q_s32(tmp[3][m] + 4, _tmp31);

                r0 += max_jj * 6 * 8;
                r1 += max_jj * 6 * 8;
                r2 += max_jj * 6 * 8;
                r3 += max_jj * 6 * 8;
                r4 += max_jj * 6 * 8;
                r5 += max_jj * 6 * 8;
            }
            for (int m = 5; m < 6; m++)
            {
                int32x4_t _r00 = vld1q_s32(r0);
                int32x4_t _r01 = vld1q_s32(r0 + 4);
                int32x4_t _r10 = vld1q_s32(r1);
                int32x4_t _r11 = vld1q_s32(r1 + 4);
                int32x4_t _r20 = vld1q_s32(r2);
                int32x4_t _r21 = vld1q_s32(r2 + 4);
                int32x4_t _r30 = vld1q_s32(r3);
                int32x4_t _r31 = vld1q_s32(r3 + 4);
                int32x4_t _r40 = vld1q_s32(r4);
                int32x4_t _r41 = vld1q_s32(r4 + 4);
                int32x4_t _r50 = vld1q_s32(r5);
                int32x4_t _r51 = vld1q_s32(r5 + 4);

                int32x4_t _tmp02a0 = vaddq_s32(_r10, _r20);
                int32x4_t _tmp02a1 = vaddq_s32(_r11, _r21);
                int32x4_t _tmp02b0 = vaddq_s32(_r30, _r40);
                int32x4_t _tmp02b1 = vaddq_s32(_r31, _r41);
                int32x4_t _tmp13a0 = vsubq_s32(_r10, _r20);
                int32x4_t _tmp13a1 = vsubq_s32(_r11, _r21);
                int32x4_t _tmp13b0 = vsubq_s32(_r30, _r40);
                int32x4_t _tmp13b1 = vsubq_s32(_r31, _r41);

                int32x4_t _tmp00 = vaddq_s32(vaddq_s32(_tmp02a0, _tmp02b0), _r00);
                int32x4_t _tmp01 = vaddq_s32(vaddq_s32(_tmp02a1, _tmp02b1), _r01);
                int32x4_t _tmp10 = vaddq_s32(_tmp13a0, vshlq_n_s32(_tmp13b0, 1));
                int32x4_t _tmp11 = vaddq_s32(_tmp13a1, vshlq_n_s32(_tmp13b1, 1));
                int32x4_t _tmp20 = vaddq_s32(_tmp02a0, vshlq_n_s32(_tmp02b0, 2));
                int32x4_t _tmp21 = vaddq_s32(_tmp02a1, vshlq_n_s32(_tmp02b1, 2));
                int32x4_t _tmp30 = vaddq_s32(vaddq_s32(_tmp13a0, vshlq_n_s32(_tmp13b0, 3)), vshlq_n_s32(_r50, 2));
                int32x4_t _tmp31 = vaddq_s32(vaddq_s32(_tmp13a1, vshlq_n_s32(_tmp13b1, 3)), vshlq_n_s32(_r51, 2));

                _tmp00 = vshlq_n_s32(_tmp00, 2);
                _tmp01 = vshlq_n_s32(_tmp01, 2);
                _tmp10 = vshlq_n_s32(_tmp10, 2);
                _tmp11 = vshlq_n_s32(_tmp11, 2);
                _tmp20 = vshlq_n_s32(_tmp20, 2);
                _tmp21 = vshlq_n_s32(_tmp21, 2);
                _tmp30 = vshlq_n_s32(_tmp30, 2);
                _tmp31 = vshlq_n_s32(_tmp31, 2);

                vst1q_s32(tmp[0][m], _tmp00);
                vst1q_s32(tmp[0][m] + 4, _tmp01);
                vst1q_s32(tmp[1][m], _tmp10);
                vst1q_s32(tmp[1][m] + 4, _tmp11);
                vst1q_s32(tmp[2][m], _tmp20);
                vst1q_s32(tmp[2][m] + 4, _tmp21);
                vst1q_s32(tmp[3][m], _tmp30);
                vst1q_s32(tmp[3][m] + 4, _tmp31);

                r0 += max_jj * 6 * 8;
                r1 += max_jj * 6 * 8;
                r2 += max_jj * 6 * 8;
                r3 += max_jj * 6 * 8;
                r4 += max_jj * 6 * 8;
                r5 += max_jj * 6 * 8;
            }

            int* outptr0 = top_blob.channel((i + ii) / out_elempack).row<int>(ti * 4) + (tj * 4) * out_elempack;

            for (int m = 0; m < 4; m++)
            {
                if (ti * 4 + m >= outh)
                    continue;

                int32x4_t _r00 = vld1q_s32(tmp[m][0]);
                int32x4_t _r01 = vld1q_s32(tmp[m][0] + 4);
                int32x4_t _r10 = vld1q_s32(tmp[m][1]);
                int32x4_t _r11 = vld1q_s32(tmp[m][1] + 4);
                int32x4_t _r20 = vld1q_s32(tmp[m][2]);
                int32x4_t _r21 = vld1q_s32(tmp[m][2] + 4);
                int32x4_t _r30 = vld1q_s32(tmp[m][3]);
                int32x4_t _r31 = vld1q_s32(tmp[m][3] + 4);
                int32x4_t _r40 = vld1q_s32(tmp[m][4]);
                int32x4_t _r41 = vld1q_s32(tmp[m][4] + 4);
                int32x4_t _r50 = vld1q_s32(tmp[m][5]);
                int32x4_t _r51 = vld1q_s32(tmp[m][5] + 4);

                int32x4_t _tmp02a0 = vaddq_s32(_r10, _r20);
                int32x4_t _tmp02a1 = vaddq_s32(_r11, _r21);
                int32x4_t _tmp02b0 = vaddq_s32(_r30, _r40);
                int32x4_t _tmp02b1 = vaddq_s32(_r31, _r41);
                int32x4_t _tmp13a0 = vsubq_s32(_r10, _r20);
                int32x4_t _tmp13a1 = vsubq_s32(_r11, _r21);
                int32x4_t _tmp13b0 = vsubq_s32(_r30, _r40);
                int32x4_t _tmp13b1 = vsubq_s32(_r31, _r41);

                int32x4_t _tmp00 = vaddq_s32(vaddq_s32(_tmp02a0, _tmp02b0), _r00);
                int32x4_t _tmp01 = vaddq_s32(vaddq_s32(_tmp02a1, _tmp02b1), _r01);
                int32x4_t _tmp10 = vaddq_s32(_tmp13a0, vshlq_n_s32(_tmp13b0, 1));
                int32x4_t _tmp11 = vaddq_s32(_tmp13a1, vshlq_n_s32(_tmp13b1, 1));
                int32x4_t _tmp20 = vaddq_s32(_tmp02a0, vshlq_n_s32(_tmp02b0, 2));
                int32x4_t _tmp21 = vaddq_s32(_tmp02a1, vshlq_n_s32(_tmp02b1, 2));
                int32x4_t _tmp30 = vaddq_s32(vaddq_s32(_tmp13a0, vshlq_n_s32(_tmp13b0, 3)), _r50);
                int32x4_t _tmp31 = vaddq_s32(vaddq_s32(_tmp13a1, vshlq_n_s32(_tmp13b1, 3)), _r51);

                // TODO use integer trick for division by 576
                float32x4_t _v576 = vdupq_n_f32(1.0 / 576);
                _tmp00 = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_tmp00), _v576));
                _tmp01 = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_tmp01), _v576));
                _tmp10 = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_tmp10), _v576));
                _tmp11 = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_tmp11), _v576));
                _tmp20 = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_tmp20), _v576));
                _tmp21 = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_tmp21), _v576));
                _tmp30 = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_tmp30), _v576));
                _tmp31 = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_tmp31), _v576));

                if (out_elempack == 8)
                {
                    vst1q_s32(outptr0, _tmp00);
                    vst1q_s32(outptr0 + 4, _tmp01);
                    if (tj * 4 + 1 < outw)
                    {
                        vst1q_s32(outptr0 + 8, _tmp10);
                        vst1q_s32(outptr0 + 12, _tmp11);
                    }
                    if (tj * 4 + 2 < outw)
                    {
                        vst1q_s32(outptr0 + 16, _tmp20);
                        vst1q_s32(outptr0 + 20, _tmp21);
                    }
                    if (tj * 4 + 3 < outw)
                    {
                        vst1q_s32(outptr0 + 24, _tmp30);
                        vst1q_s32(outptr0 + 28, _tmp31);
                    }
                }
                if (out_elempack == 4)
                {
                    int* outptr1 = outptr0 + N;

                    vst1q_s32(outptr0, _tmp00);
                    vst1q_s32(outptr1, _tmp01);
                    if (tj * 4 + 1 < outw)
                    {
                        vst1q_s32(outptr0 + 4, _tmp10);
                        vst1q_s32(outptr1 + 4, _tmp11);
                    }
                    if (tj * 4 + 2 < outw)
                    {
                        vst1q_s32(outptr0 + 8, _tmp20);
                        vst1q_s32(outptr1 + 8, _tmp21);
                    }
                    if (tj * 4 + 3 < outw)
                    {
                        vst1q_s32(outptr0 + 12, _tmp30);
                        vst1q_s32(outptr1 + 12, _tmp31);
                    }
                }
                if (out_elempack == 1)
                {
                    int* outptr1 = outptr0 + N;
                    int* outptr2 = outptr0 + N * 2;
                    int* outptr3 = outptr0 + N * 3;
                    int* outptr4 = outptr0 + N * 4;
                    int* outptr5 = outptr0 + N * 5;
                    int* outptr6 = outptr0 + N * 6;
                    int* outptr7 = outptr0 + N * 7;

                    outptr0[0] = vgetq_lane_s32(_tmp00, 0);
                    outptr1[0] = vgetq_lane_s32(_tmp00, 1);
                    outptr2[0] = vgetq_lane_s32(_tmp00, 2);
                    outptr3[0] = vgetq_lane_s32(_tmp00, 3);
                    outptr4[0] = vgetq_lane_s32(_tmp01, 0);
                    outptr5[0] = vgetq_lane_s32(_tmp01, 1);
                    outptr6[0] = vgetq_lane_s32(_tmp01, 2);
                    outptr7[0] = vgetq_lane_s32(_tmp01, 3);
                    if (tj * 4 + 1 < outw)
                    {
                        outptr0[1] = vgetq_lane_s32(_tmp10, 0);
                        outptr1[1] = vgetq_lane_s32(_tmp10, 1);
                        outptr2[1] = vgetq_lane_s32(_tmp10, 2);
                        outptr3[1] = vgetq_lane_s32(_tmp10, 3);
                        outptr4[1] = vgetq_lane_s32(_tmp11, 0);
                        outptr5[1] = vgetq_lane_s32(_tmp11, 1);
                        outptr6[1] = vgetq_lane_s32(_tmp11, 2);
                        outptr7[1] = vgetq_lane_s32(_tmp11, 3);
                    }
                    if (tj * 4 + 2 < outw)
                    {
                        outptr0[2] = vgetq_lane_s32(_tmp20, 0);
                        outptr1[2] = vgetq_lane_s32(_tmp20, 1);
                        outptr2[2] = vgetq_lane_s32(_tmp20, 2);
                        outptr3[2] = vgetq_lane_s32(_tmp20, 3);
                        outptr4[2] = vgetq_lane_s32(_tmp21, 0);
                        outptr5[2] = vgetq_lane_s32(_tmp21, 1);
                        outptr6[2] = vgetq_lane_s32(_tmp21, 2);
                        outptr7[2] = vgetq_lane_s32(_tmp21, 3);
                    }
                    if (tj * 4 + 3 < outw)
                    {
                        outptr0[3] = vgetq_lane_s32(_tmp30, 0);
                        outptr1[3] = vgetq_lane_s32(_tmp30, 1);
                        outptr2[3] = vgetq_lane_s32(_tmp30, 2);
                        outptr3[3] = vgetq_lane_s32(_tmp30, 3);
                        outptr4[3] = vgetq_lane_s32(_tmp31, 0);
                        outptr5[3] = vgetq_lane_s32(_tmp31, 1);
                        outptr6[3] = vgetq_lane_s32(_tmp31, 2);
                        outptr7[3] = vgetq_lane_s32(_tmp31, 3);
                    }
                }

                outptr0 += outw * out_elempack;
            }
        }
    }
    for (; ii + 3 < max_ii; ii += 4)
    {
        int tmp[4][6][4];

        int jj = 0;
        for (; jj < max_jj; jj++)
        {
            int ti = (j + jj) / w_tiles;
            int tj = (j + jj) % w_tiles;

            const int* r0 = (const int*)top_tile + ii * max_jj * 36 + jj * 4;
            const int* r1 = r0 + max_jj * 4;
            const int* r2 = r0 + max_jj * 4 * 2;
            const int* r3 = r0 + max_jj * 4 * 3;
            const int* r4 = r0 + max_jj * 4 * 4;
            const int* r5 = r0 + max_jj * 4 * 5;

            for (int m = 0; m < 5; m++)
            {
                int32x4_t _r0 = vld1q_s32(r0);
                int32x4_t _r1 = vld1q_s32(r1);
                int32x4_t _r2 = vld1q_s32(r2);
                int32x4_t _r3 = vld1q_s32(r3);
                int32x4_t _r4 = vld1q_s32(r4);
                int32x4_t _r5 = vld1q_s32(r5);

                int32x4_t _tmp02a = vaddq_s32(_r1, _r2);
                int32x4_t _tmp02b = vaddq_s32(_r3, _r4);
                int32x4_t _tmp13a = vsubq_s32(_r1, _r2);
                int32x4_t _tmp13b = vsubq_s32(_r3, _r4);

                int32x4_t _tmp0 = vaddq_s32(vaddq_s32(_tmp02a, _tmp02b), _r0);
                int32x4_t _tmp1 = vaddq_s32(_tmp13a, vshlq_n_s32(_tmp13b, 1));
                int32x4_t _tmp2 = vaddq_s32(_tmp02a, vshlq_n_s32(_tmp02b, 2));
                int32x4_t _tmp3 = vaddq_s32(vaddq_s32(_tmp13a, vshlq_n_s32(_tmp13b, 3)), vshlq_n_s32(_r5, 2));

                vst1q_s32(tmp[0][m], _tmp0);
                vst1q_s32(tmp[1][m], _tmp1);
                vst1q_s32(tmp[2][m], _tmp2);
                vst1q_s32(tmp[3][m], _tmp3);

                r0 += max_jj * 6 * 4;
                r1 += max_jj * 6 * 4;
                r2 += max_jj * 6 * 4;
                r3 += max_jj * 6 * 4;
                r4 += max_jj * 6 * 4;
                r5 += max_jj * 6 * 4;
            }
            for (int m = 5; m < 6; m++)
            {
                int32x4_t _r0 = vld1q_s32(r0);
                int32x4_t _r1 = vld1q_s32(r1);
                int32x4_t _r2 = vld1q_s32(r2);
                int32x4_t _r3 = vld1q_s32(r3);
                int32x4_t _r4 = vld1q_s32(r4);
                int32x4_t _r5 = vld1q_s32(r5);

                int32x4_t _tmp02a = vaddq_s32(_r1, _r2);
                int32x4_t _tmp02b = vaddq_s32(_r3, _r4);
                int32x4_t _tmp13a = vsubq_s32(_r1, _r2);
                int32x4_t _tmp13b = vsubq_s32(_r3, _r4);

                int32x4_t _tmp0 = vaddq_s32(vaddq_s32(_tmp02a, _tmp02b), _r0);
                int32x4_t _tmp1 = vaddq_s32(_tmp13a, vshlq_n_s32(_tmp13b, 1));
                int32x4_t _tmp2 = vaddq_s32(_tmp02a, vshlq_n_s32(_tmp02b, 2));
                int32x4_t _tmp3 = vaddq_s32(vaddq_s32(_tmp13a, vshlq_n_s32(_tmp13b, 3)), vshlq_n_s32(_r5, 2));

                _tmp0 = vshlq_n_s32(_tmp0, 2);
                _tmp1 = vshlq_n_s32(_tmp1, 2);
                _tmp2 = vshlq_n_s32(_tmp2, 2);
                _tmp3 = vshlq_n_s32(_tmp3, 2);

                vst1q_s32(tmp[0][m], _tmp0);
                vst1q_s32(tmp[1][m], _tmp1);
                vst1q_s32(tmp[2][m], _tmp2);
                vst1q_s32(tmp[3][m], _tmp3);

                r0 += max_jj * 6 * 4;
                r1 += max_jj * 6 * 4;
                r2 += max_jj * 6 * 4;
                r3 += max_jj * 6 * 4;
                r4 += max_jj * 6 * 4;
                r5 += max_jj * 6 * 4;
            }

            int* outptr0 = top_blob.channel((i + ii) / out_elempack).row<int>(ti * 4) + (tj * 4) * out_elempack;

            for (int m = 0; m < 4; m++)
            {
                if (ti * 4 + m >= outh)
                    continue;

                int32x4_t _r0 = vld1q_s32(tmp[m][0]);
                int32x4_t _r1 = vld1q_s32(tmp[m][1]);
                int32x4_t _r2 = vld1q_s32(tmp[m][2]);
                int32x4_t _r3 = vld1q_s32(tmp[m][3]);
                int32x4_t _r4 = vld1q_s32(tmp[m][4]);
                int32x4_t _r5 = vld1q_s32(tmp[m][5]);

                int32x4_t _tmp02a = vaddq_s32(_r1, _r2);
                int32x4_t _tmp02b = vaddq_s32(_r3, _r4);
                int32x4_t _tmp13a = vsubq_s32(_r1, _r2);
                int32x4_t _tmp13b = vsubq_s32(_r3, _r4);

                int32x4_t _tmp0 = vaddq_s32(vaddq_s32(_tmp02a, _tmp02b), _r0);
                int32x4_t _tmp1 = vaddq_s32(_tmp13a, vshlq_n_s32(_tmp13b, 1));
                int32x4_t _tmp2 = vaddq_s32(_tmp02a, vshlq_n_s32(_tmp02b, 2));
                int32x4_t _tmp3 = vaddq_s32(vaddq_s32(_tmp13a, vshlq_n_s32(_tmp13b, 3)), _r5);

                // TODO use integer trick for division by 576
                float32x4_t _v576 = vdupq_n_f32(1.0 / 576);
                _tmp0 = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_tmp0), _v576));
                _tmp1 = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_tmp1), _v576));
                _tmp2 = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_tmp2), _v576));
                _tmp3 = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(_tmp3), _v576));

                if (out_elempack == 4)
                {
                    vst1q_s32(outptr0, _tmp0);
                    if (tj * 4 + 1 < outw) vst1q_s32(outptr0 + 4, _tmp1);
                    if (tj * 4 + 2 < outw) vst1q_s32(outptr0 + 8, _tmp2);
                    if (tj * 4 + 3 < outw) vst1q_s32(outptr0 + 12, _tmp3);
                }
                if (out_elempack == 1)
                {
                    int* outptr1 = outptr0 + N;
                    int* outptr2 = outptr0 + N * 2;
                    int* outptr3 = outptr0 + N * 3;

                    outptr0[0] = vgetq_lane_s32(_tmp0, 0);
                    outptr1[0] = vgetq_lane_s32(_tmp0, 1);
                    outptr2[0] = vgetq_lane_s32(_tmp0, 2);
                    outptr3[0] = vgetq_lane_s32(_tmp0, 3);
                    if (tj * 4 + 1 < outw)
                    {
                        outptr0[1] = vgetq_lane_s32(_tmp1, 0);
                        outptr1[1] = vgetq_lane_s32(_tmp1, 1);
                        outptr2[1] = vgetq_lane_s32(_tmp1, 2);
                        outptr3[1] = vgetq_lane_s32(_tmp1, 3);
                    }
                    if (tj * 4 + 2 < outw)
                    {
                        outptr0[2] = vgetq_lane_s32(_tmp2, 0);
                        outptr1[2] = vgetq_lane_s32(_tmp2, 1);
                        outptr2[2] = vgetq_lane_s32(_tmp2, 2);
                        outptr3[2] = vgetq_lane_s32(_tmp2, 3);
                    }
                    if (tj * 4 + 3 < outw)
                    {
                        outptr0[3] = vgetq_lane_s32(_tmp3, 0);
                        outptr1[3] = vgetq_lane_s32(_tmp3, 1);
                        outptr2[3] = vgetq_lane_s32(_tmp3, 2);
                        outptr3[3] = vgetq_lane_s32(_tmp3, 3);
                    }
                }

                outptr0 += outw * out_elempack;
            }
        }
    }
#endif // __ARM_NEON
    for (; ii + 1 < max_ii; ii += 2)
    {
        int tmp[4][6][2];

        int jj = 0;
        for (; jj < max_jj; jj++)
        {
            int ti = (j + jj) / w_tiles;
            int tj = (j + jj) % w_tiles;

            const int* r0 = (const int*)top_tile + ii * max_jj * 36 + jj * 2;
            const int* r1 = r0 + max_jj * 2;
            const int* r2 = r0 + max_jj * 2 * 2;
            const int* r3 = r0 + max_jj * 2 * 3;
            const int* r4 = r0 + max_jj * 2 * 4;
            const int* r5 = r0 + max_jj * 2 * 5;

            for (int m = 0; m < 5; m++)
            {
                int tmp02a0 = r1[0] + r2[0];
                int tmp02a1 = r1[1] + r2[1];
                int tmp02b0 = r3[0] + r4[0];
                int tmp02b1 = r3[1] + r4[1];
                int tmp13a0 = r1[0] - r2[0];
                int tmp13a1 = r1[1] - r2[1];
                int tmp13b0 = r3[0] - r4[0];
                int tmp13b1 = r3[1] - r4[1];

                int tmp00 = tmp02a0 + tmp02b0 + r0[0];
                int tmp01 = tmp02a1 + tmp02b1 + r0[1];
                int tmp10 = tmp13a0 + tmp13b0 * 2;
                int tmp11 = tmp13a1 + tmp13b1 * 2;
                int tmp20 = tmp02a0 + tmp02b0 * 4;
                int tmp21 = tmp02a1 + tmp02b1 * 4;
                int tmp30 = tmp13a0 + tmp13b0 * 8 + r5[0] * 4;
                int tmp31 = tmp13a1 + tmp13b1 * 8 + r5[1] * 4;

                tmp[0][m][0] = tmp00;
                tmp[0][m][1] = tmp01;
                tmp[1][m][0] = tmp10;
                tmp[1][m][1] = tmp11;
                tmp[2][m][0] = tmp20;
                tmp[2][m][1] = tmp21;
                tmp[3][m][0] = tmp30;
                tmp[3][m][1] = tmp31;

                r0 += max_jj * 6 * 2;
                r1 += max_jj * 6 * 2;
                r2 += max_jj * 6 * 2;
                r3 += max_jj * 6 * 2;
                r4 += max_jj * 6 * 2;
                r5 += max_jj * 6 * 2;
            }
            for (int m = 5; m < 6; m++)
            {
                int tmp02a0 = r1[0] + r2[0];
                int tmp02a1 = r1[1] + r2[1];
                int tmp02b0 = r3[0] + r4[0];
                int tmp02b1 = r3[1] + r4[1];
                int tmp13a0 = r1[0] - r2[0];
                int tmp13a1 = r1[1] - r2[1];
                int tmp13b0 = r3[0] - r4[0];
                int tmp13b1 = r3[1] - r4[1];

                int tmp00 = tmp02a0 + tmp02b0 + r0[0];
                int tmp01 = tmp02a1 + tmp02b1 + r0[1];
                int tmp10 = tmp13a0 + tmp13b0 * 2;
                int tmp11 = tmp13a1 + tmp13b1 * 2;
                int tmp20 = tmp02a0 + tmp02b0 * 4;
                int tmp21 = tmp02a1 + tmp02b1 * 4;
                int tmp30 = tmp13a0 + tmp13b0 * 8 + r5[0] * 4;
                int tmp31 = tmp13a1 + tmp13b1 * 8 + r5[1] * 4;

                tmp00 = tmp00 * 4;
                tmp01 = tmp01 * 4;
                tmp10 = tmp10 * 4;
                tmp11 = tmp11 * 4;
                tmp20 = tmp20 * 4;
                tmp21 = tmp21 * 4;
                tmp30 = tmp30 * 4;
                tmp31 = tmp31 * 4;

                tmp[0][m][0] = tmp00;
                tmp[0][m][1] = tmp01;
                tmp[1][m][0] = tmp10;
                tmp[1][m][1] = tmp11;
                tmp[2][m][0] = tmp20;
                tmp[2][m][1] = tmp21;
                tmp[3][m][0] = tmp30;
                tmp[3][m][1] = tmp31;

                r0 += max_jj * 6 * 2;
                r1 += max_jj * 6 * 2;
                r2 += max_jj * 6 * 2;
                r3 += max_jj * 6 * 2;
                r4 += max_jj * 6 * 2;
                r5 += max_jj * 6 * 2;
            }

            int* outptr0 = top_blob.channel(i + ii).row<int>(ti * 4) + (tj * 4);

            for (int m = 0; m < 4; m++)
            {
                if (ti * 4 + m >= outh)
                    continue;

                int tmp02a0 = tmp[m][1][0] + tmp[m][2][0];
                int tmp02a1 = tmp[m][1][1] + tmp[m][2][1];
                int tmp02b0 = tmp[m][3][0] + tmp[m][4][0];
                int tmp02b1 = tmp[m][3][1] + tmp[m][4][1];
                int tmp13a0 = tmp[m][1][0] - tmp[m][2][0];
                int tmp13a1 = tmp[m][1][1] - tmp[m][2][1];
                int tmp13b0 = tmp[m][3][0] - tmp[m][4][0];
                int tmp13b1 = tmp[m][3][1] - tmp[m][4][1];

                int tmp00 = tmp02a0 + tmp02b0 + tmp[m][0][0];
                int tmp01 = tmp02a1 + tmp02b1 + tmp[m][0][1];
                int tmp10 = tmp13a0 + tmp13b0 * 2;
                int tmp11 = tmp13a1 + tmp13b1 * 2;
                int tmp20 = tmp02a0 + tmp02b0 * 4;
                int tmp21 = tmp02a1 + tmp02b1 * 4;
                int tmp30 = tmp13a0 + tmp13b0 * 8 + tmp[m][5][0];
                int tmp31 = tmp13a1 + tmp13b1 * 8 + tmp[m][5][1];

                tmp00 = tmp00 / 576;
                tmp01 = tmp01 / 576;
                tmp10 = tmp10 / 576;
                tmp11 = tmp11 / 576;
                tmp20 = tmp20 / 576;
                tmp21 = tmp21 / 576;
                tmp30 = tmp30 / 576;
                tmp31 = tmp31 / 576;

                // if (out_elempack == 1)
                {
                    int* outptr1 = outptr0 + N;

                    outptr0[0] = tmp00;
                    outptr1[0] = tmp01;
                    if (tj * 4 + 1 < outw)
                    {
                        outptr0[1] = tmp10;
                        outptr1[1] = tmp11;
                    }
                    if (tj * 4 + 2 < outw)
                    {
                        outptr0[2] = tmp20;
                        outptr1[2] = tmp21;
                    }
                    if (tj * 4 + 3 < outw)
                    {
                        outptr0[3] = tmp30;
                        outptr1[3] = tmp31;
                    }
                }

                outptr0 += outw;
            }
        }
    }
    for (; ii < max_ii; ii++)
    {
        int tmp[4][6];

        int jj = 0;
        for (; jj < max_jj; jj++)
        {
            int ti = (j + jj) / w_tiles;
            int tj = (j + jj) % w_tiles;

            const int* r0 = (const int*)top_tile + ii * max_jj * 36 + jj;
            const int* r1 = r0 + max_jj;
            const int* r2 = r0 + max_jj * 2;
            const int* r3 = r0 + max_jj * 3;
            const int* r4 = r0 + max_jj * 4;
            const int* r5 = r0 + max_jj * 5;

            for (int m = 0; m < 5; m++)
            {
                int tmp02a = r1[0] + r2[0];
                int tmp02b = r3[0] + r4[0];
                int tmp13a = r1[0] - r2[0];
                int tmp13b = r3[0] - r4[0];

                int tmp0 = tmp02a + tmp02b + r0[0];
                int tmp1 = tmp13a + tmp13b * 2;
                int tmp2 = tmp02a + tmp02b * 4;
                int tmp3 = tmp13a + tmp13b * 8 + r5[0] * 4;

                tmp[0][m] = tmp0;
                tmp[1][m] = tmp1;
                tmp[2][m] = tmp2;
                tmp[3][m] = tmp3;

                r0 += max_jj * 6;
                r1 += max_jj * 6;
                r2 += max_jj * 6;
                r3 += max_jj * 6;
                r4 += max_jj * 6;
                r5 += max_jj * 6;
            }
            for (int m = 5; m < 6; m++)
            {
                int tmp02a = r1[0] + r2[0];
                int tmp02b = r3[0] + r4[0];
                int tmp13a = r1[0] - r2[0];
                int tmp13b = r3[0] - r4[0];

                int tmp0 = tmp02a + tmp02b + r0[0];
                int tmp1 = tmp13a + tmp13b * 2;
                int tmp2 = tmp02a + tmp02b * 4;
                int tmp3 = tmp13a + tmp13b * 8 + r5[0] * 4;

                tmp0 = tmp0 * 4;
                tmp1 = tmp1 * 4;
                tmp2 = tmp2 * 4;
                tmp3 = tmp3 * 4;

                tmp[0][m] = tmp0;
                tmp[1][m] = tmp1;
                tmp[2][m] = tmp2;
                tmp[3][m] = tmp3;

                r0 += max_jj * 6;
                r1 += max_jj * 6;
                r2 += max_jj * 6;
                r3 += max_jj * 6;
                r4 += max_jj * 6;
                r5 += max_jj * 6;
            }

            int* outptr0 = top_blob.channel(i + ii).row<int>(ti * 4) + (tj * 4);

            for (int m = 0; m < 4; m++)
            {
                if (ti * 4 + m >= outh)
                    continue;

                int tmp02a = tmp[m][1] + tmp[m][2];
                int tmp02b = tmp[m][3] + tmp[m][4];
                int tmp13a = tmp[m][1] - tmp[m][2];
                int tmp13b = tmp[m][3] - tmp[m][4];

                int tmp0 = tmp02a + tmp02b + tmp[m][0];
                int tmp1 = tmp13a + tmp13b * 2;
                int tmp2 = tmp02a + tmp02b * 4;
                int tmp3 = tmp13a + tmp13b * 8 + tmp[m][5];

                tmp0 = tmp0 / 576;
                tmp1 = tmp1 / 576;
                tmp2 = tmp2 / 576;
                tmp3 = tmp3 / 576;

                // if (out_elempack == 1)
                {
                    outptr0[0] = tmp0;
                    if (tj * 4 + 1 < outw) outptr0[1] = tmp1;
                    if (tj * 4 + 2 < outw) outptr0[2] = tmp2;
                    if (tj * 4 + 3 < outw) outptr0[3] = tmp3;
                }

                outptr0 += outw;
            }
        }
    }
}

static void conv3x3s1_winograd43_int8(const Mat& bottom_blob, Mat& top_blob, const Mat& AT, int nT, const Option& opt)
{
    int outw = top_blob.w;
    int outh = top_blob.h;

    // pad to 4n+2, winograd F(4,3)
    int w_tiles = (outw + 3) / 4;
    int h_tiles = (outh + 3) / 4;
    int tiles = w_tiles * h_tiles;

    const int M = top_blob.c * top_blob.elempack;
    const int N = tiles;
    const int K = bottom_blob.c * bottom_blob.elempack;
    const int B = 36;

    // NCNN_LOGE("conv3x3s1_winograd43_int8 %d %d %d", M, N, K);

    int TILE_M, TILE_N, TILE_K;
    get_optimal_tile_mnk_int8(M, N, K, TILE_M, TILE_N, TILE_K, nT);

    const int nn_M = (M + TILE_M - 1) / TILE_M;
    const int nn_N = (N + TILE_N - 1) / TILE_N;
    const int nn_K = (K + TILE_K - 1) / TILE_K;

    // NCNN_LOGE("TILE M/N/K = %d %d %d -> %d %d %d", M, N, K, TILE_M, TILE_N, TILE_K);

    Mat BT(TILE_K * TILE_N, B, (K + TILE_K - 1) / TILE_K, (N + TILE_N - 1) / TILE_N, 4u, opt.workspace_allocator);

    const int nn_NK = nn_N * nn_K;

    if (nT > 1 && nn_NK < nT)
    {
        Mat B_tile(TILE_N * B * TILE_K, 4u, opt.workspace_allocator);

        for (int ppjk = 0; ppjk < nn_NK; ppjk++)
        {
            const int ppj = ppjk / nn_K;
            const int ppk = ppjk % nn_K;

            const int j = ppj * TILE_N;
            const int k = ppk * TILE_K;

            const int max_jj = std::min((N - j), TILE_N);
            const int max_kk = std::min((K - k), TILE_K);

            // transform input
            conv3x3s1_winograd43_transform_input_tile_int8(bottom_blob, B_tile, j, max_jj, k, max_kk, nT);

            Mat BT_tile = BT.channel(j / TILE_N).depth(k / TILE_K);

            transpose_pack_B_tile_int8(B_tile, BT_tile, B, max_jj, max_kk, nT);
        }
    }
    else
    {
        Mat B_tileX(TILE_N * B * TILE_K, 1, nT, 4u, opt.workspace_allocator);

        #pragma omp parallel for num_threads(nT)
        for (int ppjk = 0; ppjk < nn_NK; ppjk++)
        {
            const int ppj = ppjk / nn_K;
            const int ppk = ppjk % nn_K;

            const int j = ppj * TILE_N;
            const int k = ppk * TILE_K;

            const int max_jj = std::min((N - j), TILE_N);
            const int max_kk = std::min((K - k), TILE_K);

            Mat B_tile = B_tileX.channel(get_omp_thread_num());

            // transform input
            conv3x3s1_winograd43_transform_input_tile_int8(bottom_blob, B_tile, j, max_jj, k, max_kk, 1);

            Mat BT_tile = BT.channel(j / TILE_N).depth(k / TILE_K);

            transpose_pack_B_tile_int8(B_tile, BT_tile, B, max_jj, max_kk, 1);
        }
    }

    Mat top_tileX(TILE_N * B * TILE_M, 1, nT, 4u, opt.workspace_allocator);

    #pragma omp parallel for num_threads(nT)
    for (int ppj = 0; ppj < nn_M; ppj++)
    {
        const int i = ppj * TILE_M;

        Mat top_tile = top_tileX.channel(get_omp_thread_num());

        const int max_ii = std::min((M - i), TILE_M);

        for (int j = 0; j < N; j += TILE_N)
        {
            const int max_jj = std::min((N - j), TILE_N);

            for (int k = 0; k < K; k += TILE_K)
            {
                const int max_kk = std::min((K - k), TILE_K);

                const Mat AT_tile = AT.channel(i / TILE_M).depth(k / TILE_K);

                const Mat BT_tile = BT.channel(j / TILE_N).depth(k / TILE_K);

                gemm_transB_packed_tile_int8(AT_tile, BT_tile, top_tile, B, max_ii, max_jj, k, max_kk);
            }

            // transform output
            conv3x3s1_winograd43_transform_output_tile_int8(top_tile, top_blob, i, max_ii, j, max_jj);
        }
    }
}
