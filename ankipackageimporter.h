#ifndef ANKIPACKAGEIMPORTER_H
#define ANKIPACKAGEIMPORTER_H

#include <QString>
#include <QVector>

class DatabaseManager;

/*
 * ImportedAnkiCard 是从 .apkg 中解析出的临时卡片数据。
 *
 * 它还不是 DStudy 数据库中的记录。AnkiPackageImporter 负责解析，
 * DatabaseManager 负责用事务批量写入 flashcards 表。
 */
class ImportedAnkiCard
{
public:
    QString front;
    QString back;
    QString tag;
    QString sourceId;
};

/*
 * FlashCardImportResult 用来统一返回导入统计。
 */
class FlashCardImportResult
{
public:
    int imported = 0;
    int skipped = 0;
    int failed = 0;
    QString errorMessage;

    bool success() const { return errorMessage.isEmpty(); }
};

/*
 * AnkiPackageImporter 负责读取 Anki .apkg 包。
 *
 * 这个类不直接写 DStudy 的 SQLite 表，而是把解析出的卡片交给
 * DatabaseManager::importFlashCardsFromAnkiPackage() 批量保存。
 */
class AnkiPackageImporter
{
public:
    FlashCardImportResult importPackage(const QString &filePath,
                                        DatabaseManager *databaseManager,
                                        const QString &group,
                                        const QString &deck,
                                        const QString &cardColor);
};

#endif // ANKIPACKAGEIMPORTER_H
