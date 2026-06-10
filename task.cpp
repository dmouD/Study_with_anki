#include "task.h"

#include <QtGlobal>

// 默认任务的 id 为 -1，表示它还不是数据库中的有效记录。
Task::Task()
    : id(-1),
      taskType("每日任务"),
      progress(0),
      classHours(0),
      done(false)
{
}

// 这个构造函数主要给 DatabaseManager 使用，把查询结果直接转成 Task。
Task::Task(int id,
           const QString &title,
           const QString &content,
           const QString &taskType,
           const QString &startDate,
           const QString &endDate,
           const QString &courseTime,
           int classHours,
           const QString &priority,
           int progress,
           bool done,
           const QString &createdAt)
    : id(id),
      title(title),
      content(content),
      taskType(taskType),
      startDate(startDate),
      endDate(endDate),
      courseTime(courseTime),
      classHours(classHours),
      priority(priority),
      progress(progress),
      done(done),
      createdAt(createdAt)
{
}

int Task::completedClassHours() const
{
    /*
     * 这里用向下取整，避免进度还没到下一课时就提前算作完成。
     * 例如 33 课时、50% 进度会显示 16/33 课时；100% 时一定显示 33/33。
     */
    if (classHours <= 0) {
        return 0;
    }

    const int boundedProgress = qBound(0, progress, 100);
    return (classHours * boundedProgress) / 100;
}

QString Task::classHoursProgressText() const
{
    if (classHours <= 0) {
        return "未填写";
    }

    return QString("%1/%2课时").arg(completedClassHours()).arg(classHours);
}
