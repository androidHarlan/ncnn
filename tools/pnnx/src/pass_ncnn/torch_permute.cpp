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

class torch_permute : public GraphRewriterPass
{
public:
    const char* match_pattern_graph() const
    {
        return R"PNNXIR(7767517
3 2
pnnx.Input              input       0 1 input
torch.permute           op_0        1 1 input out dims=%dims
pnnx.Output             output      1 0 out
)PNNXIR";
    }

    const char* type_str() const
    {
        return "Permute";
    }

    const char* name_str() const
    {
        return "permute";
    }

    void write(Operator* op, const std::map<std::string, Parameter>& captured_params) const
    {
        op->params["0"] = 0;

        const std::vector<int>& dims = captured_params.at("dims").ai;

        int input_rank = (int)op->inputs[0]->shape.size();

        const int batch_index = op->inputs[0]->params["__batch_index"].i;

        if (input_rank > 5)
        {
            fprintf(stderr, "permute %d-rank tensor is not supported yet!\n", input_rank);
            return;
        }

        if (input_rank == 0)
        {
            // assume input is fine
            input_rank = (int)dims.size();
        }

        if (input_rank != (int)dims.size())
        {
            fprintf(stderr, "permute %d-rank tensor with %d-rank dims is not possible\n", input_rank, (int)dims.size());
            return;
        }

        // drop permute batch index
        std::vector<int> new_dims;
        for (int i = 0; i < (int)dims.size(); i++)
        {
            if (dims[i] == batch_index)
                continue;

            int new_dim = dims[i] > batch_index ? dims[i] - 1 : dims[i];
            new_dims.push_back(new_dim);
        }

        if (input_rank == 2)
        {
            // noop
        }
        if (input_rank == 3)
        {
            if (new_dims == std::vector<int>{1, 0})
                op->params["0"] = 1;
        }
        if (input_rank == 4)
        {
            if (new_dims == std::vector<int>{0, 2, 1})
                op->params["0"] = 1;
            else if (new_dims == std::vector<int>{1, 0, 2})
                op->params["0"] = 2;
            else if (new_dims == std::vector<int>{1, 2, 0})
                op->params["0"] = 3;
            else if (new_dims == std::vector<int>{2, 0, 1})
                op->params["0"] = 4;
            else if (new_dims == std::vector<int>{2, 1, 0})
                op->params["0"] = 5;
        }
        if (input_rank == 5)
        {
            if (new_dims == std::vector<int>{0, 2, 3, 1})
                op->params["0"] = 1;
            else if (new_dims == std::vector<int>{1, 0, 2, 3})
                op->params["0"] = 2;
            else if (new_dims == std::vector<int>{1, 2, 3, 0})
                op->params["0"] = 3;
            else if (new_dims == std::vector<int>{2, 3, 0, 1})
                op->params["0"] = 4;
            else if (new_dims == std::vector<int>{2, 3, 1, 0})
                op->params["0"] = 5;
            else
                fprintf(stderr, "unsupported permute dims!\n");
        }
    }
};

REGISTER_GLOBAL_PNNX_NCNN_GRAPH_REWRITER_PASS(torch_permute, 20)

} // namespace ncnn

} // namespace pnnx
