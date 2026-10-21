#include "RegisterCodec.h"
#include <QtEndian>
#include <cstring>

namespace dm{

bool decodeRegisters(const QByteArray &raw,
                     DataType type,
                     ByteOrder order,
                     double *out)
{
    if (out == nullptr)
    {
        return false;
    }

    //长度校验
    const int span = registerSpan(type);
    if (span == 0 || raw.size() != span*2)
    {
        return false;
    }
    //单寄存器规定大端序
    if (span == 1)
    {
        const quint16 u =qFromBigEndian<quint16>(raw.constData());

        if (type == DataType::UInt16)
        {
            *out = u;
        }
        else
        {
            *out = static_cast<qint16>(u);
        }
        return true;
    }
    if (span == 2)
    {
        //字节重排
        QByteArray ordered;
        switch (order)
        {
        case ByteOrder::ABCD:
            ordered = raw;
            break;
        case ByteOrder::CDAB:
        {
            const char tmp[4] = {raw.at(2), raw.at(3), raw.at(0), raw.at(1)};
            ordered = QByteArray(tmp,4);
            break;
        }
        case ByteOrder::BADC:
        {
            const char tmp[4] = {raw.at(1), raw.at(0), raw.at(3), raw.at(2)};
            ordered = QByteArray(tmp,4);
            break;
        }
        case ByteOrder::DCBA:
        {
            const char tmp[4] = {raw.at(3), raw.at(2), raw.at(1), raw.at(0)};
            ordered = QByteArray(tmp,4);
            break;
        }
        default:
            return false;
        }

        //按类型取值
        const quint32 bits = qFromBigEndian<quint32>(ordered.constData());
        switch (type)
        {
        case DataType::UInt32:
            *out = bits;
            return true;

        case DataType::Int32:
            *out = static_cast<qint32>(bits);
            return true;

        case DataType::Float32:
        {
            float f;
            memcpy(&f, &bits, sizeof(f));
            *out = f;;
            return true;
        }
        default:
            return false;
        }
    }
    return false;
}

//物理量=原始值*scale+offset
bool decodePointValue(const QByteArray &raw,
                      const PointConfig &point,
                      double *out)
{
    if (out == nullptr)
    {
        return false;
    }

    double rawValue = 0;
    if(!decodeRegisters(raw, point.dataType, point.byteOrder, &rawValue))
    {
        return false;
    }
    //计算物理量
    *out = rawValue * point.scale + point.offset;
    return true;
}
}