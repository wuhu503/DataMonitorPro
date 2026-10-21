#include "core/Crc16.h"

namespace dm {

quint16 crc16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;

    for (char ch : data)
    {
        crc ^= static_cast<quint8>(ch);
        for (int bit = 0; bit < 8; ++bit)
        {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

} // namespace dm
