#include "protocol/ModbusTcpClient.h"

namespace dm {

ModbusTcpClient::ModbusTcpClient(ITransport *transport)
    : m_transport(transport)
{
}

quint16 ModbusTcpClient::nextTransactionId()
{
    ++m_transactionId;
    return m_transactionId;
}

bool ModbusTcpClient::readRegisters(quint8 slaveId, quint8 funcCode,
                                    quint16 address, quint16 count,
                                    QByteArray *data, QString *error)
{

    if (m_transport == nullptr || data == nullptr || error == nullptr)
        return false;

    error->clear();

    const quint16 tid = nextTransactionId();

    //组帧并发送
    const QByteArray request =
        ModbusTcp::buildReadRequest(tid, slaveId, funcCode, address, count);

    if (!m_transport->write(request))
    {
        *error = m_transport->lastError();
        return false;
    }

    //事务号2+协议号2+长度2
    QByteArray head;
    if (!m_transport->readExact(6, &head, m_responseTimeoutMs))
    {
        *error = m_transport->lastError();
        return false;
    }

    //从长度字段算出还要读多少
    const quint16 length =
        static_cast<quint16>((static_cast<quint8>(head.at(4)) << 8)
                             | static_cast<quint8>(head.at(5)));

    //合理性检查，防止垃圾长度
    if (length < 3 || length > 254)
    {
        *error = QStringLiteral("MBAP 长度字段异常：%1").arg(length);
        return false;
    }

    //剩下的length个字节
    QByteArray body;
    if (!m_transport->readExact(length, &body, m_responseTimeoutMs))
    {
        *error = m_transport->lastError();
        return false;
    }

    //拼成完整帧，交给解帧函数
    return ModbusTcp::parseReadResponse(head + body, tid, slaveId, funcCode, data, error);
}

}