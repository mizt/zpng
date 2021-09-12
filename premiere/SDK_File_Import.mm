#import <Foundation/Foundation.h>
#import "SDK_File_Import.h"
#import <assert.h>
#import <iostream>
#import <zstd.h>
#import "Filter.h"

#if IMPORTMOD_VERSION <= IMPORTMOD_VERSION_9
typedef PrSDKPPixCacheSuite2 PrCacheSuite;
#define PrCacheVersion kPrSDKPPixCacheSuiteVersion2
#else
typedef PrSDKPPixCacheSuite PrCacheSuite;
#define PrCacheVersion kPrSDKPPixCacheSuiteVersion
#endif

typedef struct {
    csSDK_int32 importerID;
    csSDK_int32 fileType;
    csSDK_int32 width;
    csSDK_int32 height;
    csSDK_int32 frameRateNum;
    csSDK_int32 frameRateDen;
    PlugMemoryFuncsPtr memFuncs;
    SPBasicSuite *BasicSuite;
    PrSDKPPixCreatorSuite *PPixCreatorSuite;
    PrCacheSuite *PPixCacheSuite;
    PrSDKPPixSuite *PPixSuite;
    PrSDKTimeSuite *TimeSuite;
    PrSDKImporterFileManagerSuite *FileSuite;
    unsigned char *zpng;
    unsigned char *buffer;
    unsigned int depth;
    unsigned int *frame_head;
    unsigned int *frame_size;
    Filter::Decoder *decoder;
} ImporterLocalRec8, *ImporterLocalRec8Ptr, **ImporterLocalRec8H;


static prMALError SDKInit(imStdParms *stdParms, imImportInfoRec *importInfo) {
    importInfo->canSave = kPrFalse;
    importInfo->canDelete = kPrFalse;
    importInfo->canCalcSizes = kPrFalse;
    importInfo->canTrim = kPrFalse;
    importInfo->hasSetup = kPrFalse;
    importInfo->setupOnDblClk = kPrFalse;
    importInfo->dontCache = kPrFalse;
    importInfo->keepLoaded = kPrFalse;
    importInfo->priority = 0;
    importInfo->avoidAudioConform = kPrFalse;
    return malNoError;
}

static prMALError SDKGetIndFormat(imStdParms *stdParms, csSDK_size_t index, imIndFormatRec *SDKIndFormatRec) {
        
    prMALError result = malNoError;
    char formatname[255] = "zpng";
    char shortname[32] = "zpng";
    char platformXten[256] = "zpng\0";

    switch(index) {
        
        case 0:
            SDKIndFormatRec->filetype = 'zpng';
            SDKIndFormatRec->canWriteTimecode = kPrFalse;
            SDKIndFormatRec->canWriteMetaData = kPrFalse;
            SDKIndFormatRec->flags = xfCanImport|xfIsMovie;
            strcpy(SDKIndFormatRec->FormatName,formatname);
            strcpy(SDKIndFormatRec->FormatShortName,shortname);
            strcpy(SDKIndFormatRec->PlatformExtension,platformXten);
            break;

        default:
            result = imBadFormatIndex;
    }

    return result;
}


