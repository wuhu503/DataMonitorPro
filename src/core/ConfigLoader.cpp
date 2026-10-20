#include "core/ConfigLoader.h"

#include <QDir>
#include <QFile>
#include <QString>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace dm {

namespace {

const QString kLinksFile = QStringLiteral("links.json");
const QString kDeviceFile = QStringLiteral("devices.json");
const QString kPointsFile = QStringLiteral("points.json");

//从pareseError里找到准确的行号和大致的列号
void lineColumnOf(const QByteArray &data, int offset, int *line, int *column)
{
    int currentLine = 1;
    int currentColumn = 1;

    const int limit = qMin(offset, int(data.size()));
    for (int i = 0; i < limit; ++i)
    {
        if (data.at(i) == '\n')
        {
            ++currentLine;
            currentColumn = 1;
        }

        else
        {
            ++currentColumn;
        }
    }

    *line = currentLine;
    *column = currentColumn;
}

//读JSON文件并记录错误信息
bool readJsonArray(const QString &configDir,
                   const QString &fileName,
                   QJsonArray *out,
                   QVector<ConfigIssue> *issues)
{
    const QString path = QDir(configDir).filePath(fileName);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        addIssue(issues, fileName, -1, QString(), QStringLiteral("无法打开文件 %1：%2").arg(path, file.errorString()));
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        int line = 0;
        int column = 0;
        lineColumnOf(data, parseError.offset, &line, &column);

        addIssue(issues, fileName, -1, QString(), QStringLiteral("JSON 语法错误：%1（第 %2 行，第 %3 列）")
                                                 .arg(parseError.errorString())
                                                 .arg(line)
                                                 .arg(column)
                 );
        return false;
    }

    if (!doc.isArray())
    {
        addIssue(issues, fileName, -1, QString(), QStringLiteral("顶层必须是数组，形如 [ {\"id\": ...}, {...} ]"));
        return false;
    }

    *out = doc.array();
    return true;
}

//必读字段处理
bool readRequiredString(const QJsonObject &obj,
                        const QString &fileName,
                        const QString &key,
                        int index,
                        QVector<ConfigIssue> *issues,
                        QString *out)
{
    const QJsonValue jsonValue = obj.value(key);
    if (!jsonValue.isString() || jsonValue.toString().trimmed().isEmpty())
    {
        addIssue(issues, fileName, index, key, QStringLiteral("%1 是必填字段，且必须是非空字符串").arg(key));
        return false;
    }

    *out = jsonValue.toString().trimmed();
    return true;
}

bool readOptionalInt(const QJsonObject &obj,
                     const QString &fileName,
                     const QString &key,
                     int minValue,
                     int maxValue,
                     int index,
                     QVector<ConfigIssue> *issues,
                     int *out)
{
    const QJsonValue jsonValue = obj.value(key);
    if(jsonValue.isUndefined())
    {
        return true;
    }

    if(!jsonValue.isDouble())
    {
        addIssue(issues, fileName, index, key, QStringLiteral("%1错误数据类型").arg(key));
        return false;
    }

    const int intValue = jsonValue.toInt();

    if(intValue < minValue || intValue > maxValue)
    {
        addIssue(issues, fileName, index, key, QStringLiteral("超出范围 %1..%2，实际是 %3").arg(minValue).arg(maxValue).arg(intValue));
        return false;
    }

    *out = intValue;
    return true;
}

//必填整数
bool readRequiredInt(const QJsonObject &obj,
                     const QString &fileName,
                     const QString &key,
                     int minValue,
                     int maxValue,
                     int index,
                     QVector<ConfigIssue> *issues,
                     int *out)
{
    if (!obj.contains(key))
    {
        addIssue(issues, fileName, index, key,
                 QStringLiteral("%1 是必填字段").arg(key));
        return false;
    }

    return readOptionalInt(obj, fileName, key, minValue, maxValue, index, issues, out);
}

bool readOptionalString(const QJsonObject &obj,
                        const QString &fileName,
                        const QString &key,
                        int index,
                        QVector<ConfigIssue> *issues,
                        QString *out)
{
    const QJsonValue jsonValue = obj.value(key);

    if(jsonValue.isUndefined())
    {
        return true;
    }
    if(!jsonValue.isString())
    {
        addIssue(issues, fileName, index, key, QStringLiteral("%1输入内容必须为字符串").arg(key));
        return false;
    }

    *out = jsonValue.toString().trimmed();
    return true;
}

