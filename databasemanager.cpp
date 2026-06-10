#include "databasemanager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QStringList>
#include <QStringConverter>
#include <QTextStream>
#include <QtGlobal>
#include <QVariant>

namespace {
int normalizedProgress(int progress)
{
    return qBound(0, progress, 100);
}

QString normalizedTaskType(const QString &taskType)
{
    return taskType == "长期任务" ? "长期任务" : "每日任务";
}

QString normalizedEndDate(const QString &taskType, const QString &startDate, const QString &endDate)
{
    if (normalizedTaskType(taskType) == "每日任务") {
        return startDate;
    }

    if (endDate.isEmpty() || (!startDate.isEmpty() && endDate < startDate)) {
        return startDate;
    }

    return endDate;
}

int normalizedClassHours(int classHours)
{
    return qMax(0, classHours);
}

int normalizedReviewRating(int rating)
{
    return qBound(0, rating, 3);
}

int normalizedIntervalDays(int intervalDays)
{
    return qMax(1, intervalDays);
}

double normalizedEaseFactor(double easeFactor)
{
    return qMax(1.3, easeFactor);
}

QString flashCardSourceId(const QString &front, const QString &back)
{
    const QByteArray raw = (front + "\t" + back).toUtf8();
    const QByteArray hash = QCryptographicHash::hash(raw, QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(hash);
}

QString normalizedDeckName(const QString &deck)
{
    const QString trimmedDeck = deck.trimmed();
    return trimmedDeck.isEmpty() || trimmedDeck == "全部牌组" ? QString("默认牌组") : trimmedDeck;
}

QString normalizedCardColor(const QString &cardColor)
{
    const QString trimmedColor = cardColor.trimmed();
    return trimmedColor.isEmpty() ? QString("#ffffff") : trimmedColor;
}
}

DatabaseManager::DatabaseManager()
{
    /*
     * Qt 允许给数据库连接起名字。这里使用固定连接名，
     * 可以避免程序中重复创建多个 SQLite 连接。
     */
    const QString connectionName = "StudyPlannerConnection";

    if (QSqlDatabase::contains(connectionName)) {
        m_database = QSqlDatabase::database(connectionName);
    } else {
        m_database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    }
}

DatabaseManager::~DatabaseManager()
{
    // 程序退出或 DatabaseManager 销毁时关闭数据库连接。
    if (m_database.isOpen()) {
        m_database.close();
    }
}

bool DatabaseManager::initialize()
{
    /*
     * planner.db 放在可执行文件所在目录。
     * 使用 Qt Creator 运行时，它通常位于 build 目录中。
     */
    const QString databasePath = QCoreApplication::applicationDirPath()
                                 + QDir::separator()
                                 + "planner.db";

    m_database.setDatabaseName(databasePath);

    if (!m_database.open()) {
        qDebug() << "打开数据库失败:" << m_database.lastError().text();
        return false;
    }

    return createTasksTable() && createFlashcardsTable() && createFlashCardDecksTable();
}

bool DatabaseManager::createTasksTable()
{
    QSqlQuery query(m_database);

    /*
     * IF NOT EXISTS 保证重复启动软件时不会覆盖旧数据。
     * task_type 用来区分“每日任务”和“长期任务”。
     * date 表示开始日期，end_date 表示结束日期，二者相同就是单日任务。
     * course_time 用来记录课程总时长，例如“90分钟”或“2小时30分钟”。
     * course_hours 用来记录课时数，例如“33课时”。
     * progress 使用 INTEGER 保存 0-100 的完成进度。
     */
    const QString sql = R"(
        CREATE TABLE IF NOT EXISTS tasks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            content TEXT,
            task_type TEXT DEFAULT '每日任务',
            date TEXT,
            end_date TEXT,
            course_time TEXT,
            course_hours INTEGER DEFAULT 0,
            priority TEXT,
            progress INTEGER DEFAULT 0,
            done INTEGER DEFAULT 0,
            created_at TEXT
        );
    )";

    if (!query.exec(sql)) {
        qDebug() << "创建 tasks 表失败:" << query.lastError().text();
        return false;
    }

    return ensureProgressColumn()
           && ensureDateRangeColumn()
           && ensureTaskTypeColumn()
           && ensureCourseTimeColumn()
           && ensureClassHoursColumn();
}

