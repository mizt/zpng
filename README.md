# zpng

Dependency on [facebook](https://github.com/facebook)/[zstd](https://github.com/facebook/zstd).

Apply zstd compression after PNG-like filtering process.    
Full HD Realtime encoding in lossless format (ARGB) is fast enough on M1 MacBook Air.


### Encoding to zpng sequence

```
#import "zstd.h"
#import "Filter.h"
#import "QTZPNG.h"
```

```
Filter *filter = new Filter(w,h,Filter::RGB,false);
filter->encode(src,Filter::ARGB,5);
size_t len = ZSTD_compress(dst,(w*h)*Filter::ARGB,src,(w*h)*Filter::RGB,1);
QTZPNGRecorder *recorder = new QTZPNGRecorder(w,h,30,@"./zpng.mov");
recorder->add((unsigned char *)dst,len);
recorder->save();
```
### Decoding to zpng sequence

```
QTZPNGParser *parser = new QTZPNGParser(@"./zpng.mov");
NSData *zpng = parser->get(0);
ZSTD_decompress(src,(w*h)*Filter::ARGB,[zpng bytes],[zpng length]);
Filter *filter = new Filter(w,h,Filter::RGB,false);
filter->decode(src,Filter::ARGB);
```

### Converting to QuickTime PNG sequence

Any library is required to convert to PNG.    
Using [stb_image_write](https://github.com/nothings/stb/blob/master/stb_image_write.h) as example.

```
QTZPNGParser *parser = new QTZPNGParser(@"./zpng.mov");
NSData *zpng = parser->get(0);
ZSTD_decompress(src,(w*h)<<2,[zpng bytes],[zpng length]);

ABGRFilter *filter = new ABGRFilter(w,h);
filter->add(src);

int len = 0;
unsigned char *png = stbi_write_png_to_mem((const unsigned char *)src,w<<2,w,h,4,&len);
QTPNGRecorder *recorder = new QTPNGRecorder(w,h,30,@"./png.mov");
recorder->add(png,len);
recorder->save();
```

### Browser Support

Support for decoding via emscripten.    
[https://github.com/mizt/zpng/tree/master/emscripten/](https://github.com/mizt/zpng/tree/master/emscripten/)

### Quick Look

File Extension is 'zpng'.

[https://github.com/mizt/zpng/tree/master/QuickLook/](https://github.com/mizt/zpng/tree/master/QuickLook/)

```
mdls test.zpng
```

Copy `kMDItemContentType` value  to `QLSupportedContentTypes` in Info.plist

### Premiere Pro importers

`/Library/Application\ Support/Adobe/Common/Plug-ins/7.0/MediaCore/zpng_File_Import.bundle`

See aloso  [https://github.com/fnordware/AdobeOgg/tree/theora/](https://github.com/fnordware/AdobeOgg/tree/theora/)

Directly import in After Effects via Premiere Pro Importer.    
[https://github.com/mizt/zpng/tree/master/premiere/](https://github.com/mizt/zpng/tree/master/premiere/)

File Extension is 'zpng'.

```
char formatname[255] = "zpng";
char shortname[32] = "zpng";
char platformXten[256] = "zpng\0";
```

```
resource 'IMPT' (1000)
{
    0x7A706E67 // 'zpng'
};
```

### Decoding corrupted data example

[https://mizt.github.io/zpng/err/](https://mizt.github.io/zpng/err/)

### Similar project

[catid](https://github.com/catid)/[Zpng](https://github.com/catid/Zpng)

