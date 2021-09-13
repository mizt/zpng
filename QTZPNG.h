#import <vector>
#import <string>

class VideoRecorder {
    
    protected:
    
        NSString *_fileName;
    
        bool _isRunning = false;
        bool _isRecorded = false;
    
        NSFileHandle *_handle;
        std::vector<unsigned int> _frames;
    
        unsigned int _mdat_size = 0;
        unsigned long _mdat_offset = 0;
        NSMutableData *_mdat = nil;
    
        unsigned long _chanks_offset = 0;
        std::vector<unsigned long> _chanks;

    
        unsigned short _width = 1920;
        unsigned short _height = 1080;
        double _FPS = 30;
        unsigned short _depth = 32; // or 24
    
        NSString *filename() {
            long unixtime = (CFAbsoluteTimeGetCurrent()+kCFAbsoluteTimeIntervalSince1970)*1000;
            NSString *timeStampString = [NSString stringWithFormat:@"%f",(unixtime/1000.0)];
            NSDate *date = [NSDate dateWithTimeIntervalSince1970:[timeStampString doubleValue]];
            NSDateFormatter *format = [[NSDateFormatter alloc] init];
            [format setLocale:[[NSLocale alloc] initWithLocaleIdentifier:@"ja_JP"]];
            [format setDateFormat:@"yyyy_MM_dd_HH_mm_ss_SSS"];
            NSArray *paths = NSSearchPathForDirectoriesInDomains(NSMoviesDirectory,NSUserDomainMask,YES);
            NSString *documentsDirectory = [paths objectAtIndex:0];
            return [NSString stringWithFormat:@"%@/%@",documentsDirectory,[format stringFromDate:date]];
        }
    
        void setString(NSMutableData *bin,std::string str,unsigned int length=4) {
            [bin appendBytes:(unsigned char *)str.c_str() length:str.length()];
            int len = length-(int)str.length();
            if(len>=1) {
                while(len--) {
                    [bin appendBytes:new unsigned char[1]{0} length:1];
                }
            }
        }
      
        void set(NSMutableData *bin,unsigned int pos,unsigned char value) {
            *(((unsigned char *)[bin bytes])+pos) = value;
        }
    
        void setU8(NSMutableData *bin,unsigned char value) {
            [bin appendBytes:new unsigned char[1]{value} length:1];
        }
    
        void line(NSMutableData *bin) {
            NSLog(@"%lu",(unsigned long)[bin length]);
        }
            
    public:
    
        int width()  { return this->_width; }
        int height() { return this->_height; }
        VideoRecorder(unsigned short w, unsigned short h, unsigned short FPS=30, unsigned short depth=32, NSString *fileName=nil) {
            this->_width  = w;
            this->_height = h;
            this->_FPS = FPS;
            this->_depth = depth;
            if(fileName) this->_fileName = fileName;
        }
        virtual void add(unsigned int *data,int length) {}
        virtual void save() {}
        ~VideoRecorder() {}
};

// https://developer.apple.com/library/archive/documentation/QuickTime/QTFF/QTFFChap2/qtff2.html
class QTSequenceRecorder : public VideoRecorder {
    
    private:
        
        typedef std::pair<std::string,unsigned int> Atom;
        std::string _type;
        const int Transfer = 2;
        
    protected:
    
        unsigned int CreationTime = CFAbsoluteTimeGetCurrent() + kCFAbsoluteTimeIntervalSince1904;
        unsigned int ModificationTime = CreationTime;
        unsigned int TimeScale = 30000;
        unsigned short Language = 0;
          
        unsigned long swapU64(unsigned long n) {
            return ((n>>56)&0xFF)|(((n>>48)&0xFF)<<8)|(((n>>40)&0xFF)<<16)|(((n>>32)&0xFF)<<24)|(((n>>24)&0xFF)<<32)|(((n>>16)&0xFF)<<40)|(((n>>8)&0xFF)<<48)|((n&0xFF)<<56);
        }
    
