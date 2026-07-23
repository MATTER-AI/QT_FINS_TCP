# FINS TCP 调试工具

一个基于 Qt 的 FINS TCP 通信调试工具，用于与欧姆龙（Omron）PLC 进行数据交互。

## 功能特性

- 通过 FINS TCP 协议连接欧姆龙 PLC
- 读取/写入 PLC DM 存储区的字数据（16位）
- 读取/写入 PLC DM 存储区的位数据
- **线程安全**：所有读写操作使用递归互斥锁保护，支持多线程并发调用
- 支持命令行参数自动化操作
- 交互式命令终端模式

## 系统要求

- Qt 5.x 或更高版本
- C++11 编译器
- 支持 Windows/Linux/macOS

## 编译

### Qt Creator

1. 使用 Qt Creator 打开 `QT_ConsoleTest.pro` 文件
2. 选择合适的 Kit 进行构建
3. 编译运行即可

### 命令行编译

```bash
qmake QT_ConsoleTest.pro
make
```

Windows (MinGW/MSVC):
```bash
qmake QT_ConsoleTest.pro
nmake
```

## 使用方法

### 交互模式

直接运行程序进入交互式终端：

```bash
./QT_ConsoleTest
```

### 命令行模式

支持带参数启动进行自动化操作：

```bash
./QT_ConsoleTest <本机IP> <PLC IP> <端口> [命令] [参数]
```

## 命令列表

| 命令 | 说明 | 示例 |
|------|------|------|
| `connect <本机IP> <PLC IP> <端口>` | 连接 PLC | `connect 192.168.100.2 192.168.100.1 9600` |
| `disconnect` | 断开连接 | `disconnect` |
| `status` | 显示连接状态 | `status` |
| `read <地址> [数量]` | 读取字数据 | `read 0 5` (读取 D0000-D0004) |
| `readbit <地址> [位位置]` | 读取位数据 | `readbit 0 1` (读取 D0000 第1位) |
| `write <地址> <值>` | 写入字数据 | `write 0 0x1234` (写入 D0000=0x1234) |
| `writebit <地址> <位位置> <位值>` | 写入位数据 | `writebit 0 1 1` (写入 D0000 第1位=1) |
| `help` | 显示帮助信息 | `help` |
| `quit` | 退出程序 | `quit` |

## 使用示例

### 连接 PLC

```
> connect 192.168.100.2 192.168.100.1 9600
正在连接到 192.168.100.1:9600 ...
连接成功!
```

### 读取数据

```
> read 0 5
正在读取 D0(数量:5):
  D0 = 0x0000 (0)
  D1 = 0x0000 (0)
  D2 = 0x0000 (0)
  D3 = 0x0000 (0)
  D4 = 0x0000 (0)
```

### 写入数据

```
> write 0 0x1234
正在写入 D0 = 0x1234 (4660)
写入成功!
```

### 读取位数据

```
> readbit 0 1
正在读取 D0(位位置:1):
  D0 第1位=0
```

### 写入位数据

```
> writebit 0 1 1
正在写入 D0(位位置:1,1):
写入成功!
```

### 查看状态

```
> status
状态: 已连接
  本机 IP: 192.168.100.2
  PLC IP: 192.168.100.1
  端口: 9600
```

## 项目结构

```
QT_ConsoleTest/
├── main.cpp        # 主程序入口和交互式命令行
├── finstcp.h       # FINS TCP 通信类头文件
├── finstcp.cpp     # FINS TCP 通信类实现
└── README.md       # 说明文档
```

## 类说明

### Finstcp

主要类，负责与 PLC 的 FINS TCP 通信：

**公共方法：**
- `connectToPLC(localIP, plcIP, port)` - 建立连接
- `disconnectPLC()` - 断开连接
- `isConnected()` - 检查连接状态
- `readWord(address)` - 读取单个字
- `readWords(address, count)` - 读取多个字
- `readBit(address, bitPos)` - 读取单个位
- `writeWord(address, value)` - 写入单个字
- `writeWords(address, values)` - 写入多个字
- `writeBit(address, bitPos, bitValue)` - 写入单个位
- `autoConnect()` - 使用默认参数自动连接

**信号：**
- `connected` - 连接成功
- `disconnected` - 断开连接
- `errorOccurred(error)` - 发生错误
- `dataReceived(data)` - 收到数据

## 协议说明

本工具使用 **FINS TCP** 协议与欧姆龙 PLC 通信，默认端口为 **9600**。

FINS TCP 通信流程：
1. 建立 TCP 连接
2. 发送 FINS 连接命令（握手）
3. 交换 FINS 帧进行数据读写操作
4. 断开连接时自动清理

## 默认配置

| 参数 | 默认值 |
|------|--------|
| 本机 IP | 192.168.100.2 |
| PLC IP | 192.168.100.1 |
| 端口 | 9600 |

## 注意事项

- 确保网络连接正常，PLC 和 PC 在同一网段
- 部分 PLC 可能需要配置 FINS/TCP 通信参数
- 写入操作会直接影响 PLC 内存，请谨慎操作
- 建议在进行写入操作前确认地址范围

## License

MIT License
