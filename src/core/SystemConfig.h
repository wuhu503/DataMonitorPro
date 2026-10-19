#ifndef DM_SYSTEM_CONFIG_H
#define DM_SYSTEM_CONFIG_H

#include <QHash>
#include <QString>
#include <QVector>

#include "core/ConfigIssue.h"
#include "core/Types.h"

namespace dm {


struct LinkConfig
{
    QString id;
    LinkType type = LinkType::Tcp;
    QString description;

    //TCP使用
    QString host;
    quint16 port = 502;

    //串口链路使用
    QString serialPort;
    int baudRate = 9600;
    int dataBits = 8;
    int parity = 0;
    int stopBits = 1;

    //公共参数
    int responseTimeoutMs = 1000;              // 等从站响应的超时
    int retries = 3;                           // 超时后的重试次数
    int retryIntervalMs = 200;                 // 两次重试之间的间隔
    int interFrameDelayMs = 0;                 // 仅串口有意义：两次请求间的最小间隔
};


struct DeviceConfig
{
    QString id;
    QString name;
    QString linkId;                            // 必须存在于某个 LinkConfig::id 中
    int slaveId = 1;                           // 1..247，同一条链路内不得重复
    bool enabled = true;                       // false = 配置里有，但不参与采集
    QString description;
};


struct PointConfig
{
    QString id;
    QString name;
    QString deviceId;

    int funcCode = 3;                          // 1=线圈 2=离散输入 3=保持寄存器 4=输入寄存器
    quint16 address = 0;                       // 0 基址的 PDU 地址
    DataType dataType = DataType::UInt16;
    ByteOrder byteOrder = ByteOrder::ABCD;     // 仅 32 位类型有意义

    double scale = 1.0;                        // 物理量 = 原始值 * scale + offset
    double offset = 0.0;
    QString unit;
    int decimals = 2;

    PollGroup pollGroup = PollGroup::Normal;
    double deadband = 0.0;

    double alarmLow = 0.0;
    double alarmHigh = 0.0;
    AlarmLevel alarmLevel = AlarmLevel::None;
};

class SystemConfig
{
public:
    void clear();

    void setLinks(QVector<LinkConfig> links);
    void setDevices(QVector<DeviceConfig> devices);
    void setPoints(QVector<PointConfig> points);

    //校验跨文件引用关系与寄存器重叠，并建立索引

    bool rebuildIndexes(QVector<ConfigIssue> *issues);

    const QVector<LinkConfig>   &links() const   { return m_links; }
    const QVector<DeviceConfig> &devices() const { return m_devices; }
    const QVector<PointConfig>  &points() const  { return m_points; }

    //按id查单个对象
    const LinkConfig   *link(const QString &id) const;
    const DeviceConfig *device(const QString &id) const;
    const PointConfig  *point(const QString &id) const;

    //反向查询
    QVector<const DeviceConfig *> devicesOfLink(const QString &linkId) const;
    QVector<const PointConfig *>  pointsOfDevice(const QString &deviceId) const;
    QVector<const PointConfig *>  pointsOfLink(const QString &linkId) const;
    int pointCountOfDevice(const QString &deviceId) const;

    QString summary() const;

private:
    //数据本体
    QVector<LinkConfig>   m_links;
    QVector<DeviceConfig> m_devices;
    QVector<PointConfig>  m_points;

    //正向索引
    QHash<QString, int> m_linkIndex;
    QHash<QString, int> m_deviceIndex;
    QHash<QString, int> m_pointIndex;

    //反向索引
    QHash<QString, QVector<int>> m_devicesByLink;
    QHash<QString, QVector<int>> m_pointsByDevice;
    QHash<QString, QVector<int>> m_pointsByLink;
};

} // namespace dm

#endif // DM_SYSTEM_CONFIG_H
