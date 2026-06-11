#include "ankipackageimporter.h"

#include "databasemanager.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTemporaryDir>
#include <QUuid>
#include <QUrl>
#include <QTextDocument>

#include <cstring>
#include <functional>

#include <zlib.h>

namespace {
struct ZipEntry
{
    QString name;
    quint16 method = 0;
    quint32 compressedSize = 0;
    quint32 uncompressedSize = 0;
    quint32 localHeaderOffset = 0;
};

struct AnkiNote
{
    QString guid;
    QString mid;
    QString tags;
    QHash<QString, QString> fields;
};

quint16 readLe16(const QByteArray &data, qsizetype offset)
{
    return static_cast<quint16>(static_cast<unsigned char>(data.at(offset)))
           | static_cast<quint16>(static_cast<unsigned char>(data.at(offset + 1)) << 8);
}

quint32 readLe32(const QByteArray &data, qsizetype offset)
{
    return static_cast<quint32>(static_cast<unsigned char>(data.at(offset)))
           | (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 1))) << 8)
           | (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 2))) << 16)
           | (static_cast<quint32>(static_cast<unsigned char>(data.at(offset + 3))) << 24);
}

bool isSafeZipPath(const QString &path)
{
    const QString cleaned = QDir::cleanPath(path);
    return !cleaned.isEmpty()
           && !cleaned.startsWith("/")
           && !cleaned.contains("..")
           && cleaned != ".";
}

class SimpleZipReader
{
public:
    bool open(const QString &filePath, QString *errorMessage)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            *errorMessage = "无法打开 .apkg 文件: " + file.errorString();
            return false;
        }

        m_data = file.readAll();
        if (m_data.size() < 22) {
            *errorMessage = ".apkg 文件不是有效的 zip 包。";
            return false;
        }

        return readCentralDirectory(errorMessage);
    }

    bool contains(const QString &entryName) const
    {
        return m_entries.contains(entryName);
    }

    QByteArray readEntry(const QString &entryName, QString *errorMessage) const
    {
        if (!m_entries.contains(entryName)) {
            *errorMessage = "zip 中缺少文件: " + entryName;
            return QByteArray();
        }

        return extractEntry(m_entries.value(entryName), errorMessage);
    }

    bool extractAll(const QString &targetPath, QString *errorMessage) const
    {
        QDir targetDir(targetPath);
        if (!targetDir.exists() && !targetDir.mkpath(".")) {
            *errorMessage = "创建临时解压目录失败。";
            return false;
        }

        for (const ZipEntry &entry : m_entries) {
            if (entry.name.endsWith('/')) {
                continue;
            }

            if (!isSafeZipPath(entry.name)) {
                continue;
            }

            const QByteArray content = extractEntry(entry, errorMessage);
            if (!errorMessage->isEmpty()) {
                return false;
            }

            const QString outputPath = targetDir.filePath(entry.name);
            const QFileInfo outputInfo(outputPath);
            if (!QDir().mkpath(outputInfo.absolutePath())) {
                *errorMessage = "创建解压子目录失败: " + outputInfo.absolutePath();
                return false;
            }

            QFile outputFile(outputPath);
            if (!outputFile.open(QIODevice::WriteOnly)) {
                *errorMessage = "写入解压文件失败: " + outputFile.errorString();
                return false;
            }
            outputFile.write(content);
        }

        return true;
    }

