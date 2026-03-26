#pragma once

#include <QObject>
#include "../../core/database.h"

namespace Remus {

class ConversionService;

class ConversionController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool converting READ isConverting NOTIFY convertingChanged)
    
public:
    explicit ConversionController(Database *db = nullptr, QObject *parent = nullptr);
    ~ConversionController() override;
    
    bool isConverting() const { return m_converting; }
    
    Q_INVOKABLE void convertToCHD(const QString &path, const QString &codec);
    Q_INVOKABLE void extractCHD(const QString &path);
    Q_INVOKABLE void extractArchive(const QString &path);
    
signals:
    void convertingChanged();
    void conversionProgress(int percent);
    void conversionCompleted(const QString &outputPath);
    void conversionError(const QString &error);
    
private:
    Database *m_db;
    ConversionService *m_conversionService = nullptr;
    bool m_converting = false;
};

} // namespace Remus