        unsigned int swapU32(unsigned int n) {
            return ((n>>24)&0xFF)|(((n>>16)&0xFF)<<8)|(((n>>8)&0xFF)<<16)|((n&0xFF)<<24);
        }

        unsigned short swapU16(unsigned short n) {
            return ((n>>8)&0xFF)|((n&0xFF)<<8);
        }

        void setU64(NSMutableData *bin,unsigned long value) {
            [bin appendBytes:new unsigned long[1]{swapU64(value)} length:8];
        }
    
        void setU32(NSMutableData *bin,unsigned int value) {
            [bin appendBytes:new unsigned int[1]{swapU32(value)} length:4];
        }

        void setU16(NSMutableData *bin,unsigned short value) {
            [bin appendBytes:new unsigned short[1]{swapU16(value)} length:2];
        }

        Atom initAtom(NSMutableData *bin,std::string str, unsigned int size=0,bool dump = false) {
            assert(str.length()<=4);
            unsigned int pos = (unsigned int)[bin length];
            [bin appendBytes:new unsigned int[1]{swapU32(size)} length:4];
            setString(bin,str);
            if(dump) NSLog(@"%s,%d",str.c_str(),pos);
            return std::make_pair(str,pos);
        }
        
        void setAtomSize(NSMutableData *bin,unsigned int pos) {
            *(unsigned int *)(((unsigned char *)[bin bytes])+pos) = swapU32(((unsigned int)bin.length)-pos);
        }
    
        void setPascalString(NSMutableData *bin,std::string str) {
            assert(str.length()<255);
            [bin appendBytes:new unsigned char[1]{(unsigned char)str.size()} length:1];
            setString(bin,str,(unsigned int)str.size());
        }

        void setCompressorName(NSMutableData *bin,std::string str) {
            assert(str.length()<32);
            setPascalString(bin,str);
            int len = 31-(int)str.length();
            while(len--) {
                [bin appendBytes:new unsigned char[1]{0} length:1];
            }
        }
        
        void setMatrix(NSMutableData *bin) {
            // All values in the matrix are 32-bit fixed-point numbers divided as 16.16, except for the {u, v, w} column, which contains 32-bit fixed-point numbers divided as 2.30.
            [bin appendBytes:new unsigned int[4*9]{
                swapU32(0x00010000),swapU32(0x00000000),swapU32(0x00000000),
                swapU32(0x00000000),swapU32(0x00010000),swapU32(0x00000000),
                swapU32(0x00000000),swapU32(0x00000000),swapU32(0x01000000)
            } length:(4*9)];
        }

        void setVersionWithFlag(NSMutableData *bin,unsigned char version=0,unsigned int flag=0) {
            [bin appendBytes:new unsigned char[4]{
                version,
                (unsigned char)((flag>>16)&0xFF),
                (unsigned char)((flag>>8)&0xFF),
                (unsigned char)(flag&0xFF)} length:(1+3)];
        }

    public:
    
        QTSequenceRecorder(std::string type,unsigned short w=1920,unsigned short h=1080,unsigned short FPS=30,unsigned short depth=32,NSString *fileName=nil) : VideoRecorder(w,h,FPS,depth,fileName)  {
            this->_type = type;
            if(this->_fileName==nil) this->_fileName = [NSString stringWithFormat:@"%@.mov",this->filename()];;
            NSMutableData *bin = [[NSMutableData alloc] init];
            this->initAtom(bin,"ftyp",20);
            this->setString(bin,"qt  ",4);
            this->setU32(bin,0);
            this->setString(bin,"qt  ",4);
            this->initAtom(bin,"wide",8);
            
            [bin writeToFile:this->_fileName options:NSDataWritingAtomic error:nil];
            this->_handle = [NSFileHandle fileHandleForWritingAtPath:this->_fileName];
            [this->_handle seekToEndOfFile];
            
            this->_mdat_offset = [bin length];
            this->_chanks_offset = 8+[bin length];
            
        }
    
