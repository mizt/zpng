class VideoRecorder {
    
    protected:
    
        NSString *_fileName;
    
        bool _isRunning = false;
        bool _isRecorded = false;
    
        NSFileHandle *_handle;
        unsigned int _length = 0;
        std::vector<unsigned int> _frames;

        unsigned int _width = 1920;
        unsigned int _height = 1080;
        unsigned int _FPS = 30;
    
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
        VideoRecorder(int w,int h,int FPS=30,NSString *fileName=nil) {
            
            this->_width  = w;
            this->_height = h;
            this->_FPS = FPS;
            
            if(fileName) {
                this->_fileName = fileName;
            }
            
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
  
        Atom mdat;
    
        unsigned int swapU32(unsigned int n) {
            return ((n>>24)&0xFF)|(((n>>16)&0xFF)<<8)|(((n>>8)&0xFF)<<16)|((n&0xFF)<<24);
        }

        unsigned short swapU16(unsigned int n) {
            return ((n>>8)&0xFF)|((n&0xFF)<<8);
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
    
        QTSequenceRecorder(std::string type,int w=1920,int h=1080,int FPS=30,NSString *fileName=nil) : VideoRecorder(w,h,FPS,fileName)  {
            this->_type = type;
            if(this->_fileName==nil) this->_fileName = [NSString stringWithFormat:@"%@.mov",this->filename()];;
            NSMutableData *bin = [[NSMutableData alloc] init];
            this->initAtom(bin,"ftyp",20);
            this->setString(bin,"qt  ",4);
            this->setU32(bin,0);
            this->setString(bin,"qt  ",4);
            this->initAtom(bin,"wide",8);
            this->mdat = this->initAtom(bin,"mdat");
            this->_length = 8;
            [bin writeToFile:this->_fileName options:NSDataWritingAtomic error:nil];
            this->_handle = [NSFileHandle fileHandleForWritingAtPath:this->_fileName];
            [this->_handle seekToEndOfFile];
        }
    
        QTSequenceRecorder *add(unsigned char *data,int length) {
            if(!this->_isRecorded) {
                if(this->_isRunning==false) {
                    this->_isRunning = true;
                }
                unsigned int size = length;
                unsigned int diff = 0;
                if(length%4!=0) {
                    diff=(((length+3)>>2)<<2)-length;
                    size+=diff;
                }
                this->_frames.push_back(size);
                NSMutableData *bin = [[NSMutableData alloc] init];
                [bin appendBytes:data length:length];
                if(diff) {
                    [bin appendBytes:new unsigned char[diff]{0} length:diff];
                }
                this->_length+=(unsigned int)[bin length];
                [this->_handle writeData:bin];
            }
            
            return this;
        }
    
        void save() {
            if(this->_isRunning&&!this->_isRecorded) {
                NSMutableData *bin = [[NSMutableData alloc] init];
                NSData *tmp = [[NSData alloc] initWithBytes:new unsigned int[1]{0} length:4];
                *((unsigned int *)[tmp bytes]) = swapU32(this->_length);
                [this->_handle seekToFileOffset:this->mdat.second];
                [this->_handle writeData:tmp];
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
                this->setCompressorName(bin,"'"+this->_type+"'");
                this->setU16(bin,24); // Depth
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
                this->setU32(bin,TimeScale/this->_FPS);
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
                Atom stco = initAtom(bin,"stco");
                this->setVersionWithFlag(bin);
                this->setU32(bin,(unsigned int)this->_frames.size()); // Number of entries
                int seek = this->mdat.second+8;
                for(int k=0; k<this->_frames.size(); k++) {
                    this->setU32(bin,seek); // Chunk
                    seek+=this->_frames[k];
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
        QTPNGRecorder(int w=1920,int h=1080,int FPS=30,NSString *fileName=nil) : QTSequenceRecorder("png ",w,h,FPS,fileName) {}
        ~QTPNGRecorder() {}
    
};

class QTZPNGRecorder : public QTSequenceRecorder {
    
    public:
        QTZPNGRecorder(int w=1920,int h=1080,int FPS=30,NSString *fileName=nil) : QTSequenceRecorder("zpng",w,h,FPS,fileName) {}
        ~QTZPNGRecorder() {}
    
};

class QTZPNGParser {
    
    private:

        NSString *_path;
        std::vector<std::pair<unsigned int,unsigned int>> _frames;

        unsigned int swapU32(unsigned int n) {
            return ((n>>24)&0xFF)|(((n>>16)&0xFF)<<8)|(((n>>8)&0xFF)<<16)|((n&0xFF)<<24);
        }
    
    public:
    
        unsigned long length() { return this->_frames.size(); }

        NSData *get(off_t offset, size_t bytes) {
            FILE *file = fopen([this->_path UTF8String],"rb");
            if(file==NULL) return nil;
            void *data = malloc(bytes);
            fseeko(file,offset,SEEK_SET);
            fread(data,1,bytes,file);
            fclose(file);
            return [NSData dataWithBytesNoCopy:data length:bytes];
        }
        
        NSData *get(unsigned long n) {
            if(n<this->_frames.size()) {
                return this->get(this->_frames[n].first,this->_frames[n].second);
            }
            return nil;
        }
        
        std::vector<std::pair<unsigned int,unsigned int>> *frames() {
            return &this->_frames;
        }

        QTZPNGParser(NSString *path) {
            this->_path = path;
            NSData *header = this->get((4*7),8);
            if(header) {
                unsigned char *bytes = (unsigned char *)[header bytes];
                if((swapU32(((unsigned int *)bytes)[1])==0x6D646174)) { // mdat
                    int offset = swapU32(*((unsigned int *)bytes));
                    NSData *moov = this->get((4*8)+offset-4,8);
                    bytes = (unsigned char *)[moov bytes];
                    int len = swapU32(*((unsigned int *)bytes));
                    if(swapU32(((unsigned int *)bytes)[1])==0x6D6F6F76) { // moov
                        moov = this->get((4*8)+offset-4,len);
                        bytes = (unsigned char *)[moov bytes];
                        bool key = false;
                        for(int k=0; k<len-4; k++) {
                            if(swapU32(*((unsigned int *)(bytes+k)))==0x7A706E67) { // zpng
                                key = true;
                                break;
                            }
                        }
                        if(key) {
                            for(int k=0; k<len-4; k++) {
                                if(swapU32(*((unsigned int *)(bytes+k)))==0x7374737A) { // stsz
                                    k+=4;
                                    k+=4;
                                    k+=4;
                                    if(k<len) {
                                        int seek = 4*9;
                                        int frames = swapU32(*((unsigned int *)(bytes+k)));
                                        for(int f=0; f<frames; f++) {
                                            k+=4;
                                            int size = swapU32(*((unsigned int *)(bytes+k)));
                                            this->_frames.push_back(std::make_pair(seek,size));
                                            seek+=size;
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    
        ~QTZPNGParser() {
            this->_frames.clear();
        }
};