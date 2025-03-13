#!/bin/bash

# 释放端口 5550-5560
echo "Releasing ports 5550-5560..."
for port in {5550..5560}; do
    pid=$(lsof -ti :$port)
    if [ -n "$pid" ]; then
        echo "Killing process $pid on port $port"
        kill -9 $pid
    fi
done
sleep 5

# 创建性能分析输出目录
mkdir -p perf_data

# 启动节点
for i in {1..5}; do
    echo "Starting Node $i..."

    # 使用 perf 记录性能数据
    gnome-terminal -- bash -c "
        ulimit -u 1000000
        ulimit -n 65536
        perf record -F 99 -g -o perf_data/node_${i}.data ./cmake-build-release/FcnnNode $i 10
        exec bash" &
done

echo "All nodes started."

# 等待所有节点完成
wait

# 为每个节点生成性能报告
for i in {1..5}; do
    echo "Generating performance report for Node $i..."
    perf report -i perf_data/node_${i}.data > perf_data/report_${i}.txt
done