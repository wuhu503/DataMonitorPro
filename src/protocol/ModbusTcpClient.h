#ifndef DM_MODBUSTCPCLIENT_H
#define DM_MODBUSTCPCLIENT_H
#include "protocol/ModbusTcp.h"
#include "transport/ITransport.h"

namespace dm {


class ModbusTcpClient
{
public:
    explicit ModbusTcpClient(ITransport *transport);

    // 读寄存器，成功时把数据区写入 *data
    bool readRegisters(quint8 slaveId, quint8 funcCode,
                       quint16 address, quint16 count,
                       QByteArray *data, QString *error);

private:
    quint16 nextTransactionId();

    ITransport *m_transport = nullptr;
    quint16 m_transactionId = 0;
    int m_responseTimeoutMs = 1000;
};
}

#endif // MODBUSTCPCLIENT_H
