#include "verification_engine.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>

namespace Remus {

bool VerificationEngine::exportReport(const QList<VerificationResult> &results,
                                       const QString &outputPath,
                                       const QString &format)
{
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit error("Failed to create report file: " + outputPath);
        return false;
    }

    if (format == "json") {
        QJsonArray jsonResults;
        for (const VerificationResult &r : results) {
            QJsonObject obj;
            obj["fileId"] = r.fileId;
            obj["filePath"] = r.filePath;
            obj["filename"] = r.filename;
            obj["system"] = r.system;
            
            QString statusStr;
            switch (r.status) {
                case VerificationStatus::Verified: statusStr = "verified"; break;
                case VerificationStatus::Mismatch: statusStr = "mismatch"; break;
                case VerificationStatus::NotInDat: statusStr = "not_in_dat"; break;
                case VerificationStatus::HashMissing: statusStr = "hash_missing"; break;
                case VerificationStatus::Corrupt: statusStr = "corrupt"; break;
                default: statusStr = "unknown"; break;
            }
            obj["status"] = statusStr;
            obj["datName"] = r.datName;
            obj["datRomName"] = r.datRomName;
            obj["hashType"] = r.hashType;
            obj["fileHash"] = r.fileHash;
            obj["datHash"] = r.datHash;
            obj["notes"] = r.notes;

            jsonResults.append(obj);
        }

        QJsonDocument doc(jsonResults);
        file.write(doc.toJson(QJsonDocument::Indented));
    } else {
        // CSV format
        QTextStream out(&file);
        out << "File ID,Filename,System,Status,DAT Name,Hash Type,File Hash,DAT Hash,Notes\n";

        for (const VerificationResult &r : results) {
            QString statusStr;
            switch (r.status) {
                case VerificationStatus::Verified: statusStr = "Verified"; break;
                case VerificationStatus::Mismatch: statusStr = "Mismatch"; break;
                case VerificationStatus::NotInDat: statusStr = "Not in DAT"; break;
                case VerificationStatus::HashMissing: statusStr = "Hash Missing"; break;
                case VerificationStatus::Corrupt: statusStr = "Corrupt"; break;
                default: statusStr = "Unknown"; break;
            }

            // Escape CSV fields
            auto escape = [](QString s) {
                if (s.contains(',') || s.contains('"') || s.contains('\n')) {
                    s.replace("\"", "\"\"");
                    return "\"" + s + "\"";
                }
                return s;
            };

            out << r.fileId << ","
                << escape(r.filename) << ","
                << escape(r.system) << ","
                << statusStr << ","
                << escape(r.datName) << ","
                << r.hashType << ","
                << r.fileHash << ","
                << r.datHash << ","
                << escape(r.notes) << "\n";
        }
    }

    file.close();
    return true;
}

} // namespace Remus
