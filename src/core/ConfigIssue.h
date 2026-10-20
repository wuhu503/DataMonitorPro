#ifndef DM_CONFIG_ISSUE_H
#define DM_CONFIG_ISSUE_H

#include <QString>
#include <QVector>

namespace dm {

struct ConfigIssue
{

    enum class Severity
    {
        Error,
        Warning
    };

    Severity severity = Severity::Error;
    QString file;
    int index = -1;
    QString field;
    QString message;

    bool isError() const
    {
        return severity == Severity::Error;
    }

    QString toString() const
    {
        QString where = file;
        if (index >= 0)
            where += QStringLiteral("[%1]").arg(index);
        if (!field.isEmpty())
            where += QStringLiteral(".") + field;

        const QString kind = isError() ? QStringLiteral("错误") : QStringLiteral("警告");
        return QStringLiteral("%1: %2: %3").arg(kind, where, message);
    }
};

inline void addIssue(QVector<ConfigIssue> *issues,
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

} // namespace dm

#endif // DM_CONFIG_ISSUE_H
