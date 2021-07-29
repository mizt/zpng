#import <Foundation/Foundation.h>

#import <zstd.h>

#import "Filter.h"
#import "QTZPNG.h"

int main(int argc, const char * argv[]) {
    
    int w = 1920;
    int h = 1080;
    
    Filter *filter = new Filter(w,h);
  
    unsigned int *dst = new unsigned int[w*h];
    unsigned int *src = new unsigned int[w*h];
    for(int i=0; i<h; i++) {
        for(int j=0; j<w; j++) {
            src[i*w+j] = 0xFFFF0000; // ABGR
        }
    }
    
    QTZPNGRecorder *recorder = new QTZPNGRecorder(w,h,30,@"./test.mov");
    filter->sub(src);
    size_t len = ZSTD_compress(dst,(w*h)<<2,src,(w*h)<<2,1);
    recorder->add((unsigned char *)dst,(unsigned int)len);
    recorder->save();

    return 0;
}
