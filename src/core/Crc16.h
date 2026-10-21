#ifndef DM_CRC16_H
#define DM_CRC16_H
#include <QByteArray>

namespace dm {
//多项式0xA001（0x8005 的反转）初值0xFFFF
quint16 crc16(const QByteArray &data);
}

#endif // DM_CRC16_H
