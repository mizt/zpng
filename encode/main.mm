#import <Foundation/Foundation.h>

#import <zstd.h>

#import "Filter.h"
#import "QTZPNG.h"

int main(int argc, const char * argv[]) {
    
    int w = 1920;
    int h = 1080;
    
    Filter *filter = new Filter(w,h,Filter::RGB);
  
    unsigned int *src = new unsigned int[w*h];
    for(int i=0; i<h; i++) {
        for(int j=0; j<w; j++) {
            src[i*w+j] = 0xFFFF0000; // ABGR
        }
    }
    
    QTZPNGRecorder *recorder = new QTZPNGRecorder(w,h,30,8*filter->bpp(),@"./test.mov");
    filter->encode((unsigned char *)src,4,5);
    unsigned char *dst = new unsigned char[filter->length()];
    size_t len = ZSTD_compress(dst,w*h*filter->bpp(),filter->bytes(),filter->length(),1);
    recorder->add(dst,(unsigned int)len);
    recorder->add(dst,(unsigned int)len);
    recorder->add(dst,(unsigned int)len);
    recorder->save();
    
    delete[] dst;
    delete[] src;

    return 0;
}
