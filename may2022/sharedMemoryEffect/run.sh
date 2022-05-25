#!/bin/bash 

## Running from 64MB to 4GB per buffer
sizes=( 16777216 33554432 67108864 134217728 268435456 536870912 1073741824 )

#### Run with Shared Memory
for size in ${sizes[@]}
do
    echo "Running with Size: ${size}"
    ./levelZeroShared s $size >> SHARED.log
done

#### Run with Device Memory
for size in ${sizes[@]}
do
    echo "Running with Size: ${size}"
    ./levelZeroShared d $size >> DEVICE.log
done

#### Run with Combined Device/Host Memory
for size in ${sizes[@]}
do
    echo "Running with Size: ${size}"
    ./levelZeroShared c $size >> COMBINED_DEVICE_HOST.log
done

#### Run with Host Only Memory
for size in ${sizes[@]}
do
    echo "Running with Size: ${size}"
    ./levelZeroShared h $size >> HOST.log
done

