# SYSC4001_A3P2

This project simulates multiple TAs marking exams at the same time using shared memory and semaphores. Part 2a shows what happens without any synchronization (race conditions), and Part 2b fixes everything using semaphores. There are also two small demo programs that show deadlock and livelock.

## How to Run

### Part 2a
```bash
gcc -o part2a part2a_101236784_101272210.c
./part2a <num_TAs>

### Part 2b

gcc -o part2b part2b_101236784_101272210.c -pthread
./part2b <num_TAs>

### Deadlock & Livelock Demos
gcc -o deadlock part2b_deadlock.c -pthread
gcc -o livelock part2b_livelock.c -pthread

./deadlock 2
./livelock 2
```

## What Each File Does
part2a_101236784_101272210.c – unsynchronized version (race conditions)
part2b_101236784_101272210.c – synchronized version (uses semaphores)
part2b_deadlock.c – intentional deadlock example
part2b_livelock.c – livelock example
rubric.txt – initial rubric
exams/ – exam files with 4-digit student numbers
reportPartC.pdf – write-up for Part 2c

## Key Idea 
Part 2a runs with race conditions.
Part 2b uses semaphores to fix them.
The program stops when it reaches student 9999.

