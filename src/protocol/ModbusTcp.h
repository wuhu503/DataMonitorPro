#ifndef DM_MODBUSTCP_H
#define DM_MODBUSTCP_H
#include <QByteArray>

// src/protocol/ModbusTcp.h

namespace dm {
class ModbusTcp
{
public:
    //MBAP(7)+PDU(5)
    static QByteArray buildReadRequest(quint16 transactionId,
                                       quint8 slaveId,
                                       quint8 funcCode,
                                       quint16 address,
                                       quint16 count);

    //解读响应
    //异常响应
    static bool parseReadResponse(const QByteArray &frame,
                                  quint16 expectedTransactionId,
                                  quint8 expectedSlaveId,
                                  quint8 expectedFuncCode,
                                  QByteArray *data,
                                  QString *error);
};

} // namespace dm

#endif // MODBUSTCP_H
