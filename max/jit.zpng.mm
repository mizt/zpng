#import <Cocoa/Cocoa.h>

#import "jit.common.h"
#import "max.jit.mop.h"

#import "zstd.h"
#import "Filter.h"
#import "QTZPNG.h"

class ZPNG {
    
    private:
    
        unsigned int _width = 1920;
        unsigned int _height = 1080;
        
        bool _loaded = false;
    
        NSMutableString *_path;
        NSFileManager *_fileManager = [NSFileManager defaultManager];
        FILE *_fp = nullptr;
    
        QTZPNGParser *_parser = nullptr;
        Filter::Decoder *_decoder = nullptr;
    
        unsigned char *_buffer = nullptr;
    
        unsigned int _frame = 0;
        
    public:
    
        static void fill(long *dim,t_jit_matrix_info *dst_minfo, char *bop) {
            for(long i=0; i<dst_minfo->dim[1]; i++) {
                uchar *dst = (uchar *)(bop+i*dst_minfo->dimstride[1]);
                for(long j=0; j<dst_minfo->dim[0]; j++) {
                    *dst++ = 0xFF;
                    *dst++ = 0;
                    *dst++ = 0;
                    *dst++ = 0xFF;
                }
            }
        }
    
        int width() { return this->_width; }
        int height() { return this->_height; }
    
        bool loaded() { return this->_loaded; }
        
        ZPNG() {}
    
        ~ZPNG() {
            if(this->_parser) delete this->_parser;
            if(this->_decoder) delete this->_decoder;
            if(this->_buffer) delete this->_buffer;
        }
    
        void load(NSString *path) {
            this->_loaded = false;
            this->_path = [NSMutableString stringWithString:path];
            if([this->_fileManager fileExistsAtPath:this->_path]) {
                this->_fp = fopen([this->_path UTF8String],"rb");
                if(this->_fp) {
                    if(this->_parser) delete this->_parser;
                    this->_parser = new QTZPNGParser(this->_fp);
                    if(this->_parser->length()>0) {
                        
                        if(this->_decoder) delete this->_decoder;
                        this->_decoder = new Filter::Decoder(this->_width,this->_height,Filter::RGBA);
                        
                        this->_buffer = new unsigned char[(this->_width*this->_height)*Filter::RGBA+this->_height];
                        
                        this->_width = this->_parser->width();
                        this->_height = this->_parser->height();
                        
                        this->_frame = 0;
                        this->_loaded = true;
                    }
                }
            }
        }
    
        void copy(long *dim,t_jit_matrix_info *dst_minfo, char *bop) {
            if(this->_loaded&&this->_parser->length()>0&&dim[0]==this->width()&&dim[1]==this->height()) {
                if(this->_fp&&[this->_fileManager fileExistsAtPath:this->_path]) {
                    this->_frame++;
                    if(this->_frame>=this->_parser->length()) this->_frame = 0;
                    NSData *zpng = this->_parser->get(this->_frame);
                    if(zpng) {
                        ZSTD_decompress(this->_buffer,(this->_width*this->_height)*Filter::RGBA+this->_height,[zpng bytes],[zpng length]);
                        this->_decoder->decode(this->_buffer,this->_parser->depth()>>3);
                        for(long i=0; i<this->_height; i++) {
                            uchar *src = this->_decoder->bytes()+i*this->_width*Filter::RGBA;
                            uchar *dst = (uchar *)(bop+i*dst_minfo->dimstride[1]);
                            for(long j=0; j<this->_width; j++) {
                                unsigned char b = *src++;
                                unsigned char g = *src++;
                                unsigned char r = *src++;
                                unsigned char a = *src++;
                                *dst++ = a;
                                *dst++ = b;
                                *dst++ = g;
                                *dst++ = r;
                            }
                        }
                        zpng = nil;
                    }
                    else {
                        ZPNG::fill(dim,dst_minfo,bop);
                    }
                }
            }
            else {
                ZPNG::fill(dim,dst_minfo,bop);
            }
        }
};

typedef struct _jit_zpng {
	t_object ob;
    ZPNG *zpng;
} t_jit_zpng;

typedef struct _max_jit_zpng {
    t_object ob;
    void *obex;
} t_max_jit_zpng;

void *_jit_zpng_class;
t_messlist *max_jit_zpng_class;

t_jit_err jit_zpng_init(void);
t_jit_zpng *jit_zpng_new(void);
void jit_zpng_free(t_jit_zpng *x);
t_jit_err jit_zpng_matrix_calc(t_jit_zpng *x, void *inputs, void *outputs);
void jit_zpng_read(t_jit_zpng *x, t_symbol *s);

void *max_jit_zpng_new(t_symbol *s, long argc, t_atom *argv);
void max_jit_zpng_free(t_max_jit_zpng *x);
void max_jit_zpng_outputmatrix(t_max_jit_zpng *x);

t_jit_err jit_zpng_init() {
    
	_jit_zpng_class = jit_class_new("jit_zpng",(method)jit_zpng_new,(method)jit_zpng_free,sizeof(t_jit_zpng),0L);

    t_jit_object *mop = (t_jit_object *)jit_object_new(_jit_sym_jit_mop,0,1);
    jit_class_addadornment(_jit_zpng_class,mop);
    jit_mop_single_type(mop,_jit_sym_char);
    jit_mop_single_planecount(mop,4);
    jit_class_addmethod(_jit_zpng_class, (method)jit_zpng_matrix_calc,"matrix_calc",A_CANT,0L);
    jit_class_addmethod(_jit_zpng_class,(method)jit_zpng_read,"read",A_DEFSYM,0L);
	jit_class_register(_jit_zpng_class);
    
	return JIT_ERR_NONE;
}

