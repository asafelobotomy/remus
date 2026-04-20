#pragma once

#include <QCommandLineParser>
#include <QSet>
#include <QString>

namespace Remus {

void registerAllOptions(QCommandLineParser &parser, QSet<QString> &actionOptions);

} // namespace Remus
