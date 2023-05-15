#!/bin/bash

set -Eeuo pipefail

num_items=100000
samples=10

source_file="$(realpath producer_consumer.cpp)"
executable="bin/producer_consumer_$(md5sum "$source_file" | cut -c -8)"

if [[ ! -f "$executable" || "$source_file" -nt "$executable" ]]; then
    echo "Compiling source"
    mkdir -p bin
    g++ -o "$executable" -O2 -pthread "$source_file"
fi

echo "buffer_size,producer_threads,consumer_threads,avg_time" > results.csv

for buffer_size in 1 10 100 1000
do
  for num_producers in 1 2 4 8
  do
    for num_consumers in 1 2 4 8
    do
      if [[ ($num_producers -eq 1) || ($num_consumers -eq 1) ]]
      then
        total_time=0

        avg_time=$(./"$executable" $buffer_size $num_producers $num_consumers $num_items $samples)

        echo "$buffer_size,$num_producers,$num_consumers,$avg_time" >> results.csv

        echo "buffer_size: $buffer_size, producers: $num_producers, consumers: $num_consumers, avg_time: $avg_time"
      fi
    done
  done
done
