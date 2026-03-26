#include "verification_controller.h"
#include <QFileInfo>
#include <QDebug>

namespace Remus {

VerificationController::VerificationController(Database *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_engine(new VerificationEngine(db, this))
{
    connect(m_engine, &VerificationEngine::verificationProgress,
            this, &VerificationController::onVerificationProgress);
    connect(m_engine, &VerificationEngine::datImportProgress,
            this, &VerificationController::onImportProgress);
    connect(m_engine, &VerificationEngine::verificationComplete,
            this, &VerificationController::onVerificationComplete);
    connect(m_engine, &VerificationEngine::error,
            this, &VerificationController::onError);
}

bool VerificationController::importDatFile(const QString &filePath, const QString &systemName)
{
    if (m_importing) {
        emit importError("Import already in progress");
        return false;
    }

    m_importing = true;
    emit importingChanged();
    emit importStarted();

    int count = m_engine->importDat(filePath, systemName);
    
    m_importing = false;
    emit importingChanged();

    if (count < 0) {
        emit importError("Failed to import DAT file");
        return false;
    }

    emit importCompleted(count);
    qInfo() << "Imported DAT for" << systemName << "with" << count << "entries";
    return true;
}

void VerificationController::removeDat(const QString &systemName)
{
    if (m_engine->removeDat(systemName)) {
        qInfo() << "Removed DAT for" << systemName;
    }
}

QVariantList VerificationController::getImportedDats()
{
    QVariantList list;
    auto dats = m_engine->getImportedDats();

    for (auto it = dats.begin(); it != dats.end(); ++it) {
        QVariantMap map;
        map["system"] = it.key();
        map["name"] = it.value().name;
        map["version"] = it.value().version;
        map["description"] = it.value().description;
        map["source"] = it.value().category;  // We stored source in category
        list.append(map);
    }

    return list;
}

bool VerificationController::hasDatForSystem(const QString &systemName)
{
    return m_engine->hasDat(systemName);
}

void VerificationController::verifyAll()
{
    if (m_verifying) {
        emit verificationError("Verification already in progress");
        return;
    }

    m_verifying = true;
    m_cancelRequested = false;
    m_results.clear();
    m_summary.clear();
    emit verifyingChanged();
    emit resultsChanged();
    emit summaryChanged();
    emit verificationStarted();

    auto results = m_engine->verifyLibrary();
    
    for (const auto &result : results) {
        m_results.append(resultToVariant(result));
    }

    m_verifying = false;
    emit verifyingChanged();
    emit resultsChanged();
    emit verificationCompleted();
}

void VerificationController::verifySystem(const QString &systemName)
{
    if (m_verifying) {
        emit verificationError("Verification already in progress");
        return;
    }

    m_verifying = true;
    m_cancelRequested = false;
    m_results.clear();
    m_summary.clear();
    emit verifyingChanged();
    emit resultsChanged();
    emit summaryChanged();
    emit verificationStarted();

    auto results = m_engine->verifyLibrary(systemName);
    
    for (const auto &result : results) {
        m_results.append(resultToVariant(result));
    }

    m_verifying = false;
    emit verifyingChanged();
    emit resultsChanged();
    emit verificationCompleted();
}

void VerificationController::verifyFiles(const QVariantList &fileIds)
{
    if (m_verifying) {
        emit verificationError("Verification already in progress");
        return;
    }

    QList<int> ids;
    for (const auto &id : fileIds) {
        ids.append(id.toInt());
    }

    m_verifying = true;
    m_cancelRequested = false;
    m_results.clear();
    emit verifyingChanged();
    emit resultsChanged();
    emit verificationStarted();

    auto results = m_engine->verifyFiles(ids);
    
    for (const auto &result : results) {
        m_results.append(resultToVariant(result));
    }

    m_verifying = false;
    emit verifyingChanged();
    emit resultsChanged();
    emit verificationCompleted();
}

void VerificationController::cancelVerification()
{
    m_cancelRequested = true;
}

QVariantList VerificationController::getMissingGames(const QString &systemName)
{
    QVariantList list;
    auto missing = m_engine->getMissingGames(systemName);

    for (const auto &entry : missing) {
        QVariantMap map;
        map["gameName"] = entry.gameName;
        map["romName"] = entry.romName;
        map["description"] = entry.description;
        map["size"] = entry.size;
        map["crc32"] = entry.crc32;
        map["md5"] = entry.md5;
        map["sha1"] = entry.sha1;
        list.append(map);
    }

    return list;
}

bool VerificationController::exportResults(const QString &outputPath, const QString &format)
{
    // Convert QVariantList back to VerificationResult list
    QList<VerificationResult> results;
    
    for (const auto &var : m_results) {
        QVariantMap map = var.toMap();
        VerificationResult result;
        result.fileId = map["fileId"].toInt();
        result.filePath = map["filePath"].toString();
        result.filename = map["filename"].toString();
        result.system = map["system"].toString();
        result.datName = map["datName"].toString();
        result.datRomName = map["datRomName"].toString();
        result.datDescription = map["datDescription"].toString();
        result.hashType = map["hashType"].toString();
        result.fileHash = map["fileHash"].toString();
        result.datHash = map["datHash"].toString();
        result.notes = map["notes"].toString();
        
        QString status = map["status"].toString();
        if (status == "verified") result.status = VerificationStatus::Verified;
        else if (status == "mismatch") result.status = VerificationStatus::Mismatch;
        else if (status == "not_in_dat") result.status = VerificationStatus::NotInDat;
        else if (status == "hash_missing") result.status = VerificationStatus::HashMissing;
        else if (status == "corrupt") result.status = VerificationStatus::Corrupt;
        else result.status = VerificationStatus::Unknown;
        
        results.append(result);
    }

    return m_engine->exportReport(results, outputPath, format);
}

void VerificationController::clearResults()
{
    m_results.clear();
    m_summary.clear();
    m_progress = 0;
    m_total = 0;
    m_currentFile.clear();
    emit resultsChanged();
    emit summaryChanged();
    emit progressChanged();
    emit totalChanged();
    emit currentFileChanged();
}

void VerificationController::onVerificationProgress(int current, int total, const QString &file)
{
    m_progress = current;
    m_total = total;
    m_currentFile = QFileInfo(file).fileName();
    emit progressChanged();
    emit totalChanged();
    emit currentFileChanged();
}

void VerificationController::onImportProgress(int current, int total)
{
    m_progress = current;
    m_total = total;
    emit progressChanged();
    emit totalChanged();
}

void VerificationController::onVerificationComplete(const VerificationSummary &summary)
{
    m_summary["totalFiles"] = summary.totalFiles;
    m_summary["verified"] = summary.verified;
    m_summary["mismatched"] = summary.mismatched;
    m_summary["notInDat"] = summary.notInDat;
    m_summary["noHash"] = summary.noHash;
    m_summary["corrupt"] = summary.corrupt;
    m_summary["datName"] = summary.datName;
    m_summary["datVersion"] = summary.datVersion;
    m_summary["datSource"] = summary.datSource;
    emit summaryChanged();
}

void VerificationController::onError(const QString &message)
{
    m_verifying = false;
    m_importing = false;
    emit verifyingChanged();
    emit importingChanged();
    emit verificationError(message);
}

QVariantMap VerificationController::resultToVariant(const VerificationResult &result)
{
    QVariantMap map;
    map["fileId"] = result.fileId;
    map["filePath"] = result.filePath;
    map["filename"] = result.filename;
    map["system"] = result.system;
    map["status"] = statusToString(result.status);
    map["datName"] = result.datName;
    map["datRomName"] = result.datRomName;
    map["datDescription"] = result.datDescription;
    map["hashType"] = result.hashType;
    map["fileHash"] = result.fileHash;
    map["datHash"] = result.datHash;
    map["headerStripped"] = result.headerStripped;
    map["notes"] = result.notes;
    return map;
}

QString VerificationController::statusToString(VerificationStatus status)
{
    switch (status) {
        case VerificationStatus::Verified: return "verified";
        case VerificationStatus::Mismatch: return "mismatch";
        case VerificationStatus::NotInDat: return "not_in_dat";
        case VerificationStatus::HashMissing: return "hash_missing";
        case VerificationStatus::Corrupt: return "corrupt";
        case VerificationStatus::HeaderMismatch: return "header_mismatch";
        default: return "unknown";
    }
}

} // namespace Remus
