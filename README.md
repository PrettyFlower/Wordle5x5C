C port of https://github.com/PrettyFlower/Wordle5x5CSharp

Debug build:
```
gcc -c allocation_block.c allocator.c int_hashset.c main.c prime_utils.c -g
```

Release build:
```
gcc -c allocation_block.c allocator.c int_hashset.c main.c prime_utils.c -O3
```

Link step:
```
gcc allocation_block.o allocator.o int_hashset.o main.o prime_utils.o -o wordle5x5c -lm -lpthread
```

Benchmark:
```
hyperfine --warmup 3 ./wordle5x5c
```
