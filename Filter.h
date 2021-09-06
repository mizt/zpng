class Filter {
  
    protected:
    
        NSString *FILTER[5] = {
            @"None",
            @"Sub",
            @"Up",
            @"Average",
            @"Paeth"
        };

        bool _subtractGreen = true;
    
        unsigned int _width;
        unsigned int _height;
        unsigned int _bpp;
    
        unsigned char *_bytes = nullptr;
        unsigned int _length = 0;
    
        unsigned char black[4] = {0,0,0,0};
    
        unsigned char *_filters[5] = {nullptr,nullptr,nullptr,nullptr,nullptr};

        inline unsigned char predictor(unsigned char a, unsigned char b, unsigned char c) {
             int p = a+b-c;
             int pa = abs(p-a);
             int pb = abs(p-b);
             int pc = abs(p-c);
             return (pa<=pb&&pa<=pc)?a:((pb<=pc)?b:c);
         }
    
    public:
        
        const static int RGB = 3;
        const static int RGBA = 4;
    
        unsigned int width() { return this->_width; }
        unsigned int height() { return this->_height; }
        unsigned int bpp() { return this->_bpp; }
        
        unsigned char *bytes() { return this->_bytes; }
        unsigned int length() { return this->_length; }
    
        Filter(unsigned int w,unsigned int h,unsigned int ch, int subtractGreen=false) {
            this->_subtractGreen = subtractGreen;
            this->_width = w;
            this->_height = h;
            this->_bpp = ch;
            this->_length = this->_height*(this->_width+1)*this->_bpp;
            this->_bytes = new unsigned char[this->_length];
            for(int f=1; f<5; f++) this->_filters[f] = new unsigned char[this->_width*this->_bpp];
        }
    
        void encode(unsigned char *buffer,int ch,unsigned int filter) {
            
            this->_length = this->_height*(this->_width+1)*this->_bpp;
            
            unsigned int src_row = this->_width*ch;
            
            if(this->_subtractGreen) {
                for(int i=0; i<this->_height; i++) {
                    unsigned char *pixel = buffer+i*src_row;
                    for(int j=0; j<this->_width; j++) {
                        unsigned char g = *(pixel+1);
                        *(pixel) = (*(pixel)-g)&0xFF;
                        *(pixel+2) = (*(pixel+2)-g)&0xFF;
                        pixel+=this->_bpp;
                    }
                }
            }
            
            unsigned int dst_row = this->_width*this->_bpp;
            
            int num = (this->_bpp>ch)?ch:this->_bpp;

            for(int i=0; i<this->_height; i++) {
                
                unsigned char *src = buffer+i*src_row;
                unsigned char *dst = this->_bytes+i*(dst_row+1)+1;
                
                unsigned int _filter = filter;
                
                if(_filter==1) {
                    for(int j=0; j<this->_width; j++) {
                        unsigned char *W = (j==0)?black:buffer+i*src_row+(j-1)*ch;
                        for(int n=0; n<num; n++) *dst++ = ((*src++)-(*W++))&0xFF;
                    }
                }
                else if(_filter==2) {
                    for(int j=0; j<this->_width; j++) {
                        unsigned char *N = (i==0)?black:buffer+(i-1)*src_row+j*ch;
                        for(int n=0; n<num; n++) *dst++ = ((*src++)-(*N++))&0xFF;
                    }
                }
                else if(_filter==3) {
                    for(int j=0; j<this->_width; j++) {
                        unsigned char *W = (j==0)?black:buffer+i*src_row+(j-1)*ch;
                        unsigned char *N = (i==0)?black:buffer+(i-1)*src_row+j*ch;
                        for(int n=0; n<num; n++) *dst++ = ((*src++)-(((*W++)+(*N++))>>1))&0xFF;
                    }
                }
                else if(_filter==4) {
                    for(int j=0; j<this->_width; j++) {
                        unsigned char *W = (j==0)?black:buffer+i*src_row+(j-1)*ch;
                        unsigned char *N = (i==0)?black:buffer+(i-1)*src_row+j*ch;
                        unsigned char *NW = (i==0||j==0)?black:buffer+(i-1)*src_row+(j-1)*ch;
                        for(int n=0; n<num; n++) *dst++ = ((*src++)-predictor((*W++),(*N++),(*NW++)))&0xFF;
                    }
                }
                else if(_filter==5) {
                    for(int j=0; j<this->_width; j++) {
                        unsigned char *W = (j==0)?black:buffer+i*src_row+(j-1)*ch;
                        unsigned char *N = (i==0)?black:buffer+(i-1)*src_row+j*ch;
                        unsigned char *NW = (i==0||j==0)?black:buffer+(i-1)*src_row+(j-1)*ch;
                        for(int n=0; n<num; n++) {
                            this->_filters[1][j*this->_bpp+n] = (*src-*W)&0xFF;
                            this->_filters[2][j*this->_bpp+n] = (*src-*N)&0xFF;
                            this->_filters[3][j*this->_bpp+n] = (*src-((*W+*N)>>1))&0xFF;
                            this->_filters[4][j*this->_bpp+n] = (*src-predictor(*W,*N,*NW))&0xFF;
                            src++;
                            W++;
                            N++;
                            NW++;
                        }
                        if(this->_bpp<ch) src++;
                        else if(this->_bpp>ch) *dst++ = 0xFF;
                    }
                    
                    const int INDEX = 0, VALUE = 1;
                    int best_filter[2] = { 1, 0x7FFFFFFF };
                    for(int f=1; f<5; f++) {
                        for(int n=0; n<num; n++) {
                            int est = 0;
                            for(int j=0; j<this->_width; j++) {
                               est+=abs((char)this->_filters[f][j*this->_bpp+n]);
                            }
                            if(est<best_filter[VALUE]) {
                                best_filter[INDEX] = f;
                                best_filter[VALUE] = est;
                            }
                        }
                    }
                    _filter = best_filter[INDEX];
                    //NSLog(@"%@",FILTER[_filter]);
                    
                    for(int j=0; j<this->_width; j++) {
                        for(int n=0; n<num; n++) {
                            *dst++ = this->_filters[_filter][j*this->_bpp+n];
                        }
                    }
                }
                else {
                    for(int j=0; j<this->_width; j++) {
                        for(int n=0; n<num; n++) *dst++ = *src++;
                    }
                }
                
                if(this->_bpp<ch) src++;
                else if(this->_bpp>ch) *dst++ = 0xFF;
            
                this->_bytes[i*(this->_width*this->_bpp+1)+0] = _filter;
        
            
                if(this->_subtractGreen) {
                
                }
            }
        }
        
        void decode(unsigned char *buffer, int ch) {
            
            unsigned int src_row = this->_width*ch;
            unsigned int dst_row = this->_width*this->_bpp;
            
            this->_length = this->_height*dst_row;
            
            int num = (this->_bpp>ch)?ch:this->_bpp;

            for(int i=0; i<this->_height; i++) {
                
                unsigned char *src = buffer+i*(src_row+1);
                unsigned char *dst = this->_bytes+i*dst_row;

                unsigned int filter = *src++;
                 
                if(filter==1) {
                    for(int j=0; j<this->_width; j++) {
                        unsigned char *W = (j==0)?black:this->_bytes+i*dst_row+(j-1)*this->_bpp;
                        for(int n=0; n<num; n++) *dst++ = ((*src++)+(*W++))&0xFF;
                    }
                }
                else if(filter==2) {
                    for(int j=0; j<this->_width; j++) {
                        unsigned char *N = (i==0)?black:this->_bytes+(i-1)*dst_row+j*this->_bpp;
                        for(int n=0; n<num; n++) *dst++ = ((*src++)+(*N++))&0xFF;
                    }
                }
                else if(filter==3) {
                    for(int j=0; j<this->_width; j++) {
                        unsigned char *W = (j==0)?black:this->_bytes+i*dst_row+(j-1)*this->_bpp;
                        unsigned char *N = (i==0)?black:this->_bytes+(i-1)*dst_row+j*this->_bpp;
                        for(int n=0; n<num; n++) *dst++ = ((*src++)+(((*W++)+(*N++))>>1))&0xFF;
                    }
                }
                else if(filter==4) {
                    for(int j=0; j<this->_width; j++) {
                        unsigned char *W = (j==0)?black:this->_bytes+i*dst_row+(j-1)*this->_bpp;
                        unsigned char *N = (i==0)?black:this->_bytes+(i-1)*dst_row+j*this->_bpp;
                        unsigned char *NW = (i==0||j==0)?black:this->_bytes+(i-1)*dst_row+(j-1)*this->_bpp;
                        for(int n=0; n<num; n++) *dst++ = ((*src++)+predictor((*W++),(*N++),(*NW++)))&0xFF;
                    }
                }
                else { // 0
                    for(int j=0; j<this->_width; j++) {
                        for(int n=0; n<num; n++) *dst++ = *src++;
                    }
                }
                
                if(this->_bpp<ch) src++;
                else if(this->_bpp>ch) *dst++ = 0xFF;
            }
            
            if(this->_subtractGreen) {
                if(this->_subtractGreen) {
                    for(int i=0; i<this->_height; i++) {
                        unsigned char *pixel = this->_bytes+i*src_row;
                        for(int j=0; j<this->_width; j++) {
                            unsigned char g = *(pixel+1);
                            *(pixel) = (*(pixel)+g)&0xFF;
                            *(pixel+2) = (*(pixel+2)+g)&0xFF;
                            pixel+=this->_bpp;
                        }
                    }
                }
                
            }
        }
        
        ~Filter() {
            for(int f=1; f<5; f++) {
                delete[] this->_filters[f];
            }
            delete[] this->_bytes;
        }
};