        QTSequenceRecorder *add(unsigned char *data,int length) {
            if(!this->_isRecorded) {
                if(this->_isRunning==false) this->_isRunning = true;
                
                unsigned int size = length;
                unsigned int diff = 0;
                if(length%4!=0) {
                    diff=(((length+3)>>2)<<2)-length;
                    size+=diff;
                }
                
                if(_mdat==nil) {
                    this->_mdat = [[NSMutableData alloc] init];
                    [this->_mdat appendBytes:new unsigned int[1]{swapU32(0)} length:4];
                    setString(this->_mdat,"mdat");
                }
                
                this->_frames.push_back(size);
                this->_chanks.push_back(this->_chanks_offset);
                
                [this->_mdat appendBytes:data length:length];
                if(diff) {
                    [this->_mdat appendBytes:new unsigned char[diff]{0} length:diff];
                }
                
                [this->_handle seekToEndOfFile];
                [this->_handle writeData:this->_mdat];
                
                this->_mdat_size+=(unsigned int)[this->_mdat length];
                
                if(this->_mdat) {
                    [this->_handle seekToFileOffset:this->_mdat_offset];
                    NSData *tmp = [[NSData alloc] initWithBytes:new unsigned int[1]{0} length:4];
                    *((unsigned int *)[tmp bytes]) = swapU32(this->_mdat_size);
                    [this->_handle writeData:tmp];
                    [this->_handle seekToEndOfFile];
                    this->_mdat_size = 0;
                    this->_mdat = nil;
                }
                
                if(this->_mdat) {
                    this->_chanks_offset+=(size);
                    this->_mdat_offset+=(size);
                }
                else {
                    this->_chanks_offset+=(8+size);
                    this->_mdat_offset+=(8+size);
                }
                
            }
            
            return this;
        }
    
