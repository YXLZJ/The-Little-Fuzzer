### The Little Fuzzer

|Functionality|Finish|
|:------------:|:-----:|
|depth control |😄|
|shortest path searching in advance|😄|

```
mkdir build
cd build
cmake ..
make
./fuzzer -path <grammar.json> -depth <depth,recommend>=128> -o <jit c file name> [--show(enable output example)]
```
example
```
./fuzzer -path ../html.json -depth 256 -o html.c
```
## New Feature
* Compress graph when establishing grammatical rules.
* Bottom-up update of shortest path before Fuzzing(from terminal to non-terminal and expression(rules), the worst time complexity is O(N^2))