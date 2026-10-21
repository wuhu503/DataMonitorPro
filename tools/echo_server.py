#!/usr/bin/env python3
"""
传输层测试用的回声服务器。

不是 Modbus 从站——它不懂功能码，只用来验证 TcpTransport 的
write() / readExact() 在三种网络行为下是否正确：

  连接 1  echo    : 收到什么原样回什么
  连接 2  split   : 先回前 4 字节，停 200ms，再回后 4 字节（制造"半包"）
  连接 3  silent  : 收到不回（制造"超时"）

跑完三次连接后自动退出。
"""

import socket
import time

HOST = "127.0.0.1"
PORT = 15502

# 一帧 Modbus TCP 读请求的长度：
#   MBAP: 事务号2 + 协议号2 + 长度2 + 单元号1 = 7
#   PDU : 功能码1 + 起始地址2 + 数量2      = 5
FRAME_LEN = 12


def recv_exact(conn, n):
    """凑齐 n 个字节；对端关闭则提前返回已收到的部分。"""
    buf = b""
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            break
        buf += chunk
    return buf


def main():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((HOST, PORT))
    server.listen(3)
    print("echo server listening on %s:%d" % (HOST, PORT), flush=True)

    modes = ["echo", "split", "silent"]

    for mode in modes:
        conn, _ = server.accept()
        print("accepted connection, mode=%s" % mode, flush=True)
        try:
            request = recv_exact(conn, FRAME_LEN)
            print("  received %d bytes: %s"
                  % (len(request), request.hex().upper()), flush=True)

            if mode == "echo":
                conn.sendall(request)
            elif mode == "split":
                half = FRAME_LEN // 2
                conn.sendall(request[:half])
                time.sleep(0.2)
                conn.sendall(request[half:])
            elif mode == "silent":
                time.sleep(2.0)
        except (ConnectionAbortedError, ConnectionResetError, OSError) as exc:
            # 客户端提前断开是正常现象（尤其是超时测试），继续服务下一种模式
            print("  connection closed early: %s" % exc, flush=True)
        finally:
            conn.close()

    server.close()
    print("echo server done", flush=True)


if __name__ == "__main__":
    main()
