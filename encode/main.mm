#import <Foundation/Foundation.h>

#import <zstd.h>

#import "Filter.h"
#import "QTZPNG.h"


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomma"
#pragma clang diagnostic ignored "-Wunused-function"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
namespace stb_image {
    #import "stb_image.h"
}
#pragma clang diagnostic pop


int main(int argc, const char * argv[]) {
        
    int w = 1920;
    int h = 1080;

    Filter::Encoder *encoder = new Filter::Encoder(w,h,Filter::Color::RGBA);

    unsigned int *src = new unsigned int[w*h];
    for(int i=0; i<h; i++) {
        for(int j=0; j<w; j++) {
            src[i*w+j] = 0xFFFF0000; // ABGR
        }
    }
    
    QTZPNGRecorder *recorder = new QTZPNGRecorder(w,h,30,8*encoder->bpp(),@"./test.mov");
    
    double _then = CFAbsoluteTimeGetCurrent();
    
    encoder->encode((unsigned char *)src,Filter::Color::RGBA,Filter::Type::Sub);
    unsigned char *dst = new unsigned char[encoder->length()];
    size_t len = ZSTD_compress(dst,(w+1)*h*encoder->bpp(),encoder->bytes(),encoder->length(),1);
    recorder->add(dst,(unsigned int)len);
    recorder->add(dst,(unsigned int)len);
    recorder->add(dst,(unsigned int)len);
    recorder->save();
    
    double current = CFAbsoluteTimeGetCurrent();
    NSLog(@"%zu",len);
    NSLog(@"%f",current-_then);
    
    delete[] dst;
    delete[] src;
    
    return 0;
}
