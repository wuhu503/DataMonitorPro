#include "ui/MainWindow.h"

#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QString>
#include <QVector>

#include <cmath>

#include "core/ConfigLoader.h"
#include "core/Crc16.h"
#include "core/RegisterCodec.h"
#include "core/SystemConfig.h"
#include "core/Types.h"
#include "transport/ITransport.h"
#include "transport/TcpTransport.h"

// ===============================================================
// Types 自检（临时验证代码）
//
// 这段代码在 S2 引入 Qt Test 之后会被迁移成正式的单元测试，现在只是
// 为了在没有测试框架的情况下把 Types 的正确性钉死。
//
// 检查项的文字故意用 ASCII：Windows 上 qDebug 走管道输出中文可能被
// 按本地代码页编码，Qt Creator 按 UTF-8 解读就会显示成乱码。注释里
// 用中文说明每一项在验证什么。
// ===============================================================
namespace {

int g_passed = 0;
int g_failed = 0;

// 把"期望值 vs 实际值"的比较交给程序做，而不是打印出来靠人眼看。
// 人眼在第 20 行之后就开始放空了，而这一行代码永远不会。
void check(bool ok, const QString &what)
{
    if (ok) {
        ++g_passed;
        qDebug().noquote() << "  [PASS]" << what;
    } else {
        ++g_failed;
        qDebug().noquote() << "  [FAIL]" << what;
    }
}

// ===============================================================
// CRC16 自检
//
// 期望值由独立的 Python 实现算出，不是拿被测代码"验自己"。
// ===============================================================
// ===============================================================
// TcpTransport 自检（连接失败路径）
//
// 现在还没有服务端，所以只能验证"连不上"这条路径。
// 成功路径要等 write/readExact 写完、并且起了仿真从站之后才能测。
// ===============================================================
void selfTestTcpTransport()
{
    qDebug().noquote() << "=== TcpTransport self-check ===";

    // 127.0.0.1 上没有程序监听 59999
    dm::TcpTransport transport(QStringLiteral("127.0.0.1"), 59999);

    check(!transport.isOpen(), QStringLiteral("new transport starts closed"));
    check(!transport.open(), QStringLiteral("connect to closed port fails"));
    check(!transport.isOpen(), QStringLiteral("transport stays closed after failure"));
    check(!transport.lastError().isEmpty(),
          QStringLiteral("lastError is set after failure"));

    transport.close();
    check(!transport.isOpen(), QStringLiteral("close() leaves transport closed"));
}

// ===============================================================
// TcpTransport 端到端测试
//
// 需要先启动 tools/echo_server.py——它按顺序提供三种行为：
//   连接 1 echo   ：原样回
//   连接 2 split  ：分两次发送（半包）
//   连接 3 silent ：不回（超时）
//
// 单独运行：DataMonitorPro.exe --test-tcp
// ===============================================================
int runTcpIntegrationTest()
{
    qDebug().noquote() << "=== TcpTransport integration test ===";

    const QString host = QStringLiteral("127.0.0.1");
    const quint16 port = 15502;
    // MBAP(7) + PDU(5) = 12 字节：
    //   事务号 0001 | 协议号 0000 | 长度 0006 | 单元号 01 | 03 | 起始 0000 | 数量 0002
    const QByteArray frame = QByteArray::fromHex("000100000006010300000002");

    // ---- 1. 基本收发 ----
    {
        dm::TcpTransport transport(host, port);
        check(transport.open(), QStringLiteral("echo: connect"));
        check(transport.write(frame), QStringLiteral("echo: write 12 bytes"));

        QByteArray response;
        const bool ok = transport.readExact(frame.size(), &response, 1000);
        check(ok, QStringLiteral("echo: readExact(12) returns true"));
        check(response == frame, QStringLiteral("echo: bytes match"));
    }

    // ---- 2. 半包：服务端先发 4 字节，200ms 后再发 4 字节 ----
    {
        dm::TcpTransport transport(host, port);
        check(transport.open(), QStringLiteral("split: connect"));
        check(transport.write(frame), QStringLiteral("split: write 12 bytes"));

        QByteArray response;
        const bool ok = transport.readExact(frame.size(), &response, 2000);
        check(ok, QStringLiteral("split: readExact waits for both chunks"));
        check(response == frame, QStringLiteral("split: frame reassembled"));
    }

    // ---- 3. 超时：服务端收到但不回 ----
    {
        dm::TcpTransport transport(host, port);
        check(transport.open(), QStringLiteral("timeout: connect"));
        check(transport.write(frame), QStringLiteral("timeout: write 12 bytes"));

        QByteArray response;
        QElapsedTimer timer;
        timer.start();
        const bool ok = transport.readExact(frame.size(), &response, 400);
        const qint64 elapsed = timer.elapsed();

        check(!ok, QStringLiteral("timeout: readExact returns false"));
        check(elapsed >= 300 && elapsed < 2000,
              QStringLiteral("timeout: waited roughly 400ms"));
    }

    return g_failed == 0 ? 0 : 1;
}

void selfTestCrc16()
{
    qDebug().noquote() << "=== CRC16 self-check ===";

    check(dm::crc16(QByteArray::fromHex("01030000000A")) == 0xCDC5,
          QStringLiteral("crc16(01 03 00 00 00 0A) == 0xCDC5"));
    check(dm::crc16(QByteArray::fromHex("010300000001")) == 0x0A84,
          QStringLiteral("crc16(01 03 00 00 00 01) == 0x0A84"));
    check(dm::crc16(QByteArray::fromHex("110300000002")) == 0x9BC6,
          QStringLiteral("crc16(11 03 00 00 00 02) == 0x9BC6"));

    // 空数据：循环一次都不跑，返回初值
    check(dm::crc16(QByteArray()) == 0xFFFF,
          QStringLiteral("crc16(empty) == 0xFFFF"));

    // 改一个字节，结果必须变（防止"永远返回常量"这类实现）
    check(dm::crc16(QByteArray::fromHex("01030000000A"))
              != dm::crc16(QByteArray::fromHex("01030000000B")),
          QStringLiteral("crc16 changes when one byte changes"));

    // Modbus 的经典性质：把 CRC 低字节在前追加到帧尾后，
    // 整帧再算一次 CRC 应得 0。
    QByteArray frame = QByteArray::fromHex("01030000000A");
    const quint16 crc = dm::crc16(frame);
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    check(dm::crc16(frame) == 0x0000,
          QStringLiteral("appended CRC makes whole-frame CRC == 0"));

    qDebug().noquote() << QStringLiteral("  full frame: %1")
                              .arg(QString::fromLatin1(frame.toHex(' ').toUpper()));
}

// ===============================================================
// RegisterCodec 自检（第一层：16 位类型）
//
// 期望值：
//   41 8C  uint16 → 16780
//   FF FF  int16  → -1
//   FF FF  uint16 → 65535
//   02 D5  int16  → 725
// ===============================================================
void selfTestRegisterCodec()
{
    qDebug().noquote() << "=== RegisterCodec self-check ===";

    double value = 0;

    bool ok = dm::decodeRegisters(QByteArray::fromHex("418C"), dm::DataType::UInt16,
                                  dm::ByteOrder::ABCD, &value);
    check(ok && value == 16780.0, QStringLiteral("uint16 41 8C == 16780"));

    value = 0;
    ok = dm::decodeRegisters(QByteArray::fromHex("FFFF"), dm::DataType::Int16,
                             dm::ByteOrder::ABCD, &value);
    check(ok && value == -1.0, QStringLiteral("int16 FF FF == -1"));

    value = 0;
    ok = dm::decodeRegisters(QByteArray::fromHex("FFFF"), dm::DataType::UInt16,
                             dm::ByteOrder::ABCD, &value);
    check(ok && value == 65535.0, QStringLiteral("uint16 FF FF == 65535"));

    value = 0;
    ok = dm::decodeRegisters(QByteArray::fromHex("02D5"), dm::DataType::Int16,
                             dm::ByteOrder::ABCD, &value);
    check(ok && value == 725.0, QStringLiteral("int16 02 D5 == 725"));

    // 负向：长度不足
    check(!dm::decodeRegisters(QByteArray::fromHex("41"), dm::DataType::UInt16,
                               dm::ByteOrder::ABCD, &value),
          QStringLiteral("1 byte + uint16 returns false"));

    // ---- 32 位（当前只处理 ABCD 字序）----
    value = 0;
    ok = dm::decodeRegisters(QByteArray::fromHex("0000007D"), dm::DataType::UInt32,
                             dm::ByteOrder::ABCD, &value);
    check(ok && value == 125.0, QStringLiteral("uint32 00 00 00 7D == 125"));

    value = 0;
    ok = dm::decodeRegisters(QByteArray::fromHex("FFFFFFFF"), dm::DataType::Int32,
                             dm::ByteOrder::ABCD, &value);
    check(ok && value == -1.0, QStringLiteral("int32 FF FF FF FF == -1"));

    value = 0;
    ok = dm::decodeRegisters(QByteArray::fromHex("FFFFFFFF"), dm::DataType::UInt32,
                             dm::ByteOrder::ABCD, &value);
    check(ok && value == 4294967295.0,
          QStringLiteral("uint32 FF FF FF FF == 4294967295"));

    value = 0;
    ok = dm::decodeRegisters(QByteArray::fromHex("418CCCCD"), dm::DataType::Float32,
                             dm::ByteOrder::ABCD, &value);
    check(ok && std::abs(value - 17.6) < 1e-6,
          QStringLiteral("float32 41 8C CC CD == 17.6"));

    // ---- 四种字序：同一台设备，四种排列都应解出 17.6 ----
    value = 0;
    ok = dm::decodeRegisters(QByteArray::fromHex("CCCD418C"), dm::DataType::Float32,
                             dm::ByteOrder::CDAB, &value);
    check(ok && std::abs(value - 17.6) < 1e-6,
          QStringLiteral("float32 CDAB CC CD 41 8C == 17.6"));

    value = 0;
    ok = dm::decodeRegisters(QByteArray::fromHex("8C41CDCC"), dm::DataType::Float32,
                             dm::ByteOrder::BADC, &value);
    check(ok && std::abs(value - 17.6) < 1e-6,
          QStringLiteral("float32 BADC 8C 41 CD CC == 17.6"));

    value = 0;
    ok = dm::decodeRegisters(QByteArray::fromHex("CDCC8C41"), dm::DataType::Float32,
                             dm::ByteOrder::DCBA, &value);
    check(ok && std::abs(value - 17.6) < 1e-6,
          QStringLiteral("float32 DCBA CD CC 8C 41 == 17.6"));

    // 同样的字节，字序配错时必须解出完全不同的值
    value = 0;
    ok = dm::decodeRegisters(QByteArray::fromHex("418CCCCD"), dm::DataType::Float32,
                             dm::ByteOrder::CDAB, &value);
    check(ok && std::abs(value - 17.6) > 1.0,
          QStringLiteral("wrong byte order gives a different value"));

    // ---- decodePointValue：套上 scale / offset ----
    dm::PointConfig p;
    p.dataType  = dm::DataType::Int16;
    p.byteOrder = dm::ByteOrder::ABCD;
    p.scale     = 0.1;
    p.offset    = 0.0;

    value = 0;
    ok = dm::decodePointValue(QByteArray::fromHex("02D5"), p, &value);
    check(ok && std::abs(value - 72.5) < 1e-9,
          QStringLiteral("point: 725 * 0.1 == 72.5"));

    p.offset = -10.0;
    value = 0;
    ok = dm::decodePointValue(QByteArray::fromHex("02D5"), p, &value);
    check(ok && std::abs(value - 62.5) < 1e-9,
          QStringLiteral("point: 725 * 0.1 - 10 == 62.5"));

    p.dataType = dm::DataType::Float32;
    p.scale    = 1.0;
    p.offset   = 0.0;
    value = 0;
    ok = dm::decodePointValue(QByteArray::fromHex("418CCCCD"), p, &value);
    check(ok && std::abs(value - 17.6) < 1e-6,
          QStringLiteral("point: float32 passthrough == 17.6"));

    // 长度不足：必须失败，且不能污染 out
    value = 999.0;
    ok = dm::decodePointValue(QByteArray::fromHex("02"), p, &value);
    check(!ok && value == 999.0,
          QStringLiteral("short input fails and leaves out untouched"));
}

void selfTestTypes()
{
    qDebug().noquote() << "=== Types self-check ===";

    // enum class 不会隐式转成整数，所以遍历要手动列出全部取值
    const dm::DataType allTypes[] = {
        dm::DataType::UInt16, dm::DataType::Int16, dm::DataType::UInt32,
        dm::DataType::Int32,  dm::DataType::Float32
    };
    const dm::ByteOrder allOrders[] = {
        dm::ByteOrder::ABCD, dm::ByteOrder::CDAB,
        dm::ByteOrder::BADC, dm::ByteOrder::DCBA
    };
    const dm::PollGroup allGroups[] = {
        dm::PollGroup::Fast, dm::PollGroup::Normal, dm::PollGroup::Slow
    };
    const dm::AlarmLevel allLevels[] = {
        dm::AlarmLevel::None, dm::AlarmLevel::Warning,
        dm::AlarmLevel::Major, dm::AlarmLevel::Critical
    };
    const dm::LinkType allLinkTypes[] = {
        dm::LinkType::Tcp, dm::LinkType::Serial
    };

    // ---------- A. registerSpan ----------
    // 16 位类型占 1 个寄存器，32 位类型占 2 个
    check(dm::registerSpan(dm::DataType::UInt16)  == 1, QStringLiteral("registerSpan(uint16) == 1"));
    check(dm::registerSpan(dm::DataType::Int16)   == 1, QStringLiteral("registerSpan(int16) == 1"));
    check(dm::registerSpan(dm::DataType::UInt32)  == 2, QStringLiteral("registerSpan(uint32) == 2"));
    check(dm::registerSpan(dm::DataType::Int32)   == 2, QStringLiteral("registerSpan(int32) == 2"));
    check(dm::registerSpan(dm::DataType::Float32) == 2, QStringLiteral("registerSpan(float32) == 2"));
    // 非法枚举值必须返回 0，调用方据此判断"这个类型无效"
    check(dm::registerSpan(static_cast<dm::DataType>(99)) == 0,
          QStringLiteral("registerSpan(invalid enum) == 0"));

    // ---------- B. 解析成功路径：返回值和出参都要查 ----------
    // 只查返回值是不够的：出参忘赋值时返回值照样是真的。
    {
        dm::DataType t = dm::DataType::UInt16;
        check(dm::parseDataType(QStringLiteral("float32"), &t),
              QStringLiteral("parseDataType(\"float32\") returns true"));
        check(t == dm::DataType::Float32,
              QStringLiteral("parseDataType(\"float32\") sets out to Float32"));

        t = dm::DataType::UInt16;
        check(dm::parseDataType(QStringLiteral("FLOAT32"), &t),
              QStringLiteral("parseDataType(\"FLOAT32\") case-insensitive"));
        check(t == dm::DataType::Float32,
              QStringLiteral("parseDataType(\"FLOAT32\") sets out to Float32"));

        t = dm::DataType::UInt16;
        check(dm::parseDataType(QStringLiteral("  float32  "), &t),
              QStringLiteral("parseDataType tolerates surrounding spaces"));
    }
    {
        dm::ByteOrder bo = dm::ByteOrder::ABCD;
        check(dm::parseByteOrder(QStringLiteral("Cdab"), &bo),
              QStringLiteral("parseByteOrder(\"Cdab\") returns true"));
        check(bo == dm::ByteOrder::CDAB,
              QStringLiteral("parseByteOrder(\"Cdab\") sets out to CDAB"));
    }
    {
        dm::PollGroup g = dm::PollGroup::Normal;
        check(dm::parsePollGroup(QStringLiteral("slow"), &g),
              QStringLiteral("parsePollGroup(\"slow\") returns true"));
        check(g == dm::PollGroup::Slow,
              QStringLiteral("parsePollGroup(\"slow\") sets out to Slow"));
    }
    {
        dm::AlarmLevel l = dm::AlarmLevel::None;
        check(dm::parseAlarmLevel(QStringLiteral("major"), &l),
              QStringLiteral("parseAlarmLevel(\"major\") returns true"));
        check(l == dm::AlarmLevel::Major,
              QStringLiteral("parseAlarmLevel(\"major\") sets out to Major"));
    }
    {
        // 这里曾经写成 return 0，导致 "serial" 被判定为解析失败，
        // links.json 里两条串口链路会全部加载不了。
        dm::LinkType lt = dm::LinkType::Tcp;
        check(dm::parseLinkType(QStringLiteral("serial"), &lt),
              QStringLiteral("parseLinkType(\"serial\") returns true"));
        check(lt == dm::LinkType::Serial,
              QStringLiteral("parseLinkType(\"serial\") sets out to Serial"));
    }

    // ---------- C. 解析失败路径：必须失败，且不得污染出参 ----------
    {
        dm::DataType t = dm::DataType::Int32;
        check(!dm::parseDataType(QStringLiteral("float64"), &t),
              QStringLiteral("parseDataType(\"float64\") returns false"));
        check(t == dm::DataType::Int32,
              QStringLiteral("failed parse must not modify out (dataType)"));
    }
    {
        dm::ByteOrder bo = dm::ByteOrder::ABCD;
        check(!dm::parseByteOrder(QStringLiteral("XXXX"), &bo),
              QStringLiteral("parseByteOrder(\"XXXX\") returns false"));
        check(bo == dm::ByteOrder::ABCD,
              QStringLiteral("failed parse must not modify out (byteOrder)"));
    }
    {
        // CDBA 是不存在的字序。曾经这里写成了 "cdba"，
        // 而且括号位置错误导致条件恒为假，函数永远返回 false。
        dm::ByteOrder bo = dm::ByteOrder::DCBA;
        check(!dm::parseByteOrder(QStringLiteral("CDBA"), &bo),
              QStringLiteral("parseByteOrder(\"CDBA\") returns false (not a real byte order)"));
    }
    {
        dm::LinkType lt = dm::LinkType::Serial;
        check(!dm::parseLinkType(QString(), &lt),
              QStringLiteral("parseLinkType(empty) returns false"));
        check(!dm::parseLinkType(QStringLiteral("tcp"), nullptr),
              QStringLiteral("parseLinkType with null out is safe"));
        check(!dm::parseDataType(QStringLiteral("uint16"), nullptr),
              QStringLiteral("parseDataType with null out is safe"));
    }

    // ---------- D. key / display 的期望值 ----------
    // key 是持久化契约，必须和 config/*.json 里的写法完全一致
    check(dm::dataTypeKey(dm::DataType::Float32) == QStringLiteral("float32"),
          QStringLiteral("dataTypeKey(Float32) == \"float32\""));
    check(dm::byteOrderKey(dm::ByteOrder::CDAB) == QStringLiteral("CDAB"),
          QStringLiteral("byteOrderKey(CDAB) == \"CDAB\" (uppercase, matches points.json)"));
    check(dm::pollGroupKey(dm::PollGroup::Normal) == QStringLiteral("normal"),
          QStringLiteral("pollGroupKey(Normal) == \"normal\""));
    check(dm::alarmLevelKey(dm::AlarmLevel::Critical) == QStringLiteral("critical"),
          QStringLiteral("alarmLevelKey(Critical) == \"critical\""));
    check(dm::linkTypeKey(dm::LinkType::Serial) == QStringLiteral("serial"),
          QStringLiteral("linkTypeKey(Serial) == \"serial\""));

    // display 是表现层，给人和日志看
    check(dm::linkTypeDisplay(dm::LinkType::Serial) == QStringLiteral("串口"),
          QStringLiteral("linkTypeDisplay(Serial)"));
    check(dm::dataTypeDisplay(dm::DataType::Float32) == QStringLiteral("32位浮点"),
          QStringLiteral("dataTypeDisplay(Float32)"));
    check(dm::alarmLevelDisplay(dm::AlarmLevel::Major) == QStringLiteral("重大"),
          QStringLiteral("alarmLevelDisplay(Major)"));

    // ---------- E. 往返一致性：值 -> key -> parse -> 值 ----------
    // 这一组以后会自动覆盖新加的枚举值：只要 key 或 parse 有一边漏写，这里就红。
    for (dm::DataType t : allTypes) {
        dm::DataType back = dm::DataType::UInt16;
        const bool ok = dm::parseDataType(dm::dataTypeKey(t), &back);
        check(ok && back == t,
              QStringLiteral("roundtrip dataType: ") + dm::dataTypeKey(t));
    }
    for (dm::ByteOrder o : allOrders) {
        dm::ByteOrder back = dm::ByteOrder::ABCD;
        const bool ok = dm::parseByteOrder(dm::byteOrderKey(o), &back);
        check(ok && back == o,
              QStringLiteral("roundtrip byteOrder: ") + dm::byteOrderKey(o));
    }
    for (dm::PollGroup g : allGroups) {
        dm::PollGroup back = dm::PollGroup::Normal;
        const bool ok = dm::parsePollGroup(dm::pollGroupKey(g), &back);
        check(ok && back == g,
              QStringLiteral("roundtrip pollGroup: ") + dm::pollGroupKey(g));
    }
    for (dm::AlarmLevel l : allLevels) {
        dm::AlarmLevel back = dm::AlarmLevel::None;
        const bool ok = dm::parseAlarmLevel(dm::alarmLevelKey(l), &back);
        check(ok && back == l,
              QStringLiteral("roundtrip alarmLevel: ") + dm::alarmLevelKey(l));
    }
    for (dm::LinkType lt : allLinkTypes) {
        dm::LinkType back = dm::LinkType::Tcp;
        const bool ok = dm::parseLinkType(dm::linkTypeKey(lt), &back);
        check(ok && back == lt,
              QStringLiteral("roundtrip linkType: ") + dm::linkTypeKey(lt));
    }

    qDebug().noquote()
        << QStringLiteral("=== Types self-check done: %1 passed, %2 failed ===")
               .arg(g_passed)
               .arg(g_failed);
}

// ===============================================================
// ConfigLoader 自检
//
// 这一步只验证一件事：links.json 里的 5 条链路能不能变成内存里的
// 5 个 LinkConfig 对象。这是"配置文件"和"程序"第一次真正接通。
// ===============================================================
void selfTestConfigLoader()
{
    qDebug().noquote() << "=== ConfigLoader self-check ===";

#ifdef DM_SOURCE_CONFIG_DIR
    const QString configDir = QString::fromUtf8(DM_SOURCE_CONFIG_DIR);
#else
    const QString configDir = QStringLiteral("config");
#endif
    qDebug().noquote() << "  config dir:" << configDir;

    QVector<dm::LinkConfig> links;
    QVector<dm::ConfigIssue> issues;

    const bool ok = dm::ConfigLoader::loadLinks(configDir, &links, &issues);

    // 不管成没成，先把收集到的问题全部打印出来。
    // 注意这里是"全部"——加载器不会遇到第一个错误就停下。
    for (const dm::ConfigIssue &issue : issues)
        qDebug().noquote() << "  " << issue.toString();

    check(ok, QStringLiteral("loadLinks returns true"));
    check(issues.isEmpty(), QStringLiteral("no config issues"));
    check(links.size() == 5, QStringLiteral("links.json contains 5 links"));

    // 把读到的内容打出来 —— 这就是"JSON 变成内存对象"的那一刻
    for (const dm::LinkConfig &link : links) {
        QString line = QStringLiteral("    %1").arg(link.id, -14);
        line += (QStringLiteral("type=") + dm::linkTypeKey(link.type)).leftJustified(12);

        // 按类型显示各自的连接参数
        QString where;
        if (link.type == dm::LinkType::Tcp)
            where = QStringLiteral("tcp=%1:%2").arg(link.host).arg(link.port);
        else
            where = QStringLiteral("serial=%1").arg(link.serialPort);
        line += where.leftJustified(26);

        line += (QStringLiteral("timeout=") + QString::number(link.responseTimeoutMs)
                 + QStringLiteral("ms")).leftJustified(16);
        line += QStringLiteral("retries=") + QString::number(link.retries);
        line += QStringLiteral("  desc=%1").arg(link.description);
        qDebug().noquote() << line;
    }

    // ---- devices.json ----
    QVector<dm::DeviceConfig> devices;
    QVector<dm::ConfigIssue> deviceIssues;
    const bool devOk = dm::ConfigLoader::loadDevices(configDir, &devices, &deviceIssues);

    for (const dm::ConfigIssue &issue : deviceIssues)
        qDebug().noquote() << "  " << issue.toString();

    check(devOk, QStringLiteral("loadDevices returns true"));
    check(deviceIssues.isEmpty(), QStringLiteral("no device issues"));
    check(devices.size() == 9, QStringLiteral("devices.json contains 9 devices"));

    for (const dm::DeviceConfig &dev : devices) {
        QString line = QStringLiteral("    %1").arg(dev.id, -18);
        line += QStringLiteral("link=%1").arg(dev.linkId, -12);
        line += QStringLiteral("slave=%1").arg(dev.slaveId, -4);
        line += QStringLiteral("enabled=%1").arg(dev.enabled ? QStringLiteral("true")
                                                             : QStringLiteral("false"));
        qDebug().noquote() << line;
    }

    // ---- points.json ----
    QVector<dm::PointConfig> points;
    QVector<dm::ConfigIssue> pointIssues;
    const bool ptOk = dm::ConfigLoader::loadPoints(configDir, &points, &pointIssues);

    for (const dm::ConfigIssue &issue : pointIssues)
        qDebug().noquote() << "  " << issue.toString();

    check(ptOk, QStringLiteral("loadPoints returns true"));
    check(pointIssues.isEmpty(), QStringLiteral("no point issues"));
    check(points.size() == 29, QStringLiteral("points.json contains 29 points"));

    for (const dm::PointConfig &pt : points) {
        QString line = QStringLiteral("    %1").arg(pt.id, -24);
        line += QStringLiteral("%1").arg(pt.deviceId, -18);
        line += QStringLiteral("fc%1 ").arg(pt.funcCode);
        line += QStringLiteral("addr=%1").arg(pt.address, -7);
        line += QStringLiteral("%1").arg(dm::dataTypeKey(pt.dataType), -9);
        line += QStringLiteral("%1").arg(dm::byteOrderKey(pt.byteOrder), -6);
        line += QStringLiteral("scale=%1").arg(pt.scale, -7);
        line += QStringLiteral("db=%1").arg(pt.deadband, -6);
        line += pt.unit;
        qDebug().noquote() << line;
    }

    // 定点抽查：这几个字段在 points.json 里都写了非默认值。
    // 如果读取时 key 拼错（比如 deadband 写成 deadbandt），这里会立刻报红。
    const dm::PointConfig *spindle = nullptr;
    for (const dm::PointConfig &pt : points) {
        if (pt.id == QStringLiteral("p-line1-spindle-speed"))
            spindle = &pt;
    }
    check(spindle != nullptr, QStringLiteral("found p-line1-spindle-speed"));
    if (spindle) {
        check(spindle->deadband == 5.0, QStringLiteral("spindle deadband == 5"));
        check(spindle->byteOrder == dm::ByteOrder::CDAB,
              QStringLiteral("spindle byteOrder == CDAB"));
        check(spindle->pollGroup == dm::PollGroup::Fast,
              QStringLiteral("spindle pollGroup == fast"));
    }

    // ---- SystemConfig：先把三份数据装进盒子 ----
    qDebug().noquote() << "=== SystemConfig self-check ===";

    dm::SystemConfig cfg;
    cfg.setLinks(links);
    cfg.setDevices(devices);
    cfg.setPoints(points);

    check(cfg.links().size() == 5, QStringLiteral("SystemConfig holds 5 links"));
    check(cfg.devices().size() == 9, QStringLiteral("SystemConfig holds 9 devices"));
    check(cfg.points().size() == 29, QStringLiteral("SystemConfig holds 29 points"));

    QVector<dm::ConfigIssue> cfgIssues;
    const bool cfgOk = cfg.rebuildIndexes(&cfgIssues);
    for (const dm::ConfigIssue &issue : cfgIssues)
        qDebug().noquote() << "  " << issue.toString();

    check(cfgOk, QStringLiteral("rebuildIndexes returns true"));
    check(cfgIssues.isEmpty(), QStringLiteral("no cross-file issues"));

    const dm::LinkConfig *busA = cfg.link(QStringLiteral("rtu-bus-a"));
    check(busA != nullptr, QStringLiteral("link(rtu-bus-a) found"));
    check(busA && busA->type == dm::LinkType::Serial,
          QStringLiteral("rtu-bus-a is serial"));
    check(busA && busA->serialPort == QStringLiteral("COM11"),
          QStringLiteral("rtu-bus-a is COM11"));
    check(cfg.link(QStringLiteral("not-a-real-id")) == nullptr,
          QStringLiteral("missing link returns nullptr"));

    const dm::DeviceConfig *tempDev = cfg.device(QStringLiteral("meter-temp-01"));
    check(tempDev != nullptr, QStringLiteral("device(meter-temp-01) found"));
    check(tempDev && tempDev->linkId == QStringLiteral("rtu-bus-a"),
          QStringLiteral("meter-temp-01 belongs to rtu-bus-a"));
    check(cfg.device(QStringLiteral("not-a-real-id")) == nullptr,
          QStringLiteral("missing device returns nullptr"));

    const dm::PointConfig *tempPt = cfg.point(QStringLiteral("p-temp1-value"));
    check(tempPt != nullptr, QStringLiteral("point(p-temp1-value) found"));
    check(tempPt && tempPt->deviceId == QStringLiteral("meter-temp-01"),
          QStringLiteral("p-temp1-value belongs to meter-temp-01"));
    check(cfg.point(QStringLiteral("not-a-real-id")) == nullptr,
          QStringLiteral("missing point returns nullptr"));

    check(cfg.devicesOfLink(QStringLiteral("rtu-bus-a")).size() == 2,
          QStringLiteral("rtu-bus-a has 2 devices"));
    check(cfg.pointsOfDevice(QStringLiteral("meter-temp-01")).size() == 3,
          QStringLiteral("meter-temp-01 has 3 points"));
    check(cfg.pointsOfLink(QStringLiteral("rtu-bus-a")).size() == 5,
          QStringLiteral("rtu-bus-a has 5 points"));
    check(cfg.pointsOfLink(QStringLiteral("rtu-bus-b")).size() == 4,
          QStringLiteral("rtu-bus-b has 4 pollable points (disabled device excluded)"));
    check(cfg.pointsOfLink(QStringLiteral("not-a-real-id")).isEmpty(),
          QStringLiteral("unknown link yields no points"));
    check(cfg.pointCountOfDevice(QStringLiteral("plc-line1")) == 6,
          QStringLiteral("plc-line1 has 6 points"));
    check(cfg.devicesOfLink(QStringLiteral("not-a-real-id")).isEmpty(),
          QStringLiteral("unknown link returns empty list"));
    check(cfg.summary() == QStringLiteral("链路 5 · 设备 9 · 点位 29"),
          QStringLiteral("summary reports 5/9/29"));

    qDebug().noquote() << QStringLiteral("  %1").arg(cfg.summary());
    qDebug().noquote() << QStringLiteral("  pointsOfLink(\"rtu-bus-a\"):");
    for (const dm::PointConfig *pt : cfg.pointsOfLink(QStringLiteral("rtu-bus-a")))
        qDebug().noquote() << QStringLiteral("    %1  %2  %3")
                                  .arg(pt->id, -24)
                                  .arg(dm::dataTypeKey(pt->dataType), -9)
                                  .arg(pt->unit);

    cfg.clear();
    check(cfg.links().isEmpty() && cfg.devices().isEmpty() && cfg.points().isEmpty(),
          QStringLiteral("clear() empties all data"));

    qDebug().noquote() << QStringLiteral("=== ConfigLoader self-check done ===");
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // --self-test：只跑自检然后退出，不弹窗口。
    // 这样既能在 Qt Creator 的 Application Output 里看，也能在命令行 / CI 里
    // 直接跑，并用退出码判断结果（0 = 全过，1 = 有失败项）。
    const bool selfTestOnly = a.arguments().contains(QStringLiteral("--self-test"));

    // --test-tcp：连本地回声服务器，验证 TcpTransport 的收发
    if (a.arguments().contains(QStringLiteral("--test-tcp")))
        return runTcpIntegrationTest();

    selfTestCrc16();
    selfTestTcpTransport();
    selfTestRegisterCodec();
    selfTestTypes();
    selfTestConfigLoader();

    if (g_failed > 0)
        qCritical().noquote() << QStringLiteral("%1 check(s) FAILED").arg(g_failed);

    if (selfTestOnly)
        return g_failed == 0 ? 0 : 1;

    MainWindow w;
    w.show();
    return QApplication::exec();
}