prMALError SDKOpenFile8(imStdParms *stdParms, imFileRef *SDKfileRef, imFileOpenRec8 *SDKfileOpenRec8) {
        
    prMALError result = malNoError;

    ImporterLocalRec8H localRecH = NULL;
    ImporterLocalRec8Ptr localRecP = NULL;

    if(SDKfileOpenRec8->privatedata) {
        localRecH = (ImporterLocalRec8H)SDKfileOpenRec8->privatedata;
        stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(localRecH));
        localRecP = reinterpret_cast<ImporterLocalRec8Ptr>(*localRecH);
    }
    else {
        
        localRecH = (ImporterLocalRec8H)stdParms->piSuites->memFuncs->newHandle(sizeof(ImporterLocalRec8));
        SDKfileOpenRec8->privatedata = (PrivateDataPtr)localRecH;
        stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(localRecH));
        localRecP = reinterpret_cast<ImporterLocalRec8Ptr>(*localRecH);
        
        localRecP->memFuncs = stdParms->piSuites->memFuncs;
        localRecP->BasicSuite = stdParms->piSuites->utilFuncs->getSPBasicSuite();
        if(localRecP->BasicSuite) {
            localRecP->BasicSuite->AcquireSuite(kPrSDKPPixCreatorSuite,kPrSDKPPixCreatorSuiteVersion,(const void**)&localRecP->PPixCreatorSuite);
            localRecP->BasicSuite->AcquireSuite(kPrSDKPPixCacheSuite,PrCacheVersion,(const void**)&localRecP->PPixCacheSuite);
            localRecP->BasicSuite->AcquireSuite(kPrSDKPPixSuite,kPrSDKPPixSuiteVersion,(const void**)&localRecP->PPixSuite);
            localRecP->BasicSuite->AcquireSuite(kPrSDKTimeSuite,kPrSDKTimeSuiteVersion,(const void**)&localRecP->TimeSuite);
            localRecP->BasicSuite->AcquireSuite(kPrSDKImporterFileManagerSuite,kPrSDKImporterFileManagerSuiteVersion,(const void**)&localRecP->FileSuite);
        }

        localRecP->importerID = SDKfileOpenRec8->inImporterID;
        localRecP->fileType = SDKfileOpenRec8->fileinfo.filetype;
        
        localRecP->depth = 0;
        
        localRecP->frame_head = nullptr;
        localRecP->frame_size = nullptr;
        
        localRecP->zpng = nullptr;
        localRecP->buffer = nullptr;
        localRecP->decoder = nullptr;
    }

    SDKfileOpenRec8->fileinfo.fileref = *SDKfileRef = reinterpret_cast<imFileRef>(imInvalidHandleValue);

    if(localRecP) {
        const prUTF16Char *path = SDKfileOpenRec8->fileinfo.filepath;
        FSIORefNum refNum = CAST_REFNUM(imInvalidHandleValue);
        CFStringRef filePathCFSR = CFStringCreateWithCharacters(NULL,path,prUTF16CharLength(path));
        CFURLRef filePathURL = CFURLCreateWithFileSystemPath(NULL,filePathCFSR,kCFURLPOSIXPathStyle,false);
        if(filePathURL != NULL) {
            FSRef fileRef;
            Boolean success = CFURLGetFSRef(filePathURL,&fileRef);
            if(success) {
                HFSUniStr255 dataForkName;
                FSGetDataForkName(&dataForkName);
                if(!FSOpenFork(&fileRef,dataForkName.length,dataForkName.unicode,fsRdPerm,&refNum)) {
                    result = imFileOpenFailed;
                }
            }
            CFRelease(filePathURL);
        }
                                    
        CFRelease(filePathCFSR);

        if(CAST_FILEREF(refNum)!=imInvalidHandleValue) {
                        
            SDKfileOpenRec8->fileinfo.fileref = *SDKfileRef = CAST_FILEREF(refNum);
        }
        else {
            result = imFileOpenFailed;
        }
    }
    
    if(result!=malNoError) {
        if(SDKfileOpenRec8->privatedata) {
            stdParms->piSuites->memFuncs->disposeHandle(reinterpret_cast<PrMemoryHandle>(SDKfileOpenRec8->privatedata));
            SDKfileOpenRec8->privatedata = NULL;
        }
    }
    else {
        stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(SDKfileOpenRec8->privatedata));
    }

    return result;
}

static prMALError SDKQuietFile(imStdParms *stdParms, imFileRef *SDKfileRef,void *privateData) {

    if(SDKfileRef && *SDKfileRef != imInvalidHandleValue) {
        ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(privateData);
        stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));
        stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));
        FSCloseFork((FSIORefNum)CAST_REFNUM(*SDKfileRef));
        *SDKfileRef = imInvalidHandleValue;
    }

    return malNoError;
}

