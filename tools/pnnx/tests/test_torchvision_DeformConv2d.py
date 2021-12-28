# Tencent is pleased to support the open source community by making ncnn available.
#
# Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
import torchvision

class Model(nn.Module):
    def __init__(self):
        super(Model, self).__init__()

        self.conv_0 = nn.Conv2d(in_channels=12, out_channels=2*3*3, kernel_size=3)
        self.conv_1 = torchvision.ops.DeformConv2d(in_channels=12, out_channels=16, kernel_size=3)

    def forward(self, x):
        offset = self.conv_0(x)
        x = self.conv_1(x, offset)
        return x

def test():
    net = Model()
    net.eval()

    torch.manual_seed(0)
    x = torch.rand(1, 12, 64, 64)

    a = net(x)

    # export torchscript
    mod = torch.jit.trace(net, x)
    mod.save("test_torchvision_DeformConv2d.pt")

    # torchscript to pnnx
    import os
    #os.system("../src/pnnx test_torchvision_DeformConv2d.pt inputshape=[1,12,64,64] customop=/home/nihui/osd/vision/build/install/lib64/libtorchvision.so")
    #os.system("../src/pnnx test_torchvision_DeformConv2d.pt inputshape=[1,12,64,64]")
    #os.system("../src/pnnx test_torchvision_DeformConv2d.pt inputshape=[1,12,64,64] customop=/home/nihui/.local/lib/python3.9/site-packages/torchvision/image.so")
    #os.system("../src/pnnx test_torchvision_DeformConv2d.pt inputshape=[1,12,64,64] customop=/home/nihui/.local/lib/python3.9/site-packages/torchvision/_C.so")

    # pnnx inference
    import test_torchvision_DeformConv2d_pnnx
    b = test_torchvision_DeformConv2d_pnnx.test_inference()

    return torch.equal(a, b)

if __name__ == "__main__":
    if test():
        exit(0)
    else:
        exit(1)
