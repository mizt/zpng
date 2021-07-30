class Filter {
  
    private:
    
        unsigned int _width;
        unsigned int _height;
    
        unsigned char *V[4] = {nullptr,nullptr,nullptr,nullptr};

    public:
        
        unsigned int width() { return this->_width; }
        unsigned int height() { return this->_height; }
        
        Filter(int w,int h) {
            this->_width = w;
            this->_height = h;
            for(int n=0; n<4; n++) {
                this->V[n] = new unsigned char[w*h]{0};
            }
        }
    
        void sub(unsigned int *src) {
            int w = this->width();
            int h = this->height();
            for(int i=0; i<h; i++) {
                unsigned char c[4] = {0,0,0,0};
                for(int j=0; j<w; j++) {
                    int addr = i*w+j;
                    unsigned int p = src[addr];
                    for(int n=0; n<4; n++) {
                        int v = (p>>((3-n)<<3))&0xFF;
                        this->V[n][addr] = (v-c[n])&0xFF;
                        c[n] = v;
                    }
                }
            }
            unsigned char *p = (unsigned char *)src;
            for(int n=0; n<4; n++) {
                unsigned char *q = this->V[n];
                for(int k=0; k<w*h; k++) *p++ = *q++;
            }
        }
        
        void add(unsigned int *src) {
            int w = this->width();
            int h = this->height();
            unsigned char *p = (unsigned char *)src;
            for(int n=0; n<4; n++) {
                unsigned char *q = this->V[n];
                for(int k=0; k<w*h; k++) *q++ = *p++;
            }
            unsigned char v[4] = {0,0,0,0};
            for(int i=0; i<h; i++) {
                unsigned char c[4] = {0,0,0,0};
                for(int j=0; j<w; j++) {
                    int addr = i*w+j;
                    for(int n=0; n<4; n++) {
                        v[n] = this->V[n][addr];
                        c[n] = v[n] = (v[n]+c[n])&0xFF;
                    }
                    src[addr] = v[0]<<24|v[1]<<16|v[2]<<8|v[3];
                }
            }
        }
        
        ~Filter() {
            for(int n=0; n<4; n++) {
                delete[] this->V[n];
            }
        }
};