private:
    bool readCentralDirectory(QString *errorMessage)
    {
        const quint32 eocdSignature = 0x06054b50;
        qsizetype eocdOffset = -1;
        const qsizetype searchStart = qMax<qsizetype>(0, m_data.size() - 65557);

        for (qsizetype offset = m_data.size() - 22; offset >= searchStart; --offset) {
            if (readLe32(m_data, offset) == eocdSignature) {
                eocdOffset = offset;
                break;
            }
        }

        if (eocdOffset < 0) {
            *errorMessage = ".apkg 文件缺少 zip 结束记录。";
            return false;
        }

        const quint16 entryCount = readLe16(m_data, eocdOffset + 10);
        const quint32 centralDirectoryOffset = readLe32(m_data, eocdOffset + 16);
        qsizetype offset = centralDirectoryOffset;

        for (int index = 0; index < entryCount; ++index) {
            if (offset + 46 > m_data.size() || readLe32(m_data, offset) != 0x02014b50) {
                *errorMessage = ".apkg 文件中央目录损坏。";
                return false;
            }

            ZipEntry entry;
            entry.method = readLe16(m_data, offset + 10);
            entry.compressedSize = readLe32(m_data, offset + 20);
            entry.uncompressedSize = readLe32(m_data, offset + 24);
            const quint16 fileNameLength = readLe16(m_data, offset + 28);
            const quint16 extraLength = readLe16(m_data, offset + 30);
            const quint16 commentLength = readLe16(m_data, offset + 32);
            entry.localHeaderOffset = readLe32(m_data, offset + 42);

            if (entry.compressedSize == 0xffffffff
                || entry.uncompressedSize == 0xffffffff
                || entry.localHeaderOffset == 0xffffffff) {
                *errorMessage = "暂不支持 Zip64 格式的 .apkg 文件。";
                return false;
            }

            const qsizetype nameOffset = offset + 46;
            if (nameOffset + fileNameLength > m_data.size()) {
                *errorMessage = ".apkg 文件名记录损坏。";
                return false;
            }

            entry.name = QString::fromUtf8(m_data.mid(nameOffset, fileNameLength));
            m_entries.insert(entry.name, entry);

            offset = nameOffset + fileNameLength + extraLength + commentLength;
        }

        return true;
    }

    QByteArray extractEntry(const ZipEntry &entry, QString *errorMessage) const
    {
        const qsizetype headerOffset = entry.localHeaderOffset;
        if (headerOffset + 30 > m_data.size() || readLe32(m_data, headerOffset) != 0x04034b50) {
            *errorMessage = "zip 本地文件头损坏: " + entry.name;
            return QByteArray();
        }

        const quint16 fileNameLength = readLe16(m_data, headerOffset + 26);
        const quint16 extraLength = readLe16(m_data, headerOffset + 28);
        const qsizetype dataOffset = headerOffset + 30 + fileNameLength + extraLength;
        if (dataOffset + entry.compressedSize > m_data.size()) {
            *errorMessage = "zip 文件数据越界: " + entry.name;
            return QByteArray();
        }

        const QByteArray compressed = m_data.mid(dataOffset, entry.compressedSize);
        if (entry.method == 0) {
            return compressed;
        }

        if (entry.method != 8) {
            *errorMessage = "暂不支持的 zip 压缩方式: " + QString::number(entry.method);
            return QByteArray();
        }

        QByteArray output;
        output.resize(static_cast<int>(entry.uncompressedSize));

        z_stream stream;
        memset(&stream, 0, sizeof(stream));
        stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.constData()));
        stream.avail_in = static_cast<uInt>(compressed.size());
        stream.next_out = reinterpret_cast<Bytef *>(output.data());
        stream.avail_out = static_cast<uInt>(output.size());

        const int initResult = inflateInit2(&stream, -MAX_WBITS);
        if (initResult != Z_OK) {
            *errorMessage = "初始化 zlib 解压失败。";
            return QByteArray();
        }

        const int inflateResult = inflate(&stream, Z_FINISH);
        inflateEnd(&stream);

        if (inflateResult != Z_STREAM_END) {
            *errorMessage = "zlib 解压失败: " + entry.name;
            return QByteArray();
        }

        return output;
    }

    QByteArray m_data;
    QHash<QString, ZipEntry> m_entries;
};

QString stripHtml(const QString &html)
{
    QTextDocument document;
    document.setHtml(html);
    return document.toPlainText();
}

QString replaceRegexWithValue(const QString &input,
                              const QRegularExpression &expression,
                              const std::function<QString(const QRegularExpressionMatch &)> &producer)
{
    QString result;
    qsizetype lastEnd = 0;
    const auto matches = expression.globalMatch(input);
    QRegularExpressionMatchIterator iterator = matches;

    while (iterator.hasNext()) {
        const QRegularExpressionMatch match = iterator.next();
        result += input.mid(lastEnd, match.capturedStart() - lastEnd);
        result += producer(match);
        lastEnd = match.capturedEnd();
    }

    result += input.mid(lastEnd);
    return result;
}

