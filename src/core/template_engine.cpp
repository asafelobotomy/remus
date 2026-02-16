#include "template_engine.h"
#include <QRegularExpression>
#include <QFileInfo>
#include <QDebug>
#include "constants/templates.h"

namespace Remus {

TemplateEngine::TemplateEngine(QObject *parent)
    : QObject(parent)
{
}

QString TemplateEngine::applyTemplate(const QString &templateStr,
                                     const GameMetadata &metadata,
                                     const QMap<QString, QString> &fileInfo)
{
    QMap<QString, QString> variables = buildVariableMap(metadata, fileInfo);
    QString result = replaceVariables(templateStr, variables);
    result = cleanupEmptyGroups(result);
    
    emit templateApplied(result);
    return result;
}

QString TemplateEngine::getNoIntroTemplate()
{
    return Constants::Templates::DEFAULT_NO_INTRO;
}

QString TemplateEngine::getRedumpTemplate()
{
    return Constants::Templates::DEFAULT_REDUMP;
}

QString TemplateEngine::moveArticleToEnd(const QString &title)
{
    // List of articles to move
    static const QStringList articles = {"The", "A", "An"};
    
    for (const QString &article : articles) {
        QString pattern = "^" + article + "\\s+(.+)$";
        QRegularExpression re(pattern, QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = re.match(title);
        
        if (match.hasMatch()) {
            QString remainder = match.captured(1);
            return remainder + ", " + article;
        }
    }
    
    return title;
}

int TemplateEngine::extractDiscNumber(const QString &filename)
{
    // Match patterns like "Disc 1", "Disc 01", "(Disc 1)", etc.
    QRegularExpression re("\\b[Dd]isc\\s+(\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = re.match(filename);
    
    if (match.hasMatch()) {
        return match.captured(1).toInt();
    }
    
    return 0;
}

QString TemplateEngine::normalizeTitle(const QString &title)
{
    QString normalized = title.trimmed();
    
    // Move articles to end
    normalized = moveArticleToEnd(normalized);
    
    // Convert special characters to Low ASCII (basic implementation)
    normalized.replace("™", "");
    normalized.replace("®", "");
    normalized.replace("©", "");
    normalized.replace("'", "'");
    normalized.replace("\"", "\"");
    normalized.replace("\"", "\"");
    
    return normalized;
}

bool TemplateEngine::validateTemplate(const QString &templateStr)
{
    // Check for balanced braces
    int openCount = templateStr.count('{');
    int closeCount = templateStr.count('}');
    
    if (openCount != closeCount) {
        return false;
    }
    
    // Check for valid variable names
    QRegularExpression re("\\{([a-zA-Z_][a-zA-Z0-9_]*)\\}");
    QRegularExpressionMatchIterator it = re.globalMatch(templateStr);
    
    static const QStringList validVars = Constants::Templates::ALL_VARIABLES;
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString varName = match.captured(1);
        
        if (!validVars.contains(varName)) {
            qWarning() << "Invalid template variable:" << varName;
            return false;
        }
    }
    
    return true;
}

QString TemplateEngine::replaceVariables(const QString &templateStr, 
                                        const QMap<QString, QString> &variables)
{
    QString result = templateStr;
    
    for (auto it = variables.constBegin(); it != variables.constEnd(); ++it) {
        QString placeholder = "{" + it.key() + "}";
        result.replace(placeholder, it.value());
    }
    
    return result;
}

QString TemplateEngine::cleanupEmptyGroups(const QString &filename)
{
    QString result = filename;
    
    // Remove empty parentheses: () or ( )
    QRegularExpression emptyParens("\\(\\s*\\)");
    result.replace(emptyParens, "");
    
    // Remove empty brackets: [] or [ ]
    QRegularExpression emptyBrackets("\\[\\s*\\]");
    result.replace(emptyBrackets, "");
    
    // Clean up multiple spaces
    result.replace(QRegularExpression("\\s{2,}"), " ");
    
    // Clean up space before extension
    result.replace(QRegularExpression("\\s+\\."), ".");
    
    // Trim
    result = result.trimmed();
    
    return result;
}

QMap<QString, QString> TemplateEngine::buildVariableMap(const GameMetadata &metadata,
                                                        const QMap<QString, QString> &fileInfo)
{
    QMap<QString, QString> variables;
    
    // Title (normalized with articles moved)
    variables[Constants::Templates::Variables::TITLE] = normalizeTitle(metadata.title);
    
    // Region
    variables[Constants::Templates::Variables::REGION] = metadata.region;
    
    // Languages (from genres for now, proper support needs metadata enhancement)
    variables[Constants::Templates::Variables::LANGUAGES] = "";
    
    // Version (only if specified in metadata)
    variables[Constants::Templates::Variables::VERSION] = "";
    
    // Status (Beta, Proto, Sample, etc.)
    variables[Constants::Templates::Variables::STATUS] = "";
    
    // Additional (Limited Edition, Greatest Hits, etc.)
    variables[Constants::Templates::Variables::ADDITIONAL] = "";
    
    // Tags (verification/mod tags)
    variables[Constants::Templates::Variables::TAGS] = "";
    
    // Disc number (from fileInfo or 0)
    if (fileInfo.contains("disc")) {
        variables[Constants::Templates::Variables::DISC] = fileInfo["disc"];
    } else {
        variables[Constants::Templates::Variables::DISC] = "";
    }
    
    // Year (from releaseDate)
    if (!metadata.releaseDate.isEmpty()) {
        QDate date = QDate::fromString(metadata.releaseDate, Qt::ISODate);
        if (date.isValid()) {
            variables[Constants::Templates::Variables::YEAR] = QString::number(date.year());
        }
    }
    
    // Publisher
    variables[Constants::Templates::Variables::PUBLISHER] = metadata.publisher;
    
    // System
    variables[Constants::Templates::Variables::SYSTEM] = metadata.system;
    
    // Extension (from fileInfo)
    if (fileInfo.contains("ext")) {
        QString ext = fileInfo["ext"];
        if (!ext.startsWith('.')) {
            ext = "." + ext;
        }
        variables[Constants::Templates::Variables::EXT] = ext;
    } else {
        variables[Constants::Templates::Variables::EXT] = "";
    }

    variables[Constants::Templates::Variables::ID] = metadata.id;
    
    return variables;
}

} // namespace Remus