bool DatabaseManager::createFlashcardsTable()
{
    QSqlQuery query(m_database);

    /*
     * flashcards 表保存 Anki 风格卡片。
     * due_time 使用 Qt::ISODate 字符串，格式稳定，便于和当前时间比较。
     */
    const QString sql = R"(
        CREATE TABLE IF NOT EXISTS flashcards (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            front TEXT NOT NULL,
            back TEXT NOT NULL,
            tag TEXT,
            deck TEXT DEFAULT '默认牌组',
            card_color TEXT DEFAULT '#ffffff',
            due_time TEXT NOT NULL,
            interval_days INTEGER DEFAULT 1,
            ease_factor REAL DEFAULT 2.5,
            review_count INTEGER DEFAULT 0,
            created_at TEXT NOT NULL,
            source TEXT DEFAULT 'manual',
            source_id TEXT
        );
    )";

    if (!query.exec(sql)) {
        qDebug() << "创建 flashcards 表失败:" << query.lastError().text();
        return false;
    }

    return ensureFlashCardSourceColumns()
           && ensureFlashCardDeckColumn()
           && ensureFlashCardColorColumn();
}

bool DatabaseManager::createFlashCardDecksTable()
{
    QSqlQuery query(m_database);

    /*
     * flashcard_decks 专门保存牌组名。
     * 这样即使牌组暂时没有卡片，也能在下拉框中显示。
     */
    const QString sql = R"(
        CREATE TABLE IF NOT EXISTS flashcard_decks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            created_at TEXT NOT NULL
        );
    )";

    if (!query.exec(sql)) {
        qDebug() << "创建 flashcard_decks 表失败:" << query.lastError().text();
        return false;
    }

    return addFlashCardDeck("默认牌组");
}

