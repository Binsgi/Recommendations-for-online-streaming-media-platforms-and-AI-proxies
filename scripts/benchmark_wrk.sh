#!/bin/bash
# ==============================================================================
# 基于 wrk 的自动化阶梯式高并发压力测试脚本
# ==============================================================================

TARGET_HOST="http://127.0.0.1:8080"
DURATION="10s" # 每个阶段测试时长
THREADS=$(nproc) # 压测线程数匹配 CPU 核心数

echo "=========================================================="
echo "🎯 Demo Player 高并发极限自动化压测工具 (wrk Engine)"
echo "=========================================================="

# 1. 检查是否安装 wrk
if ! command -v wrk &> /dev/null; then
    echo "⚠️ 未检测到 wrk 工具，正在为您自动安装..."
    sudo apt update && sudo apt install -y wrk
    if [ $? -ne 0 ]; then
        echo "❌ wrk 安装失败，请手动执行: sudo apt install wrk"
        exit 1
    fi
fi

# 2. 检查目标服务是否存活
curl -s --connect-timeout 2 "$TARGET_HOST" > /dev/null
if [ $? -ne 0 ]; then
    echo "❌ 目标服务 $TARGET_HOST 未启动！请先在后台运行 ./bin/demo_player"
    exit 1
fi

echo "✅ 目标服务存活: $TARGET_HOST"
echo "⚙️ 压测线程数: $THREADS, 单轮压测时长: $DURATION"
echo ""

# 阶梯并发连接数测试队列
CONCURRENCY_LEVELS=(100 500 1000 2000 5000 10000)

echo "----------------------------------------------------------------------------------"
echo "📋 测试场景 1: 【静态文件高并发分发测试】 (GET /index.html)"
echo "----------------------------------------------------------------------------------"
for c in "${CONCURRENCY_LEVELS[@]}"; do
    echo "▶️ [正在压测] 并发连接数: $c, 线程数: $THREADS..."
    wrk -t$THREADS -c$c -d$DURATION --latency "$TARGET_HOST/index.html" | grep -E "Requests/sec|Latency|Transfer/sec|Socket errors|non-2xx"
    echo ""
done

echo "----------------------------------------------------------------------------------"
echo "📋 测试场景 2: 【LRU + MySQL 歌词缓存并发吞吐测试】 (GET /api/song/lyric?id=186016)"
echo "----------------------------------------------------------------------------------"
# 先预热一次缓存
curl -s "$TARGET_HOST/api/song/lyric?id=186016" > /dev/null

for c in "${CONCURRENCY_LEVELS[@]}"; do
    echo "▶️ [正在压测] 并发连接数: $c, 线程数: $THREADS..."
    wrk -t$THREADS -c$c -d$DURATION --latency "$TARGET_HOST/api/song/lyric?id=186016" | grep -E "Requests/sec|Latency|Transfer/sec|Socket errors|non-2xx"
    echo ""
done

echo "=========================================================="
echo "🎉 阶梯压测全部完成！"
echo "💡 指标解读："
echo "   - Requests/sec (QPS/RPS): 系统每秒处理的最大请求数"
echo "   - Latency (Avg / 99%): 平均响应时间与 99 分位最大延迟"
echo "   - Socket errors: 若出现大量 connect/read 错误，说明达到连接数/文件句柄上限"
echo "=========================================================="
