#pragma once

#include "disc_converter.h"
#include <QString>
#include <QStringList>

namespace Remus {

class CSOConverter : public DiscConverter {
    Q_OBJECT

public:
    explicit CSOConverter(QObject *parent = nullptr);
    ~CSOConverter() = default;

    bool isMaxcsoAvailable() const;
    QString getMaxcsoVersion() const;
    void setMaxcsoPath(const QString &path);

    ConversionResult convertIsoToCSO(const QString &isoPath,
                                      const QString &outputPath = QString());

    ConversionResult extractCSOToIso(const QString &csoPath,
                                      const QString &outputPath = QString());

    QList<ConversionResult> batchConvert(const QStringList &inputPaths,
                                          const QString &outputDir = QString());

private:
    QString m_maxcsoPath;
};

} // namespace Remus
