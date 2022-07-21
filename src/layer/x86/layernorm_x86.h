#ifndef LAYER_LAYERNORM_X86_H
#define LAYER_LAYERNORM_X86_H

#include "layernorm.h"

namespace ncnn {

class LayerNorm_x86 : virtual public LayerNorm
{
public:
    LayerNorm_x86();

    virtual int forward_inplace(Mat& bottom_top_blob, const Option& opt) const;

protected:
    NCNN_FORCEINLINE void fast_1d_layer_norm(float* ptr, int elemcount) const;
    NCNN_FORCEINLINE void fast_1d_layer_norm_packed(float* ptr, int elempack, int elemcount, int size) const;
    NCNN_FORCEINLINE void fast_fmadd_fmadd(float* ptr, float a, float b, int elemcount) const;
    NCNN_FORCEINLINE void fast_fmadd_fmadd_packed(float* ptr, float* a, float* b, int elempack, int size) const;

    NCNN_FORCEINLINE int forward_inplace_unpacked(Mat& bottom_top_blob, const Option& opt) const;
    NCNN_FORCEINLINE int forward_inplace_packed(Mat& bottom_top_blob, const Option& opt) const;
};

} // namespace ncnn

#endif // LAYER_LAYERNORM_X86_H