#include "databasemanager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QSqlError>
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

    return createTasksTable();
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