bool readOptionalBool(const QJsonObject &obj,
                      const QString &fileName,
                      const QString &key,
                      int index,
                      QVector<ConfigIssue> *issues,
                      bool *out)
{
    const QJsonValue jsonValue = obj.value(key);
    if(jsonValue.isUndefined())
    {
        return true;
    }
    if(!jsonValue.isBool())
    {
        addIssue(issues, fileName, index, key, QStringLiteral("%1输入内容必须为Bool").arg(key));
        return false;
    }

    *out = jsonValue.toBool();
    return true;
}

bool readOptionalDouble(const QJsonObject &obj,
                        const QString &fileName,
                        const QString &key,
                        int index,
                        QVector<ConfigIssue> *issues,
                        double *out)
{
    const QJsonValue jsonValue = obj.value(key);
    if(jsonValue.isUndefined())
    {
        return true;
    }
    if(!jsonValue.isDouble())
    {
        addIssue(issues, fileName, index, key, QStringLiteral("输入错误的数据类型"));
        return false;
    }

    *out = jsonValue.toDouble();
    return true;
}

} // namespace

bool ConfigLoader::loadLinks(const QString &configDir,
                             QVector<LinkConfig> *out,
                             QVector<ConfigIssue> *issues)
{
    //清空内容，防止旧数据污染
    out->clear();

    QJsonArray array;
    if (!readJsonArray(configDir, kLinksFile, &array, issues))
    {
        return false;
    }

    //记下调用前的长度
    const int issueCountBefore = issues->size();

    for (int i = 0; i < array.size(); ++i)
    {
        const QJsonValue value = array.at(i);

        if (!value.isObject())
        {
            addIssue(issues, kLinksFile, i, QString(), QStringLiteral("数组元素必须是对象 { ... }"));
            continue;
        }

        const QJsonObject obj = value.toObject();
        LinkConfig link;

        //必填ID
        if (!readRequiredString(obj, kLinksFile, QStringLiteral("id"), i, issues, &link.id))
        {
            continue;
        }

        //必填type，必须是已知值
        const QJsonValue typeValue = obj.value(QStringLiteral("type"));
        if (!typeValue.isString())
        {
            addIssue(issues, kLinksFile, i, QStringLiteral("type"), QStringLiteral("缺少必填字段，或它不是非空字符串"));
            continue;
        }

        //判断类型，并且生成枚举
        if (!parseLinkType(typeValue.toString(), &link.type))
        {
            addIssue(issues, kLinksFile, i, QStringLiteral("type"), QStringLiteral("未知的链路类型 \"%1\"，可选值：tcp / serial")
                                                                       .arg(typeValue.toString()));
            continue;
        }

        //分别校验responseTimeoutMs|retries|retryIntervalMs|description
        bool linkOk = true;
        //一次性跑完全程
        if(!readOptionalInt(obj, kLinksFile, QStringLiteral("responseTimeoutMs"), 50, 60000, i, issues, &link.responseTimeoutMs))
        {
            linkOk = false;
        }
        if(!readOptionalInt(obj, kLinksFile, QStringLiteral("retries"), 0, 10, i, issues, &link.retries))
        {
            linkOk = false;
        }
        if(!readOptionalInt(obj, kLinksFile, QStringLiteral("retryIntervalMs"), 0, 60000, i, issues, &link.retryIntervalMs))
        {
            linkOk = false;
        }
        if(!readOptionalString(obj, kLinksFile, QStringLiteral("description"), i, issues, &link.description))
        {
            linkOk = false;
        }

        //Tcp链路校验
        if (link.type == LinkType::Tcp)
        {
            //host校验
            if (!readRequiredString(obj, kLinksFile, QStringLiteral("host"), i, issues, &link.host))
            {
                linkOk = false;
            }
            //port校验
            int portValue = 502;
            if (!readOptionalInt(obj, kLinksFile, QStringLiteral("port"), 1, 65535, i, issues, &portValue))
            {
                linkOk = false;
            }
            else
            {
                link.port = static_cast<quint16>(portValue);
            }
        }
        //Serial链路校验
        else if(link.type == LinkType::Serial)
        {
            //port校验
            if (!readRequiredString(obj, kLinksFile, QStringLiteral("serialPort"), i, issues, &link.serialPort))
            {
                linkOk = false;
            }

            //baud校验
            const QJsonValue baudRateValue = obj.value(QStringLiteral("baudRate"));
            if(!baudRateValue.isUndefined())
            {
                if(!baudRateValue.isDouble())
                {
                    addIssue(issues, kLinksFile, i, QStringLiteral("baudRate"), QStringLiteral("baudRate数据类型错误"));
                    linkOk = false;
                }
                else
                {
                    switch (baudRateValue.toInt())
                    {
                    case 1200:
                    case 2400:
                    case 4800:
                    case 9600:
                    case 19200:
                    case 38400:
                    case 57600:
                    case 115200:
                        link.baudRate = baudRateValue.toInt();
                        break;
                    default:
                        addIssue(issues, kLinksFile, i, QStringLiteral("baudRate"), QStringLiteral("baudRate数值无效"));
                        linkOk = false;
                        break;
                    }
                }
            }

            //dataBits校验
            if(!readOptionalInt(obj, kLinksFile, QStringLiteral("dataBits"), 5, 8, i, issues, &link.dataBits))
            {
                linkOk = false;
            }
            //parity校验
            if(!readOptionalInt(obj, kLinksFile, QStringLiteral("parity"), 0, 2, i, issues, &link.parity))
            {
                linkOk = false;
            }
            //stopBits校验
            if(!readOptionalInt(obj, kLinksFile, QStringLiteral("stopBits"), 1, 2, i, issues, &link.stopBits))
            {
                linkOk = false;
            }
            //interFrameDelayMs校验
            if(!readOptionalInt(obj, kLinksFile, QStringLiteral("interFrameDelayMs"), 0, 60000, i, issues, &link.interFrameDelayMs))
            {
                linkOk = false;
            }
        }

        // 类型与字段不匹配：不阻断，但多半是 type 写错了
        if (link.type == LinkType::Tcp && obj.contains(QStringLiteral("serialPort")))
        {
            addIssue(issues, kLinksFile, i, QStringLiteral("serialPort"),
                     QStringLiteral("TCP 链路不应有 serialPort 字段，请确认 type"),
                     ConfigIssue::Severity::Warning);
        }
        else if (link.type == LinkType::Serial && obj.contains(QStringLiteral("host")))
        {
            addIssue(issues, kLinksFile, i, QStringLiteral("host"),
                     QStringLiteral("串口链路不应有 host 字段，请确认 type"),
                     ConfigIssue::Severity::Warning);
        }

        if (!linkOk)
        {
            continue;
        }

        out->append(link);
    }

    //本次调用只要产生过一条 Error，整体就算失败
    for (int k = issueCountBefore; k < issues->size(); ++k) {
        if (issues->at(k).isError())
            return false;
    }
    return true;
}

