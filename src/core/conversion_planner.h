#pragma once

#include <QString>
#include <QStringList>

namespace Remus {

class ConversionPlanner
{
public:
    enum class PlanningIntent {
        AutoProcess,
        ExplicitExport,
    };

    enum class FormatRole {
        Canonical,
        NormalizationOnly,
        ExportOnly,
        ArchiveOnly,
        Deferred,
    };

    enum class PlannedAction {
        KeepAsIs,
        ConvertToChd,
        ConvertToRvz,
        ConvertToCso,
        NormalizeToIso,
        ExportPbp,
        ArchiveAsIs,
        NoOp,
        Deferred,
    };

    struct ToolAvailability {
        bool chdmanAvailable = false;
        bool dolphinToolAvailable = false;
        bool maxcsoAvailable = false;
        bool witAvailable = false;
        bool psxPackagerAvailable = false;
    };

    struct Request {
        int systemId = 0;
        QString extension;
        PlanningIntent intent = PlanningIntent::AutoProcess;
        ToolAvailability availableTools;
    };

    struct Plan {
        FormatRole role = FormatRole::Deferred;
        PlannedAction action = PlannedAction::Deferred;
        QString canonicalExtension;
        QString intermediateExtension;
        QStringList requiredTools;
        QString fallbackBehavior;
        QString reason;

        bool isValid() const;
    };

    static Plan plan(const Request &request);

    static QString canonicalExtensionForSystem(int systemId);
    static bool isSystemClassified(int systemId);

    static QString normalizedExtension(const QString &extension);
    static QString toString(FormatRole role);
    static QString toString(PlannedAction action);
};

} // namespace Remus