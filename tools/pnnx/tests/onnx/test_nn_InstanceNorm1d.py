# Tencent is pleased to support the open source community by making ncnn available.
#
# Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
#
# Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
# in compliance with the License. You may obtain a copy of the License at
#
# https://opensource.org/licenses/BSD-3-Clause
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

import torch
import torch.nn as nn
import torch.nn.functional as F

class Model(nn.Module):
    def __init__(self):
        super(Model, self).__init__()

        self.in_0 = nn.InstanceNorm1d(num_features=12, affine=True)
        self.in_0.weight = nn.Parameter(torch.rand(12))
        self.in_0.bias = nn.Parameter(torch.rand(12))
        self.in_1 = nn.InstanceNorm1d(num_features=12, eps=1e-2, affine=False)
        self.in_2 = nn.InstanceNorm1d(num_features=12, eps=1e-4, affine=True, track_running_stats=True)
        self.in_2.weight = nn.Parameter(torch.rand(12))
        self.in_2.bias = nn.Parameter(torch.rand(12))

    def forward(self, x):
        x = self.in_0(x)
        x = self.in_1(x)
        x = self.in_2(x)
        return x

def test():
    net = Model()
    net.eval()

    torch.manual_seed(0)
    x = torch.rand(1, 12, 24)

    a = net(x)

    # export onnx
    torch.onnx.export(net, (x,), "test_nn_InstanceNorm1d.onnx")

    # onnx to pnnx
    import os
    os.system("../../src/pnnx test_nn_InstanceNorm1d.onnx inputshape=[1,12,24]")

    # pnnx inference
    import test_nn_InstanceNorm1d_pnnx
    b = test_nn_InstanceNorm1d_pnnx.test_inference()

    return torch.equal(a, b)

if __name__ == "__main__":
    if test():
        exit(0)
    else:
        exit(1)