bool ConfigLoader::loadDevices(const QString &configDir,
                        QVector<DeviceConfig> *out,
                        QVector<ConfigIssue> *issues)
{
    out->clear();

    QJsonArray array;
    if (!readJsonArray(configDir, kDeviceFile, &array, issues))
    {
        return false;
    }

    const int issueCountBefore = issues->size();

    for (int i = 0; i < array.size(); ++i)
    {
        const QJsonValue value = array.at(i);
        bool deviceOk = true;

        //顶层必须为数组
        if (!value.isObject())
        {
            addIssue(issues, kDeviceFile, i, QString(), QStringLiteral("数组元素必须是对象 { ... }"));
            continue;
        }

        const QJsonObject obj = value.toObject();
        DeviceConfig device;

        //必填Id
        if (!readRequiredString(obj, kDeviceFile, QStringLiteral("id"), i, issues, &device.id))
        {
            continue;
        }

        //name非必填，无则id填充
        QString name;
        if (!readOptionalString(obj, kDeviceFile, QStringLiteral("name"), i, issues, &name))
        {
            deviceOk = false;
        }
        else
        {
            device.name = name.isEmpty() ? device.id : name;
        }

        //必填linkId校验
        if (!readRequiredString(obj, kDeviceFile, QStringLiteral("linkId"), i, issues, &device.linkId))
        {
            continue;
        }

        //slaveId校验
        if (!readRequiredInt(obj, kDeviceFile, QStringLiteral("slaveId"), 1, 247,
                             i, issues, &device.slaveId))
        {
            continue;
        }

        //enabled校验
        if(!readOptionalBool(obj, kDeviceFile, QStringLiteral("enabled"), i, issues, &device.enabled))
        {
            deviceOk = false;
        }
        //description校验
        if(!readOptionalString(obj, kDeviceFile, QStringLiteral("description"), i, issues, &device.description))
        {
            deviceOk = false;
        }
        if (!deviceOk)
        {
            continue;
        }

        out->append(device);
    }

    //本次调用只要产生过一条 Error，整体就算失败
    for (int k = issueCountBefore; k < issues->size(); ++k) {
        if (issues->at(k).isError())
            return false;
    }
    return true;
}

