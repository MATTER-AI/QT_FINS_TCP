#include "finstcp.h"
#include <QDebug>
#include <QThread>
#include <QCoreApplication>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @brief 调试信息输出函数
 * @param msg 要输出的信息
 */
void dbg(const QString &msg) {
    std::cout << msg.toStdString() << std::endl;
}

/**
 * @brief 构造函数
 * @param parent 父对象指针
 */
Finstcp::Finstcp(QObject *parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_port(9600)
    , m_plcNode(0)
    , m_pcNode(0)
    , m_connected(false)
{
    m_socket = new QTcpSocket(this);
}

/**
 * @brief 析构函数
 */
Finstcp::~Finstcp()
{
    disconnectPLC();
}

/**
 * @brief 自动连接（使用默认IP地址）
 */
void Finstcp::autoConnect()
{
    connectToPLC("192.168.100.2", "192.168.100.1", 9600);
}

/**
 * @brief 连接到PLC（线程安全）
 * @param localIP 本机IP地址
 * @param plcIP PLC IP地址
 * @param port 端口号
 * @return 连接是否成功
 */
bool Finstcp::connectToPLC(const QString &localIP, const QString &plcIP, quint16 port)
{
    QMutexLocker locker(&m_mutex);
    
    // 检查是否已连接
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        dbg("已连接");
        return true;
    }

    m_localIP = localIP;
    m_plcIP = plcIP;
    m_port = port;

    // 从IP地址中解析节点号
    QStringList localParts = localIP.split('.');
    QStringList plcParts = plcIP.split('.');

    if (localParts.size() == 4 && plcParts.size() == 4) {
        m_pcNode = localParts.at(3).toUInt();
        m_plcNode = plcParts.at(3).toUInt();
    }

    dbg(QString("正在连接到 %1:%2 (PC节点=%3, PLC节点=%4)")
        .arg(plcIP).arg(port).arg(m_pcNode).arg(m_plcNode));

    m_socket->connectToHost(plcIP, port);

    // 等待连接超时（5秒）
    if (!m_socket->waitForConnected(5000)) {
        m_lastError = "连接超时: " + m_socket->errorString();
        emit errorOccurred(m_lastError);
        return false;
    }

    // 发送FINS连接命令
    // 报文头: "FINS"(4字节) + 长度(4字节) = 8字节
    // 帧: 命令(4字节) + 错误码(4字节) + 客户端节点号(4字节) = 12字节
    // 总计: 20字节, 长度字段 = 0x0C (12)
    QByteArray connReq;
    connReq.append("FINS");                        // 4字节
    connReq.append(static_cast<char>(0x00)); connReq.append(static_cast<char>(0x00));
    connReq.append(static_cast<char>(0x00)); connReq.append(static_cast<char>(0x0C)); // 长度 = 12
    connReq.append(static_cast<char>(0x00)); connReq.append(static_cast<char>(0x00));
    connReq.append(static_cast<char>(0x00)); connReq.append(static_cast<char>(0x00)); // 命令 = 0
    connReq.append(static_cast<char>(0x00)); connReq.append(static_cast<char>(0x00));
    connReq.append(static_cast<char>(0x00)); connReq.append(static_cast<char>(0x00)); // 错误码 = 0
    // 客户端节点号 (4字节, 大端序)
    connReq.append(static_cast<char>(0x00)); connReq.append(static_cast<char>(0x00));
    connReq.append(static_cast<char>(0x00)); connReq.append(static_cast<char>(m_pcNode));

    dbg(QString("发送连接请求 (%1 字节): %2").arg(connReq.size()).arg(toHex(connReq)));

    m_socket->write(connReq);
    m_socket->flush();

    // 等待响应
    if (m_socket->waitForReadyRead(3000)) {
        QByteArray response = m_socket->readAll();
        dbg(QString("接收连接响应 (%1 字节): %2")
            .arg(response.size())
            .arg(toHex(response)));

        // 检查响应是否以"FINS"开头
        if (response.size() >= 16 && response.mid(0, 4) == "FINS") {
            // 解析响应: FINS(4) + 长度(4) + 命令(4) + 错误码(4) + PC节点(4) + PLC节点(4)
            quint32 respCommand = (static_cast<quint8>(response[8]) << 24) |
                                  (static_cast<quint8>(response[9]) << 16) |
                                  (static_cast<quint8>(response[10]) << 8) |
                                  static_cast<quint8>(response[11]);
            quint32 respError = (static_cast<quint8>(response[12]) << 24) |
                                (static_cast<quint8>(response[13]) << 16) |
                                (static_cast<quint8>(response[14]) << 8) |
                                static_cast<quint8>(response[15]);

            if (respCommand == 0x00000001 && respError == 0x00000000) {
                m_connected = true;
                dbg("连接成功!");
                emit connected();
                return true;
            } else {
                dbg(QString("连接失败: 命令=%1, 错误=%2")
                    .arg(respCommand, 8, 16, QChar('0'))
                    .arg(respError, 8, 16, QChar('0')));
            }
        }
    }

    dbg("连接响应超时或无效");
    m_socket->disconnectFromHost();
    return false;
}