static prMALError SDKCloseFile(imStdParms *stdParms, imFileRef *SDKfileRef, void *privateData) {
        
    ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(privateData);
    
    if(SDKfileRef && *SDKfileRef!=imInvalidHandleValue) SDKQuietFile(stdParms,SDKfileRef,privateData);

    if(ldataH&&*ldataH&&(*ldataH)->BasicSuite) {
        stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));
        ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>(*ldataH);
        localRecP->BasicSuite->ReleaseSuite(kPrSDKPPixCreatorSuite,kPrSDKPPixCreatorSuiteVersion);
        localRecP->BasicSuite->ReleaseSuite(kPrSDKPPixCacheSuite,PrCacheVersion);
        localRecP->BasicSuite->ReleaseSuite(kPrSDKPPixSuite,kPrSDKPPixSuiteVersion);
        localRecP->BasicSuite->ReleaseSuite(kPrSDKTimeSuite,kPrSDKTimeSuiteVersion);
        localRecP->BasicSuite->ReleaseSuite(kPrSDKImporterFileManagerSuite,kPrSDKImporterFileManagerSuiteVersion);
        stdParms->piSuites->memFuncs->disposeHandle(reinterpret_cast<PrMemoryHandle>(ldataH));
    }

    return malNoError;
}

static prMALError SDKGetIndPixelFormat(imStdParms *stdParms, csSDK_size_t idx,imIndPixelFormatRec *SDKIndPixelFormatRec) {
    prMALError result = malNoError;
    switch(idx) {
        case 0:
            SDKIndPixelFormatRec->outPixelFormat = PrPixelFormat_BGRA_4444_8u;
            break;
    
        default:
            result = imBadFormatIndex;
            break;
    }
    return result;
}

static unsigned int atom(std::string str) {
    assert(str.length()==4);
    unsigned char *key =(unsigned char *)str.c_str();
    return key[0]<<24|key[1]<<16|key[2]<<8|key[3];
}

static unsigned short swapU16(unsigned short n) {
    return ((n>>8)&0xFF)|((n&0xFF)<<8);
}

static unsigned int swapU32(unsigned int n) {
    return ((n>>24)&0xFF)|(((n>>16)&0xFF)<<8)|(((n>>8)&0xFF)<<16)|((n&0xFF)<<24);
}

