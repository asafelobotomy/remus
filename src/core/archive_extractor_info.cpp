#include "archive_extractor.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace Remus {

ArchiveInfo ArchiveExtractor::getArchiveInfo(const QString &path)
{
    ArchiveInfo info;
    info.path = path;
    info.format = detectFormat(path);
    info.compressedSize = QFileInfo(path).size();

    ProcessResult processResult;
    bool parsedWithSevenZip = false;

    switch (info.format) {
    case ArchiveFormat::ZIP:
        if (isToolAvailable(m_unzipPath)) {
            processResult = runProcess(m_unzipPath, QStringList() << "-l" << path, 30000);
        } else if (isToolAvailable(m_sevenZipPath)) {
            parsedWithSevenZip = true;
            processResult = runProcess(m_sevenZipPath, QStringList() << "l" << path, 30000);
        }
        break;

    case ArchiveFormat::SevenZip:
        if (isToolAvailable(m_sevenZipPath)) {
            processResult = runProcess(m_sevenZipPath, QStringList() << "l" << path, 30000);
        }
        break;

    case ArchiveFormat::RAR:
        if (isToolAvailable(m_unrarPath)) {
            processResult = runProcess(m_unrarPath, QStringList() << "l" << path, 30000);
        } else if (isToolAvailable(m_sevenZipPath)) {
            parsedWithSevenZip = true;
            processResult = runProcess(m_sevenZipPath, QStringList() << "l" << path, 30000);
        }
        break;

    default:
        break;
    }

    const QStringList lines = processResult.stdOutput.split('\n');

    if (info.format == ArchiveFormat::ZIP && !parsedWithSevenZip) {
        for (const QString &line : lines) {
            if (line.contains("Archive:") || line.contains("Name") ||
                line.contains("---------") || line.contains("files") ||
                line.trimmed().isEmpty()) {
                continue;
            }

            if (line[0].isDigit() || line[0] == ' ') {
                // Support both YYYY-MM-DD and MM-DD-YYYY date formats from different unzip versions
                const QRegularExpression re("\\s+(\\d+)\\s+(\\d{2,4}-\\d{2}-\\d{2,4}\\s+\\d{2}:\\d{2})\\s+(.+)");
                const QRegularExpressionMatch match = re.match(line);
                if (match.hasMatch()) {
                    const qint64 size = match.captured(1).toLongLong();
                    const QString filename = match.captured(3).trimmed();
                    if (!filename.isEmpty() && filename != "1 file") {
                        const QString normalized = normalizeArchiveMemberPath(filename);
                        if (normalized.isEmpty()) {
                            info.unsafeEntries.append(filename);
                            continue;
                        }
                        info.contents.append(normalized);
                        info.entrySizes.insert(normalized, size);
                        info.uncompressedSize += size;
                        info.fileCount++;
                    }
                }
            }
        }
    } else if (parsedWithSevenZip || info.format == ArchiveFormat::SevenZip ||
               info.format == ArchiveFormat::GZip ||
               info.format == ArchiveFormat::TarGz ||
               info.format == ArchiveFormat::TarBz2) {
        // Pre-process: join continuation lines for long filenames.
        // 7z wraps filenames that exceed terminal width across multiple lines.
        // We use the separator lines (----) to delimit the file table, then
        // detect entry starts by date or attribute pattern. Anything else
        // between separators is a filename continuation from the previous entry.
        static const QRegularExpression separatorRe(R"(^-{5,})");
        static const QRegularExpression entryStartRe(
            R"(^\s*(?:\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}|[A-Z.]{5}\s+\d))");
        static const QRegularExpression summaryRe(R"(\d+\s+files?\s*$|\d+\s+folders?\s*$)");

        // Phase 1: Extract and join lines from the file table
        QStringList joinedLines;
        bool inTable = false;
        for (const QString &line : lines) {
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty()) continue;

            if (separatorRe.match(trimmed).hasMatch()) {
                if (inTable) break;  // Second separator → end of table
                inTable = true;
                continue;
            }

            if (!inTable) continue;

            // Skip summary lines that sneak inside the table
            if (summaryRe.match(trimmed).hasMatch()) continue;

            // Entry lines start with a date or attribute+size pattern
            if (entryStartRe.match(line).hasMatch()) {
                joinedLines.append(line);
            } else if (!joinedLines.isEmpty()) {
                // Continuation of previous line's wrapped filename
                joinedLines.last() += line;
            }
        }

        // Parse the joined entry lines
        for (const QString &line : joinedLines) {
            const QString trimmed = line.trimmed();
            const QRegularExpression re(R"(^(?:\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}(?::\d{2})?\s+)?([A-Z.]{5})\s+(\d+)\s+(?:\d+\s+)?(.+)$)");
            const QRegularExpressionMatch match = re.match(trimmed);

            if (match.hasMatch()) {
                const qint64 size = match.captured(2).toLongLong();
                const QString filename = match.captured(3).trimmed();
                const QString normalized = normalizeArchiveMemberPath(filename);
                if (!filename.isEmpty() && !normalized.isEmpty()) {
                    info.contents.append(normalized);
                    info.entrySizes.insert(normalized, size);
                    info.uncompressedSize += size;
                    info.fileCount++;
                } else if (!filename.isEmpty()) {
                    info.unsafeEntries.append(filename);
                }
                continue;
            }

            // Fallback: find the attribute column and reconstruct the full filename
            // from all remaining parts after the numeric metadata columns
            const QStringList parts = trimmed.split(QRegularExpression(R"(\s+)"));
            if (parts.size() >= 3) {
                int nameStartIdx = 0;
                for (int i = 0; i < parts.size(); ++i) {
                    if (QRegularExpression(R"(^[A-Z.]{5}$)").match(parts[i]).hasMatch()
                        || QRegularExpression(R"(^\d)").match(parts[i]).hasMatch()
                        || QRegularExpression(R"(^\d{4}-\d{2}-\d{2}$)").match(parts[i]).hasMatch()
                        || QRegularExpression(R"(^\d{2}:\d{2})").match(parts[i]).hasMatch()) {
                        nameStartIdx = i + 1;
                    }
                }
                if (nameStartIdx > 0 && nameStartIdx < parts.size()) {
                    const QString filename = QStringList(parts.mid(nameStartIdx)).join(' ');
                    const QString normalized = normalizeArchiveMemberPath(filename);
                    if (!filename.isEmpty() && !QRegularExpression(R"(^\d+$)").match(filename).hasMatch() &&
                        !normalized.isEmpty()) {
                        info.contents.append(normalized);
                        info.fileCount++;
                    } else if (!filename.isEmpty() && normalized.isEmpty()) {
                        info.unsafeEntries.append(filename);
                    }
                }
            }
        }
    } else if (info.format == ArchiveFormat::RAR) {
        for (const QString &line : lines) {
            if (line.contains("RAR") || line.contains("Name") ||
                line.contains("-----") || line.trimmed().isEmpty()) {
                continue;
            }

            const QString trimmed = line.trimmed();
            if (!trimmed.isEmpty()) {
                const QRegularExpression re("^(.+?)\\s+(\\d+)\\s+\\d+\\s+\\d+%.*$");
                const QRegularExpressionMatch match = re.match(trimmed);
                if (match.hasMatch()) {
                    const QString filename = match.captured(1).trimmed();
                    const qint64 size = match.captured(2).toLongLong();
                    const QString normalized = normalizeArchiveMemberPath(filename);
                    if (!filename.isEmpty() && !normalized.isEmpty()) {
                        info.contents.append(normalized);
                        info.entrySizes.insert(normalized, size);
                        info.uncompressedSize += size;
                        info.fileCount++;
                    } else if (!filename.isEmpty()) {
                        info.unsafeEntries.append(filename);
                    }
                }
            }
        }
    }

    return info;
}

} // namespace Remus