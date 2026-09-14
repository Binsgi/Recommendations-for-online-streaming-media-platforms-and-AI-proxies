#!/bin/bash
# ==============================================================================
# Linux 内核与系统网络参数调优脚本 (突破高并发瓶颈必备)
# 适用环境: Ubuntu 22.04 / Debian / CentOS
# ==============================================================================

echo "=========================================================="
echo "🚀 正在优化系统内核与网络参数以支持 10,000+ 高并发连接..."
echo "=========================================================="

# 1. 调整用户级最大打开文件描述符 (文件句柄数)
ulimit -n 1000000 2>/dev/null
echo "* soft nofile 1000000" | sudo tee -a /etc/security/limits.conf > /dev/null
echo "* hard nofile 1000000" | sudo tee -a /etc/security/limits.conf > /dev/null

# 2. 调优内核 TCP 参数
sudo sysctl -w fs.file-max=1000000 > /dev/null
# 扩大 TCP 监听队列上限 (somaxconn)
sudo sysctl -w net.core.somaxconn=65535 > /dev/null
sudo sysctl -w net.core.netdev_max_backlog=65535 > /dev/null
# 扩大 SYN 半连接队列
sudo sysctl -w net.ipv4.tcp_max_syn_backlog=65535 > /dev/null
# 允许复用 TIME_WAIT 状态的套接字 (大幅降低压测时端口耗尽风险)
sudo sysctl -w net.ipv4.tcp_tw_reuse=1 > /dev/null
# 加快 TIME_WAIT 回收
sudo sysctl -w net.ipv4.tcp_fin_timeout=15 > /dev/null
# 扩大可用临时端口范围
sudo sysctl -w net.ipv4.ip_local_port_range="1024 65535" > /dev/null
# 扩大读写缓冲区
sudo sysctl -w net.ipv4.tcp_rmem="4096 87380 16777216" > /dev/null
sudo sysctl -w net.ipv4.tcp_wmem="4096 65536 16777216" > /dev/null

echo "✅ 系统内核与 Socket 参数调优完成！当前 ulimit -n: $(ulimit -n)"