prMALError SDKGetInfo8(imStdParms *stdParms, imFileAccessRec8 *fileAccessInfo8, imFileInfoRec8 *SDKFileInfo8) {
    
    prMALError result = malNoError;
    
    SDKFileInfo8->vidInfo.supportsAsyncIO = kPrFalse;
    SDKFileInfo8->vidInfo.supportsGetSourceVideo = kPrTrue;
    SDKFileInfo8->vidInfo.hasPulldown = kPrFalse;
    SDKFileInfo8->hasDataRate = kPrFalse;
    
    assert(SDKFileInfo8->privatedata);
    ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(SDKFileInfo8->privatedata);
    stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));
    ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>(*ldataH);
    
    SDKFileInfo8->hasVideo = kPrFalse;
    SDKFileInfo8->hasAudio = kPrFalse;
    
    if(localRecP) {
        int frames = 0;
        unsigned int type = atom("zpng");
        unsigned int buffer = 0;
        ByteCount actualCount = 0;
        FSReadFork((FSIORefNum)CAST_REFNUM(fileAccessInfo8->fileref),fsFromStart,4*7,4,&buffer,&actualCount);
        int offset = swapU32(buffer);
        FSReadFork((FSIORefNum)CAST_REFNUM(fileAccessInfo8->fileref),fsFromStart,4*8,4,&buffer,&actualCount);
        if(swapU32(buffer)==atom("mdat")) {
            FSReadFork((FSIORefNum)CAST_REFNUM(fileAccessInfo8->fileref),fsFromStart,(4*8)+offset-4,4,&buffer,&actualCount);
            int len = swapU32(buffer);
            FSReadFork((FSIORefNum)CAST_REFNUM(fileAccessInfo8->fileref),fsFromStart,(4*8)+offset,4,&buffer,&actualCount);
            if(swapU32(buffer)==atom("moov")) {
                
                unsigned char *moov = new unsigned char[len-8];
                
                FSReadFork((FSIORefNum)CAST_REFNUM(fileAccessInfo8->fileref),fsFromStart,(4*8)+offset+4,len-8,moov,&actualCount);                
                bool key = false;
                int seek = ((4*4)+4+6+2+2+2+4+4+4);
                for(int k=0; k<(len-8)-(seek+(2)*2+(4+4+4+2+32+2)+2); k++) {
                    if(swapU32(*((unsigned int *)(moov+k)))==atom("stsd")) {
                        if(swapU32(*((unsigned int *)(moov+k+(4*4))))==type) {
                            SDKFileInfo8->vidInfo.imageWidth  = swapU16(*((unsigned short *)(moov+k+seek)));
                            SDKFileInfo8->vidInfo.imageHeight = swapU16(*((unsigned short *)(moov+k+seek+2)));
                            localRecP->depth = (swapU16(*((unsigned short *)(moov+k+seek+2+4+4+4+2+32+2))))>>3;
                            key = true;
                            break;
                        }
                    }
                }
            
                if(key) {
                    
                    unsigned int TimeScale = 0;
                    for(int k=0; k<(len-8)-3; k++) {
                        if(swapU32(*((unsigned int *)(moov+k)))==atom("mvhd")) {
                            if(k+(4*4)<(len-8)) {
                                TimeScale = swapU32(*((unsigned int *)(moov+k+(4*4))));
                                break;
                            }
                        }
                    }
                                        
                    if(TimeScale>0) {
                        double FPS = 0;
                        for(int k=0; k<(len-8)-3; k++) {
                            if(swapU32(*((unsigned int *)(moov+k)))==atom("stts")) {
                                if(k+(4*4)<(len-8)) {
                                    FPS = TimeScale/(double)(swapU32(*((unsigned int *)(moov+k+(4*4)))));
                                    SDKFileInfo8->vidScale = TimeScale;
                                    SDKFileInfo8->vidSampleSize = (swapU32(*((unsigned int *)(moov+k+(4*4)))));
                                    break;
                                }
                            }
                        }
                                                
                        if(FPS>0) {
                            for(int k=0; k<(len-8)-3; k++) {
                                if(swapU32(*((unsigned int *)(moov+k)))==atom("stsz")) {
                                    k+=(4*3);
                                    if(k<(len-8)) {
                                        unsigned int head = 4*9;
                                        frames = swapU32(*((unsigned int *)(moov+k)));
                                        if(localRecP->frame_head) delete[] localRecP->frame_head;
                                        if(localRecP->frame_size) delete[] localRecP->frame_size;
                                        localRecP->frame_head = new unsigned int[frames];
                                        localRecP->frame_size = new unsigned int[frames];
                                        for(int f=0; f<frames; f++) {
                                            k+=4;
                                            if(k<len-8) {
                                                unsigned int size = swapU32(*((unsigned int *)(moov+k)));
                                                localRecP->frame_head[f] = head;
                                                localRecP->frame_size[f] = size;
                                                head+=size;
                                            }
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                        else {
                            result = imFileOpenFailed;
                        }
                        
                    }
                    else {
                        result = imFileOpenFailed;
                    }
                    
                }
                else {
                    result = imFileOpenFailed;
                }
                
                delete[] moov;
            }
        }
                
        if((localRecP->depth==3||localRecP->depth==4)&&frames>0) {
            
            SDKFileInfo8->hasVideo = kPrTrue;
            SDKFileInfo8->vidInfo.subType = PrPixelFormat_BGRA_4444_8u;
            SDKFileInfo8->vidInfo.depth = 32;
            SDKFileInfo8->vidInfo.fieldType = prFieldsUnknown;
            SDKFileInfo8->vidInfo.isStill = kPrFalse;
            SDKFileInfo8->vidInfo.noDuration = imNoDurationFalse;
            
            SDKFileInfo8->vidInfo.alphaType = alphaStraight;
            
            SDKFileInfo8->vidDuration = frames * SDKFileInfo8->vidSampleSize;
            
            localRecP->width = SDKFileInfo8->vidInfo.imageWidth;
            localRecP->height = SDKFileInfo8->vidInfo.imageHeight;
            
            localRecP->frameRateNum = SDKFileInfo8->vidScale;
            localRecP->frameRateDen = SDKFileInfo8->vidSampleSize;
                        
            if(localRecP->zpng) {
                delete[] localRecP->zpng;
                localRecP->zpng  = nullptr;
            }
            
            if(localRecP->buffer) {
                delete[] localRecP->buffer;
                localRecP->buffer  = nullptr;
            }
            
            if(localRecP->decoder) {
                delete localRecP->decoder;
                localRecP->decoder = nullptr;
            }
            
            localRecP->zpng = new unsigned char[localRecP->width*localRecP->height*4];
            localRecP->buffer = new unsigned char[localRecP->width*localRecP->height*4+localRecP->height];
            localRecP->decoder = new Filter::Decoder(localRecP->width,localRecP->height,Filter::RGBA);
                        
        }
        else {
            result = imFileOpenFailed;
        }
        
    }
    else {
        result = imFileOpenFailed;
    }
    
    stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));
    
    return result;
}

static prMALError SDKPreferredFrameSize(imStdParms *stdparms, imPreferredFrameSizeRec *preferredFrameSizeRec) {
    prMALError result = imIterateFrameSizes;
    ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(preferredFrameSizeRec->inPrivateData);
    stdparms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));
    ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>(*ldataH);

    switch(preferredFrameSizeRec->inIndex) {
        case 0:
            preferredFrameSizeRec->outWidth = localRecP->width;
            preferredFrameSizeRec->outHeight = localRecP->height;
            result = malNoError;
            break;
    
        default:
            result = imOtherErr;
    }

    stdparms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));

    return result;
}


