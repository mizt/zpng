# zpng

Dependency on [facebook](https://github.com/facebook)/[zstd](https://github.com/facebook/zstd).

Apply zstd compression after PNG-like Sub filtering process.
Full HD Realtime encoding in lossless format (ARGB) is fast enough on M1 MacBook Air.

### Encoding to zpng sequence

```
Filter *filter = new Filter(w,h);
filter->sub(src);
size_t len = ZSTD_compress(dst,(w*h)<<2,src,(w*h)<<2,1);

QTZPNGRecorder *recorder = new QTZPNGRecorder(w,h,30,@"./zpng.mov");
ecorder->add((unsigned char *)dst,len);
recorder->save();
```
### Encoding to zpng sequence

```
QTZPNGParser *parser = new QTZPNGParser(@"./zpng.mov");
NSData *zpng = parser->get(0);
ZSTD_decompress(src,(w*h)<<2,[zpng bytes],[zpng length]);

Filter *filter = new Filter(w,h);
filter->add(src);
```

### Converting to QuickTime PNG sequence

Any library is required to convert to PNG.
Using [stb_image_write](https://github.com/nothings/stb/blob/master/stb_image_write.h) as example.

```
QTZPNGParser *parser = new QTZPNGParser(@"./zpng.mov");
NSData *zpng = parser->get(0);
ZSTD_decompress(src,(w*h)<<2,[zpng bytes],[zpng length]);

Filter *filter = new Filter(w,h);
filter->add(src);

int len = 0;
unsigned char *png = stbi_write_png_to_mem((const unsigned char *)src,w<<2,w,h,4,&len);
QTPNGRecorder *recorder = new QTPNGRecorder(w,h,30,@"./png.mov");
ecorder->add(png,len);
recorder->save();
```

### Similar project

[catid](https://github.com/catid)/[Zpng](https://github.com/catid/Zpng)
