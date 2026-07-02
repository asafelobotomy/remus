#pragma once

#include <QCommandLineParser>
#include <QSet>
#include <QString>
#include <QStringList>

namespace Remus {

/// Exit code for usage errors and conflicting primary actions.
constexpr int kCliUsageError = 2;

/// Collect canonical primary action names currently set on the parser.
QStringList collectPrimaryActions(const QCommandLineParser &parser, const QSet<QString> &actionOptions);

/// Return 0 when the combination is allowed; kCliUsageError when conflicting.
int validatePrimaryActionCombination(const QStringList &actions, QString *errorOut = nullptr);

} // namespace Remus
