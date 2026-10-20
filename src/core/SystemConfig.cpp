#include "core/SystemConfig.h"

#include <utility>

namespace dm {
namespace {
const QString kLinksFile   = QStringLiteral("links.json");
const QString kDevicesFile = QStringLiteral("devices.json");
const QString kPointsFile  = QStringLiteral("points.json");
}

void SystemConfig::clear()
{
    m_links.clear();
    m_devices.clear();
    m_points.clear();

    m_linkIndex.clear();
    m_deviceIndex.clear();
    m_pointIndex.clear();
    m_devicesByLink.clear();
    m_pointsByDevice.clear();
    m_pointsByLink.clear();
}

void SystemConfig::setLinks(QVector<LinkConfig> links)
{
    m_links = std::move(links);
}
void SystemConfig::setDevices(QVector<DeviceConfig> devices)
{
    m_devices = std::move(devices);
}
void SystemConfig::setPoints(QVector<PointConfig> points)
{
    m_points = std::move(points);
}

bool SystemConfig::rebuildIndexes(QVector<ConfigIssue> *issues)
{
    m_linkIndex.clear();
    m_deviceIndex.clear();
    m_pointIndex.clear();
    m_devicesByLink.clear();
    m_pointsByDevice.clear();
    m_pointsByLink.clear();

    const int issueCountBefore = issues->size();

    //同一条链路上slaveId必须唯一
    QHash<QString, QHash<int, int>> slaveSeen;

    for(int i = 0; i < m_links.size(); ++i)
    {
        const LinkConfig &lk = m_links.at(i);
        if(m_linkIndex.contains(lk.id))
        {
            addIssue(issues, kLinksFile, i, QStringLiteral("id"),QStringLiteral("链路 id 重复：\"%1\"").arg(lk.id));
            continue;
        }
        m_linkIndex.insert(lk.id, i);
    }

    for (int i = 0; i < m_devices.size(); ++i)
    {
        const DeviceConfig &dv = m_devices.at(i);

        if (m_deviceIndex.contains(dv.id))
        {
            addIssue(issues, kDevicesFile, i, QStringLiteral("id"),
                     QStringLiteral("设备 id 重复：\"%1\"").arg(dv.id));
            continue;
        }
        m_deviceIndex.insert(dv.id, i);

        if (!m_linkIndex.contains(dv.linkId))
        {
            addIssue(issues, kDevicesFile, i, QStringLiteral("linkId"),
                     QStringLiteral("链路 \"%1\" 不存在").arg(dv.linkId));
            continue;
        }

        m_devicesByLink[dv.linkId].append(i);

        //不同链路下允许slaveId重复
        auto &slaveSlot = slaveSeen[dv.linkId];
        if (slaveSlot.contains(dv.slaveId))
        {
            addIssue(issues, kDevicesFile, i, QStringLiteral("slaveId"),
                     QStringLiteral("链路 \"%1\" 上 slaveId %2 与第 %3 台设备重复")
                         .arg(dv.linkId).arg(dv.slaveId).arg(slaveSlot.value(dv.slaveId)));
        }
        else
        {
            slaveSlot.insert(dv.slaveId, i);
        }
    }

    //同一设备、同一功能码的寄存器区间不能重叠
    QHash<QString, QVector<int>> addressGroups;

    for (int i = 0; i < m_points.size(); ++i)
    {
        const PointConfig &pn = m_points.at(i);

        if (m_pointIndex.contains(pn.id))
        {
            addIssue(issues, kPointsFile, i, QStringLiteral("id"),
                     QStringLiteral("点位 id 重复：\"%1\"").arg(pn.id));
            continue;
        }
        m_pointIndex.insert(pn.id, i);

        if (!m_deviceIndex.contains(pn.deviceId))
        {
            addIssue(issues, kPointsFile, i, QStringLiteral("deviceId"),
                     QStringLiteral("设备 \"%1\" 不存在").arg(pn.deviceId));
            continue;
        }

        m_pointsByDevice[pn.deviceId].append(i);

        const int devIndex = m_deviceIndex.value(pn.deviceId);
        const QString &linkId = m_devices.at(devIndex).linkId;
        //设备的linkId可能无效，防止点位挂到不存在的链路下
        if (m_linkIndex.contains(linkId))
            m_pointsByLink[linkId].append(i);

        //寄存器重叠检查
        const QString groupKey = pn.deviceId + QStringLiteral("#") + QString::number(pn.funcCode);
        const int lo = pn.address;
        const int hi = pn.address + registerSpan(pn.dataType) - 1;

        auto &group = addressGroups[groupKey];
        for (int otherIndex : group)
        {
            const PointConfig &other = m_points.at(otherIndex);
            const int otherLo = other.address;
            const int otherHi = other.address + registerSpan(other.dataType) - 1;

            if (lo <= otherHi && otherLo <= hi)
            {
                addIssue(issues, kPointsFile, i, QStringLiteral("address"),
                         QStringLiteral("寄存器区间 [%1,%2] 与点位 \"%3\" 的 [%4,%5] 重叠")
                             .arg(lo).arg(hi).arg(other.id).arg(otherLo).arg(otherHi),
                         ConfigIssue::Severity::Warning);
                break;      // 一个点位只报一次
            }
        }
        group.append(i);
    }
    for (int k = issueCountBefore; k < issues->size(); ++k)
    {
        if (issues->at(k).isError())
            return false;
    }
    return true;
}

const LinkConfig *SystemConfig::link(const QString &id) const
{
    const auto it = m_linkIndex.constFind(id);
    if (it == m_linkIndex.constEnd())
        return nullptr;
    return &m_links.at(*it);
}

const DeviceConfig *SystemConfig::device(const QString &id) const
{
    const auto it = m_deviceIndex.constFind(id);
    if (it == m_deviceIndex.constEnd())
        return nullptr;
    return &m_devices.at(*it);
}

const PointConfig *SystemConfig::point(const QString &id) const
{
    const auto it = m_pointIndex.constFind(id);
    if (it == m_pointIndex.constEnd())
        return nullptr;
    return &m_points.at(*it);
}

//反向查询：父级 id -> 子级对象
QVector<const DeviceConfig *> SystemConfig::devicesOfLink(const QString &linkId) const
{
    QVector<const DeviceConfig *> result;

    const auto it = m_devicesByLink.constFind(linkId);
    if (it == m_devicesByLink.constEnd())
        return result;

    for (int index : *it)
        result.append(&m_devices.at(index));

    return result;
}

QVector<const PointConfig *> SystemConfig::pointsOfDevice(const QString &deviceId) const
{
    QVector<const PointConfig *> result;

    const auto it = m_pointsByDevice.constFind(deviceId);
    if (it == m_pointsByDevice.constEnd())
        return result;

    for (int index : *it)
        result.append(&m_points.at(index));

    return result;
}

QVector<const PointConfig *> SystemConfig::pointsOfLink(const QString &linkId) const
{
    QVector<const PointConfig *> result;

    const auto it = m_pointsByLink.constFind(linkId);
    if (it == m_pointsByLink.constEnd())
        return result;

    for (int index : *it)
    {
        const PointConfig &pt = m_points.at(index);

        //enabled=false的设备不参与采集
        const auto devIt = m_deviceIndex.constFind(pt.deviceId);
        if (devIt == m_deviceIndex.constEnd())
            continue;
        if (!m_devices.at(*devIt).enabled)
            continue;

        result.append(&pt);
    }

    return result;
}

int SystemConfig::pointCountOfDevice(const QString &deviceId) const
{
    return m_pointsByDevice.value(deviceId).size();
}

QString SystemConfig::summary() const
{
    return QStringLiteral("链路 %1 · 设备 %2 · 点位 %3")
        .arg(m_links.size())
        .arg(m_devices.size())
        .arg(m_points.size());
}

}