bool DatabaseManager::ensureFlashCardDeckColumn()
{
    /*
     * deck 字段用于把复习卡片分到不同牌组。
     * 旧版本数据库会自动补成“默认牌组”，手动添加和导入都可以指定其它牌组。
     */
    QSqlQuery tableInfoQuery(m_database);
    if (!tableInfoQuery.exec("PRAGMA table_info(flashcards)")) {
        qDebug() << "读取 flashcards 表结构失败:" << tableInfoQuery.lastError().text();
        return false;
    }

    bool hasDeckColumn = false;
    while (tableInfoQuery.next()) {
        const QString columnName = tableInfoQuery.value(1).toString();
        if (columnName == "deck") {
            hasDeckColumn = true;
            break;
        }
    }

    if (!hasDeckColumn) {
        QSqlQuery alterQuery(m_database);
        if (!alterQuery.exec("ALTER TABLE flashcards ADD COLUMN deck TEXT DEFAULT '默认牌组'")) {
            qDebug() << "添加 flashcards.deck 字段失败:" << alterQuery.lastError().text();
            return false;
        }
    }

    QSqlQuery backfillQuery(m_database);
    if (!backfillQuery.exec("UPDATE flashcards SET deck = '默认牌组' WHERE deck IS NULL OR deck = ''")) {
        qDebug() << "初始化 flashcards.deck 字段失败:" << backfillQuery.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::ensureFlashCardColorColumn()
{
    /*
     * card_color 用于控制实体卡片背景色。
     * 旧卡片自动补成白色，避免影响已有复习数据。
     */
    QSqlQuery tableInfoQuery(m_database);
    if (!tableInfoQuery.exec("PRAGMA table_info(flashcards)")) {
        qDebug() << "读取 flashcards 表结构失败:" << tableInfoQuery.lastError().text();
        return false;
    }

    bool hasColorColumn = false;
    while (tableInfoQuery.next()) {
        const QString columnName = tableInfoQuery.value(1).toString();
        if (columnName == "card_color") {
            hasColorColumn = true;
            break;
        }
    }

    if (!hasColorColumn) {
        QSqlQuery alterQuery(m_database);
        if (!alterQuery.exec("ALTER TABLE flashcards ADD COLUMN card_color TEXT DEFAULT '#ffffff'")) {
            qDebug() << "添加 flashcards.card_color 字段失败:" << alterQuery.lastError().text();
            return false;
        }
    }

    QSqlQuery backfillQuery(m_database);
    if (!backfillQuery.exec("UPDATE flashcards SET card_color = '#ffffff' WHERE card_color IS NULL OR card_color = ''")) {
        qDebug() << "初始化 flashcards.card_color 字段失败:" << backfillQuery.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::ensureFlashCardSourceColumns()
{
    /*
     * 旧版本数据库里可能已经有 flashcards 表，但没有 source/source_id。
     * 启动时自动补列，避免用户为了新功能手动删库。
     */
    QSqlQuery tableInfoQuery(m_database);
    if (!tableInfoQuery.exec("PRAGMA table_info(flashcards)")) {
        qDebug() << "读取 flashcards 表结构失败:" << tableInfoQuery.lastError().text();
        return false;
    }

    bool hasSourceColumn = false;
    bool hasSourceIdColumn = false;
    while (tableInfoQuery.next()) {
        const QString columnName = tableInfoQuery.value(1).toString();
        if (columnName == "source") {
            hasSourceColumn = true;
        } else if (columnName == "source_id") {
            hasSourceIdColumn = true;
        }
    }

    if (!hasSourceColumn) {
        QSqlQuery alterQuery(m_database);
        if (!alterQuery.exec("ALTER TABLE flashcards ADD COLUMN source TEXT DEFAULT 'manual'")) {
            qDebug() << "添加 flashcards.source 字段失败:" << alterQuery.lastError().text();
            return false;
        }
    }

    if (!hasSourceIdColumn) {
        QSqlQuery alterQuery(m_database);
        if (!alterQuery.exec("ALTER TABLE flashcards ADD COLUMN source_id TEXT")) {
            qDebug() << "添加 flashcards.source_id 字段失败:" << alterQuery.lastError().text();
            return false;
        }
    }

    QSqlQuery backfillQuery(m_database);
    if (!backfillQuery.exec("UPDATE flashcards SET source = 'manual' WHERE source IS NULL OR source = ''")) {
        qDebug() << "初始化 flashcards.source 字段失败:" << backfillQuery.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::ensureProgressColumn()
{
    /*
     * 旧版本项目创建过的 planner.db 里没有 progress 字段。
     * PRAGMA table_info(tasks) 可以读取表结构；如果没找到 progress，
     * 就用 ALTER TABLE 追加一列，并把已完成任务的进度补成 100。
     */
    QSqlQuery tableInfoQuery(m_database);
    if (!tableInfoQuery.exec("PRAGMA table_info(tasks)")) {
        qDebug() << "读取 tasks 表结构失败:" << tableInfoQuery.lastError().text();
        return false;
    }

    bool hasProgressColumn = false;
    while (tableInfoQuery.next()) {
        const QString columnName = tableInfoQuery.value(1).toString();
        if (columnName == "progress") {
            hasProgressColumn = true;
            break;
        }
    }

    if (hasProgressColumn) {
        return true;
    }

    QSqlQuery alterQuery(m_database);
    if (!alterQuery.exec("ALTER TABLE tasks ADD COLUMN progress INTEGER DEFAULT 0")) {
        qDebug() << "添加 progress 字段失败:" << alterQuery.lastError().text();
        return false;
    }

    QSqlQuery backfillQuery(m_database);
    if (!backfillQuery.exec("UPDATE tasks SET progress = CASE done WHEN 1 THEN 100 ELSE 0 END")) {
        qDebug() << "初始化 progress 字段失败:" << backfillQuery.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::ensureDateRangeColumn()
{
    /*
     * 旧版本数据库只有 date 字段。现在 date 继续表示开始日期，
     * 新增 end_date 表示结束日期；旧任务的 end_date 自动补成 date。
     */
    QSqlQuery tableInfoQuery(m_database);
    if (!tableInfoQuery.exec("PRAGMA table_info(tasks)")) {
        qDebug() << "读取 tasks 表结构失败:" << tableInfoQuery.lastError().text();
        return false;
    }

    bool hasEndDateColumn = false;
    while (tableInfoQuery.next()) {
        const QString columnName = tableInfoQuery.value(1).toString();
        if (columnName == "end_date") {
            hasEndDateColumn = true;
            break;
        }
    }

    if (hasEndDateColumn) {
        return true;
    }

    QSqlQuery alterQuery(m_database);
    if (!alterQuery.exec("ALTER TABLE tasks ADD COLUMN end_date TEXT")) {
        qDebug() << "添加 end_date 字段失败:" << alterQuery.lastError().text();
        return false;
    }

    QSqlQuery backfillQuery(m_database);
    if (!backfillQuery.exec("UPDATE tasks SET end_date = date")) {
        qDebug() << "初始化 end_date 字段失败:" << backfillQuery.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::ensureTaskTypeColumn()
{
    /*
     * 任务类型字段用于把单日任务和长期任务显式区分开。
     * 对旧数据做一次温和迁移：如果 end_date 和 date 不同，就认为它是长期任务。
     */
    QSqlQuery tableInfoQuery(m_database);
    if (!tableInfoQuery.exec("PRAGMA table_info(tasks)")) {
        qDebug() << "读取 tasks 表结构失败:" << tableInfoQuery.lastError().text();
        return false;
    }

    bool hasTaskTypeColumn = false;
    while (tableInfoQuery.next()) {
        const QString columnName = tableInfoQuery.value(1).toString();
        if (columnName == "task_type") {
            hasTaskTypeColumn = true;
            break;
        }
    }

    if (!hasTaskTypeColumn) {
        QSqlQuery alterQuery(m_database);
        if (!alterQuery.exec("ALTER TABLE tasks ADD COLUMN task_type TEXT DEFAULT '每日任务'")) {
            qDebug() << "添加 task_type 字段失败:" << alterQuery.lastError().text();
            return false;
        }
    }

    QSqlQuery backfillQuery(m_database);
    if (!backfillQuery.exec(R"(
        UPDATE tasks
        SET task_type = CASE
            WHEN COALESCE(NULLIF(end_date, ''), date) != date THEN '长期任务'
            ELSE '每日任务'
        END
        WHERE task_type IS NULL OR task_type = ''
    )")) {
        qDebug() << "初始化 task_type 字段失败:" << backfillQuery.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::ensureCourseTimeColumn()
{
    /*
     * 新增 course_time 后，旧数据库需要自动补一列。
     * 课程总时长是可选字段，旧任务保持为空字符串即可。
     */
    QSqlQuery tableInfoQuery(m_database);
    if (!tableInfoQuery.exec("PRAGMA table_info(tasks)")) {
        qDebug() << "读取 tasks 表结构失败:" << tableInfoQuery.lastError().text();
        return false;
    }

    bool hasCourseTimeColumn = false;
    while (tableInfoQuery.next()) {
        const QString columnName = tableInfoQuery.value(1).toString();
        if (columnName == "course_time") {
            hasCourseTimeColumn = true;
            break;
        }
    }

    if (hasCourseTimeColumn) {
        return true;
    }

    QSqlQuery alterQuery(m_database);
    if (!alterQuery.exec("ALTER TABLE tasks ADD COLUMN course_time TEXT")) {
        qDebug() << "添加 course_time 字段失败:" << alterQuery.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::ensureClassHoursColumn()
{
    /*
     * 课时数字段是后续新增的，旧数据库需要自动补一列。
     * 0 表示未填写，界面显示时会转成“未填写”。
     */
    QSqlQuery tableInfoQuery(m_database);
    if (!tableInfoQuery.exec("PRAGMA table_info(tasks)")) {
        qDebug() << "读取 tasks 表结构失败:" << tableInfoQuery.lastError().text();
        return false;
    }

    bool hasClassHoursColumn = false;
    while (tableInfoQuery.next()) {
        const QString columnName = tableInfoQuery.value(1).toString();
        if (columnName == "course_hours") {
            hasClassHoursColumn = true;
            break;
        }
    }

    if (hasClassHoursColumn) {
        return true;
    }

    QSqlQuery alterQuery(m_database);
    if (!alterQuery.exec("ALTER TABLE tasks ADD COLUMN course_hours INTEGER DEFAULT 0")) {
        qDebug() << "添加 course_hours 字段失败:" << alterQuery.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::addTask(const Task &task)
{
    QSqlQuery query(m_database);

    /*
     * 插入数据时使用 prepare + bindValue。
     * 这样既清晰，也能避免直接拼接 SQL 带来的转义问题。
     */
    query.prepare(R"(
        INSERT INTO tasks (title, content, task_type, date, end_date, course_time, course_hours, priority, progress, done, created_at)
        VALUES (:title, :content, :task_type, :date, :end_date, :course_time, :course_hours, :priority, :progress, :done, :created_at)
    )");

    // 新增任务时自动补 created_at；修改任务时不改变创建时间。
    const QString createdAt = task.createdAt.isEmpty()
                                  ? QDateTime::currentDateTime().toString(Qt::ISODate)
                                  : task.createdAt;
    const QString taskType = normalizedTaskType(task.taskType);
    const int progress = normalizedProgress(task.progress);
    const bool done = progress >= 100;
    const QString endDate = normalizedEndDate(taskType, task.startDate, task.endDate);

    query.bindValue(":title", task.title);
    query.bindValue(":content", task.content);
    query.bindValue(":task_type", taskType);
    query.bindValue(":date", task.startDate);
    query.bindValue(":end_date", endDate);
    query.bindValue(":course_time", task.courseTime);
    query.bindValue(":course_hours", normalizedClassHours(task.classHours));
    query.bindValue(":priority", task.priority);
    query.bindValue(":progress", progress);
    query.bindValue(":done", done ? 1 : 0);
    query.bindValue(":created_at", createdAt);

    if (!query.exec()) {
        qDebug() << "添加任务失败:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::deleteTask(int id)
{
    QSqlQuery query(m_database);

    // 删除只按主键 id 进行，避免误删同标题或同日期的任务。
    query.prepare("DELETE FROM tasks WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qDebug() << "删除任务失败:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::updateTask(const Task &task)
{
    QSqlQuery query(m_database);

    /*
     * 修改任务时不更新 created_at。
     * 如果以后要增加 updated_at 字段，可以在这里一起维护。
     */
    query.prepare(R"(
        UPDATE tasks
        SET title = :title,
            content = :content,
            task_type = :task_type,
            date = :date,
            end_date = :end_date,
            course_time = :course_time,
            course_hours = :course_hours,
            priority = :priority,
            progress = :progress,
            done = :done
        WHERE id = :id
    )");

    const int progress = normalizedProgress(task.progress);
    const bool done = progress >= 100;
    const QString taskType = normalizedTaskType(task.taskType);
    const QString endDate = normalizedEndDate(taskType, task.startDate, task.endDate);

    query.bindValue(":title", task.title);
    query.bindValue(":content", task.content);
    query.bindValue(":task_type", taskType);
    query.bindValue(":date", task.startDate);
    query.bindValue(":end_date", endDate);
    query.bindValue(":course_time", task.courseTime);
    query.bindValue(":course_hours", normalizedClassHours(task.classHours));
    query.bindValue(":priority", task.priority);
    query.bindValue(":progress", progress);
    query.bindValue(":done", done ? 1 : 0);
    query.bindValue(":id", task.id);

    if (!query.exec()) {
        qDebug() << "修改任务失败:" << query.lastError().text();
        return false;
    }

    return true;
}

Task DatabaseManager::getTaskById(int id)
{
    QSqlQuery query(m_database);

    // 修改任务前需要读取完整内容，包括表格里没有显示的 content。
    query.prepare(R"(
        SELECT id, title, content, task_type, date, end_date, course_time, course_hours, priority, progress, done, created_at
        FROM tasks
        WHERE id = :id
    )");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qDebug() << "查询任务失败:" << query.lastError().text();
        return Task();
    }

    if (query.next()) {
        return taskFromQuery(query);
    }

    return Task();
}

QList<Task> DatabaseManager::getAllTasks()
{
    QList<Task> tasks;
    QSqlQuery query(m_database);

    // 按任务类型、日期和 id 排序，让每日任务与长期任务更容易区分。
    if (!query.exec(R"(
        SELECT id, title, content, task_type, date, end_date, course_time, course_hours, priority, progress, done, created_at
        FROM tasks
        ORDER BY CASE task_type WHEN '每日任务' THEN 0 ELSE 1 END, date, end_date, id
    )")) {
        qDebug() << "查询全部任务失败:" << query.lastError().text();
        return tasks;
    }

    while (query.next()) {
        tasks.append(taskFromQuery(query));
    }

    return tasks;
}

QList<Task> DatabaseManager::getTasksByDate(const QString &date)
{
    QList<Task> tasks;
    QSqlQuery query(m_database);

    // 日历筛选传入的 date 格式为 yyyy-MM-dd，只要落在开始/结束日期之间就显示。
    query.prepare(R"(
        SELECT id, title, content, task_type, date, end_date, course_time, course_hours, priority, progress, done, created_at
        FROM tasks
        WHERE date <= :date
          AND COALESCE(NULLIF(end_date, ''), date) >= :date
        ORDER BY CASE task_type WHEN '每日任务' THEN 0 ELSE 1 END, date, end_date, id
    )");
    query.bindValue(":date", date);

    if (!query.exec()) {
        qDebug() << "按日期查询任务失败:" << query.lastError().text();
        return tasks;
    }

    while (query.next()) {
        tasks.append(taskFromQuery(query));
    }

    return tasks;
}

bool DatabaseManager::toggleTaskDone(int id)
{
    QSqlQuery query(m_database);

    // CASE 表达式直接在数据库中完成状态切换，同时同步 progress。
    query.prepare(R"(
        UPDATE tasks
        SET done = CASE done WHEN 0 THEN 1 ELSE 0 END,
            progress = CASE done WHEN 0 THEN 100 ELSE 0 END
        WHERE id = :id
    )");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qDebug() << "切换任务完成状态失败:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::addFlashCard(const QString &front,
                                   const QString &back,
                                   const QString &tag,
                                   const QString &deck,
                                   const QString &cardColor)
{
    QSqlQuery query(m_database);

    /*
     * 新卡片默认立即到期，这样用户刚添加后就可以马上在 ReviewWidget 中复习。
    */
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QString deckName = normalizedDeckName(deck);
    const QString color = normalizedCardColor(cardColor);
    addFlashCardDeck(deckName);

    query.prepare(R"(
        INSERT INTO flashcards (front, back, tag, deck, card_color, due_time, interval_days, ease_factor, review_count, created_at, source, source_id)
        VALUES (:front, :back, :tag, :deck, :card_color, :due_time, :interval_days, :ease_factor, :review_count, :created_at, :source, :source_id)
    )");
    query.bindValue(":front", front.trimmed());
    query.bindValue(":back", back.trimmed());
    query.bindValue(":tag", tag.trimmed());
    query.bindValue(":deck", deckName);
    query.bindValue(":card_color", color);
    query.bindValue(":due_time", now);
    query.bindValue(":interval_days", 1);
    query.bindValue(":ease_factor", 2.5);
    query.bindValue(":review_count", 0);
    query.bindValue(":created_at", now);
    query.bindValue(":source", "manual");
    query.bindValue(":source_id", "");

    if (!query.exec()) {
        qDebug() << "添加复习卡片失败:" << query.lastError().text();
        return false;
    }

    return true;
}

int DatabaseManager::importFlashCardsFromTextFile(const QString &filePath, const QString &deck, const QString &cardColor)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "打开 Anki 文本文件失败:" << filePath << file.errorString();
        return -1;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT INTO flashcards (front, back, tag, deck, card_color, due_time, interval_days, ease_factor, review_count, created_at, source, source_id)
        VALUES (:front, :back, :tag, :deck, :card_color, :due_time, :interval_days, :ease_factor, :review_count, :created_at, :source, :source_id)
    )");

    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QString deckName = normalizedDeckName(deck);
    const QString color = normalizedCardColor(cardColor);
    addFlashCardDeck(deckName);
    int importedCount = 0;
    int lineNumber = 0;

    /*
     * 导入格式：
     * front<TAB>back<TAB>tag
     * 空行、列数不足、front/back 为空的行会跳过。
     */
    while (!stream.atEnd()) {
        ++lineNumber;
        const QString line = stream.readLine();
        if (line.trimmed().isEmpty()) {
            continue;
        }

        const QStringList columns = line.split('\t');
        if (columns.size() < 2) {
            qDebug() << "跳过 Anki 文本导入行，字段不足，行号:" << lineNumber;
            continue;
        }

        const QString front = columns.at(0).trimmed();
        const QString back = columns.at(1).trimmed();
        const QString tag = columns.size() >= 3 ? columns.at(2).trimmed() : QString();

        if (front.isEmpty() || back.isEmpty()) {
            qDebug() << "跳过 Anki 文本导入行，正面或背面为空，行号:" << lineNumber;
            continue;
        }

        query.bindValue(":front", front);
        query.bindValue(":back", back);
        query.bindValue(":tag", tag);
        query.bindValue(":deck", deckName);
        query.bindValue(":card_color", color);
        query.bindValue(":due_time", now);
        query.bindValue(":interval_days", 1);
        query.bindValue(":ease_factor", 2.5);
        query.bindValue(":review_count", 0);
        query.bindValue(":created_at", now);
        query.bindValue(":source", "anki_txt");
        query.bindValue(":source_id", flashCardSourceId(front, back));

        if (!query.exec()) {
            qDebug() << "导入 Anki 文本卡片失败，行号:" << lineNumber << query.lastError().text();
            continue;
        }

        ++importedCount;
    }

    return importedCount;
}

QVector<FlashCard> DatabaseManager::getDueFlashCards(const QString &deck)
{
    QVector<FlashCard> cards;
    QSqlQuery query(m_database);
    const QString deckName = deck.trimmed();

    /*
     * 只读取 due_time 小于等于当前时间的卡片。
     * ORDER BY due_time, id 可以让更早到期的卡片排在前面。
     */
    QString sql = R"(
        SELECT id, front, back, tag, deck, card_color, due_time, interval_days, ease_factor, review_count, created_at, source, source_id
        FROM flashcards
        WHERE due_time <= :now
    )";
    if (!deckName.isEmpty()) {
        sql += " AND deck = :deck";
    }
    sql += " ORDER BY due_time, id";

    query.prepare(sql);
    query.bindValue(":now", QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!deckName.isEmpty()) {
        query.bindValue(":deck", normalizedDeckName(deckName));
    }

    if (!query.exec()) {
        qDebug() << "查询到期复习卡片失败:" << query.lastError().text();
        return cards;
    }

    while (query.next()) {
        cards.append(flashCardFromQuery(query));
    }

    return cards;
}

QStringList DatabaseManager::getFlashCardDecks()
{
    QStringList decks;
    decks.append("默认牌组");

    QSqlQuery query(m_database);
    if (!query.exec(R"(
        SELECT name AS deck_name
        FROM flashcard_decks
        UNION
        SELECT COALESCE(NULLIF(deck, ''), '默认牌组') AS deck_name
        FROM flashcards
        ORDER BY deck_name
    )")) {
        qDebug() << "查询复习牌组失败:" << query.lastError().text();
        return decks;
    }

    while (query.next()) {
        const QString deckName = normalizedDeckName(query.value(0).toString());
        if (!decks.contains(deckName)) {
            decks.append(deckName);
        }
    }

    return decks;
}

bool DatabaseManager::addFlashCardDeck(const QString &deck)
{
    QSqlQuery query(m_database);
    const QString deckName = normalizedDeckName(deck);
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    query.prepare(R"(
        INSERT OR IGNORE INTO flashcard_decks (name, created_at)
        VALUES (:name, :created_at)
    )");
    query.bindValue(":name", deckName);
    query.bindValue(":created_at", now);

    if (!query.exec()) {
        qDebug() << "添加复习牌组失败:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::updateFlashCardAfterReview(int cardId, int rating)
{
    QSqlQuery selectQuery(m_database);
    selectQuery.prepare(R"(
        SELECT interval_days, ease_factor, review_count
        FROM flashcards
        WHERE id = :id
    )");
    selectQuery.bindValue(":id", cardId);

    if (!selectQuery.exec()) {
        qDebug() << "查询复习卡片失败:" << selectQuery.lastError().text();
        return false;
    }

    if (!selectQuery.next()) {
        qDebug() << "复习卡片不存在，id:" << cardId;
        return false;
    }

    const int currentIntervalDays = normalizedIntervalDays(selectQuery.value(0).toInt());
    const double currentEaseFactor = normalizedEaseFactor(selectQuery.value(1).toDouble());
    const int currentReviewCount = qMax(0, selectQuery.value(2).toInt());

    int nextIntervalDays = currentIntervalDays;
    double nextEaseFactor = currentEaseFactor;

    /*
     * 简化版间隔重复算法：
     * - 忘了：回到 1 天，并降低熟练度。
     * - 模糊：保持至少 1 天，并轻微降低熟练度。
     * - 记住了：按熟练度拉长间隔。
     * - 很熟：更积极地拉长间隔，并提高熟练度。
     */
    switch (normalizedReviewRating(rating)) {
    case 0:
        nextIntervalDays = 1;
        nextEaseFactor = normalizedEaseFactor(currentEaseFactor - 0.2);
        break;
    case 1:
        nextIntervalDays = qMax(1, currentIntervalDays);
        nextEaseFactor = normalizedEaseFactor(currentEaseFactor - 0.1);
        break;
    case 2:
        nextIntervalDays = qMax(2, qRound(currentIntervalDays * currentEaseFactor));
        break;
    case 3:
        nextIntervalDays = qMax(3, qRound(currentIntervalDays * (currentEaseFactor + 0.3)));
        nextEaseFactor = normalizedEaseFactor(currentEaseFactor + 0.1);
        break;
    default:
        break;
    }

    const QString nextDueTime = QDateTime::currentDateTime()
                                    .addDays(nextIntervalDays)
                                    .toString(Qt::ISODate);

    QSqlQuery updateQuery(m_database);
    updateQuery.prepare(R"(
        UPDATE flashcards
        SET due_time = :due_time,
            interval_days = :interval_days,
            ease_factor = :ease_factor,
            review_count = :review_count
        WHERE id = :id
    )");
    updateQuery.bindValue(":due_time", nextDueTime);
    updateQuery.bindValue(":interval_days", nextIntervalDays);
    updateQuery.bindValue(":ease_factor", nextEaseFactor);
    updateQuery.bindValue(":review_count", currentReviewCount + 1);
    updateQuery.bindValue(":id", cardId);

    if (!updateQuery.exec()) {
        qDebug() << "更新复习卡片失败:" << updateQuery.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::deleteFlashCard(int cardId)
{
    QSqlQuery query(m_database);

    query.prepare("DELETE FROM flashcards WHERE id = :id");
    query.bindValue(":id", cardId);

    if (!query.exec()) {
        qDebug() << "删除复习卡片失败:" << query.lastError().text();
        return false;
    }

    return true;
}

Task DatabaseManager::taskFromQuery(const QSqlQuery &query) const
{
    /*
     * 这里按 SELECT 字段顺序取值：
     * 0 id, 1 title, 2 content, 3 task_type, 4 date, 5 end_date,
     * 6 course_time, 7 course_hours, 8 priority, 9 progress, 10 done, 11 created_at。
     */
    return Task(query.value(0).toInt(),
                query.value(1).toString(),
                query.value(2).toString(),
                normalizedTaskType(query.value(3).toString()),
                query.value(4).toString(),
                query.value(5).toString(),
                query.value(6).toString(),
                normalizedClassHours(query.value(7).toInt()),
                query.value(8).toString(),
                normalizedProgress(query.value(9).toInt()),
                query.value(10).toInt() == 1,
                query.value(11).toString());
}

FlashCard DatabaseManager::flashCardFromQuery(const QSqlQuery &query) const
{
    /*
     * 这里按 SELECT 字段顺序取值：
     * 0 id, 1 front, 2 back, 3 tag, 4 deck, 5 card_color,
     * 6 due_time, 7 interval_days, 8 ease_factor, 9 review_count,
     * 10 created_at, 11 source, 12 source_id。
     */
    FlashCard card;
    card.id = query.value(0).toInt();
    card.front = query.value(1).toString();
    card.back = query.value(2).toString();
    card.tag = query.value(3).toString();
    card.deck = normalizedDeckName(query.value(4).toString());
    card.cardColor = normalizedCardColor(query.value(5).toString());
    card.dueTime = query.value(6).toString();
    card.intervalDays = normalizedIntervalDays(query.value(7).toInt());
    card.easeFactor = normalizedEaseFactor(query.value(8).toDouble());
    card.reviewCount = qMax(0, query.value(9).toInt());
    card.createdAt = query.value(10).toString();
    card.source = query.value(11).toString().isEmpty() ? "manual" : query.value(11).toString();
    card.sourceId = query.value(12).toString();
    return card;
}