QString applyConditionals(QString text, const QHash<QString, QString> &fields)
{
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
        const QString escapedName = QRegularExpression::escape(it.key());
        const bool hasValue = !stripHtml(it.value()).trimmed().isEmpty();

        const QRegularExpression positive("\\{\\{#" + escapedName + "\\}\\}(.*?)\\{\\{/" + escapedName + "\\}\\}",
                                          QRegularExpression::DotMatchesEverythingOption);
        text = replaceRegexWithValue(text, positive, [hasValue](const QRegularExpressionMatch &match) {
            return hasValue ? match.captured(1) : QString();
        });

        const QRegularExpression negative("\\{\\{\\^" + escapedName + "\\}\\}(.*?)\\{\\{/" + escapedName + "\\}\\}",
                                          QRegularExpression::DotMatchesEverythingOption);
        text = replaceRegexWithValue(text, negative, [hasValue](const QRegularExpressionMatch &match) {
            return hasValue ? QString() : match.captured(1);
        });
    }

    return text;
}

QString renderClozeText(const QString &fieldHtml, int clozeNumber, bool reveal)
{
    const QRegularExpression clozeExpression("\\{\\{c(\\d+)::(.*?)(?:::(.*?))?\\}\\}",
                                             QRegularExpression::DotMatchesEverythingOption);

    return replaceRegexWithValue(fieldHtml, clozeExpression, [clozeNumber, reveal](const QRegularExpressionMatch &match) {
        const int number = match.captured(1).toInt();
        const QString answer = match.captured(2);
        const QString hint = match.captured(3);

        if (number != clozeNumber) {
            return answer;
        }

        if (reveal) {
            return "<span style=\"color:#1f7a55;font-weight:700;\">" + answer + "</span>";
        }

        const QString placeholder = hint.trimmed().isEmpty() ? "..." : hint.trimmed();
        return "<span style=\"color:#2f6fed;font-weight:700;\">[" + placeholder + "]</span>";
    });
}

QString renderTemplate(QString templateText,
                       const QHash<QString, QString> &fields,
                       const QString &frontSide,
                       int clozeNumber,
                       bool revealCloze)
{
    templateText = applyConditionals(templateText, fields);
    templateText.replace("{{FrontSide}}", frontSide);

    const QRegularExpression clozeFieldExpression("\\{\\{cloze:([^}]+)\\}\\}");
    templateText = replaceRegexWithValue(templateText, clozeFieldExpression, [&fields, clozeNumber, revealCloze](const QRegularExpressionMatch &match) {
        const QString fieldName = match.captured(1).trimmed();
        return renderClozeText(fields.value(fieldName), clozeNumber, revealCloze);
    });

    const QRegularExpression fieldExpression("\\{\\{\\s*(?:(text|type):)?\\s*([^{}:]+?)\\s*\\}\\}");
    templateText = replaceRegexWithValue(templateText, fieldExpression, [&fields](const QRegularExpressionMatch &match) {
        const QString prefix = match.captured(1).trimmed();
        const QString fieldName = match.captured(2).trimmed();

        if (fieldName == "FrontSide") {
            return QString();
        }

        const QString value = fields.value(fieldName);
        if (prefix == "text") {
            return stripHtml(value).toHtmlEscaped();
        }

        return value;
    });

    const QRegularExpression leftoverExpression("\\{\\{[^}]+\\}\\}");
    templateText.remove(leftoverExpression);
    return templateText.trimmed();
}

QString safeMediaFileName(const QString &fileName, const QString &prefix)
{
    const QString baseName = QFileInfo(fileName).fileName();
    return prefix + "_" + baseName;
}

QString writableDStudyDataPath()
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!dataPath.isEmpty()) {
        return dataPath;
    }

    const QString genericDataPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (!genericDataPath.isEmpty()) {
        return genericDataPath + QDir::separator() + "DStudy";
    }

    return QDir::homePath() + QDir::separator() + ".dstudy";
}

