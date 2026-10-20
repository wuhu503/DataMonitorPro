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
        ConfigIssue issue;
        issue.file = fileName;
        issue.message = QStringLiteral("无法打开文件 %1：%2").arg(path, file.errorString());
        issues->append(issue);
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

        ConfigIssue issue;
        issue.file = fileName;
        issue.message = QStringLiteral("JSON 语法错误：%1（第 %2 行，第 %3 列）")
                            .arg(parseError.errorString())
                            .arg(line)
                            .arg(column);
        issues->append(issue);
        return false;
    }

    if (!doc.isArray())
    {
        ConfigIssue issue;
        issue.file = fileName;
        issue.message = QStringLiteral("顶层必须是数组，形如 [ {\"id\": ...}, {...} ]");
        issues->append(issue);
        return false;
    }

    *out = doc.array();
    return true;
}

void addIssue(QVector<ConfigIssue> *issues,
              const QString &file,
              int index,
              const QString &field,
              const QString &message,
              ConfigIssue::Severity severity = ConfigIssue::Severity::Error)
{
    ConfigIssue issue;
    issue.severity = severity;
    issue.file = file;
    issue.index = index;
    issue.field = field;
    issue.message = message;
    issues->append(issue);
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

    *out = jsonValue.toString();
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
            addIssue(issues, kLinksFile, i, NULL, QStringLiteral("数组元素必须是对象 { ... }"));
            continue;
        }

        const QJsonObject obj = value.toObject();
        LinkConfig link;

        //必填ID
        const QJsonValue idValue = obj.value(QStringLiteral("id"));
        if (!idValue.isString() || idValue.toString().isEmpty())
        {
            addIssue(issues, kLinksFile, i, QStringLiteral("id"), QStringLiteral("缺少必填字段，或它不是非空字符串"));
            continue;
        }
        link.id = idValue.toString();

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
            const QJsonValue hostValue = obj.value(QStringLiteral("host"));
            if (!hostValue.isString() || hostValue.toString().trimmed().isEmpty())
            {
                addIssue(issues, kLinksFile, i, QStringLiteral("host"), QStringLiteral("TCP 链路必须提供 host"));
                linkOk = false;
            }
            else
            {
                link.host = hostValue.toString().trimmed();
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
            const QJsonValue serialPortValue = obj.value(QStringLiteral("serialPort"));
            if (!serialPortValue.isString() || serialPortValue.toString().isEmpty())
            {
                addIssue(issues, kLinksFile, i, QStringLiteral("serialPort"), QStringLiteral("缺少必填字段，或它不是非空字符串"));
                linkOk = false;
            }
            else
            {
                link.serialPort = serialPortValue.toString();
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
            addIssue(issues, kDeviceFile, i, NULL, QStringLiteral("数组元素必须是对象 { ... }"));
            continue;
        }

        const QJsonObject obj = value.toObject();

        //必填Id
        const QJsonValue idValue = obj.value(QStringLiteral("id"));
        DeviceConfig device;
        if(!idValue.isString() || idValue.toString().isEmpty())
        {
            addIssue(issues, kDeviceFile, i, QStringLiteral("id"), QStringLiteral("必填字段不能为空"));
            continue;
        }
        device.id = idValue.toString();

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

        //必填linkIdValue校验
        const QJsonValue linkIdValue = obj.value(QStringLiteral("linkId"));
        if(!linkIdValue.isString() || linkIdValue.toString().isEmpty())
        {
            addIssue(issues, kDeviceFile, i, QStringLiteral("linkId"), QStringLiteral("必填字段不能为空"));
            continue;
        }
        device.linkId = linkIdValue.toString().trimmed();

        //slaveId校验
        const QJsonValue slaveIdValue = obj.value(QStringLiteral("slaveId"));
        if(!slaveIdValue.isDouble())
        {
            addIssue(issues, kDeviceFile, i, QStringLiteral("slaveId"), QStringLiteral("必填字段不能为空"));
            continue;
        }
        int slaveId = slaveIdValue.toInt();
        if(slaveId < 1 || slaveId > 247 )
        {
            addIssue(issues, kDeviceFile, i, QStringLiteral("slaveId"), QStringLiteral("salveId超出范围1..247,实际为%1").arg(slaveId));
            continue;
        }
        device.slaveId = slaveId;

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

} // namespace dm
