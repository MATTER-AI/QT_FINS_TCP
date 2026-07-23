#include <QCoreApplication>
#include <QTextCodec>
#include <QDebug>
#include <iostream>
#include "finstcp.h"

#ifdef _WIN32
#include <windows.h>
#endif

#define OUT qDebug()

/**
 * @brief 打印帮助信息
 */
void printHelp() {
    OUT << "";
    OUT << "========== FINS TCP 调试工具 ==========";
    OUT << "命令列表:";
    OUT << "  connect <本机IP> <PLC IP> <端口>  - 连接PLC";
    OUT << "  disconnect                        - 断开连接";
    OUT << "  status                            - 显示连接状态";
    OUT << "  read <地址> [数量]               - 读取数据(默认数量=1)";
    OUT << "  readbit <地址> [位位置]           - 读取位数据";
    OUT << "  write <地址> <值>                 - 写入数据";
    OUT << "  writebit <地址> <位位置> <位值>   - 写入位数据";
    OUT << "  help                              - 显示帮助信息";
    OUT << "  quit                              - 退出程序";
    OUT << "使用示例:";
    OUT << "  connect 192.168.100.2 192.168.100.1 9600";
    OUT << "  read 0 5          (读取 D0000-D0004)";
    OUT << "  write 0 0x1234   (写入 D0000=0x1234)";
    OUT << "========================================";
    OUT << "";
}

/**
 * @brief 主函数
 */
