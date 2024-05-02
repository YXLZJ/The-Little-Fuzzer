### The Little Fuzzer

|Functionality|Finish|
|:------------:|:-----:|
|depth control |😄|
|shortest path searching|😄|

```
mkdir build
cd build
cmake ..
make
./fuzzer -path <grammar.json> -depth <depth,recommend>=128> -o <jit c file name> 
```