#include <zstd.h>
#include "Filter.h"

Filter *filter = nullptr;

extern "C" {
    void decode(unsigned char *dst,int w, int h,unsigned char *src,unsigned int length) {
        if(filter==nullptr) filter = new SubtractGreenFilter(w,h);
        ZSTD_decompress(dst,(w*h)<<2,src,length);
        filter->add((unsigned int *)dst);
    }
}
