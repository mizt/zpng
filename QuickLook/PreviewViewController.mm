#import <Cocoa/Cocoa.h>
#import <Quartz/Quartz.h>
#import "Filter.h"
#import "QTZPNG.h"
#import <zstd.h>

namespace zpng {

    int width = 0;
    int height = 0;
    
    NSImageView *view = nil;
    NSImage *img;
    NSBitmapImageRep *bmp;

    dispatch_source_t timer = nullptr;

    int frames = 0;

    NSURL *url;
    FILE *fp = nullptr;

    unsigned char *buffer;
    unsigned char *src;

    Filter::Decoder *decoder = nullptr;
    QTZPNGParser *parser = nullptr;

    void resrt() {
        
        if(fp) {
            fclose(fp);
            fp = nullptr;
        }
        
        if(timer){
            dispatch_source_cancel(timer);
            timer = nullptr;
        }
        
        if(view) {
            [view removeFromSuperview];
            view = nil;
        }
        
        if(img) {
            if(bmp) {
                [img removeRepresentation:bmp];
                bmp = nil;
            }
            img = nil;
        }
                
        if(decoder) {
            delete decoder;
            decoder = nullptr;
        }
        
        if(parser) {
            delete[] parser;
            parser = nullptr;
        }
        
        if(buffer) {
            delete[] buffer;
            buffer = nullptr;
        }
        
        if(src) {
            delete[] src;
            src = nullptr;
        }
    }
}


@interface PreviewViewController : NSViewController<QLPreviewingController> @end
@implementation PreviewViewController

-(NSString *)nibName {
    return @"PreviewViewController";
}

-(void)viewDidDisappear {
    zpng::resrt();
}

-(void)loadView {
    [super loadView];
    zpng::resrt();
}

-(void)preparePreviewOfFileAtURL:(NSURL *)url completionHandler:(void (^)(NSError * _Nullable))handler {
    
    zpng::resrt();
    if(zpng::timer==nullptr) {
        
        zpng::url = url;
        zpng::fp = fopen(zpng::url.fileSystemRepresentation,"rb");
        
        if(zpng::fp) {
            
            zpng::parser = new QTZPNGParser(zpng::fp);

            zpng::width = zpng::parser->width();
            zpng::height = zpng::parser->height();
            
            zpng::buffer = new unsigned char[zpng::width*zpng::height*4+zpng::height];
            zpng::src = new unsigned char[zpng::width*zpng::height*4];

            zpng::decoder = new Filter::Decoder(zpng::width,zpng::height,4);

            zpng::view = [[NSImageView alloc] initWithFrame:CGRectMake(0,0,self.view.frame.size.width,self.view.frame.size.height)];

            zpng::bmp = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:zpng::width pixelsHigh:zpng::height bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bitmapFormat:NSBitmapFormatAlphaNonpremultiplied bytesPerRow:NULL bitsPerPixel:NULL];
            zpng::img = [[NSImage alloc] initWithSize:NSMakeSize(zpng::width,zpng::height)];
            [zpng::img addRepresentation:zpng::bmp];
            [zpng::view setImage:zpng::img];
            [zpng::view setImageScaling:NSImageScaleProportionallyDown];
            [self.view addSubview:zpng::view];
            [zpng::view setAutoresizingMask:NSViewHeightSizable|NSViewWidthSizable];
            
            zpng::timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,0,0,dispatch_queue_create("ENTER_FRAME",0));
            dispatch_source_set_timer(zpng::timer,dispatch_time(0,0),(1.0/(double)zpng::parser->FPS())*1000000000,0);
            dispatch_source_set_event_handler(zpng::timer,^{
                dispatch_async(dispatch_get_main_queue(),^{
                    if(zpng::view) {
                                 
                        if(zpng::parser->length()>=1) {
                            
                            std::pair<unsigned int,unsigned int> data = zpng::parser->frame(zpng::frames);

                            fseeko(zpng::fp,data.first,SEEK_SET);
                            fread(zpng::buffer,sizeof(unsigned char),data.second,zpng::fp);
                            ZSTD_decompress(zpng::src,(zpng::width*zpng::height+zpng::height)*4,zpng::buffer,data.second);
                            zpng::decoder->decode(zpng::src,zpng::parser->depth()>>3);
                            
                            for(int i=0; i<zpng::height; i++) {
                                unsigned int *src = (unsigned int *)(zpng::decoder->bytes()+i*zpng::width*4);
                                unsigned int *dst = ((unsigned int *)[zpng::bmp bitmapData]+i*([zpng::bmp bytesPerRow]>>2));
                                for(int j=0; j<zpng::width; j++) {
                                    *dst++ =  *src++;
                                }
                            }
                            
                            zpng::frames++;
                            if(zpng::frames>=zpng::parser->length()) {
                                zpng::frames=0;
                            }
                            
                        }
                        else {
                            for(int i=0; i<zpng::height; i++) {
                                unsigned int *dst = ((unsigned int *)[zpng::bmp bitmapData]+i*([zpng::bmp bytesPerRow]>>2));
                                for(int j=0; j<zpng::width; j++) {
                                    *dst++ = 0xFFFF0000;
                                }
                            }
                        }
                        
                        [zpng::img addRepresentation:zpng::bmp];
                        [zpng::view display];
                        
                    }
                });
            });
            if(zpng::timer) dispatch_resume(zpng::timer);
        }
    }
 
    handler(nil);
}

@end
