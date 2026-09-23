#!/bin/bash
# ==============================================================================
# Demo Player 一体化高并发系统调优与极限性能压测脚本
# 功能：
# 1. 自动调整系统内核 TCP 参数与文件句柄限制 (ulimit -n)
# 2. 自动化安装与校验压测引擎 (wrk)
# 3. 阶梯式高并发性能压力测试 (100 ~ 3000 并发)
# 4. 规范日志输出: [INFO], [WARNING], [ERROR] (ERROR 高亮显示)
# ==============================================================================

# ANSI 颜色定义
CLR_RESET="\033[0m"
CLR_INFO="\033[1;36m"      # 青色加粗
CLR_WARN="\033[1;33m"      # 黄色加粗
CLR_ERR="\033[1;41;37m"    # 红色背景白字高亮 (强醒目)
CLR_ERR_TXT="\033[1;31m"  # 红色文字加粗
CLR_SUCCESS="\033[1;32m"  # 绿色加粗
CLR_BOLD="\033[1m"

# 统一日志打印函数
log_info() {
    echo -e "${CLR_INFO}[INFO]${CLR_RESET} $(date '+%Y-%m-%d %H:%M:%S') - $1"
}

log_warn() {
    echo -e "${CLR_WARN}[WARNING]${CLR_RESET} $(date '+%Y-%m-%d %H:%M:%S') - $1"
}

log_error() {
    echo -e "${CLR_ERR} [ERROR] ${CLR_RESET} ${CLR_ERR_TXT}$(date '+%Y-%m-%d %H:%M:%S') - $1${CLR_RESET}"
}

log_success() {
    echo -e "${CLR_SUCCESS}[SUCCESS]${CLR_RESET} $(date '+%Y-%m-%d %H:%M:%S') - $1"
}

echo -e "${CLR_BOLD}====================================================================${CLR_RESET}"
echo -e "${CLR_BOLD}🚀 Demo Player 一体化高并发调优与极限压力测试工具${CLR_RESET}"
echo -e "${CLR_BOLD}====================================================================${CLR_RESET}"

# ==============================================================================
# 阶段 1: 系统内核与并发参数自动调优 (System Tuning)
# ==============================================================================
log_info "正在检测并调优 Linux 系统内核与并发连接限制..."

# 1. 尝试解除当前会话文件句柄限制
TARGET_ULIMIT=65535
ulimit -n $TARGET_ULIMIT 2>/dev/null
CURRENT_ULIMIT=$(ulimit -n)

if [ "$CURRENT_ULIMIT" -ge "$TARGET_ULIMIT" ] 2>/dev/null; then
    log_info "当前会话文件描述符限制 (ulimit -n) 已成功提升至: $CURRENT_ULIMIT"
else
    log_warn "普通用户权限受限，尝试以 sudo 申请提权调优..."
    if command -v sudo &> /dev/null; then
        sudo sh -c "ulimit -n $TARGET_ULIMIT" 2>/dev/null
        # 持久化到 limits.conf
        echo "* soft nofile $TARGET_ULIMIT" | sudo tee -a /etc/security/limits.conf > /dev/null
        echo "* hard nofile $TARGET_ULIMIT" | sudo tee -a /etc/security/limits.conf > /dev/null
        log_info "已将 $TARGET_ULIMIT 写入 /etc/security/limits.conf"
    else
        log_error "未检测到 sudo 命令，无法修改系统全局 limits.conf！"
    fi
fi

# 2. 自动写入 ~/.bashrc 保证后续登录永久生效
if ! grep -q "ulimit -n $TARGET_ULIMIT" "$HOME/.bashrc" 2>/dev/null; then
    echo "ulimit -n $TARGET_ULIMIT" >> "$HOME/.bashrc"
    log_info "已将 'ulimit -n $TARGET_ULIMIT' 自动追加至 ~/.bashrc (永久生效)"
fi

# 3. 调优内核 TCP 参数
log_info "正在调优内核 TCP 参数 (somaxconn, tcp_tw_reuse, tcp_max_syn_backlog)..."
if command -v sudo &> /dev/null; then
    sudo sysctl -w fs.file-max=1000000 > /dev/null 2>&1
    sudo sysctl -w net.core.somaxconn=65535 > /dev/null 2>&1
    sudo sysctl -w net.core.netdev_max_backlog=65535 > /dev/null 2>&1
    sudo sysctl -w net.ipv4.tcp_max_syn_backlog=65535 > /dev/null 2>&1
    sudo sysctl -w net.ipv4.tcp_tw_reuse=1 > /dev/null 2>&1
    sudo sysctl -w net.ipv4.tcp_fin_timeout=15 > /dev/null 2>&1
    sudo sysctl -w net.ipv4.ip_local_port_range="1024 65535" > /dev/null 2>&1
    log_success "内核 TCP 参数调优生效完成！"
else
    log_warn "无 sudo 权限，跳过内核 sysctl 参数设置（若在容器或受限环境可忽略）。"
fi

echo ""

