# Final Validation Record

Student: Newton Ntoiti
Registration: SCT212-0180/2024
Environment: Ubuntu 24.04.5 LTS
Compiler: GCC 13.3.0

## Official test harness

Command: make test
Result: 29 passed, 0 failed

## UndefinedBehaviorSanitizer

Command: make ubsan
Result: 29 passed, 0 failed

## Benchmark

Command: make bench

Operations: 50,000
Slots: 512
Seed: 20261120

Custom first-fit: 0.063255 seconds
Custom best-fit: 0.058341 seconds
System malloc/free: 0.002438 seconds

The benchmark completed successfully.
