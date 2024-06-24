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
from packaging import version

class Model(nn.Module):
    def __init__(self):
        super(Model, self).__init__()

    def forward(self, x):
        x = F.max_pool3d(x, kernel_size=3)
        if version.parse(torch.__version__) < version.parse('1.12'):
            x = F.max_pool3d(x, kernel_size=4, stride=2, padding=2)
            x = F.max_pool3d(x, kernel_size=(1,2,3), stride=1, padding=(0,0,1), return_indices=False, ceil_mode=False)
            x = F.max_pool3d(x, kernel_size=(3,4,5), stride=(1,2,2), padding=(1,2,2), return_indices=False, ceil_mode=True)
            x = F.max_pool3d(x, kernel_size=(2,3,3), stride=1, padding=1, return_indices=False, ceil_mode=False)
            x = F.max_pool3d(x, kernel_size=2, stride=1, padding=0, return_indices=False, ceil_mode=True)
            x, indices = F.max_pool3d(x, kernel_size=(5,4,4), stride=1, padding=2, return_indices=True, ceil_mode=False)
        else:
            x = F.max_pool3d(x, kernel_size=4, stride=2, padding=2, dilation=1)
            x = F.max_pool3d(x, kernel_size=(1,2,3), stride=1, padding=(0,0,1), dilation=1, return_indices=False, ceil_mode=False)
            x = F.max_pool3d(x, kernel_size=(3,4,5), stride=(1,2,2), padding=(1,2,2), dilation=1, return_indices=False, ceil_mode=True)
            x = F.max_pool3d(x, kernel_size=(2,3,3), stride=1, padding=1, dilation=(1,2,2), return_indices=False, ceil_mode=False)
            x = F.max_pool3d(x, kernel_size=2, stride=1, padding=0, dilation=1, return_indices=False, ceil_mode=True)
            x, indices = F.max_pool3d(x, kernel_size=(5,4,4), stride=1, padding=2, dilation=1, return_indices=True, ceil_mode=False)
        return x, indices

def test():
    net = Model()
    net.eval()

    torch.manual_seed(0)
    x = torch.rand(1, 12, 96, 128, 128)

    a = net(x)

    # export onnx
    torch.onnx.export(net, (x,), "test_F_max_pool3d.onnx")

    # onnx to pnnx
    import os
    os.system("../../src/pnnx test_F_max_pool3d.onnx inputshape=[1,12,96,128,128]")

    # pnnx inference
    import test_F_max_pool3d_pnnx
    b = test_F_max_pool3d_pnnx.test_inference()

    for a0, b0 in zip(a, b):
        if not torch.equal(a0, b0):
            return False
    return True

if __name__ == "__main__":
    if test():
        exit(0)
    else:
        exit(1)