        void save() {
            if(this->_isRunning&&!this->_isRecorded) {
                
                if(this->_mdat) {
                    
                    
                    
                }
                
                NSMutableData *bin = [[NSMutableData alloc] init];
                unsigned int Duration = (unsigned int)this->_frames.size()*1000;
                Atom moov = this->initAtom(bin,"moov");
                Atom mvhd = this->initAtom(bin,"mvhd");
                this->setVersionWithFlag(bin);
                this->setU32(bin,this->CreationTime);
                this->setU32(bin,this->ModificationTime);
                this->setU32(bin,this->TimeScale);
                this->setU32(bin,Duration);
                this->setU32(bin,1<<16); // Preferred rate
                this->setU16(bin,0); // Preferred volume
                [bin appendBytes:new unsigned char[10]{0} length:(10)]; // Reserved
                this->setMatrix(bin);
                this->setU32(bin,0); // Preview time
                this->setU32(bin,0); // Preview duration
                this->setU32(bin,0); // Poster time
                this->setU32(bin,0); // Selection time
                this->setU32(bin,0); // Selection duration
                this->setU32(bin,0); // Current time
                this->setU32(bin,2); // Next track ID
                this->setAtomSize(bin,mvhd.second);
                Atom trak = this->initAtom(bin,"trak");
                Atom tkhd = this->initAtom(bin,"tkhd");
                this->setVersionWithFlag(bin,0,0xF);
                this->setU32(bin,CreationTime);
                this->setU32(bin,ModificationTime);
                this->setU32(bin,1); // Track id
                [bin appendBytes:new unsigned int[1]{0} length:(4)]; // Reserved
                this->setU32(bin,Duration);
                [bin appendBytes:new unsigned int[2]{0} length:(8)]; // Reserved
                this->setU16(bin,0); // Layer
                this->setU16(bin,0); // Alternate group
                this->setU16(bin,0); // Volume
                this->setU16(bin,0); // Reserved
                this->setMatrix(bin);
                this->setU32(bin,this->_width<<16);
                this->setU32(bin,this->_height<<16);
                this->setAtomSize(bin,tkhd.second);
                Atom tapt = this->initAtom(bin,"tapt");
                this->initAtom(bin,"clef",20);
                this->setVersionWithFlag(bin);
                this->setU32(bin,this->_width<<16);
                this->setU32(bin,this->_height<<16);
                this->initAtom(bin,"prof",20);
                this->setVersionWithFlag(bin);
                this->setU32(bin,this->_width<<16);
                this->setU32(bin,this->_height<<16);
                this->initAtom(bin,"enof",20);
                this->setVersionWithFlag(bin);
                this->setU32(bin,this->_width<<16);
                this->setU32(bin,this->_height<<16);
                this->setAtomSize(bin,tapt.second);
                Atom edts = initAtom(bin,"edts");
                Atom elst = initAtom(bin,"elst");
                this->setVersionWithFlag(bin);
                this->setU32(bin,1); // Number of entries
                this->setU32(bin,Duration);
                this->setU32(bin,0); // Media time
                this->setU32(bin,1<<16); // Media rate
                this->setAtomSize(bin,elst.second);
                this->setAtomSize(bin,edts.second);
                Atom mdia = this->initAtom(bin,"mdia");
                Atom mdhd = this->initAtom(bin,"mdhd");
                this->setVersionWithFlag(bin);
                this->setU32(bin,CreationTime);
                this->setU32(bin,ModificationTime);
                this->setU32(bin,TimeScale);
                this->setU32(bin,Duration);
                this->setU16(bin,Language);
                this->setU16(bin,0); // Quality
                this->setAtomSize(bin,mdhd.second);
                Atom hdlr = initAtom(bin,"hdlr");
                this->setVersionWithFlag(bin);
                this->setString(bin,"mhlr");
                this->setString(bin,"vide");
                this->setU32(bin,0); // Reserved
                [bin appendBytes:new unsigned int[2]{0,0} length:8]; // Reserved
                this->setPascalString(bin,"Video");
                this->setAtomSize(bin,hdlr.second);
                Atom minf = initAtom(bin,"minf");
                Atom vmhd = initAtom(bin,"vmhd");
                this->setVersionWithFlag(bin,0,1);
                this->setU16(bin,0); // Graphics mode
                this->setU16(bin,32768); // Opcolor
                this->setU16(bin,32768); // Opcolor
                this->setU16(bin,32768); // Opcolor
                this->setAtomSize(bin,vmhd.second);
                hdlr = initAtom(bin,"hdlr");
                this->setVersionWithFlag(bin);
                this->setString(bin,"dhlr");
                this->setString(bin,"alis");
                this->setU32(bin,0); // Reserved 0
                [bin appendBytes:new unsigned int[2]{0,0} length:8]; // Reserved
                this->setPascalString(bin,"Handler");
                this->setAtomSize(bin,hdlr.second);
                Atom dinf = this->initAtom(bin,"dinf");
                Atom dref = this->initAtom(bin,"dref");
                this->setVersionWithFlag(bin);
                this->setU32(bin,1); // Number of entries
                this->initAtom(bin,"alis",12);
                this->setVersionWithFlag(bin,0,1);
                this->setAtomSize(bin,dref.second);
                this->setAtomSize(bin,dinf.second);
                Atom stbl = this->initAtom(bin,"stbl");
                Atom stsd = this->initAtom(bin,"stsd");
                this->setVersionWithFlag(bin);
                this->setU32(bin,1); // Number of entries
                Atom table = initAtom(bin,this->_type);
                [bin appendBytes:new unsigned char[6]{0,0,0,0,0,0} length:(6)]; // Reserved
                this->setU16(bin,1); // Data reference index
                this->setU16(bin,0); // Version
                this->setU16(bin,0); // Revision level
                this->setU32(bin,0); // Vendor
                this->setU32(bin,0); // Temporal quality
                this->setU32(bin,1024); // Spatial quality
                this->setU16(bin,this->_width);
                this->setU16(bin,this->_height);
                this->setU32(bin,72<<16); // Horizontal resolution
                this->setU32(bin,72<<16); // Vertical resolution
                [bin appendBytes:new unsigned int[1]{0} length:4];
                this->setU16(bin,1); // Frame count
                this->setCompressorName(bin,"'"+this->_type+"'"); // 32
                this->setU16(bin,this->_depth); // Depth
                this->setU16(bin,0xFFFF); // Color table ID
                this->initAtom(bin,"colr",18);
                this->setString(bin,"nclc");
                this->setU16(bin,1); // Primaries index
                this->setU16(bin,Transfer); // Transfer function index
                this->setU16(bin,1); // Matrix index
                if(this->Transfer==2) {
                    this->initAtom(bin,"gama",12);
                    this->setU32(bin,144179); // 2.2 = 144179, 1.96 = 128512
                }
                this->initAtom(bin,"fiel",10);
                this->setU16(bin,1<<8);
                this->initAtom(bin,"pasp",16);
                this->setU32(bin,1);
                this->setU32(bin,1);
                this->setU32(bin,0); // Some sample descriptions terminate with four zero bytes that are not otherwise indicated.
                this->setAtomSize(bin,table.second);
                this->setAtomSize(bin,stsd.second);
                this->initAtom(bin,"stts",24);
                this->setVersionWithFlag(bin);
                this->setU32(bin,1); // Number of entries
                this->setU32(bin,(unsigned int)this->_frames.size());
                this->setU32(bin,(unsigned int)(TimeScale/this->_FPS));
                this->initAtom(bin,"stsc",28);
                this->setVersionWithFlag(bin);
                this->setU32(bin,1); // Number of entries
                this->setU32(bin,1); // First chunk
                this->setU32(bin,1); // Samples per chunk
                this->setU32(bin,1); // Sample description ID
                Atom stsz = this->initAtom(bin,"stsz",20);
                this->setVersionWithFlag(bin);
                this->setU32(bin,0); // If this field is set to 0, then the samples have different sizes, and those sizes are stored in the sample size table.
                this->setU32(bin,(unsigned int)this->_frames.size()); // Number of entries
                for(int k=0; k<this->_frames.size(); k++) {
                    this->setU32(bin,this->_frames[k]);
                }
                this->setAtomSize(bin,stsz.second);
                Atom stco = initAtom(bin,"co64");
                this->setVersionWithFlag(bin);
                this->setU32(bin,(unsigned int)this->_chanks.size()); // Number of entries
                for(int k=0; k<this->_chanks.size(); k++) {
                    this->setU64(bin,this->_chanks[k]); // Chunk
                }
                this->setAtomSize(bin,minf.second);
                this->setAtomSize(bin,stco.second);
                this->setAtomSize(bin,stbl.second);
                this->setAtomSize(bin,mdia.second);
                this->setAtomSize(bin,trak.second);
                this->setAtomSize(bin,moov.second);
                [this->_handle seekToEndOfFile];
                [this->_handle writeData:bin];
                this->_isRunning = false;
                this->_isRecorded = true;
            }
        }
    