# ==============================================================================
# 阶段 2: 压测环境探针与依赖检查
# ==============================================================================
TARGET_HOST="http://127.0.0.1:8080"
DURATION="10s"
THREADS=$(nproc 2>/dev/null || echo 4)

log_info "正在检查目标播放器服务状态: $TARGET_HOST ..."
curl -s --connect-timeout 2 "$TARGET_HOST" > /dev/null 2>&1
if [ $? -ne 0 ]; then
    log_error "目标服务 [$TARGET_HOST] 连接失败！请确保已在后台启动 ./bin/demo_player 服务！"
    echo -e "${CLR_WARN}👉 启动命令提示: ./bin/demo_player &${CLR_RESET}"
    exit 1
else
    log_success "目标服务响应正常，服务处于存活状态！"
fi

# 检查 wrk 工具
if ! command -v wrk &> /dev/null; then
    log_warn "未检测到 wrk 压测引擎，正在尝试自动安装 (sudo apt install -y wrk)..."
    if command -v sudo &> /dev/null; then
        sudo apt update > /dev/null 2>&1 && sudo apt install -y wrk > /dev/null 2>&1
    fi
    
    if ! command -v wrk &> /dev/null; then
        log_error "wrk 自动安装失败！请手动执行: sudo apt install wrk 或使用自带的 python3 scripts/stress_test.py"
        exit 1
    fi
    log_success "wrk 压测工具安装就绪！"
else
    log_info "检测到 wrk 压测引擎可用: $(which wrk)"
fi

echo ""

# ==============================================================================
# 阶段 3: 执行阶梯式高并发压力测试
# ==============================================================================
CONCURRENCY_LEVELS=(100 500 1000 2000 2500 3000)

log_info "开始执行多阶段阶梯压测 (线程数: $THREADS, 单轮时长: $DURATION)..."
echo -e "${CLR_BOLD}--------------------------------------------------------------------${CLR_RESET}"
echo -e "🎯 测试模块 A: 【静态资源高并发分发能力】 (GET /index.html)"
echo -e "${CLR_BOLD}--------------------------------------------------------------------${CLR_RESET}"

for c in "${CONCURRENCY_LEVELS[@]}"; do
    log_info "正在施加并发压力 -> 连接数: ${CLR_BOLD}$c${CLR_RESET}, 工作线程: $THREADS ..."
    RESULT=$(wrk -t$THREADS -c$c -d$DURATION --latency "$TARGET_HOST/index.html" 2>&1)
    
    # 检测压测结果中是否有错误
    if echo "$RESULT" | grep -q "Socket errors"; then
        ERR_INFO=$(echo "$RESULT" | grep "Socket errors")
        log_error "高并发压力下出现网络错误: $ERR_INFO"
    fi
    
    # 提取核心数据输出
    echo "$RESULT" | grep -E "Requests/sec|Latency|Transfer/sec|Socket errors" | sed 's/^[ \t]*/   📌 /'
    echo ""
done

echo -e "${CLR_BOLD}--------------------------------------------------------------------${CLR_RESET}"
echo -e "🎯 测试模块 B: 【MySQL + 内存 LRU 歌词缓存并发吞吐能力】 (GET /api/song/lyric?id=186016)"
echo -e "${CLR_BOLD}--------------------------------------------------------------------${CLR_RESET}"

# 预热一次缓存
curl -s "$TARGET_HOST/api/song/lyric?id=186016" > /dev/null 2>&1

for c in "${CONCURRENCY_LEVELS[@]}"; do
    log_info "正在施加并发压力 -> 连接数: ${CLR_BOLD}$c${CLR_RESET}, 工作线程: $THREADS ..."
    RESULT=$(wrk -t$THREADS -c$c -d$DURATION --latency "$TARGET_HOST/api/song/lyric?id=186016" 2>&1)
    
    if echo "$RESULT" | grep -q "Socket errors"; then
        ERR_INFO=$(echo "$RESULT" | grep "Socket errors")
        log_error "高并发压力下出现网络错误: $ERR_INFO"
    fi
    
    echo "$RESULT" | grep -E "Requests/sec|Latency|Transfer/sec|Socket errors" | sed 's/^[ \t]*/   📌 /'
    echo ""
done

echo -e "${CLR_BOLD}====================================================================${CLR_RESET}"
log_success "全流程高并发压力测试执行完毕！"
echo -e "💡 ${CLR_BOLD}核心指标评估标准：${CLR_RESET}"
echo -e "   1. ${CLR_INFO}Requests/sec (QPS)${CLR_RESET}: 每秒处理请求数，越高代表吞吐量越强。"
echo -e "   2. ${CLR_INFO}Latency Avg / 99%${CLR_RESET}: 平均延迟与 99 分位最大延迟（建议 < 30ms）。"
echo -e "   3. ${CLR_INFO}Socket errors${CLR_RESET}: 若全为 0，说明在当前并发量下服务器运行极度稳定无丢包。"
echo -e "${CLR_BOLD}====================================================================${CLR_RESET}"
