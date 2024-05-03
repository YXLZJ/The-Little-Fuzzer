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
./fuzzer -path <grammar.json> -depth <depth,recommend>=128> -o <jit c file name> [--show(enable output example)]
```

## New Feature
* compress graph when dealing with grammar
* Backforward updating shortest path(from terminal to non-terminal and expression, the worst time complexity is O(N^2))