        ~QTSequenceRecorder() {
        }
};

class QTPNGRecorder : public QTSequenceRecorder {
    public:
        QTPNGRecorder(unsigned short w=1920,unsigned short h=1080,unsigned short FPS=30,unsigned short depth=32,NSString *fileName=nil) : QTSequenceRecorder("png ",w,h,FPS,depth,fileName) {}
        QTPNGRecorder(NSString *fileName) : QTSequenceRecorder("png ",1920,1080,30,32,fileName) {}
        ~QTPNGRecorder() {}
};

class QTZPNGRecorder : public QTSequenceRecorder {
    public:
        QTZPNGRecorder(unsigned short w=1920,unsigned short h=1080,unsigned short FPS=30,unsigned short depth=32,NSString *fileName=nil) : QTSequenceRecorder("zpng",w,h,FPS,depth,fileName) {}
        QTZPNGRecorder(NSString *fileName) : QTSequenceRecorder("zpng",1920,1080,30,32,fileName) {}
        ~QTZPNGRecorder() {}
};

class QTSequenceParser {
    
    private:
    
        FILE *_fp;

        unsigned short _width = 0;
        unsigned short _height = 0;
        double _FPS = 0;
        unsigned short _depth = 0;
    
        unsigned int _totalFrames = 0;
        std::pair<unsigned long,unsigned int> *_frames = nullptr;
    
