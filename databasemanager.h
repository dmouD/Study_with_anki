#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include "task.h"

#include <QList>
#include <QSqlDatabase>
#include <QSqlQuery>

/*
 * DatabaseManager 是项目里的数据库访问层。
 *
 * MainWindow、TaskDialog 等界面类不应该关心 SQL 语句怎么写，
 * 它们只需要调用 addTask()、getAllTasks() 这类函数即可。
 *
 * 这样拆分有两个好处：
 * 1. 数据库代码集中在一个地方，之后想修改表结构或 SQL 更容易。
 * 2. 界面代码更干净，适合初学者逐个模块学习。
 */
class DatabaseManager
{
public:
    DatabaseManager();
    ~DatabaseManager();

    // 打开 planner.db，并确保 tasks 表存在。
    bool initialize();

    // 基本增删改查接口。
    bool addTask(const Task &task);
    bool deleteTask(int id);
    bool updateTask(const Task &task);
    Task getTaskById(int id);

    // 查询接口：全部任务和按日期筛选任务。
    QList<Task> getAllTasks();
    QList<Task> getTasksByDate(const QString &date);

    // 将任务在 0% 未完成和 100% 已完成之间切换。
    bool toggleTaskDone(int id);

private:
    // 初始化数据库时调用；如果表已存在，SQL 不会重复创建。
    bool createTasksTable();

    // 兼容旧版本数据库：如果 tasks 表还没有 progress 字段，就自动添加。
    bool ensureProgressColumn();

    // 兼容旧版本数据库：如果 tasks 表还没有 end_date 字段，就自动添加。
    bool ensureDateRangeColumn();

    // 兼容旧版本数据库：如果 tasks 表还没有 task_type 字段，就自动添加。
    bool ensureTaskTypeColumn();

    // 兼容旧版本数据库：如果 tasks 表还没有 course_time 字段，就自动添加。
    // 字段名沿用 course_time，但业务含义是“课程总时长”。
    bool ensureCourseTimeColumn();

    // 兼容旧版本数据库：如果 tasks 表还没有 course_hours 字段，就自动添加。
    bool ensureClassHoursColumn();

    // 把 QSqlQuery 当前行转换成 Task 对象，减少重复取字段代码。
    Task taskFromQuery(const QSqlQuery &query) const;

    // Qt 的数据库连接对象。这里使用一个固定连接名，避免重复 addDatabase。
    QSqlDatabase m_database;
};

#endif // DATABASEMANAGER_H
