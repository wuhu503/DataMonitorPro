#!/usr/bin/env python3
"""
最小 Modbus TCP 仿真从站，用来联调 ModbusTcpClient。

只实现功能码 03/04（读保持/输入寄存器），行为由请求的起始地址决定：

  地址 0     正常返回：寄存器 0=0x418C, 1=0xCCCD（float32 的 17.6）
  地址 9999  返回异常 0x02（非法数据地址）
  地址 5000  先睡 2 秒再返回，用来测客户端超时
  其它地址   正常返回 0

可以接受多次连接，直到进程被结束。
"""

import socket
import struct
import threading
import time

HOST = "127.0.0.1"
PORT = 15502

# 寄存器表：地址 -> 16 位值
REGISTERS = {
    0: 0x418C,
    1: 0xCCCD,
}

EXCEPTION_ILLEGAL_ADDRESS = 0x02
SLOW_ADDRESS = 5000
EXCEPTION_ADDRESS = 9999


def recv_exact(conn, n):
    buf = b""
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def send_frame(conn, tid, unit, pdu):
    frame = struct.pack(">HHH", tid, 0, 1 + len(pdu)) + bytes([unit]) + pdu
    conn.sendall(frame)


def handle_one(conn):
    """处理一次请求；连接断开返回 False。"""
    head = recv_exact(conn, 6)
    if head is None:
        return False

    tid, _pid, length = struct.unpack(">HHH", head)
    body = recv_exact(conn, length)
    if body is None:
        return False

    unit = body[0]
    func = body[1]
    address, count = struct.unpack(">HH", body[2:6])

    print("  request: tid=%d unit=%d func=%d addr=%d count=%d"
          % (tid, unit, func, address, count), flush=True)

    if address == EXCEPTION_ADDRESS:
        print("  -> exception 0x02", flush=True)
        send_frame(conn, tid, unit,
                   bytes([func | 0x80, EXCEPTION_ILLEGAL_ADDRESS]))
        return True

    if address == SLOW_ADDRESS:
        print("  -> slow path, sleeping 2s", flush=True)
        time.sleep(2.0)

    data = b""
    for offset in range(count):
        value = REGISTERS.get(address + offset, 0)
        data += struct.pack(">H", value)

    send_frame(conn, tid, unit, bytes([func, len(data)]) + data)
    print("  -> %d bytes of data: %s"
          % (len(data), data.hex().upper()), flush=True)
    return True


def serve_connection(conn):
    try:
        while handle_one(conn):
            pass
    except (ConnectionAbortedError, ConnectionResetError, OSError) as exc:
        print("  connection closed: %s" % exc, flush=True)
    finally:
        conn.close()
        print("  connection closed", flush=True)


def main():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((HOST, PORT))
    server.listen(4)
    print("modbus slave listening on %s:%d" % (HOST, PORT), flush=True)

    while True:
        conn, _ = server.accept()
        print("accepted connection", flush=True)
        # 每个连接一个线程：慢响应阻塞的只是它自己，
        # 主线程可以立刻接受新连接。
        threading.Thread(target=serve_connection, args=(conn,), daemon=True).start()


if __name__ == "__main__":
    main()