        unsigned long swapU64(unsigned long n) {
            return ((n>>56)&0xFF)|(((n>>48)&0xFF)<<8)|(((n>>40)&0xFF)<<16)|(((n>>32)&0xFF)<<24)|(((n>>24)&0xFF)<<32)|(((n>>16)&0xFF)<<40)|(((n>>8)&0xFF)<<48)|((n&0xFF)<<56);
        }
        
        unsigned int swapU32(unsigned int n) {
            return ((n>>24)&0xFF)|(((n>>16)&0xFF)<<8)|(((n>>8)&0xFF)<<16)|((n&0xFF)<<24);
        }
        
        unsigned short swapU16(unsigned short n) {
            return ((n>>8)&0xFF)|((n&0xFF)<<8);
        }
        
    public:
    
        unsigned short width() { return this->_width; }
        unsigned short height() { return this->_height; }
        double FPS() { return this->_FPS; }
        unsigned short depth() { return this->_depth; }
        unsigned int length() { return this->_totalFrames; }

        std::pair<unsigned int,unsigned int> frame(int n) {
            if(n<0) n = 0;
            else if(n>=this->_totalFrames) n = this->_totalFrames-1;
            return this->_frames[n];
        }
    
        NSData *get(off_t offset, size_t bytes) {
            unsigned char *data = new unsigned char[bytes];
            fseeko(this->_fp,offset,SEEK_SET);
            fread(data,1,bytes,this->_fp);
            NSData *buffer = [NSData dataWithBytesNoCopy:data length:bytes];
            delete[] data;
            return buffer;
        }
    
        NSData *get(unsigned long n) {
            if(n<this->_totalFrames) {
                return this->get(this->_frames[n].first,this->_frames[n].second);
            }
            return nil;
        }
    
        unsigned int atom(std::string str) {
            assert(str.length()==4);
            unsigned char *key =(unsigned char *)str.c_str();
            return key[0]<<24|key[1]<<16|key[2]<<8|key[3];
        }
    
        unsigned long size() {
            fpos_t size = 0;
            fseeko(this->_fp,0,SEEK_END);
            fgetpos(this->_fp,&size);
            return size;
        }

