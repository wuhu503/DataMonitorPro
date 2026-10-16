#include "core/Types.h"

namespace dm {


QString linkTypeKey(LinkType v)
{
    switch (v)
    {
    case LinkType::Tcp:
        return QStringLiteral("tcp");

    case LinkType::Serial:
        return QStringLiteral("serial");
    }

    return QString();
}

bool parseLinkType(const QString& text, LinkType* out)
{
    if (out == nullptr)
        return false;

    const QString value = text.trimmed();

    if (value.compare(QStringLiteral("tcp"), Qt::CaseInsensitive) == 0)
    {
        *out = LinkType::Tcp;
        return true;
    }

    if (value.compare(QStringLiteral("serial"), Qt::CaseInsensitive) == 0)
    {
        *out = LinkType::Serial;
        return true;
    }

    return false;
}

QString linkTypeDisplay(LinkType v)
{
    switch (v)
    {
    case LinkType::Tcp:
        return QStringLiteral("TCP");

    case LinkType::Serial:
        return QStringLiteral("串口");
    }

    return QStringLiteral("未知");
}




QString dataTypeKey(DataType v)
{
    switch (v)
    {
    case DataType::UInt16:
        return QStringLiteral("uint16");

    case DataType::Int16:
        return QStringLiteral("int16");

    case DataType::UInt32:
        return QStringLiteral("uint32");

    case DataType::Int32:
        return QStringLiteral("int32");

    case DataType::Float32:
        return QStringLiteral("float32");
    }

    return QString();
}

bool parseDataType(const QString& text, DataType* out)
{
    if (out == nullptr)
        return false;

    const QString value = text.trimmed();

    if (value.compare(QStringLiteral("uint16"), Qt::CaseInsensitive) == 0)
    {
        *out = DataType::UInt16;
        return true;
    }

    if (value.compare(QStringLiteral("int16"), Qt::CaseInsensitive) == 0)
    {
        *out = DataType::Int16;
        return true;
    }

    if (value.compare(QStringLiteral("uint32"), Qt::CaseInsensitive) == 0)
    {
        *out = DataType::UInt32;
        return true;
    }

    if (value.compare(QStringLiteral("int32"), Qt::CaseInsensitive) == 0)
    {
        *out = DataType::Int32;
        return true;
    }

    if (value.compare(QStringLiteral("float32"), Qt::CaseInsensitive) == 0)
    {
        *out = DataType::Float32;
        return true;
    }

    return false;
}

QString dataTypeDisplay(DataType v)
{
    switch (v)
    {
    case DataType::UInt16:
        return QStringLiteral("16位无符号");

    case DataType::Int16:
        return QStringLiteral("16位有符号");

    case DataType::UInt32:
        return QStringLiteral("32位无符号");

    case DataType::Int32:
        return QStringLiteral("32位有符号");

    case DataType::Float32:
        return QStringLiteral("32位浮点");
    }

    return QStringLiteral("未知");
}




QString byteOrderKey(ByteOrder v)
{
    switch (v)
    {
    case ByteOrder::ABCD:
        return QStringLiteral("ABCD");

    case ByteOrder::CDAB:
        return QStringLiteral("CDAB");

    case ByteOrder::BADC:
        return QStringLiteral("BADC");

    case ByteOrder::DCBA:
        return QStringLiteral("DCBA");
    }

    return QString();
}

bool parseByteOrder(const QString& text, ByteOrder* out)
{
    if (out == nullptr)
        return false;

    const QString value = text.trimmed();

    if (value.compare(QStringLiteral("ABCD"), Qt::CaseInsensitive) == 0)
    {
        *out = ByteOrder::ABCD;
        return true;
    }

    if (value.compare(QStringLiteral("CDAB"), Qt::CaseInsensitive) == 0)
    {
        *out = ByteOrder::CDAB;
        return true;
    }

    if (value.compare(QStringLiteral("BADC"), Qt::CaseInsensitive) == 0)
    {
        *out = ByteOrder::BADC;
        return true;
    }

    if (value.compare(QStringLiteral("DCBA"), Qt::CaseInsensitive) == 0)
    {
        *out = ByteOrder::DCBA;
        return true;
    }

    return false;
}

QString byteOrderDisplay(ByteOrder v)
{
    switch (v)
    {
    case ByteOrder::ABCD:
        return QStringLiteral("ABCD");

    case ByteOrder::CDAB:
        return QStringLiteral("CDAB");

    case ByteOrder::BADC:
        return QStringLiteral("BADC");

    case ByteOrder::DCBA:
        return QStringLiteral("DCBA");
    }

    return QStringLiteral("未知");
}




QString pollGroupKey(PollGroup v)
{
    switch (v)
    {
    case PollGroup::Fast:
        return QStringLiteral("fast");

    case PollGroup::Normal:
        return QStringLiteral("normal");

    case PollGroup::Slow:
        return QStringLiteral("slow");
    }

    return QString();
}

bool parsePollGroup(const QString& text, PollGroup* out)
{
    if (out == nullptr)
        return false;

    const QString value = text.trimmed();

    if (value.compare(QStringLiteral("fast"), Qt::CaseInsensitive) == 0)
    {
        *out = PollGroup::Fast;
        return true;
    }

    if (value.compare(QStringLiteral("normal"), Qt::CaseInsensitive) == 0)
    {
        *out = PollGroup::Normal;
        return true;
    }

    if (value.compare(QStringLiteral("slow"), Qt::CaseInsensitive) == 0)
    {
        *out = PollGroup::Slow;
        return true;
    }

    return false;
}

QString pollGroupDisplay(PollGroup v)
{
    switch (v)
    {
    case PollGroup::Fast:
        return QStringLiteral("快");

    case PollGroup::Normal:
        return QStringLiteral("普通");

    case PollGroup::Slow:
        return QStringLiteral("慢");
    }

    return QStringLiteral("未知");
}




QString alarmLevelKey(AlarmLevel v)
{
    switch (v)
    {
    case AlarmLevel::None:
        return QStringLiteral("none");

    case AlarmLevel::Warning:
        return QStringLiteral("warning");

    case AlarmLevel::Major:
        return QStringLiteral("major");

    case AlarmLevel::Critical:
        return QStringLiteral("critical");
    }

    return QString();
}

bool parseAlarmLevel(const QString& text, AlarmLevel* out)
{
    if (out == nullptr)
        return false;

    const QString value = text.trimmed();

    if (value.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0)
    {
        *out = AlarmLevel::None;
        return true;
    }

    if (value.compare(QStringLiteral("warning"), Qt::CaseInsensitive) == 0)
    {
        *out = AlarmLevel::Warning;
        return true;
    }

    if (value.compare(QStringLiteral("major"), Qt::CaseInsensitive) == 0)
    {
        *out = AlarmLevel::Major;
        return true;
    }

    if (value.compare(QStringLiteral("critical"), Qt::CaseInsensitive) == 0)
    {
        *out = AlarmLevel::Critical;
        return true;
    }

    return false;
}

QString alarmLevelDisplay(AlarmLevel v)
{
    switch (v)
    {
    case AlarmLevel::None:
        return QStringLiteral("无");

    case AlarmLevel::Warning:
        return QStringLiteral("警告");

    case AlarmLevel::Major:
        return QStringLiteral("重大");

    case AlarmLevel::Critical:
        return QStringLiteral("紧急");
    }

    return QStringLiteral("未知");
}




int registerSpan(DataType t)
{
    switch (t)
    {

    case DataType::UInt16:
    case DataType::Int16:
        return 1;

    case DataType::UInt32:
    case DataType::Int32:
    case DataType::Float32:
        return 2;
    }

    return 0;
}

} // namespace dm
