// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "fuse_slice_copy.h"

#include <limits.h>
#include <algorithm>
#include <stack>
#include "pass_level2.h"

namespace pnnx {

void fuse_slice_copy(Graph& graph)
{
    while (1)
    {
        bool matched = false;

        for (size_t i = 0; i < graph.ops.size(); i++)
        {
            Operator* op = graph.ops[i];

            if (op->type != "Tensor.copy")
                continue;

            // collect slice / select op chain
            std::stack<const Operator*> slice_select_ops;
            int descent_dim_current = INT_MAX;
            const Operand* in0 = op->inputs[0];
            while (in0->producer->type == "Tensor.slice" || in0->producer->type == "Tensor.select")
            {
                const Operator* sop = in0->producer;
                if (sop->type == "Tensor.slice")
                {
                    if (sop->params.find("dims") == sop->params.end()
                            || sop->params.find("starts") == sop->params.end()
                            || sop->params.find("ends") == sop->params.end()
                            || sop->params.find("steps") == sop->params.end())
                    {
                        fprintf(stderr, "dynamic index in slice copy chain is not supported\n");
                        break;
                    }

                    int dims0 = sop->params.at("dims").ai[0];
                    if (descent_dim_current < dims0)
                    {
                        break;
                    }

                    descent_dim_current = dims0;
                }

                if (sop->type == "Tensor.select")
                {
                    if (sop->params.find("dim") == sop->params.end()
                            || sop->params.find("index") == sop->params.end())
                    {
                        fprintf(stderr, "dynamic index in select copy chain is not supported\n");
                        break;
                    }

                    int dim = sop->params.at("dim").i;
                    if (descent_dim_current < dim)
                    {
                        break;
                    }

                    descent_dim_current = dim;
                }

                slice_select_ops.push(sop);
                in0 = sop->inputs[0];
            }

            if (slice_select_ops.empty())
                continue;

            matched = true;

            const Operator* top_sop = slice_select_ops.top();

            // construct one-step slice
            std::vector<int> new_dims;
            std::vector<int> new_starts;
            std::vector<int> new_ends;
            std::vector<int> new_steps;

            int select_dims_offset = 0;
            while (!slice_select_ops.empty())
            {
                const Operator* sop = slice_select_ops.top();
                slice_select_ops.pop();

                if (sop->type == "Tensor.slice")
                {
                    std::vector<int> dims = sop->params.at("dims").ai;
                    std::vector<int> starts = sop->params.at("starts").ai;
                    std::vector<int> ends = sop->params.at("ends").ai;
                    std::vector<int> steps = sop->params.at("steps").ai;

                    for (size_t j = 0; j < dims.size(); j++)
                    {
                        dims[j] += select_dims_offset;
                    }

                    new_dims.insert(new_dims.end(), dims.begin(), dims.end());
                    new_starts.insert(new_starts.end(), starts.begin(), starts.end());
                    new_ends.insert(new_ends.end(), ends.begin(), ends.end());
                    new_steps.insert(new_steps.end(), steps.begin(), steps.end());
                }
                else if (sop->type == "Tensor.select")
                {
                    int dim = sop->params.at("dim").i;
                    int index = sop->params.at("index").i;

                    dim += select_dims_offset;
                    int end = index + 1;
                    if (index == -1)
                        end = INT_MAX;

                    new_dims.push_back(dim);
                    new_starts.push_back(index);
                    new_ends.push_back(end);
                    new_steps.push_back(1);

                    select_dims_offset += 1;
                }
            }

            op->type = "Tensor.slice_copy";

            op->inputs[0]->remove_consumer(op);
            op->inputs[0] = top_sop->inputs[0];
            top_sop->inputs[0]->consumers.push_back(op);

            op->params["dims"] = new_dims;
            op->params["starts"] = new_starts;
            op->params["ends"] = new_ends;
            op->params["steps"] = new_steps;

            break;
        }

        if (!matched)
            break;
    }
}

} // namespace pnnx
