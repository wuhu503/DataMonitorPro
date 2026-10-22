#include "protocol/ModbusTcp.h"
#include <QString>

namespace dm {

namespace {


void appendBigEndian (QByteArray *out, quint16 value)
{
    out->append(static_cast<char>((value >>8) & 0xFF));
    out->append(static_cast<char>(value & 0xFF));
}

quint16 readBigEndian(const QByteArray &buf, int offset)
{
    return static_cast<quint16>(
        (static_cast<quint8>(buf.at(offset)) << 8)
        |  static_cast<quint8>(buf.at(offset + 1)));
}

QString exceptionText(quint8 code)
{
    switch (code)
    {
    case 0x01: return QStringLiteral("非法功能码");
    case 0x02: return QStringLiteral("非法数据地址");
    case 0x03: return QStringLiteral("非法数据值");
    case 0x04: return QStringLiteral("从站设备故障");
    default:   return QStringLiteral("未知异常");
    }
}

}

QByteArray ModbusTcp::buildReadRequest(quint16 transactionId,
                                       quint8 slaveId,
                                       quint8 funcCode,
                                       quint16 address,
                                       quint16 count)
{
    QByteArray frame;
    frame.reserve(12);

    appendBigEndian(&frame, transactionId);
    appendBigEndian(&frame, 0x0000);
    appendBigEndian(&frame, 6);
    frame.append(static_cast<char>(slaveId));

    frame.append(static_cast<char>(funcCode));
    appendBigEndian(&frame, address);
    appendBigEndian(&frame, count);
    return frame;
}

bool ModbusTcp::parseReadResponse(const QByteArray &frame,
                       quint16 expectedTransactionId,
                       quint8 expectedSlaveId,
                       quint8 expectedFuncCode,
                       QByteArray *data,
                       QString *error)
{
    //最小长度
    if (frame.size() < 9)
    {
        if (data == nullptr || error == nullptr)
            return false;

        error->clear();
        *error = QStringLiteral("响应不足9字节");
        return false;
    }


    //事物号
    const quint16 transactionId = readBigEndian(frame, 0);

    if (transactionId != expectedTransactionId)
    {
        *error = QStringLiteral("事务号不匹配：期望 %1，收到 %2")
                     .arg(expectedTransactionId).arg(transactionId);
        return false;
    }

    //协议号
    if (readBigEndian(frame, 2) != 0)
    {
        *error = QStringLiteral("协议号不是 0，可能不是 Modbus 帧");
        return false;
    }

    //长度字段
    const quint16 declared = readBigEndian(frame, 4);
    if (declared != frame.size() - 6)
    {
        *error = QStringLiteral("长度字段 %1 与实际 %2 不一致")
                     .arg(declared).arg(frame.size() - 6);
        return false;
    }

    //单元号
    if (quint8(frame.at(6)) != expectedSlaveId)
    {
        *error = QStringLiteral("单元号不匹配");
        return false;
    }

    //功能码
    const quint8 funcCode = quint8(frame.at(7));

    if (funcCode & 0x80)          // 最高位为 1 = 异常响应
    {
        const quint8 code = quint8(frame.at(8));
        *error = QStringLiteral("从站返回异常 %1：%2")
                     .arg(code, 2, 16, QLatin1Char('0')).arg(exceptionText(code));
        return false;
    }

    if (funcCode != expectedFuncCode)
    {
        *error = QStringLiteral("功能码不匹配");
        return false;
    }

    //字节数和数据区
    const quint8 byteCount = quint8(frame.at(8));
    if (frame.size() != 9 + byteCount)
    {
        *error = QStringLiteral("字节数 %1 与数据区长度不符").arg(byteCount);
        return false;
    }

    *data = frame.mid(9, byteCount);
    return true;



}



}