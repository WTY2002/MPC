#!/bin/bash

echo "Releasing ports 5550-5560..."
for port in {5550..5560}; do
    pid=$(lsof -ti :$port)
    if [ -n "$pid" ]; then
        echo "Killing process $pid on port $port"
        kill -9 $pid
    fi
done
sleep 5

for i in {1..5}; do
    echo "Starting Node $i..."
    gnome-terminal -- bash -c "./cmake-build-release/ReShareNode $i; exec bash" &
done

echo "All nodes started."
