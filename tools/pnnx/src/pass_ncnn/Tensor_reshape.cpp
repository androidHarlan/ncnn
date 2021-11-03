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

#include "pass_ncnn.h"

namespace pnnx {

namespace ncnn {

class Tensor_reshape : public GraphRewriterPass
{
public:
    const char* match_pattern_graph() const
    {
        return R"PNNXIR(7767517
3 2
pnnx.Input              input       0 1 input
Tensor.reshape          op_0        1 1 input out shape=%shape
pnnx.Output             output      1 0 out
)PNNXIR";
    }

    const char* type_str() const
    {
        return "Reshape";
    }

    const char* name_str() const
    {
        return "reshape";
    }

    void write(Operator* op, const std::map<std::string, Parameter>& captured_params) const
    {
        const std::vector<int>& shape = captured_params.at("shape").ai;
        const int shape_rank = (int)shape.size();

        if (shape_rank > 5)
        {
            fprintf(stderr, "reshape to %d-rank tensor is not supported yet!\n", shape_rank);
            return;
        }

        if (shape_rank == 1)
        {
            fprintf(stderr, "reshape across batch dim is not supported yet!\n");
            op->params["0"] = shape[0]; // should never reach here
        }
        if (shape_rank == 2)
        {
            op->params["0"] = shape[1];
        }
        if (shape_rank == 3)
        {
            op->params["0"] = shape[2];
            op->params["1"] = shape[1];
        }
        if (shape_rank == 4)
        {
            op->params["0"] = shape[3];
            op->params["1"] = shape[2];
            op->params["2"] = shape[1];
        }
        if (shape_rank == 5)
        {
            op->params["0"] = shape[4] == -1 || shape[3] == -1 ? -1 : shape[4] * shape[3];
            op->params["1"] = shape[2];
            op->params["2"] = shape[1];
        }
    }
};

REGISTER_GLOBAL_PNNX_NCNN_GRAPH_REWRITER_PASS(Tensor_reshape, 20)

} // namespace ncnn

} // namespace pnnx
