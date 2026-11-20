# Allocator Benchmark Report

Newton Ntoiti — SCT212-0180/2024
Ubuntu 24.04.5 LTS
 GCC 13.3.0



- Operations: 50,000
- Slots: 512
- Random seed: 20261120
- Timing: CLOCK_MONOTONIC

## Results

allocator:          first fit
runtime:            0.060871 s	
heap bytes:          1,179,648
free bytes:         824,992
free blocks:        3
largest free:       354,384	
fragmentation:       0.5704

allocator:          best fit
runtime:            0.050602 s
heap bytes:         1,179,648
free bytes:         824,992	
free blocks:        3
largest free:        354,384	
fragmentation:      0.5704

allocator:          system malloc
runtime:            0.002002 s
heap bytes:         n/a
free bytes:         n/a
free blocks:         n/a
largest free:        n/a
fragmentation:       n/a



In this benchmark run, the system allocator completed the workload considerably faster than the custom allocator. First-fit recorded 0.050574 seconds, while best-fit recorded 0.048833 seconds. Both custom policies reported identical heap usage and fragmentation statistics.

The custom allocator obtained 1,179,648 bytes from the operating system, with 824,992 bytes of free payload distributed across three blocks. The largest free block was 354,384 bytes. External fragmentation was 0.5704, calculated using 1 - (largest_free / free_bytes).

Best-fit was marginally faster in this particular run, but this does not establish that it is always faster. Results can vary between executions. System malloc statistics are not directly exposed by this project, so only execution time is compared for that implementation.