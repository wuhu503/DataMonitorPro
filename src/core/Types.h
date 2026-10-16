#ifndef DM_TYPES_H
#define DM_TYPES_H

#include <QString>

namespace dm {

//枚举类：链路类型/数据类型/字节序/轮询度/报警程度
enum class LinkType { Tcp, Serial };
enum class DataType { UInt16, Int16, UInt32, Int32, Float32 };
enum class ByteOrder { ABCD, CDAB, BADC, DCBA };
enum class PollGroup { Fast, Normal, Slow };
enum class AlarmLevel { None, Warning, Major, Critical };

QString linkTypeKey(LinkType v);
bool parseLinkType(const QString& text, LinkType* out);
QString linkTypeDisplay(LinkType v);

QString dataTypeKey(DataType v);
bool parseDataType(const QString& text, DataType* out);
QString dataTypeDisplay(DataType v);

QString byteOrderKey(ByteOrder v);
bool parseByteOrder(const QString& text, ByteOrder* out);
QString byteOrderDisplay(ByteOrder v);

QString pollGroupKey(PollGroup v);
bool parsePollGroup(const QString& text, PollGroup* out);
QString pollGroupDisplay(PollGroup v);

QString alarmLevelKey(AlarmLevel v);
bool parseAlarmLevel(const QString& text, AlarmLevel* out);
QString alarmLevelDisplay(AlarmLevel v);

//返回占用寄存器个数
int registerSpan(DataType t);

} // namespace dm

#endif // DM_TYPES_H
