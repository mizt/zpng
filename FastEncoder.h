namespace Filter {

    class FastEncoder : public Base {
        
        private:
        
            dispatch_group_t _group = dispatch_group_create();
            dispatch_queue_t _queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH,0);
        
            void encode(unsigned int *buffer,int begin, int end) {
                
                for(int i=begin; i<end; i++) {
                    
                    unsigned int *src = buffer+i*this->_width;
                    unsigned int pixel = *src++;

                    this->_bytes[i*((this->_width*4)+1)] = 1; // filter
                    unsigned int *dst = (unsigned int *)(this->_bytes+i*((this->_width*4)+1)+1);
                    *dst++ = pixel;

                    unsigned char a = pixel>>24;
                    unsigned char r = (pixel>>16)&0xFF;
                    unsigned char g = (pixel>>8)&0xFF;
                    unsigned char b = pixel&0xFF;
                    
                    unsigned char _a = a;
                    unsigned char _r = r;
                    unsigned char _g = g;
                    unsigned char _b = b;
                    
                    for(int j=1; j<this->_width; j++) {
                        pixel = *src++;
                        
                        pixel = *src++;
                        a = pixel>>24;
                        r = (pixel>>16)&0xFF;
                        g = (pixel>>8)&0xFF;
                        b = pixel&0xFF;
                        
                        *dst++ = (((a-_a))&0xFF)<<24|(((r-_r))&0xFF)<<16|(((g-_g))&0xFF)<<8|(((b-_b))&0xFF);
                        
                        _a = a;
                        _r = r;
                        _g = g;
                        _b = b;
                    }
                }
            }
        
        public:
        
            FastEncoder(unsigned int w, unsigned int h) : Base(w,h,4) { // unsigned int bpp=4
            }
            
            ~FastEncoder() {
            }
        
            void encode(unsigned int *buffer) { // int ch=4 ,unsigned int filter=1
                
                int h = this->_height>>1;
                
                dispatch_group_async(this->_group,this->_queue,^{
                    this->encode(buffer,0,h);
                });
                dispatch_group_async(this->_group,this->_queue,^{
                    this->encode(buffer,h,this->_height);
                });
                
                dispatch_group_wait(this->_group,DISPATCH_TIME_FOREVER);
            }
    };
};
