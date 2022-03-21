# Tencent is pleased to support the open source community by making ncnn available.
#
# Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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

        self.w0 = nn.Parameter(torch.rand(1, 12, 52))
        self.w1 = nn.Parameter(torch.rand(1, 12, 52))
        self.w2 = nn.Parameter(torch.rand(1, 12, 1))
        self.w3 = nn.Parameter(torch.rand(1, 12, 52))

    def forward(self, x):
        b = (self.w0 + self.w1 + 0.22) + self.w2 * 0.1
        x = x + b - self.w3 / 2
        return x

def test():
    net = Model()
    net.eval()

    torch.manual_seed(0)
    x = torch.rand(1, 12, 52)

    a = net(x)

    # export torchscript
    mod = torch.jit.trace(net, x)
    mod.save("test_pnnx_fold_constant.pt")

    # torchscript to pnnx
    import os
    os.system("../src/pnnx test_pnnx_fold_constant.pt inputshape=[1,12,52]")

    # pnnx inference
    import test_pnnx_fold_constant_pnnx
    b = test_pnnx_fold_constant_pnnx.test_inference()

    return torch.equal(a, b)

if __name__ == "__main__":
    if test():
        exit(0)
    else:
        exit(1)
