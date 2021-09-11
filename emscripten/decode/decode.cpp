#include <zstd.h>
#include <math.h>
#include "Filter.h"

Filter::Decoder *decoder = nullptr;

extern "C" {
    void decode(unsigned char *dst,int w, int h, int bpp, unsigned char *src, unsigned int length) {
        if(decoder==nullptr) decoder = new Filter::Decoder(w,h,Filter::RGBA);
        ZSTD_decompress(dst,(w*h)*4,src,length);
        decoder->decode(dst,bpp);
        unsigned int *p = (unsigned int *)decoder->bytes();
        unsigned int *q = (unsigned int *)dst;
        for(int k=0; k<w*h; k++) q[k] = p[k];
    }
}
