#ifndef DM_TCP_TRANSPORT_H
#define DM_TCP_TRANSPORT_H

#include <QTcpSocket>

#include "transport/ITransport.h"

namespace dm {

class TcpTransport : public ITransport
{
public:
    TcpTransport(QString host, quint16 port);
    ~TcpTransport() override;

    bool open() override;
    void close() override;
    bool isOpen() const override;

    bool write(const QByteArray &data) override;
    bool readExact(int n, QByteArray *out, int timeoutMs) override;

    QString lastError() const override;

private:
    QTcpSocket m_socket;
    QString m_host;
    quint16 m_port = 502;
    QString m_lastError;
    QByteArray m_buffer;
};

} // namespace dm

#endif // DM_TCP_TRANSPORT_H