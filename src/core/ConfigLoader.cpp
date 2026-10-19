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

bool readOptionalInt(const QJsonObject &obj,
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
        ConfigIssue issue;
        issue.file = kLinksFile;
        issue.index = index;
        issue.field = key;
        issue.message = QStringLiteral("%1错误数据类型").arg(key);
        issues->append(issue);
        return false;
    }

    const int intValue = jsonValue.toInt();

    if(intValue < minValue || intValue > maxValue)
    {
        ConfigIssue issue;
        issue.file = kLinksFile;
        issue.index = index;
        issue.field = key;
        issue.message = QStringLiteral("超出范围 %1..%2，实际是 %3").arg(minValue).arg(maxValue).arg(intValue);
        issues->append(issue);
        return false;
    }

    *out = intValue;
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
        return false;

    //记下调用前的长度
    const int issueCountBefore = issues->size();

    for (int i = 0; i < array.size(); ++i)
    {
        const QJsonValue value = array.at(i);

        if (!value.isObject())
        {
            ConfigIssue issue;
            issue.file = kLinksFile;
            issue.index = i;
            issue.message = QStringLiteral("数组元素必须是对象 { ... }");
            issues->append(issue);
            continue;
        }

        const QJsonObject obj = value.toObject();
        LinkConfig link;

        //必填ID
        const QJsonValue idValue = obj.value(QStringLiteral("id"));
        if (!idValue.isString() || idValue.toString().isEmpty())
        {
            ConfigIssue issue;
            issue.file = kLinksFile;
            issue.index = i;
            issue.field = QStringLiteral("id");
            issue.message = QStringLiteral("缺少必填字段，或它不是非空字符串");
            issues->append(issue);
            continue;
        }
        link.id = idValue.toString();

        //必填type，必须是已知值
        const QJsonValue typeValue = obj.value(QStringLiteral("type"));
        if (!typeValue.isString())
        {
            ConfigIssue issue;
            issue.file = kLinksFile;
            issue.index = i;
            issue.field = QStringLiteral("type");
            issue.message = QStringLiteral("缺少必填字段，或它不是字符串");
            issues->append(issue);
            continue;
        }

        //判断类型，并且生成枚举
        if (!parseLinkType(typeValue.toString(), &link.type))
        {
            ConfigIssue issue;
            issue.file = kLinksFile;
            issue.index = i;
            issue.field = QStringLiteral("type");
            issue.message = QStringLiteral("未知的链路类型 \"%1\"，可选值：tcp / serial")
                                .arg(typeValue.toString());
            issues->append(issue);
            continue;
        }

        //分别校验responseTimeoutMs|retries|retryIntervalMs
        bool linkOk = true;
        //一次性跑完全程
        if(!readOptionalInt(obj,QStringLiteral("responseTimeoutMs"),50,60000,i,issues,&link.responseTimeoutMs))
        {
            linkOk = false;
        }
        if(!readOptionalInt(obj,QStringLiteral("retries"),0,10,i,issues,&link.retries))
        {
            linkOk = false;
        }
        if(!readOptionalInt(obj,QStringLiteral("retryIntervalMs"),0,60000,i,issues,&link.retryIntervalMs))
        {
            linkOk = false;
        }

        if (link.type == LinkType::Tcp)
        {
            const QJsonValue hostValue = obj.value(QStringLiteral("host"));
            if (!hostValue.isString() || hostValue.toString().trimmed().isEmpty())
            {
                ConfigIssue issue;
                issue.file = kLinksFile;
                issue.index = i;
                issue.field = QStringLiteral("host");
                issue.message = QStringLiteral("TCP 链路必须提供 host");
                issues->append(issue);
                linkOk = false;
            }
            else
            {
                link.host = hostValue.toString().trimmed();
            }

            int portValue = 502;
            if (!readOptionalInt(obj, QStringLiteral("port"), 1, 65535, i, issues, &portValue))
            {
                linkOk = false;
            }
            else
            {
                link.port = static_cast<quint16>(portValue);
            }
        }
        else if(link.type == LinkType::Serial)
        {
            const QJsonValue serialPortValue = obj.value(QStringLiteral("serialPort"));
            if (!serialPortValue.isString() || serialPortValue.toString().isEmpty())
            {
                ConfigIssue issue;
                issue.file = kLinksFile;
                issue.index = i;
                issue.field = QStringLiteral("serialPort");
                issue.message = QStringLiteral("缺少必填字段，或它不是非空字符串");
                issues->append(issue);
                linkOk = false;
            }
            else
            {
                link.serialPort = serialPortValue.toString();
            }

            const QJsonValue baudRateValue = obj.value(QStringLiteral("baudRate"));

            if(!baudRateValue.isUndefined())
            {
                if(!baudRateValue.isDouble())
                {
                    ConfigIssue issue;
                    issue.file = kLinksFile;
                    issue.index = i;
                    issue.field = QStringLiteral("baudRate");
                    issue.message = QStringLiteral("baudRate数据类型错误");
                    issues->append(issue);
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
                        ConfigIssue issue;
                        issue.file = kLinksFile;
                        issue.index = i;
                        issue.field = QStringLiteral("baudRate");
                        issue.message = QStringLiteral("baudRate数值无效");
                        issues->append(issue);
                        linkOk = false;
                        break;
                    }
                }
            }

            const QJsonValue dataBitsValue = obj.value(QStringLiteral("dataBits"));
            if(!readOptionalInt(obj, QStringLiteral("dataBits"), 5, 8, i, issues, &link.dataBits))
            {
                linkOk = false;
            }
            const QJsonValue parityValue = obj.value(QStringLiteral("parity"));
            if(!readOptionalInt(obj, QStringLiteral("parity"), 0, 2, i, issues, &link.parity))
            {
                linkOk = false;
            }
            const QJsonValue stopBitsValue = obj.value(QStringLiteral("stopBits"));
            if(!readOptionalInt(obj, QStringLiteral("stopBits"), 1, 2, i, issues, &link.stopBits))
            {
                linkOk = false;
            }
            const QJsonValue interFrameDelayMs = obj.value(QStringLiteral("interFrameDelayMs"));
            if(!readOptionalInt(obj, QStringLiteral("interFrameDelayMs"), 0, 60000, i, issues, &link.interFrameDelayMs))
            {
                linkOk = false;
            }
        }

        if (!linkOk)
        {
            continue;
        }
        // 其余字段下一步再读，现在先用结构体里的默认值。
        out->append(link);
    }

    // 本次调用只要产生过一条 Error，整体就算失败
    for (int k = issueCountBefore; k < issues->size(); ++k) {
        if (issues->at(k).isError())
            return false;
    }
    return true;
}

} // namespace dm