/**
 * @brief 断开与PLC的连接（线程安全）
 */
void Finstcp::disconnectPLC()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
        m_socket->waitForDisconnected(2000);
    }
    m_connected = false;
    emit disconnected();
}

/**
 * @brief 检查是否已连接（线程安全）
 * @return 是否已连接
 */
bool Finstcp::isConnected() const
{
    QMutexLocker locker(&m_mutex);
    return m_socket->state() == QAbstractSocket::ConnectedState && m_connected;
}

/**
 * @brief 将字节数组转换为十六进制字符串
 * @param data 要转换的数据
 * @return 十六进制字符串
 */
QString Finstcp::toHex(const QByteArray &data)
{
    QString hex;
    for (int i = 0; i < data.size(); i++) {
        hex += QString("%1 ").arg((uchar)data[i], 2, 16, QChar('0')).toUpper();
    }
    return hex;
}

/**
 * @brief 发送FINS命令并接收响应（线程安全）
 * @param response 发送/接收数据缓冲区
 * @param timeout 超时时间（毫秒）
 * @return 是否成功
 */
bool Finstcp::sendFinsCommand(QByteArray &response, int timeout)
{
    QMutexLocker locker(&m_mutex);
    
    // 检查连接状态
    if (!isConnected()) {
        m_lastError = "未连接";
        return false;
    }

    m_responseBuffer.clear();

    dbg(QString(">>> 发送 FINS (%1 字节): %2")
        .arg(response.size())
        .arg(toHex(response)));

    qint64 written = m_socket->write(response);
    if (written == -1) {
        m_lastError = "发送失败";
        return false;
    }
    m_socket->flush();
    dbg(">>> 已发送, 等待响应...");

    // 等待响应
    if (m_socket->waitForReadyRead(timeout)) {
        response = m_socket->readAll();
        dbg(QString(">>> 接收 (%1 字节): %2")
            .arg(response.size())
            .arg(toHex(response)));
        return !response.isEmpty();
    }

    m_lastError = "等待响应超时";
    dbg(QString(">>> %1").arg(m_lastError));
    return false;
}

/**
 * @brief 读取多个字（16位）（线程安全）
 * @param address 起始地址
 * @param count 读取字数
 * @return 读取的数据数组
 */
