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

class Model(nn.Module):
    def __init__(self):
        super(Model, self).__init__()

    def forward(self, x, w0, w1, b1):
        x = F.conv3d(x, w0, None, stride=(2,2,2), padding=(1,0,1))
        if torch.__version__ < '1.9':
            x = F.conv3d(x, w1, b1, stride=(1,1,1), padding=(1,1,1), dilation=(2,2,1), groups=2)
        else:
            x = F.conv3d(x, w1, b1, stride=(1,1,1), padding='same', dilation=(2,2,1), groups=2)
        return x

def test():
    net = Model()
    net.eval()

    torch.manual_seed(0)
    x = torch.rand(1, 12, 20, 32, 40)
    w0 = torch.rand(16, 12, 3, 2, 3)
    w1 = torch.rand(16, 8, 5, 4, 5)
    b1 = torch.rand(16)

    a = net(x, w0, w1, b1)

    # export torchscript
    mod = torch.jit.trace(net, (x, w0, w1, b1))
    mod.save("test_F_conv3d.pt")

    # torchscript to pnnx
    import os
    os.system("../src/pnnx test_F_conv3d.pt inputshape=[1,12,20,32,40],[16,12,3,2,3],[16,8,5,4,5],[16]")

    # pnnx inference
    import test_F_conv3d_pnnx
    b = test_F_conv3d_pnnx.test_inference()

    return torch.equal(a, b)

if __name__ == "__main__":
    if test():
        exit(0)
    else:
        exit(1)