static prMALError SDKGetSourceVideo(imStdParms *stdParms, imFileRef fileRef, imSourceVideoRec *sourceVideoRec) {
    
    ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(sourceVideoRec->inPrivateData);
    stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));
    ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );

    PrTime ticksPerSecond = 0;
    localRecP->TimeSuite->GetTicksPerSecond(&ticksPerSecond);
    
    csSDK_int32 theFrame = 0;
    if(localRecP->frameRateDen==0) {
        theFrame = 0;
    }
    else {
        PrTime ticksPerFrame = (ticksPerSecond * (PrTime)localRecP->frameRateDen) / (PrTime)localRecP->frameRateNum;
        theFrame = (csSDK_int32)(sourceVideoRec->inFrameTime/ticksPerFrame);
    }

    prMALError result = localRecP->PPixCacheSuite->GetFrameFromCache(localRecP->importerID,0,theFrame,1,sourceVideoRec->inFrameFormats,sourceVideoRec->outFrame,NULL,NULL);

    if(result != suiteError_NoError) {
        result = malNoError;
        
        imFrameFormat *frameFormat = &sourceVideoRec->inFrameFormats[0];
        prRect theRect;
        if(frameFormat->inFrameWidth==0&&frameFormat->inFrameHeight==0) {
            frameFormat->inFrameWidth = localRecP->width;
            frameFormat->inFrameHeight = localRecP->height;
        }
        
        prSetRect(&theRect,0,0,frameFormat->inFrameWidth,frameFormat->inFrameHeight);
        
        PPixHand ppix;
        localRecP->PPixCreatorSuite->CreatePPix(&ppix,PrPPixBufferAccess_ReadWrite,frameFormat->inPixelFormat,&theRect);
        
        if(frameFormat->inPixelFormat == PrPixelFormat_BGRA_4444_8u) {
            
            char *pixelAddress = nullptr;
            localRecP->PPixSuite->GetPixels(ppix,PrPPixBufferAccess_ReadWrite,&pixelAddress);
            csSDK_int32 rowBytes = 0;
            localRecP->PPixSuite->GetRowBytes(ppix,&rowBytes);
            
            const int w = localRecP->width;
            const int h = localRecP->height;
            
            assert(frameFormat->inFrameWidth==w);
            assert(frameFormat->inFrameHeight==h);
                        
            int frame_head = localRecP->frame_head[theFrame];
            int frame_size = localRecP->frame_size[theFrame];
                        
            ByteCount actualCount = 0;
            FSReadFork((FSIORefNum)CAST_REFNUM(fileRef),fsFromStart,frame_head,frame_size,localRecP->zpng,&actualCount);
            ZSTD_decompress(localRecP->buffer,(w*h)*4+h,localRecP->zpng,frame_size);
            localRecP->decoder->decode((unsigned char *)localRecP->buffer,localRecP->depth);

            unsigned int *bytes = (unsigned int *)localRecP->decoder->bytes();

            for(int i=0; i<h; i++) {
                unsigned char *p = (unsigned char *)pixelAddress+(i*rowBytes);
                for(int j=0; j<w; j++) {
                    unsigned int pixel = bytes[((h-1)-i)*w+j];
                    *p++ = (pixel>>16)&0xFF;
                    *p++ = (pixel>>8)&0xFF;
                    *p++ = (pixel)&0xFF;
                    *p++ = (pixel>>24)&0xFF;;
                }
            }
            
            localRecP->PPixCacheSuite->AddFrameToCache(localRecP->importerID,0,ppix,theFrame,NULL,NULL);
            *sourceVideoRec->outFrame = ppix;
        }
        else {
            assert(false);
        }
    }

    stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));

    return result;
}

