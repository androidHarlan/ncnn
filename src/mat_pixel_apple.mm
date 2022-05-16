#if __APPLE__
#include "mat.h"
#import <Accelerate/Accelerate.h>

namespace ncnn {
    Mat Mat::from_apple_samplebuffer(CMSampleBufferRef samplebuffer){
        CMFormatDescriptionRef des = CMSampleBufferGetFormatDescription(samplebuffer);
        if(!des){
            return Mat();
        }
        if(CMFormatDescriptionGetMediaType(des)!=kCMMediaType_Video){
            return Mat();
        }
        CVPixelBufferRef pixel = CMSampleBufferGetImageBuffer(samplebuffer);
        return Mat::from_apple_pixelbuffer(pixel);
    }

    Mat Mat::from_apple_pixelbuffer(CVPixelBufferRef pixelbuffer){
        OSType format = CVPixelBufferGetPixelFormatType(pixelbuffer);
        if(format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange){
            size_t h = CVPixelBufferGetHeight(pixelbuffer);
            size_t w = CVPixelBufferGetWidth(pixelbuffer);
            vImage_Buffer a;
            if (vImageBuffer_Init(&a, h, w, 32, kvImageNoFlags)!=kvImageNoError) {
                return Mat();
            }
            vImage_YpCbCrPixelRange pixelRange = {16, 128, 265, 240, 235, 16, 240, 16};
            vImage_YpCbCrToARGB matrix;
            if (vImageConvert_YpCbCrToARGB_GenerateConversion(kvImage_YpCbCrToARGBMatrix_ITU_R_601_4,&pixelRange,&matrix,kvImage420Yp8_CbCr8,kvImageARGB8888,0)!=kvImageNoError) {
                return Mat();
            }
            uint8_t[] map = [1,2,3,0]; 
            Mat mat = Mat(w,h,1,4);
            if(CVPixelBufferLockBaseAddress(pixelbuffer, kCVPixelBufferLock_ReadOnly) != COREVIDEO_TRUE){
                free(a);
                mat.release();
                return Mat();
            }
            void* yData = CVPixelBufferGetBaseAddressOfPlane(pixelbuffer,0);
            void* uvData = CVPixelBufferGetBaseAddressOfPlane(pixelbuffer,1);
            vImage_Buffer yBuff = {yData,h,w,w};
            vImage_Buffer uvBuff = {uvData,h/2,w/2,w};
            if(vImageConvert_420Yp8_CbCr8ToARGB8888(&yBuff,&uvBuff,&a,&matrix,map,1,kvImageNoFlags)!=kvImageNoError) {
                free(a);
                mat.release();
                free(yBuff);
                free(uvBuff);
                return Mat();
            }
            memcpy(mat.data,a.data,w*h*4);
            if(CVPixelBufferUnlockBaseAddress(pixelbuffer, kCVPixelBufferLock_ReadOnly) != COREVIDEO_TRUE){
                free(a);
                mat.release();
                CVPixelBufferUnlockBaseAddress(pixelbuffer, kCVPixelBufferLock_ReadOnly);
                free(yBuff);
                free(uvBuff);
                return Mat();
            }
            return mat;
        }else if(format == kCVPixelFormatType_32RGBA){
            size_t h = CVPixelBufferGetHeight(pixelbuffer);
            size_t w = CVPixelBufferGetWidth(pixelbuffer);
            Mat mat = Mat(w,h,1,4);
            if(CVPixelBufferLockBaseAddress(pixelbuffer, kCVPixelBufferLock_ReadOnly) != COREVIDEO_TRUE){
                return Mat();
            }
            void* pd = CVPixelBufferGetBaseAddress(pixelbuffer);
            memcpy(mat.data,pd,w*h*4);
            if(CVPixelBufferUnlockBaseAddress(pixelbuffer, kCVPixelBufferLock_ReadOnly) != COREVIDEO_TRUE){
                return Mat();
            }
            return mat;
        }else{
            return Mat();
        }
    }
    int Mat::to_apple_pixelbuffer(CVPixelBufferRef* pixelbuffer){
        if(dims == 1){
            return -1;
        }else if(dims == 2){
            if(elempack==4){
                if(elemsize==1){
                    if(CVPixelBufferCreateWithBytes(NULL,w,h,kCVPixelFormatType_32RGBA,data,w*4,NULL,NULL,NULL,pixelbuffer)==COREVIDEO_TRUE){
                        return 0;
                    }else{
                        return -1;
                    }
                }else if(elemsize==2){
                    if(CVPixelBufferCreateWithBytes(NULL,w,h,kCVPixelFormatType_64RGBAHalf,data,w*8,NULL,NULL,NULL,pixelbuffer)==COREVIDEO_TRUE){
                        return 0;
                    }else{
                        return -1;
                    }
                }else if(elemsize==4){
                    if(CVPixelBufferCreateWithBytes(NULL,w,h,kCVPixelFormatType_128RGBAFloat,data,w*16,NULL,NULL,NULL,pixelbuffer)==COREVIDEO_TRUE){
                        return 0;
                    }else{
                        return -1;
                    }
                }else{
                    return -1;
                }
            }else if(elempack==1){
                if(elemsize==1){
                    if(CVPixelBufferCreateWithBytes(NULL,w,h,kCVPixelFormatType_OneComponent8,data,w,NULL,NULL,NULL,pixelbuffer)==COREVIDEO_TRUE){
                        return 0;
                    }else{
                        return -1;
                    }
                }else if(elemsize==2){
                    if(CVPixelBufferCreateWithBytes(NULL,w,h,kCVPixelFormatType_OneComponent16Half,data,w*2,NULL,NULL,NULL,pixelbuffer)==COREVIDEO_TRUE){
                        return 0;
                    }else{
                        return -1;
                    }
                }else if(elemsize==4){
                    if(CVPixelBufferCreateWithBytes(NULL,w,h,kCVPixelFormatType_OneComponent32Float,data,w*4,NULL,NULL,NULL,pixelbuffer)==COREVIDEO_TRUE){
                        return 0;
                    }else{
                        return -1;
                    }
                }else{
                    return -1;
                }
            }else{
                return -1;
            }
        }else if(dims==3){
            if(elempack!=1){
                return -1;
            }
        }else{
            return -1;
        }
    }
}
#endif
