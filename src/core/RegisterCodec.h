#ifndef DM_REGISTER_CODEC_H
#define DM_REGISTER_CODEC_H

#include <QByteArray>

#include "core/SystemConfig.h"
#include "core/Types.h"

namespace dm {

//raw--从站返回的数据区字节，长度必须是2或4
//type--uint16/int16/uint32 /int32/float32
//order--ABCD/CDAB/BADC/DCBA
bool decodeRegisters(const QByteArray &raw,
                     DataType type,
                     ByteOrder order,
                     double *out);

//物理量=原始值*scale+offset
bool decodePointValue(const QByteArray &raw,
                      const PointConfig &point,
                      double *out);

} // namespace dm

#endif // DM_REGISTER_CODEC_H