# Packed ten-bit conversion

`RKPacked10.h` is the canonical implementation embedded in the SDK and legacy
Codec2 patches. Both builders check that the applied header matches this file.
Changing only this file is insufficient: refresh both patch payloads before a build.

The fast path unpacks sixteen samples from exactly twenty input bytes with NEON,
converts into an aligned cacheable row, and copies the visible row to the mapped
destination. It preserves all ten bits, scalar tails, row padding, mapper locking
and DMA fences. Disabling it selects the previous scalar conversion.

Build the independent test with Android NDK Clang for each ABI, API32:

```sh
armv7a-linux-androideabi32-clang++ -O2 -std=c++17 -static-libstdc++ packed10_test.cpp -o packed10_test32
aarch64-linux-android32-clang++ -O2 -std=c++17 -static-libstdc++ packed10_test.cpp -o packed10_test64
```

Push the executables to a local Android diagnostic directory, grant execute
permission and run them there. Each covers 140 cases for both conversion paths,
including an independent bit-level reference, all 1024 sample values, widths and
tails, strides, unaligned output, guard pages, untouched padding and invalid inputs.
The cached-memory timing is a microbenchmark; it is not end-to-end playback speed.
