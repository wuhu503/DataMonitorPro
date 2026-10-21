#ifndef DM_ITRANSPORT_H
#define DM_ITRANSPORT_H

#include <QByteArray>
#include <QString>

namespace dm {
class ITransport
{
public:
    virtual ~ITransport() = default;

    //建立连接
    virtual bool open() = 0;

    //关闭连接
    virtual void close() = 0;

    virtual bool isOpen() const = 0;

    //发送全部字节
    virtual bool write(const QByteArray &data) = 0;

    //在timeoutMs内凑齐n个字节写入
    virtual bool readExact(int n, QByteArray *out, int timeoutMs) = 0;

    //最近一次失败的原因
    virtual QString lastError() const = 0;
};

} // namespace dm

#endif // DM_ITRANSPORT_H