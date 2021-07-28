### Build zstd

```
cd ./zstd
emcmake make
```

### Bulid decode.js

```
cd ./decoding
em++ \
  -O3 \
  -std=c++11 \
  -Wc++11-extensions \
  --memory-init-file 0 \
  -s VERBOSE=1 \
  -s WASM=0 \
  -I../../ \
  -I../include \
  -L../libs \	
  ../libs/libzstd.a \
  -s EXPORTED_FUNCTIONS="['_decode','_malloc']" \
  -s EXTRA_EXPORTED_RUNTIME_METHODS="['cwrap']" \
  ./decode.cpp \
  -o ./decode.js
```

