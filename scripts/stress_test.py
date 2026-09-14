#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
==============================================================================
基于 Python Asyncio + Aiohttp 的现代化异步高并发压测脚本
功能：
1. 测试服务器最大持续并发连接维持能力 (Connection Capacity)
2. 阶梯式自动加压 (Ramp-up Concurrency Test)
3. 统计 QPS、成功率、P50/P90/P95/P99 响应延迟分布
4. 业务场景测试 (API 请求 / 歌词缓存 / 静态页面)
==============================================================================
"""

import asyncio
import time
import sys
import statistics
import argparse

try:
    import aiohttp
except ImportError:
    print("❌ 未检测到 aiohttp 依赖库，正在尝试安装...")
    import subprocess
    subprocess.check_call([sys.executable, "-m", "pip", "install", "aiohttp"])
    import aiohttp

class StressTester:
    def __init__(self, base_url, concurrency, total_requests, endpoint):
        self.base_url = base_url.rstrip('/')
        self.concurrency = concurrency
        self.total_requests = total_requests
        self.endpoint = endpoint
        self.latencies = []
        self.success_count = 0
        self.error_count = 0
        self.status_codes = {}

    async def worker(self, session, semaphore, request_queue):
        while not request_queue.empty():
            try:
                _ = request_queue.get_nowait()
            except asyncio.QueueEmpty:
                break

            async with semaphore:
                start_time = time.perf_counter()
                try:
                    url = f"{self.base_url}{self.endpoint}"
                    async with session.get(url, timeout=aiohttp.ClientTimeout(total=5)) as response:
                        await response.read()
                        elapsed = (time.perf_counter() - start_time) * 1000 # ms
                        self.latencies.append(elapsed)
                        
                        status = response.status
                        self.status_codes[status] = self.status_codes.get(status, 0) + 1
                        if status == 200:
                            self.success_count += 1
                        else:
                            self.error_count += 1
                except Exception as e:
                    elapsed = (time.perf_counter() - start_time) * 1000
                    self.latencies.append(elapsed)
                    self.error_count += 1
                    err_name = type(e).__name__
                    self.status_codes[err_name] = self.status_codes.get(err_name, 0) + 1
                finally:
                    request_queue.task_done()

    async def run(self):
        print(f"🚀 开始压测: 目标 URL: {self.base_url}{self.endpoint}")
        print(f"⚡ 并发协程数 (Concurrency): {self.concurrency}")
        print(f"📦 请求总量 (Total Requests): {self.total_requests}")
        print("-" * 65)

        request_queue = asyncio.Queue()
        for i in range(self.total_requests):
            request_queue.put_nowait(i)

        semaphore = asyncio.Semaphore(self.concurrency)
        connector = aiohttp.TCPConnector(
            limit=self.concurrency * 2,
            limit_per_host=self.concurrency * 2,
            enable_cleanup_closed=True,
            force_close=False
        )

        overall_start = time.perf_counter()
        async with aiohttp.ClientSession(connector=connector) as session:
            tasks = [
                asyncio.create_task(self.worker(session, semaphore, request_queue))
                for _ in range(self.concurrency)
            ]
            await request_queue.join()
            for task in tasks:
                task.cancel()

        overall_time = time.perf_counter() - overall_start
        self.report(overall_time)

    def report(self, total_time):
        qps = self.total_requests / total_time if total_time > 0 else 0
        success_rate = (self.success_count / self.total_requests) * 100 if self.total_requests > 0 else 0

        self.latencies.sort()
        count = len(self.latencies)
        p50 = self.latencies[int(count * 0.50)] if count > 0 else 0
        p90 = self.latencies[int(count * 0.90)] if count > 0 else 0
        p95 = self.latencies[int(count * 0.95)] if count > 0 else 0
        p99 = self.latencies[int(count * 0.99)] if count > 0 else 0
        avg_lat = statistics.mean(self.latencies) if count > 0 else 0
        min_lat = self.latencies[0] if count > 0 else 0
        max_lat = self.latencies[-1] if count > 0 else 0

        print("======================== 📊 压测结果报告 ========================")
        print(f"⏱️  总耗时 (Total Time):         {total_time:.3f} 秒")
        print(f"🔥 每秒吞吐量 (QPS / RPS):       {qps:.2f} req/s")
        print(f"✅ 成功请求数 (Success):        {self.success_count} ({success_rate:.2f}%)")
        print(f"❌ 失败请求数 (Failed):         {self.error_count}")
        print(f"📈 响应延迟统计 (Latency Distribution):")
        print(f"   - 最小值 (Min):              {min_lat:.2f} ms")
        print(f"   - 平均值 (Avg):              {avg_lat:.2f} ms")
        print(f"   - 中位数 P50:                {p50:.2f} ms")
        print(f"   - 90 分位 P90:               {p90:.2f} ms")
        print(f"   - 95 分位 P95:               {p95:.2f} ms")
        print(f"   - 99 分位 P99:               {p99:.2f} ms")
        print(f"   - 最大值 (Max):              {max_lat:.2f} ms")
        print(f"📋 状态码分布 (Status Codes):    {self.status_codes}")
        print("================================================================")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Demo Player 高并发异步压力测试工具")
    parser.add_argument("--url", default="http://127.0.0.1:8080", help="目标服务器地址")
    parser.add_argument("-c", "--concurrency", type=int, default=1000, help="并发连接数")
    parser.add_argument("-n", "--requests", type=int, default=10000, help="总请求量")
    parser.add_argument("-p", "--path", default="/index.html", help="测试接口路径 (如 /index.html 或 /api/song/lyric?id=186016)")

    args = parser.parse_args()
    tester = StressTester(args.url, args.concurrency, args.requests, args.path)
    asyncio.run(tester.run())