PREMPLUGENTRY DllExport xImportEntry(csSDK_int32 selector, imStdParms *stdParms, void *param1, void *param2) {
    
    prMALError result = imUnsupported;

    try{
        switch (selector) {
            case imInit:
                result = SDKInit(stdParms,reinterpret_cast<imImportInfoRec*>(param1));
                break;
            case imGetInfo8:
                result = SDKGetInfo8(stdParms,reinterpret_cast<imFileAccessRec8*>(param1),reinterpret_cast<imFileInfoRec8*>(param2));
                break;
            case imOpenFile8:
                result = SDKOpenFile8(stdParms,reinterpret_cast<imFileRef*>(param1),reinterpret_cast<imFileOpenRec8*>(param2));
                break;
            case imQuietFile:
                result = SDKQuietFile(stdParms,reinterpret_cast<imFileRef*>(param1),param2);
                break;
            case imCloseFile:
                result = SDKCloseFile(stdParms,reinterpret_cast<imFileRef*>(param1),param2);
                break;
            case imGetIndFormat:
                result = SDKGetIndFormat(stdParms,reinterpret_cast<csSDK_size_t>(param1),reinterpret_cast<imIndFormatRec*>(param2));
                break;
            case imGetIndPixelFormat:
                result = SDKGetIndPixelFormat(stdParms,reinterpret_cast<csSDK_size_t>(param1),reinterpret_cast<imIndPixelFormatRec*>(param2));
                break;
            case imGetSupports8:
                result = malSupports8;
                break;
            case imGetPreferredFrameSize:
                result = SDKPreferredFrameSize(stdParms,reinterpret_cast<imPreferredFrameSizeRec*>(param1));
                break;
            case imGetSourceVideo:
                result = SDKGetSourceVideo(stdParms,reinterpret_cast<imFileRef>(param1),reinterpret_cast<imSourceVideoRec*>(param2));
                break;
            default:
                result = imUnsupported;
                break;
        }
    }
    catch(...) { result = imOtherErr; }

    return result;
}
