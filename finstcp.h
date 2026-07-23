#ifndef FINSTCP_H
#define FINSTCP_H

#include <QObject>
#include <QAbstractSocket>
#include <QTcpSocket>
#include <QMutex>
#include <QMutexLocker>

/**
 * @brief FINS TCP 通信类
 * 
 * 该类用于与欧姆龙PLC建立FINS TCP协议通信，实现对PLC内存区（DM区等）的读写操作。
 */
class Finstcp : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit Finstcp(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~Finstcp();

    /**
     * @brief 连接到PLC
     * @param localIP 本机IP地址
     * @param plcIP PLC IP地址
     * @param port 端口号
     * @return 连接是否成功
     */
    bool connectToPLC(const QString &localIP, const QString &plcIP, quint16 port);
    
    /**
     * @brief 断开与PLC的连接
     */
    void disconnectPLC();
    
    /**
     * @brief 检查是否已连接
     * @return 是否已连接
     */
    bool isConnected() const;

    /**
     * @brief 读取多个字（16位）
     * @param address 起始地址
     * @param count 读取字数
     * @return 读取的数据数组
     */
    QVector<quint16> readWords(quint16 address, quint16 count = 1);
    
    /**
     * @brief 读取单个位
     * @param address 内存地址
     * @param bitPos 位位置 (0-15)
     * @return 位值 (0或1)
     */
    bool readBit(quint16 address, quint16 bitPos);

    /**
     * @brief 读取单个字（16位）
     * @param address 内存地址
     * @return 读取的16位数据
     */
    quint16 readWord(quint16 address);

    /**
     * @brief 写入单个位
     * @param address 内存地址
     * @param bitPos 位位置 (0-15)
     * @param bitValue 位值 (0或1)
     * @return 写入是否成功
     */
    bool writeBit(quint16 address, quint16 bitPos, quint16 bitValue);
    
    /**
     * @brief 写入单个字（16位）
     * @param address 内存地址
     * @param value 要写入的值
     * @return 写入是否成功
     */
    bool writeWord(quint16 address, quint16 value);
    
    /**
     * @brief 写入多个字
     * @param address 起始地址
     * @param values 要写入的数据数组
     * @return 写入是否成功
     */
    bool writeWords(quint16 address, const QVector<quint16> &values);

    /**
     * @brief 获取本机IP地址
     */
    QString localIP() const { return m_localIP; }
    
    /**
     * @brief 获取PLC IP地址
     */
    QString plcIP() const { return m_plcIP; }
    
    /**
     * @brief 获取端口号
     */
    quint16 port() const { return m_port; }

    /**
     * @brief 自动连接（使用默认IP地址）
     */
    void autoConnect();

signals:
    /**
     * @brief 连接成功信号
     */
    void connected();
    
    /**
     * @brief 断开连接信号
     */
    void disconnected();
    
    /**
     * @brief 错误发生信号
     * @param error 错误信息
     */
    void errorOccurred(const QString &error);
    
    /**
     * @brief 收到数据信号
     * @param data 接收到的数据
     */
    void dataReceived(const QByteArray &data);

private:
    /**
     * @brief 发送FINS命令并接收响应
     * @param response 发送/接收数据缓冲区
     * @param timeout 超时时间（毫秒）
     * @return 是否成功
     */
    bool sendFinsCommand(QByteArray &response, int timeout = 3000);
    
    /**
     * @brief 将字节数组转换为十六进制字符串
     * @param data 要转换的数据
     * @return 十六进制字符串
     */
    QString toHex(const QByteArray &data);

private:
    QTcpSocket *m_socket;          // TCP套接字
    QString m_localIP;              // 本机IP地址
    QString m_plcIP;               // PLC IP地址
    quint16 m_port;                // 端口号
    quint8 m_plcNode;              // PLC节点号（从IP地址最后一段提取）
    quint8 m_pcNode;               // PC节点号（从IP地址最后一段提取）
    QString m_lastError;            // 最近一次错误信息
    bool m_connected;              // 连接状态标志
    QByteArray m_responseBuffer;   // 响应数据缓冲区
    mutable QMutex m_mutex{QMutex::Recursive};  // 线程安全递归互斥锁（支持同一线程重复加锁）
};

#endif // FINSTCP_H
