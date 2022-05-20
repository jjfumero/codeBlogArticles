## Performance of Shared Memory

### How to compile?

```
# Setup LEVEL_ZERO_ROOT to the level zero directory

$ export CPLUS_INCLUDE_PATH=$LEVEL_ZERO_ROOT/include:$CPLUS_INCLUDE_PATH
$ export LD_LIBRARY_PATH=$LEVEL_ZERO_ROOT/build/lib:$LD_LIBRARY_PATH 

$ make 
$ ./levelZeroShared <matrixSize>
```