bool ConfigLoader::loadPoints(const QString &configDir,
                              QVector<PointConfig> *out,
                              QVector<ConfigIssue> *issues)
{
    out->clear();

    QJsonArray array;
    if (!readJsonArray(configDir, kPointsFile, &array, issues))
    {
        return false;
    }
    const int issueCountBefore = issues->size();
    for(int i = 0; i<array.size(); ++i)
    {
        QJsonValue value = array.at(i);
        //判断顶层数组
        if (!value.isObject())
        {
            addIssue(issues, kPointsFile, i, QString(), QStringLiteral("数组元素必须是对象 { ... }"));
            continue;
        }
        const QJsonObject obj = value.toObject();
        PointConfig point;

        //必填id
        if(!readRequiredString(obj, kPointsFile, QStringLiteral("id"), i, issues, &point.id))
        {
            continue;
        }

        //必填deviceId
        if(!readRequiredString(obj, kPointsFile, QStringLiteral("deviceId"), i, issues, &point.deviceId))
        {
            continue;
        }

        //必填funcCode
        if(!readRequiredInt(obj, kPointsFile, QStringLiteral("funcCode"), 1, 4, i, issues, &point.funcCode))
        {
            continue;
        }

        //必填address
        int addressValue = 0;
        if(!readRequiredInt(obj, kPointsFile, QStringLiteral("address"), 0, 65535, i, issues, &addressValue))
        {
            continue;
        }
        point.address = static_cast<quint16>(addressValue);

        //必填dataType
        QString dataTypeText;
        if(!readRequiredString(obj, kPointsFile, QStringLiteral("dataType"), i, issues, &dataTypeText))
        {
            continue;
        }
        if (!parseDataType(dataTypeText, &point.dataType))
        {
            addIssue(issues, kPointsFile, i, QStringLiteral("dataType"),
                     QStringLiteral("未知的数据类型 \"%1\"，可选值：uint16 / int16 / uint32 / int32 / float32")
                         .arg(dataTypeText));
            continue;
        }

        bool pointOk = true;
        //name校验
        QString name;
        if (!readOptionalString(obj, kPointsFile, QStringLiteral("name"), i, issues, &name))
        {
            pointOk = false;
        }
        else
        {
            point.name = name.isEmpty() ? point.id : name;
        }

        //byteOrder校验
        QString byteOrderText = byteOrderKey(point.byteOrder);     // 默认 "ABCD"
        if (!readOptionalString(obj, kPointsFile, QStringLiteral("byteOrder"), i, issues, &byteOrderText))
        {
            pointOk = false;
        }
        else if (!parseByteOrder(byteOrderText, &point.byteOrder))
        {
            addIssue(issues, kPointsFile, i, QStringLiteral("byteOrder"),
                     QStringLiteral("未知的字节序 \"%1\"，可选值：ABCD / CDAB / BADC / DCBA").arg(byteOrderText));
            pointOk = false;
        }

        //pollGroup校验
        QString pollGroupText = pollGroupKey(point.pollGroup);
        if (!readOptionalString(obj, kPointsFile, QStringLiteral("pollGroup"), i, issues, &pollGroupText))
        {
            pointOk = false;
        }
        else if (!parsePollGroup(pollGroupText, &point.pollGroup))
        {
            addIssue(issues, kPointsFile, i, QStringLiteral("pollGroup"),
                     QStringLiteral("未知的轮询分组 \"%1\"，可选值：fast / normal / slow").arg(pollGroupText));
            pointOk = false;
        }

        //alarmLevel校验
        QString alarmLevelText = alarmLevelKey(point.alarmLevel);
        if (!readOptionalString(obj, kPointsFile, QStringLiteral("alarmLevel"), i, issues, &alarmLevelText))
        {
            pointOk = false;
        }
        else if (!parseAlarmLevel(alarmLevelText, &point.alarmLevel))
        {
            addIssue(issues, kPointsFile, i, QStringLiteral("alarmLevel"),
                     QStringLiteral("未知的报警等级 \"%1\"，可选值：none / warning / major / critical").arg(alarmLevelText));
            pointOk = false;
        }

        //scale
        if (!readOptionalDouble(obj, kPointsFile, QStringLiteral("scale"), i, issues, &point.scale))
        {
            pointOk = false;
        }
        //offset
        if (!readOptionalDouble(obj, kPointsFile, QStringLiteral("offset"), i, issues, &point.offset))
        {
            pointOk = false;
        }
        //deadband
        if (!readOptionalDouble(obj, kPointsFile, QStringLiteral("deadband"), i, issues, &point.deadband))
        {
            pointOk = false;
        }
        //alarmLow
        if (!readOptionalDouble(obj, kPointsFile, QStringLiteral("alarmLow"), i, issues, &point.alarmLow))
        {
            pointOk = false;
        }
        //alarmHigh
        if (!readOptionalDouble(obj, kPointsFile, QStringLiteral("alarmHigh"), i, issues, &point.alarmHigh))
        {
            pointOk = false;
        }

        // unit
        if (!readOptionalString(obj, kPointsFile, QStringLiteral("unit"), i, issues, &point.unit))
            pointOk = false;

        // decimals
        if (!readOptionalInt(obj, kPointsFile, QStringLiteral("decimals"), 0, 6, i, issues, &point.decimals))
            pointOk = false;

        // ---- 跨字段语义校验 ----
        if (point.scale == 0.0)
        {
            addIssue(issues, kPointsFile, i, QStringLiteral("scale"),
                     QStringLiteral("scale 不能为 0，否则物理量恒等于 offset"));
            pointOk = false;
        }

        if (point.alarmLevel != AlarmLevel::None && point.alarmLow >= point.alarmHigh)
        {
            addIssue(issues, kPointsFile, i, QStringLiteral("alarmLow"),
                     QStringLiteral("报警下限 %1 必须小于上限 %2")
                         .arg(point.alarmLow).arg(point.alarmHigh));
            pointOk = false;
        }

        const int span = registerSpan(point.dataType);
        if (int(point.address) + span - 1 > 65535)
        {
            addIssue(issues, kPointsFile, i, QStringLiteral("address"),
                     QStringLiteral("地址 %1 加 %2 个寄存器后超出 65535")
                         .arg(point.address).arg(span));
            pointOk = false;
        }

        if ((point.funcCode == 1 || point.funcCode == 2) && span > 1)
        {
            addIssue(issues, kPointsFile, i, QStringLiteral("dataType"),
                     QStringLiteral("功能码 %1 是位访问，不能使用 32 位类型")
                         .arg(point.funcCode));
            pointOk = false;
        }

        if (point.alarmLevel == AlarmLevel::None
            && (point.alarmLow != 0.0 || point.alarmHigh != 0.0))
        {
            addIssue(issues, kPointsFile, i, QStringLiteral("alarmLevel"),
                     QStringLiteral("配置了报警阈值但 alarmLevel 为 none，该点位不会报警"),
                     ConfigIssue::Severity::Warning);
        }

        if (!pointOk)
            continue;

        out->append(point);
    }
    for (int k = issueCountBefore; k < issues->size(); ++k)
    {
        if (issues->at(k).isError())
            return false;
    }
    return true;
}

} // namespace dm