QHash<QString, QString> copyMediaFiles(const QString &extractPath,
                                       const QJsonObject &mediaObject,
                                       const QString &packageId)
{
    QHash<QString, QString> mediaUrls;

    const QString dataPath = writableDStudyDataPath();
    QDir mediaDir(dataPath + QDir::separator() + "anki_media");
    if (!mediaDir.exists()) {
        mediaDir.mkpath(".");
    }

    for (auto it = mediaObject.constBegin(); it != mediaObject.constEnd(); ++it) {
        const QString zipEntryName = it.key();
        const QString originalName = it.value().toString();
        if (originalName.isEmpty()) {
            continue;
        }

        const QString sourcePath = QDir(extractPath).filePath(zipEntryName);
        if (!QFile::exists(sourcePath)) {
            continue;
        }

        const QString targetName = safeMediaFileName(originalName, packageId + "_" + zipEntryName);
        const QString targetPath = mediaDir.filePath(targetName);
        if (QFile::exists(targetPath)) {
            QFile::remove(targetPath);
        }

        if (QFile::copy(sourcePath, targetPath)) {
            mediaUrls.insert(originalName, QUrl::fromLocalFile(targetPath).toString());
        }
    }

    return mediaUrls;
}

QString rewriteMediaReferences(QString html, const QHash<QString, QString> &mediaUrls)
{
    for (auto it = mediaUrls.constBegin(); it != mediaUrls.constEnd(); ++it) {
        const QString escapedName = it.key();
        const QString url = it.value();
        html.replace("src=\"" + escapedName + "\"", "src=\"" + url + "\"");
        html.replace("src='" + escapedName + "'", "src='" + url + "'");
        html.replace("href=\"" + escapedName + "\"", "href=\"" + url + "\"");
        html.replace("href='" + escapedName + "'", "href='" + url + "'");
    }

    const QRegularExpression soundExpression("\\[sound:([^\\]]+)\\]");
    html = replaceRegexWithValue(html, soundExpression, [&mediaUrls](const QRegularExpressionMatch &match) {
        const QString fileName = match.captured(1);
        const QString url = mediaUrls.value(fileName);
        if (url.isEmpty()) {
            return QString("<span style=\"color:#64748b;\">[音频：%1]</span>").arg(fileName.toHtmlEscaped());
        }

        return QString("<div style=\"margin-top:8px;color:#475569;\">音频：<a href=\"%1\">%2</a></div>")
            .arg(url, fileName.toHtmlEscaped());
    });

    return html;
}

QHash<QString, AnkiNote> readNotes(QSqlDatabase &ankiDatabase, const QJsonObject &models, FlashCardImportResult *result)
{
    QHash<QString, AnkiNote> notes;
    QSqlQuery query(ankiDatabase);
    if (!query.exec("SELECT id, guid, mid, tags, flds FROM notes")) {
        result->errorMessage = "读取 Anki notes 表失败: " + query.lastError().text();
        return notes;
    }

    while (query.next()) {
        AnkiNote note;
        const QString noteId = query.value(0).toString();
        note.guid = query.value(1).toString();
        note.mid = query.value(2).toString();
        note.tags = query.value(3).toString().simplified().replace(' ', ", ");

        const QJsonObject model = models.value(note.mid).toObject();
        const QJsonArray fieldArray = model.value("flds").toArray();
        const QStringList values = query.value(4).toString().split(QChar(0x1f), Qt::KeepEmptyParts);

        for (int index = 0; index < fieldArray.size(); ++index) {
            const QString fieldName = fieldArray.at(index).toObject().value("name").toString();
            if (fieldName.isEmpty()) {
                continue;
            }

            note.fields.insert(fieldName, index < values.size() ? values.at(index) : QString());
        }

        notes.insert(noteId, note);
    }

    return notes;
}
}