int main(int argc, char *argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    QCoreApplication a(argc, argv);

    OUT << "FINS TCP 调试工具 v1.0";
    OUT << "========================";

    Finstcp fins;

    // 连接成功信号处理
    QObject::connect(&fins, &Finstcp::connected, [&]() {
        OUT << "[系统] PLC 已连接!";
    });

    // 断开连接信号处理
    QObject::connect(&fins, &Finstcp::disconnected, [&]() {
        OUT << "[系统] PLC 已断开";
    });

    // 错误信号处理
    QObject::connect(&fins, &Finstcp::errorOccurred, [&](const QString &err) {
        OUT << "[错误]" << err;
    });

    // 收到数据信号处理
    QObject::connect(&fins, &Finstcp::dataReceived, [&](const QByteArray &data) {
        QString hex;
        for (int i = 0; i < data.size(); i++) {
            hex += QString("%1 ").arg((uchar)data[i], 2, 16, QChar('0')).toUpper();
        }
        OUT << "[接收]" << hex.trimmed();
    });

    printHelp();

    // 启动时自动连接
    QString localIP = "192.168.100.2";
    QString plcIP = "192.168.100.1";
    quint16 port = 9600;

    OUT << "正在自动连接" << localIP << "->" << plcIP << ":" << port << "...";
    fins.autoConnect();

    // 命令行参数处理
    if (argc >= 4) {
        localIP = argv[1];
        plcIP = argv[2];
        port = QString(argv[3]).toUShort();
        OUT << "正在自动连接" << localIP << "->" << plcIP << ":" << port;

        if (fins.connectToPLC(localIP, plcIP, port)) {
            OUT << "连接成功!";

            // 命令行读取/写入操作
            if (argc >= 5) {
                QString cmd = argv[4];
                if (cmd == "read" && argc >= 6) {
                    quint16 addr = QString(argv[5]).toUShort();
                    quint16 count = (argc >= 7) ? QString(argv[6]).toUShort() : 1;
                    OUT << "正在读取地址 D" << addr << ", 数量" << count << ":";
                    QVector<quint16> data = fins.readWords(addr, count);
                    for (int i = 0; i < data.size(); i++) {
                        OUT.nospace() << "  D" << (addr + i) << " = 0x"
                                          << QString::number(data[i], 16).toUpper().rightJustified(4, '0')
                                          << " (" << data[i] << ")";
                    }
                } else if (cmd == "write" && argc >= 7) {
                    quint16 addr = QString(argv[5]).toUShort();
                    bool ok;
                    quint16 value = QString(argv[6]).toUShort(&ok, 16);
                    if (!ok) value = QString(argv[6]).toUShort(&ok, 10);
                    if (ok) {
                        OUT.nospace() << "正在写入地址 D" << addr << " = 0x"
                                         << QString::number(value, 16).toUpper().rightJustified(4, '0');
                        if (fins.writeWord(addr, value)) {
                            OUT << "写入成功!";
                        } else {
                            OUT << "写入失败!";
                        }
                    }
                }
            }

            fins.disconnectPLC();
            return 0;
        } else {
            OUT << "连接失败!";
            return 1;
        }
    }

    OUT << "交互模式 (输入 help 查看命令)";
    OUT << "";

    // 交互式命令行循环
    std::string input;
    while (true) {
        std::cout << "> " << std::flush;
        std::getline(std::cin, input);
        QString line = QString::fromStdString(input).trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        QString cmd = parts.value(0).toLower();

        if (cmd == "quit" || cmd == "exit") {
            fins.disconnectPLC();
            OUT << "再见!";
            break;
        }
        else if (cmd == "help") {
            printHelp();
        }
        else if (cmd == "connect") {
            if (parts.size() >= 4) {
                localIP = parts[1];
                plcIP = parts[2];
                port = parts[3].toUShort();
                OUT << "正在连接到" << plcIP << ":" << port << "...";
                if (fins.connectToPLC(localIP, plcIP, port)) {
                    OUT << "连接成功!";
                } else {
                    OUT << "连接失败!";
                }
            } else {
                OUT << "用法: connect <本机IP> <PLC IP> <端口>";
            }
        }
        else if (cmd == "disconnect") {
            fins.disconnectPLC();
            OUT << "已断开连接";
        }
        else if (cmd == "status") {
            if (fins.isConnected()) {
                OUT << "状态: 已连接";
                OUT << "  本机 IP:" << fins.localIP();
                OUT << "  PLC IP:" << fins.plcIP();
                OUT << "  端口:" << fins.port();
            } else {
                OUT << "状态: 未连接";
            }
        }
        else if (cmd == "read") {
            if (!fins.isConnected()) {
                OUT << "错误: 请先连接 (connect)";
                continue;
            }
            if (parts.size() >= 2) {
                quint16 addr = parts[1].toUShort();
                quint16 count = (parts.size() >= 3) ? parts[2].toUShort() : 1;
                OUT << "正在读取 D" << addr << "(数量:" << count << "):";
                QVector<quint16> data = fins.readWords(addr, count);
                for (int i = 0; i < data.size(); i++) {
                    OUT.nospace() << "  D" << (addr + i) << " = 0x"
                                      << QString::number(data[i], 16).toUpper().rightJustified(4, '0')
                                      << " (" << data[i] << ")";
                }
                if (data.isEmpty()) {
                    OUT << "  未收到数据";
                }
            } else {
                OUT << "用法: read <地址> [数量]";
            }
        }
        else if(cmd == "readbit"){
            if (!fins.isConnected()) {
                OUT << "错误: 请先连接 (connect)";
                continue;
            }
            if (parts.size() >= 2) {
                quint16 addr = parts[1].toUShort();
                quint16 bitPost = (parts.size() >= 3) ? parts[2].toUShort() : 1;
                OUT << "正在读取 D" << addr << "(位位置:" << bitPost << "):";
                bool ret = fins.readBit(addr, bitPost);
                OUT.nospace() << "  D" << (addr) << " 第"<<bitPost<<"位="<<ret;
            } else {
                OUT << "用法: readbit <地址> [位位置]";
            }
        }
        else if (cmd == "write") {
            if (!fins.isConnected()) {
                OUT << "错误: 请先连接 (connect)";
                continue;
            }
            if (parts.size() >= 3) {
                quint16 addr = parts[1].toUShort();
                bool ok;
                quint16 value = parts[2].toUShort(&ok, 16);
                if (!ok) value = parts[2].toUShort(&ok, 10);
                if (ok) {
                    OUT.nospace() << "正在写入 D" << addr << " = 0x"
                                     << QString::number(value, 16).toUpper().rightJustified(4, '0')
                                     << " (" << value << ")";
                    if (fins.writeWord(addr, value)) {
                        OUT << "写入成功!";
                    } else {
                        OUT << "写入失败!";
                    }
                } else {
                    OUT << "无效的值:" << parts[2];
                }
            } else {
                OUT << "用法: write <地址> <值>";
            }
        }
        else if (cmd == "writebit") {
            if (!fins.isConnected()) {
                OUT << "错误: 请先连接 (connect)";
                continue;
            }
            if (parts.size() >= 4) {
                quint16 addr = parts[1].toUShort();
                quint16 bitPost = parts[2].toUShort();
                quint16 bitValue = parts[3].toUShort();
                OUT << "正在写入 D" << addr << "(位位置:" << bitPost << "," << bitValue<<"):";
                if (fins.writeBit(addr, bitPost, bitValue)) {
                    OUT << "写入成功!";
                } else {
                    OUT << "写入失败!";
                }

            } else {
                OUT << "用法: writebit <地址> <位位置> <位值>";
            }
        }
        else {
            OUT << "未知命令:" << cmd << "(输入 help 查看命令)";
        }
    }

    return 0;
}
