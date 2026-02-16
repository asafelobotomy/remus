#pragma once

#include <QString>
#include <QMap>
#include <QRegularExpression>
#include "../metadata/metadata_provider.h"

namespace Remus {

/**
 * @brief Template engine for generating filenames from metadata
 * 
 * Supports No-Intro and Redump naming conventions with variable substitution.
 * 
 * Variables:
 * - {title}: Game title (with proper article placement)
 * - {region}: Full region name (USA, Europe, Japan, World)
 * - {languages}: Language codes if multi-language (En,Fr,De)
 * - {version}: Version/revision only if > initial (Rev 1, v1.1)
 * - {status}: Development status (Beta, Proto, Sample)
 * - {additional}: Edition info (Limited Edition, Greatest Hits)
 * - {tags}: Verification/mod tags [!], [h], [T+Eng]
 * - {disc}: Disc number for multi-disc games
 * - {year}: Release year
 * - {publisher}: Publisher name
 * - {system}: System name
 * - {ext}: File extension (with dot)
 * - {id}: Provider-specific ID
 * 
 * Templates:
 * - No-Intro: "{title} ({region}) ({languages}) ({version}) ({status}) ({additional}) [{tags}]{ext}"
 * - Redump: "{title} ({region}) ({version}) ({additional}) (Disc {disc}){ext}"
 */
class TemplateEngine : public QObject {
    Q_OBJECT

public:
    explicit TemplateEngine(QObject *parent = nullptr);

    /**
     * @brief Apply template to generate filename
     * @param templateStr Template string with variables
     * @param metadata Game metadata
     * @param fileInfo Additional file-specific info (extension, disc number)
     * @return Generated filename
     */
    QString applyTemplate(const QString &templateStr, 
                         const GameMetadata &metadata,
                         const QMap<QString, QString> &fileInfo = QMap<QString, QString>());

    /**
     * @brief Get default No-Intro template
     */
    static QString getNoIntroTemplate();

    /**
     * @brief Get default Redump template
     */
    static QString getRedumpTemplate();

    /**
     * @brief Move article to end (The Legend -> Legend, The)
     * @param title Original title
     * @return Title with article moved to end
     */
    static QString moveArticleToEnd(const QString &title);

    /**
     * @brief Extract disc number from filename
     * @param filename Original filename
     * @return Disc number (0 if not a multi-disc game)
     */
    static int extractDiscNumber(const QString &filename);

    /**
     * @brief Normalize title for No-Intro compliance
     * @param title Original title
     * @return Normalized title (articles moved, special chars converted)
     */
    static QString normalizeTitle(const QString &title);

    /**
     * @brief Validate template string
     * @param templateStr Template to validate
     * @return True if template is valid
     */
    static bool validateTemplate(const QString &templateStr);

signals:
    void templateApplied(const QString &result);
    void errorOccurred(const QString &error);

private:
    /**
     * @brief Replace variables in template
     * @param templateStr Template with {variables}
     * @param variables Map of variable name -> value
     * @return String with variables replaced
     */
    QString replaceVariables(const QString &templateStr, const QMap<QString, QString> &variables);

    /**
     * @brief Remove empty parentheses and brackets from result
     * @param filename Filename with potential empty groups
     * @return Cleaned filename
     */
    QString cleanupEmptyGroups(const QString &filename);

    /**
     * @brief Build variable map from metadata and file info
     * @param metadata Game metadata
     * @param fileInfo Additional file info
     * @return Map of variable names to values
     */
    QMap<QString, QString> buildVariableMap(const GameMetadata &metadata, 
                                            const QMap<QString, QString> &fileInfo);
};

} // namespace Remus