QVector<quint16> Finstcp::readWords(quint16 address, quint16 count)
{
    QMutexLocker locker(&m_mutex);
    
    QVector<quint16> result;

    // FINS TCP 头: "FINS" + 长度(4字节) = 8字节
    // FINS 帧 = 26字节 (固定)
    //   保留(4) + 命令(4) + 错误码(4) + 帧头(10) + MRC(1) + SRC(1) + 参数(6)

    int finsFrameLen = 26;

    QByteArray cmd;
    cmd.append("FINS");
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x00));
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(finsFrameLen));
    // FINS 帧 (26字节)
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x00));
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x02)); // 命令 = 2
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x00));
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x00)); // 错误码 = 0

    // FINS 帧头 (10字节)
    cmd.append(static_cast<char>(0x80)); // ICF
    cmd.append(static_cast<char>(0x00)); // RSV
    cmd.append(static_cast<char>(0x07)); // GCT
    cmd.append(static_cast<char>(0x00)); // DNA
    cmd.append(static_cast<char>(m_plcNode)); // DA1 (PLC节点)
    cmd.append(static_cast<char>(0x00)); // DA2
    cmd.append(static_cast<char>(0x00)); // SNA
    cmd.append(static_cast<char>(m_pcNode)); // SA1 (PC节点)
    cmd.append(static_cast<char>(0x00)); // SA2
    cmd.append(static_cast<char>(0x00)); // SID

    // 命令: 内存读
    cmd.append(static_cast<char>(0x01)); // MRC
    cmd.append(static_cast<char>(0x01)); // SRC

    // 内存区读参数 (6字节)
    // 82 = DM区字操作
    cmd.append(static_cast<char>(0x82)); // 寄存器代码
    cmd.append(static_cast<char>((address >> 8) & 0xFF)); // 地址高字节
    cmd.append(static_cast<char>((address) & 0xFF)); // 地址低字节
    cmd.append(static_cast<char>(0)); // 从0位开始
    cmd.append(static_cast<char>((count >> 8) & 0xFF)); // 字数高字节
    cmd.append(static_cast<char>(count & 0xFF)); // 字数低字节

    dbg(QString(">>> 发送 (%1 字节): %2")
        .arg(cmd.size())
        .arg(toHex(cmd)));

    QByteArray resp = cmd;
    if (!sendFinsCommand(resp)) {
        dbg(QString("读取失败: %1").arg(m_lastError));
        return result;
    }

    // 解析响应
    // FINS 头: 4+4+4+4 = 16字节
    // FINS 帧头: 10字节
    // MRC + SRC + 结束码: 4字节
    // 数据从第30字节开始
    int dataStart = 30;
    if (resp.size() >= dataStart) {
        for (int i = dataStart; i + 1 < resp.size(); i += 2) {
            quint16 value = (static_cast<quint8>(resp[i]) << 8) | static_cast<quint8>(resp[i + 1]);
            result.append(value);
        }
    }

    return result;
}

/**
 * @brief 读取单个位（线程安全）
 * @param address 内存地址
 * @param bitPos 位位置 (0-15)
 * @return 位值 (0或1)
 */
bool Finstcp::readBit(quint16 address, quint16 bitPos)
{
    QMutexLocker locker(&m_mutex);
    
    QVector<quint16> result;

    // FINS TCP 头: "FINS" + 长度(4字节) = 8字节
    // FINS 帧 = 26字节 (固定)
    //   保留(4) + 命令(4) + 错误码(4) + 帧头(10) + MRC(1) + SRC(1) + 参数(6)

    int finsFrameLen = 26;

    QByteArray cmd;
    cmd.append("FINS");
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x00));
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(finsFrameLen));
    // FINS 帧 (26字节)
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x00));
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x02)); // 命令 = 2
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x00));
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x00)); // 错误码 = 0

    // FINS 帧头 (10字节)
    cmd.append(static_cast<char>(0x80)); // ICF
    cmd.append(static_cast<char>(0x00)); // RSV
    cmd.append(static_cast<char>(0x07)); // GCT
    cmd.append(static_cast<char>(0x00)); // DNA
    cmd.append(static_cast<char>(m_plcNode)); // DA1 (PLC节点)
    cmd.append(static_cast<char>(0x00)); // DA2
    cmd.append(static_cast<char>(0x00)); // SNA
    cmd.append(static_cast<char>(m_pcNode)); // SA1 (PC节点)
    cmd.append(static_cast<char>(0x00)); // SA2
    cmd.append(static_cast<char>(0x00)); // SID

    // 命令: 内存读
    cmd.append(static_cast<char>(0x01)); // MRC
    cmd.append(static_cast<char>(0x01)); // SRC

    // 内存区读参数 (6字节)
    // 0x30 = 位操作
    cmd.append(static_cast<char>(0x30)); // 操作代码：位读取
    cmd.append(static_cast<char>((address >> 8) & 0xFF)); // 地址高字节
    cmd.append(static_cast<char>((address) & 0xFF)); // 地址低字节
    cmd.append(static_cast<char>(bitPos)); // 位位置
    cmd.append(static_cast<char>(0)); // 
    cmd.append(static_cast<char>(1)); // 只读取1字节

    dbg(QString(">>> 发送 (%1 字节): %2")
        .arg(cmd.size())
        .arg(toHex(cmd)));

    QByteArray resp = cmd;
    if (!sendFinsCommand(resp)) {
        dbg(QString("读取失败: %1").arg(m_lastError));
        return 0;
    }

    // 解析响应
    // FINS 头: 4+4+4+4 = 16字节
    // FINS 帧头: 10字节
    // MRC + SRC + 结束码: 4字节
    // 数据从第30字节开始
    int dataStart = 30;
    if (resp.size() > dataStart) {
        return resp.at(30) == 1 ? true:false;
    }
}

