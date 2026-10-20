#ifndef DM_CONFIG_LOADER_H
#define DM_CONFIG_LOADER_H

#include <QString>
#include <QVector>

#include "core/ConfigIssue.h"
#include "core/SystemConfig.h"

namespace dm {

class ConfigLoader
{
public:
    static bool loadLinks(const QString &configDir,
                          QVector<LinkConfig> *out,
                          QVector<ConfigIssue> *issues);

    static bool loadDevices(const QString &configDir,
                          QVector<DeviceConfig> *out,
                          QVector<ConfigIssue> *issues);
};

} // namespace dm

#endif // DM_CONFIG_LOADER_H
