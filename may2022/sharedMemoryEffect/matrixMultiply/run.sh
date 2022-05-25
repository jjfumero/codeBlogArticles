#!/bin/bash 

sizes=( 32 64 128 256 512 1024 2048 )

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

#### Run with Host Memory
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