FlashCardImportResult AnkiPackageImporter::importPackage(const QString &filePath,
                                                         DatabaseManager *databaseManager,
                                                         const QString &group,
                                                         const QString &deck,
                                                         const QString &cardColor)
{
    FlashCardImportResult result;
    if (databaseManager == nullptr) {
        result.errorMessage = "数据库还没有初始化。";
        return result;
    }

    QString errorMessage;
    SimpleZipReader zipReader;
    if (!zipReader.open(filePath, &errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    const QString collectionEntryName = zipReader.contains("collection.anki21")
                                            ? QString("collection.anki21")
                                            : QString("collection.anki2");
    if (!zipReader.contains(collectionEntryName)) {
        result.errorMessage = ".apkg 中没有 collection.anki2 或 collection.anki21。";
        return result;
    }

    QTemporaryDir temporaryDir;
    if (!temporaryDir.isValid()) {
        result.errorMessage = "创建临时目录失败。";
        return result;
    }

    if (!zipReader.extractAll(temporaryDir.path(), &errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    QJsonObject mediaObject;
    if (zipReader.contains("media")) {
        const QByteArray mediaBytes = zipReader.readEntry("media", &errorMessage);
        if (!errorMessage.isEmpty()) {
            result.errorMessage = errorMessage;
            return result;
        }

        const QJsonDocument mediaDocument = QJsonDocument::fromJson(mediaBytes);
        if (mediaDocument.isObject()) {
            mediaObject = mediaDocument.object();
        }
    }

    const QByteArray packageHash = QCryptographicHash::hash(filePath.toUtf8(), QCryptographicHash::Sha1).toHex().left(10);
    const QHash<QString, QString> mediaUrls = copyMediaFiles(temporaryDir.path(),
                                                            mediaObject,
                                                            QString::fromLatin1(packageHash));

    const QString collectionPath = QDir(temporaryDir.path()).filePath(collectionEntryName);
    const QString connectionName = "AnkiImportConnection_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVector<ImportedAnkiCard> importedCards;

    {
        QSqlDatabase ankiDatabase = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        ankiDatabase.setDatabaseName(collectionPath);

        if (!ankiDatabase.open()) {
            result.errorMessage = "打开 Anki SQLite 数据库失败: " + ankiDatabase.lastError().text();
        } else {
            QSqlQuery colQuery(ankiDatabase);
            if (!colQuery.exec("SELECT decks, models FROM col LIMIT 1") || !colQuery.next()) {
                result.errorMessage = "读取 Anki col 表失败: " + colQuery.lastError().text();
            } else {
                const QJsonObject decksObject = QJsonDocument::fromJson(colQuery.value(0).toByteArray()).object();
                const QJsonObject modelsObject = QJsonDocument::fromJson(colQuery.value(1).toByteArray()).object();
                const QHash<QString, AnkiNote> notes = readNotes(ankiDatabase, modelsObject, &result);

                if (result.success()) {
                    QSqlQuery cardQuery(ankiDatabase);
                    if (!cardQuery.exec("SELECT nid, did, ord FROM cards ORDER BY id")) {
                        result.errorMessage = "读取 Anki cards 表失败: " + cardQuery.lastError().text();
                    } else {
                        while (cardQuery.next()) {
                            const QString noteId = cardQuery.value(0).toString();
                            const QString deckId = cardQuery.value(1).toString();
                            const int ord = cardQuery.value(2).toInt();

                            if (!notes.contains(noteId)) {
                                ++result.failed;
                                continue;
                            }

                            const AnkiNote note = notes.value(noteId);
                            const QJsonObject model = modelsObject.value(note.mid).toObject();
                            const QJsonArray templates = model.value("tmpls").toArray();
                            if (ord < 0 || ord >= templates.size()) {
                                ++result.failed;
                                continue;
                            }

                            const QJsonObject cardTemplate = templates.at(ord).toObject();
                            const int clozeNumber = ord + 1;
                            QString front = renderTemplate(cardTemplate.value("qfmt").toString(),
                                                           note.fields,
                                                           QString(),
                                                           clozeNumber,
                                                           false);
                            QString back = renderTemplate(cardTemplate.value("afmt").toString(),
                                                          note.fields,
                                                          front,
                                                          clozeNumber,
                                                          true);

                            front = rewriteMediaReferences(front, mediaUrls);
                            back = rewriteMediaReferences(back, mediaUrls);

                            QString ankiDeckName;
                            const QJsonObject deckObject = decksObject.value(deckId).toObject();
                            if (!deckObject.isEmpty()) {
                                ankiDeckName = deckObject.value("name").toString();
                            }

                            ImportedAnkiCard card;
                            card.front = front;
                            card.back = back;
                            card.tag = note.tags.isEmpty() ? ankiDeckName : note.tags;
                            card.sourceId = note.guid + ":" + QString::number(ord);
                            importedCards.append(card);
                        }
                    }
                }
            }
        }

        ankiDatabase.close();
    }

    QSqlDatabase::removeDatabase(connectionName);

    if (!result.success()) {
        return result;
    }

    const FlashCardImportResult databaseResult =
        databaseManager->importFlashCardsFromAnkiPackage(importedCards, group, deck, cardColor);
    result.imported += databaseResult.imported;
    result.skipped += databaseResult.skipped;
    result.failed += databaseResult.failed;
    result.errorMessage = databaseResult.errorMessage;
    return result;
}