/**
 * @brief 读取单个字（16位）（线程安全）
 * @param address 内存地址
 * @return 读取的16位数据
 */
quint16 Finstcp::readWord(quint16 address)
{
    QVector<quint16> result = readWords(address, 1);
    return result.isEmpty() ? 0 : result.at(0);
}

/**
 * @brief 写入单个位（线程安全）
 * @param address 内存地址
 * @param bitPos 位位置 (0-15)
 * @param bitValue 位值 (0或1)
 * @return 写入是否成功
 */
bool Finstcp::writeBit(quint16 address, quint16 bitPos, quint16 bitValue)
{
    QMutexLocker locker(&m_mutex);
    
    // FINS TCP 头: "FINS" + 长度(4字节) = 8字节
    // FINS 帧 = 26 + 2*count 字节
    int finsFrameLen = 27;

    QByteArray cmd;
    cmd.append("FINS");
    cmd.append(static_cast<char>((finsFrameLen >> 24) & 0xFF));
    cmd.append(static_cast<char>((finsFrameLen >> 16) & 0xFF));
    cmd.append(static_cast<char>((finsFrameLen >> 8) & 0xFF));
    cmd.append(static_cast<char>(finsFrameLen & 0xFF));
    // FINS 帧
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x00));
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x02)); // 命令 = 2
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x00));
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x00)); // 错误码 = 0

    // FINS 帧头 (10字节)
    cmd.append(static_cast<char>(0x80)); // ICF
    cmd.append(static_cast<char>(0x00)); // RSV
    cmd.append(static_cast<char>(0x07)); // GCT
    cmd.append(static_cast<char>(0x00)); // DNA
    cmd.append(static_cast<char>(0)); // DA1 (PLC节点)
    cmd.append(static_cast<char>(0x00)); // DA2
    cmd.append(static_cast<char>(0x00)); // SNA
    cmd.append(static_cast<char>(m_pcNode)); // SA1 (PC节点)
    cmd.append(static_cast<char>(0x00)); // SA2
    cmd.append(static_cast<char>(0x03)); // SID

    // 命令: 内存写
    cmd.append(static_cast<char>(0x01)); // MRC
    cmd.append(static_cast<char>(0x02)); // SRC

    // 内存区写参数 (6字节)
    cmd.append(static_cast<char>(0x30)); // 操作代码：位操作
    cmd.append(static_cast<char>((address >> 8) & 0xFF)); // 地址中高字节
    cmd.append(static_cast<char>(address & 0xFF)); // 地址低字节
    cmd.append(static_cast<char>(bitPos)); // 位位置
    cmd.append(static_cast<char>(0)); // 字数高字节
    cmd.append(static_cast<char>(1)); // 字数低字节（固定为1字节）

    // 数据
    cmd.append(static_cast<char>(bitValue));

    QByteArray resp = cmd;
    if (!sendFinsCommand(resp)) {
        dbg(QString("写入失败: %1").arg(m_lastError));
        return false;
    }

    // 检查响应中的结束码
    // FINS 头(8) + 帧头(10) + MRC + SRC (2) = 20
    // 结束码位于第28-29字节
    if (resp.size() >= 30) {
        quint16 endCode = (static_cast<quint8>(resp[28]) << 8) | static_cast<quint8>(resp[29]);
        dbg(QString("写入结束码: 0x%1").arg(endCode, 4, 16, QChar('0')).toUpper());
        return endCode == 0x0000;
    }

    return false;
}