        void parse(FILE *fp,std::string key) {
            
            this->_fp = fp;
            if(fp!=NULL){
                
                unsigned int type = this->atom(key);
                
                unsigned int buffer;
                fseeko(fp,4*7,SEEK_SET);
                fread(&buffer,sizeof(unsigned int),1,fp); // 4*7
                int len = this->swapU32(buffer);
                unsigned int offset = len;
                fread(&buffer,sizeof(unsigned int),1,fp); // 4*8

                while(true) {
                    
                    if(this->swapU32(buffer)==this->atom("mdat")) {
                        //NSLog(@"mdat");
                        fseeko(fp,(4*8)+offset-4,SEEK_SET);
                        fread(&buffer,sizeof(unsigned int),1,fp);
                        len = this->swapU32(buffer);
                        offset += len;
                        fread(&buffer,sizeof(unsigned int),1,fp);
                    }
                    else if(this->swapU32(buffer)==this->atom("moov")) {
                        //NSLog(@"moov");
                        unsigned char *moov = new unsigned char[len-8];
                        fread(moov,sizeof(unsigned char),len-8,fp);
                        bool key = false;
                        int seek = ((4*4)+4+6+2+2+2+4+4+4);
                        
                        for(int k=0; k<(len-8)-(seek+2*2+4+4+4+2+32+2); k++) {
                            if(this->swapU32(*((unsigned int *)(moov+k)))==this->atom("stsd")) {
                                if(this->swapU32(*((unsigned int *)(moov+k+(4*4))))==type) {
                                    this->_width = this->swapU16(*((unsigned short *)(moov+k+seek)));
                                    this->_height = this->swapU16(*((unsigned short *)(moov+k+seek+2)));
                                    this->_depth = this->swapU16(*((unsigned short *)(moov+k+seek+2+4+4+4+2+32+2)));
                                    if(this->_width>0&&this->_height>0) key = true;
                                    break;
                                }
                            }
                        }
                        
                        if(key) {
                            unsigned int TimeScale = 0;
                            for(int k=0; k<(len-8)-3; k++) {
                                if(this->swapU32(*((unsigned int *)(moov+k)))==atom("mvhd")) {
                                    if(k+(4*4)<len) {
                                        TimeScale = this->swapU32(*((unsigned int *)(moov+k+(4*4))));
                                        break;
                                    }
                                }
                            }
                            
                            if(TimeScale>0) {
                                for(int k=0; k<(len-8)-3; k++) {
                                    if(this->swapU32(*((unsigned int *)(moov+k)))==atom("stts")) {
                                        if(k+(4*4)<len) {
                                            this->_FPS = TimeScale/(double)(swapU32(*((unsigned int *)(moov+k+(4*4)))));
                                            break;
                                        }
                                    }
                                }
                                
                                if(this->_FPS>0) {
                                    for(int k=0; k<(len-8)-3; k++) {
                                        if(this->swapU32(*((unsigned int *)(moov+k)))==atom("stsz")) {
                                            k+=(4*3);
                                            if(k<(len-8)) {
                                                this->_totalFrames = this->swapU32(*((unsigned int *)(moov+k)));
                                                if(this->_frames) delete[] this->_frames;
                                                this->_frames = new std::pair<unsigned long,unsigned int>[this->_totalFrames];
                                                for(int f=0; f<this->_totalFrames; f++) {
                                                    k+=4;
                                                    if(k<(len-8)) {
                                                        unsigned int size = this->swapU32(*((unsigned int *)(moov+k)));
                                                        this->_frames[f] = std::make_pair(0,size);
                                                    }
                                                }
                                                break;
                                            }
                                        }
                                    }
                                    
                                    for(int k=0; k<(len-8)-3; k++) {
                                        if(this->swapU32(*((unsigned int *)(moov+k)))==atom("co64")) {
                                            k+=(4*2);
                                            if(k<(len-8)) {
                                                if(this->_totalFrames==this->swapU32(*((unsigned int *)(moov+k)))) {
                                                    k+=4;
                                                    for(int f=0; f<this->_totalFrames; f++) {
                                                        if(k<(len-8)) {
                                                            this->_frames[f].first = this->swapU64(*((unsigned long *)(moov+k)));
                                                            k+=8;
                                                        }
                                                    }
                                                    break;
                                                }
                                            }
                                        }
                                    }

                                }
                            }
                        }
                        
                        delete[] moov;
                        
                        break;
                    }
                    else {
                        
                        break;
                        
                    }
                    
                }
                
            }
                 
        }
    
        QTSequenceParser(FILE *fp,std::string key) {
            this->parse(fp,key);
        }
    
        QTSequenceParser(NSString *path,std::string key) {
            FILE *fp = fopen([path UTF8String],"rb");
            this->parse(fp,key);
            fclose(fp);
        }
    
        ~QTSequenceParser() {
            this->_fp = NULL;
            delete[] this->_frames;
            
        }
};

class QTPNGParser : public QTSequenceParser {
    public:
        QTPNGParser(NSString *path) : QTSequenceParser(path,"png ") {}
        QTPNGParser(FILE *fp) : QTSequenceParser(fp,"png ") {}
        ~QTPNGParser() {}
};

class QTZPNGParser : public QTSequenceParser {
    public:
        QTZPNGParser(NSString *path) : QTSequenceParser(path,"zpng") {}
        QTZPNGParser(FILE *fp) : QTSequenceParser(fp,"zpng") {}
        ~QTZPNGParser() {}
};
