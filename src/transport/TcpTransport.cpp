#include "transport/TcpTransport.h"
#include <QElapsedTimer>

namespace dm {
namespace {
constexpr int kConnectTimeoutMs = 3000;
constexpr int kWriteTimeoutMs = 3000;
}
TcpTransport::TcpTransport(QString host, quint16 port)
    :m_host(std::move(host))
    ,m_port(port)
{

}
TcpTransport::~TcpTransport()
{
    TcpTransport::close();
}

bool TcpTransport::open()
{
    if (m_socket.state() == QAbstractSocket::ConnectedState)
    {
        return true;
    }

    m_socket.abort();
    m_socket.connectToHost(m_host, m_port);

    if (!m_socket.waitForConnected(kConnectTimeoutMs))
    {
        m_lastError = m_socket.errorString();
        m_socket.abort();
        return false;
    }
    m_buffer.clear();
    return true;
}

void TcpTransport::close()
{
    m_socket.abort();
}

bool TcpTransport::isOpen() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

QString TcpTransport::lastError() const
{
    return m_lastError;
}

bool TcpTransport::write(const QByteArray &data)
{
    if (!isOpen())
    {
        m_lastError = QStringLiteral("socket is not connected");
        return false;
    }

    if (m_socket.write(data) != data.size())
    {
        m_lastError = QStringLiteral("write() accepted fewer bytes than expected");
        return false;
    }

    if (!m_socket.waitForBytesWritten(kWriteTimeoutMs))
    {
        m_lastError = m_socket.errorString();
        return false;
    }
    return true;
}

bool TcpTransport::readExact(int n, QByteArray *out, int timeoutMs)
{
    if (out == nullptr || n <= 0)
        return false;

    QElapsedTimer timer;
    timer.start();

    while (m_buffer.size() < n)
    {
        const int remaining = timeoutMs - int(timer.elapsed());
        if (remaining <= 0)
        {
            m_lastError = QStringLiteral("read timeout");
            m_buffer.clear();
            return false;
        }

        if (!m_socket.waitForReadyRead(remaining))
        {
            m_lastError = m_socket.errorString();
            return false;
        }

        m_buffer.append(m_socket.readAll());
    }

    *out = m_buffer.left(n);
    m_buffer.remove(0, n);
    return true;
}

}