/**
 * @brief 写入单个字（16位）（线程安全）
 * @param address 内存地址
 * @param value 要写入的值
 * @return 写入是否成功
 */
bool Finstcp::writeWord(quint16 address, quint16 value)
{
    QVector<quint16> values;
    values.append(value);
    return writeWords(address, values);
}

/**
 * @brief 写入多个字（线程安全）
 * @param address 起始地址
 * @param values 要写入的数据数组
 * @return 写入是否成功
 */
bool Finstcp::writeWords(quint16 address, const QVector<quint16> &values)
{
    QMutexLocker locker(&m_mutex);
    
    int count = values.size();

    // FINS TCP 头: "FINS" + 长度(4字节) = 8字节
    // FINS 帧 = 26 + 2*count 字节
    int finsFrameLen = 26 + 2 * count;

    QByteArray cmd;
    cmd.append("FINS");
    cmd.append(static_cast<char>((finsFrameLen >> 24) & 0xFF));
    cmd.append(static_cast<char>((finsFrameLen >> 16) & 0xFF));
    cmd.append(static_cast<char>((finsFrameLen >> 8) & 0xFF));
    cmd.append(static_cast<char>(finsFrameLen & 0xFF));
    // FINS 帧
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x00));
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x02)); // 命令 = 2
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x00));
    cmd.append(static_cast<char>(0x00)); cmd.append(static_cast<char>(0x00)); // 错误码 = 0

    // FINS 帧头 (10字节)
    cmd.append(static_cast<char>(0x80)); // ICF
    cmd.append(static_cast<char>(0x00)); // RSV
    cmd.append(static_cast<char>(0x02)); // GCT
    cmd.append(static_cast<char>(0x00)); // DNA
    cmd.append(static_cast<char>(m_plcNode)); // DA1 (PLC节点)
    cmd.append(static_cast<char>(0x00)); // DA2
    cmd.append(static_cast<char>(0x00)); // SNA
    cmd.append(static_cast<char>(m_pcNode)); // SA1 (PC节点)
    cmd.append(static_cast<char>(0x00)); // SA2
    cmd.append(static_cast<char>(0x01)); // SID

    // 命令: 内存写
    cmd.append(static_cast<char>(0x01)); // MRC
    cmd.append(static_cast<char>(0x02)); // SRC

    // 内存区写参数 (6字节)
    cmd.append(static_cast<char>(0x82)); // 寄存器代码: DM区字操作
    cmd.append(static_cast<char>((address >> 8) & 0xFF)); // 地址中高字节
    cmd.append(static_cast<char>(address & 0xFF)); // 地址低字节
    cmd.append(static_cast<char>(0)); // 位偏移
    cmd.append(static_cast<char>((count >> 8) & 0xFF0)); // 字数高字节
    cmd.append(static_cast<char>(count & 0xFF)); // 字数低字节

    // 数据
    for (quint16 v : values) {
        cmd.append(static_cast<char>((v >> 8) & 0xFF));
        cmd.append(static_cast<char>(v & 0xFF));
    }

    QByteArray resp = cmd;
    if (!sendFinsCommand(resp)) {
        dbg(QString("写入失败: %1").arg(m_lastError));
        return false;
    }

    // 检查响应中的结束码
    // FINS 头(8) + 帧头(10) + MRC + SRC (2) = 20
    // 结束码位于第28-29字节
    if (resp.size() >= 30) {
        quint16 endCode = (static_cast<quint8>(resp[28]) << 8) | static_cast<quint8>(resp[29]);
        dbg(QString("写入结束码: 0x%1").arg(endCode, 4, 16, QChar('0')).toUpper());
        return endCode == 0x0000;
    }

    return false;
}
