#pragma once

#include <memory>
#include <QObject>
#include <QVariantMap>

#include "../../services/conversion_service.h"
#include "../../core/wbfs_converter.h"
#include "../../core/pbp_exporter.h"

class QTemporaryDir;

namespace Remus {

class AppController;
struct FileRecord;

class ConversionController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool converting READ isConverting NOTIFY convertingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString progressMessage READ progressMessage NOTIFY progressMessageChanged)
    Q_PROPERTY(QString targetFormat READ targetFormat WRITE setTargetFormat NOTIFY targetFormatChanged)
    Q_PROPERTY(double compressionRatio READ compressionRatio NOTIFY conversionFinished)
    Q_PROPERTY(QString lastOutputPath READ lastOutputPath NOTIFY conversionFinished)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)
    Q_PROPERTY(QVariantMap toolStatus READ toolStatus NOTIFY toolStatusChanged)

public:
    explicit ConversionController(AppController *appController, QObject *parent = nullptr);

    bool isConverting() const { return m_converting; }
    int progress() const { return m_progress; }
    QString progressMessage() const { return m_progressMessage; }
    QString targetFormat() const { return m_targetFormat; }
    double compressionRatio() const { return m_compressionRatio; }
    QString lastOutputPath() const { return m_lastOutputPath; }
    QString lastMessage() const { return m_lastMessage; }
    QVariantMap toolStatus() const { return m_toolStatus; }

    Q_INVOKABLE void convertSelected(const QString &format, const QString &outputPath = QString(), const QString &scanDir = QString());
    Q_INVOKABLE void convertAll(const QString &format, const QString &outputPath = QString(), const QString &scanDir = QString());
    Q_INVOKABLE void refreshToolStatus();

public slots:
    void setTargetFormat(const QString &format);

signals:
    void convertingChanged();
    void progressChanged();
    void progressMessageChanged();
    void targetFormatChanged();
    void conversionFinished();
    void lastMessageChanged();
    void toolStatusChanged();
    void libraryChanged();

private:
    void applyToolPaths();
    void setLastMessage(const QString &message);
    static QString resolveAutoFormat(const QString &extension);
    static QString extractIfArchive(const QString &filePath, std::unique_ptr<QTemporaryDir> &tmpDirOut);

    AppController *m_appController;
    ConversionService m_conversionService;
    WBFSConverter m_wbfsConverter;
    PBPExporter m_pbpExporter;
    bool m_converting = false;
    int m_progress = 0;
    QString m_progressMessage;
    QString m_targetFormat = QStringLiteral("CHD");
    double m_compressionRatio = 0.0;
    QString m_lastOutputPath;
    QString m_lastMessage;
    QVariantMap m_toolStatus;
};

} // namespace Remus