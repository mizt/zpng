#import <Cocoa/Cocoa.h>
#import <Quartz/Quartz.h>
#import "Filter.h"
#import "QTZPNG.h"
#import <zstd.h>

class PreviewItem {
    
    private:
        
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
      
    public:
        
        PreviewItem(NSView *view,NSURL *url) {
            
            this->url = url;
            this->fp = fopen(this->url.fileSystemRepresentation,"rb");
            
            if(this->fp) {
                
                this->parser = new QTZPNGParser(this->fp);
                
                if(this->parser->length()>0&&this->parser->FPS()>0) {
                    this->width = this->parser->width();
                    this->height = this->parser->height();
                    
                    this->buffer = new unsigned char[this->width*this->height*4+this->height];
                    this->src = new unsigned char[this->width*this->height*4];

                    this->decoder = new Filter::Decoder(this->width,this->height,4);

                    this->view = [[NSImageView alloc] initWithFrame:CGRectMake(0,0,view.frame.size.width,view.frame.size.height)];

                    this->bmp = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:this->width pixelsHigh:this->height bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bitmapFormat:NSBitmapFormatAlphaNonpremultiplied bytesPerRow:NULL bitsPerPixel:NULL];
                    this->img = [[NSImage alloc] initWithSize:NSMakeSize(this->width,this->height)];
                    [this->img addRepresentation:this->bmp];
                    [this->view setImage:this->img];
                    [this->view setImageScaling:NSImageScaleProportionallyDown];
                    [view addSubview:this->view];
                    [this->view setAutoresizingMask:NSViewHeightSizable|NSViewWidthSizable];
                    
                    this->timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,0,0,dispatch_queue_create("ENTER_FRAME",0));
                   
                    dispatch_source_set_timer(this->timer,dispatch_time(0,0),(1.0/(double)this->parser->FPS())*1000000000,0);
                    dispatch_source_set_event_handler(this->timer,^{
                        dispatch_async(dispatch_get_main_queue(),^{
                                                       
                            if(this->view) {
                                         
                                if(this->parser->length()>0) {
                                    
                                    FILE *fp = fopen(this->url.fileSystemRepresentation,"rb");
                                    fpos_t size = 0;
                                    fseeko(fp,0,SEEK_END);
                                    fgetpos(fp,&size);
                                    fclose(fp);
                                    
                                    if(size==this->parser->size()) {
                                        
                                        std::pair<unsigned int,unsigned int> data = this->parser->frame(this->frames);
                                        fseeko(this->fp,data.first,SEEK_SET);
                                        fread(this->buffer,sizeof(unsigned char),data.second,this->fp);
                                        ZSTD_decompress(this->src,(this->width*this->height+this->height)*4,this->buffer,data.second);
                                        this->decoder->decode(this->src,this->parser->depth()>>3);
                                        
                                        for(int i=0; i<this->height; i++) {
                                            unsigned int *src = (unsigned int *)(this->decoder->bytes()+i*this->width*4);
                                            unsigned int *dst = ((unsigned int *)[this->bmp bitmapData]+i*([this->bmp bytesPerRow]>>2));
                                            for(int j=0; j<this->width; j++) {
                                                *dst++ =  *src++;
                                            }
                                        }
                                        
                                        this->frames++;
                                        if(this->frames>=this->parser->length()) {
                                            this->frames=0;
                                        }
                                        
                                    }
                                    else {
                                        for(int i=0; i<this->height; i++) {
                                            unsigned int *dst = ((unsigned int *)[this->bmp bitmapData]+i*([this->bmp bytesPerRow]>>2));
                                            for(int j=0; j<this->width; j++) {
                                                *dst++ = 0xFFFF0000;
                                            }
                                        }
                                    }
                                    
                                    [this->img addRepresentation:this->bmp];
                                    [this->view display];
                                }
                                else {
                                    
                                    for(int i=0; i<this->height; i++) {
                                        unsigned int *dst = ((unsigned int *)[this->bmp bitmapData]+i*([this->bmp bytesPerRow]>>2));
                                        for(int j=0; j<this->width; j++) {
                                            *dst++ = 0xFFFF0000;
                                        }
                                    }
                                    
                                    [this->img addRepresentation:this->bmp];
                                    [this->view display];
                                }
                            }
                          
                        });
                    });
                    if(this->timer) dispatch_resume(this->timer);
                }
            }
        }
    
        ~PreviewItem() {
            
            if(this->fp) {
                fclose(this->fp);
                this->fp = nullptr;
            }
            
            if(this->timer) {
                dispatch_source_cancel(this->timer);
                this->timer = nullptr;
            }
            
            if(this->view) {
                [this->view removeFromSuperview];
                this->view = nil;
            }
            
            if(this->img) {
                if(this->bmp) {
                    [this->img removeRepresentation:this->bmp];
                    this->bmp = nil;
                }
                this->img = nil;
            }
                    
            if(this->decoder) {
                delete this->decoder;
                this->decoder = nullptr;
            }
            
            if(this->parser) {
                delete[] this->parser;
                this->parser = nullptr;
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
};

std::vector<PreviewItem *> zpng;

@interface PreviewViewController : NSViewController<QLPreviewingController> @end
@implementation PreviewViewController

-(NSString *)nibName {
    return @"PreviewViewController";
}

-(void)viewDidDisappear {
    
    delete zpng[0];
    zpng.erase(zpng.begin());

    [super viewDidDisappear];
}

-(void)loadView {
    [super loadView];
}

-(void)preparePreviewOfFileAtURL:(NSURL *)url completionHandler:(void (^)(NSError * _Nullable))handler {
    
    PreviewItem *item = new PreviewItem(self.view,url);
    zpng.push_back(item);
    
    handler(nil);
}

@end