t_jit_err jit_zpng_matrix_calc(t_jit_zpng *x, void *inputs, void *outputs) {
	t_jit_err err=JIT_ERR_NONE;
	void *out_matrix = jit_object_method(outputs,_jit_sym_getindex,0);
    long out_savelock;
	if(x&&out_matrix) {
        out_savelock = (long) jit_object_method(out_matrix,_jit_sym_lock,1);
        t_jit_matrix_info out_minfo;
		jit_object_method(out_matrix,_jit_sym_getinfo,&out_minfo);
        out_minfo.dimcount = 2;
        if(out_minfo.dim[0]!= x->zpng->width()||out_minfo.dim[1]!= x->zpng->height()) {
            out_minfo.dim[0] = x->zpng->width();
            out_minfo.dim[1] = x->zpng->height();
        }
        out_minfo.type = gensym("char");
        out_minfo.planecount = 4;
        jit_object_method(out_matrix,_jit_sym_setinfo,&out_minfo);
        jit_object_method(out_matrix,_jit_sym_getinfo,&out_minfo);
        char *out_bp;
        jit_object_method(out_matrix,_jit_sym_getdata,&out_bp);
        if(!out_bp) { err=JIT_ERR_INVALID_OUTPUT; goto out; }
        if(x->zpng->loaded()) {
            if(x->zpng) x->zpng->copy(out_minfo.dim,&out_minfo,out_bp);
        }
        else {
            ZPNG::fill(out_minfo.dim,&out_minfo,out_bp);
        }
	}
    else {
		return JIT_ERR_INVALID_PTR;
	}

out:
	jit_object_method(out_matrix,_jit_sym_lock,out_savelock);
	return err;
}

t_jit_zpng *jit_zpng_new() {
    t_jit_zpng *x = (t_jit_zpng *)jit_object_alloc(_jit_zpng_class);
    if(x) {
        x->zpng = new ZPNG();
    }
    else {
        x = NULL;
    }
	return x;
}

void jit_zpng_read(t_jit_zpng *x, t_symbol *s) {
    if(s==gensym("")) {
        NSOpenPanel* openPanel= [NSOpenPanel openPanel];
        [openPanel setAllowsMultipleSelection:NO];
        [openPanel setCanChooseDirectories:NO];
        [openPanel setCanCreateDirectories:NO];
        [openPanel setCanChooseFiles:YES];
        [openPanel setAllowedFileTypes:[NSArray arrayWithObjects:@"zpng",nil]];
        [openPanel beginWithCompletionHandler:^(NSInteger result) {
             if(result==NSFileHandlingPanelOKButton) {
                 NSURL *url = [openPanel URLs][0];
                 x->zpng->load([url path]);
             }
        }];
    }
    else {
        NSString *filename = [NSString stringWithCString:s->s_name encoding:NSASCIIStringEncoding];
        if([[filename pathExtension] compare:@"zpng"]==NSOrderedSame) {
            x->zpng->load(filename);
        }
    }
}

void jit_zpng_free(t_jit_zpng *x) {
    delete x->zpng;
}

C74_EXPORT void ext_main(void *r) {
    jit_zpng_init();
    setup(&max_jit_zpng_class, (method)max_jit_zpng_new, (method)max_jit_zpng_free, (short)sizeof(t_max_jit_zpng),
          0L, A_GIMME, 0);
    void *p = max_jit_classex_setup(calcoffset(t_max_jit_zpng,obex));
    void *q = jit_class_findbyname(gensym("jit_zpng"));
    max_jit_classex_mop_wrap(p,q,MAX_JIT_MOP_FLAGS_OWN_OUTPUTMATRIX|MAX_JIT_MOP_FLAGS_OWN_JIT_MATRIX);
    max_jit_classex_standard_wrap(p,q,0);
    max_addmethod_usurp_low((method)max_jit_zpng_outputmatrix,"outputmatrix");
    addmess((method)max_jit_mop_assist,"assist",A_CANT,0);
}

void max_jit_zpng_outputmatrix(t_max_jit_zpng *x) {
    long outputmode=max_jit_mop_getoutputmode(x);
    void *mop=max_jit_obex_adornment_get(x,_jit_sym_jit_mop);
    t_jit_err err;
    if(outputmode&&mop) {
        if(outputmode==1) {
            t_jit_err  err=(t_jit_err)jit_object_method(max_jit_obex_jitob_get(x),_jit_sym_matrix_calc,jit_object_method(mop,_jit_sym_getinputlist),jit_object_method(mop,_jit_sym_getoutputlist));
            if(err) {
                jit_error_code(x,err);
            }
            else {
                max_jit_mop_outputmatrix(x);
            }
        }
        else {
            max_jit_mop_outputmatrix(x);
        }
    }
}

void max_jit_zpng_free(t_max_jit_zpng *x) {
    max_jit_mop_free(x);
    jit_object_free(max_jit_obex_jitob_get(x));
    max_jit_obex_free(x);
}

void *max_jit_zpng_new(t_symbol *s, long argc, t_atom *argv) {
    t_max_jit_zpng *x = (t_max_jit_zpng *)max_jit_obex_new(max_jit_zpng_class,gensym("jit_zpng"));
    if(x) {
        void *o = jit_object_new(gensym("jit_zpng"));
        if(o) {
            max_jit_mop_setup_simple(x,o,argc,argv);
            max_jit_attr_args(x,argc,argv);
        }
        else {
            jit_object_error((t_object *)x,"jit.zpng: could not allocate object");
            freeobject((t_object *)x);
            x = NULL;
        }
    }
    return (x);
}
