// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
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

#import <Cocoa/Cocoa.h>
#import <CoreVideo/CoreVideo.h>
#include "testutil.h"

int main(){
    @autoreleasepool{
        NSImage* image = [[NSImage alloc] initByReferencingURL:[NSURL URLWithString:@"https://raw.githubusercontent.com/Tencent/ncnn/master/images/16-ncnn.png"]];
        ncnn::Mat m = ncnn::Mat::from_apple_image(image);
        if(m.w!=16){
            printf("function Mat::from_apple_image test failed %d",m.w);
            return -1;
        }
        CVPixelBufferRef pixelbuffer;
        int ret = m.to_apple_pixelbuffer(&pixelbuffer);
        if(ret != 0){
            printf("function Mat::to_apple_pixelbuffer test failed");
            return ret;
        }
        ncnn::Mat m2 = ncnn::Mat::from_apple_pixelbuffer(pixelbuffer);
        if(m2.w!=16){
            printf("function Mat::from_apple_image test failed");
            return -1;
        }
        NSImage* img = m2.to_apple_image();
        if(!img){
            printf("function Mat::from_apple_image test failed");
            return -1;
        }
    }
}