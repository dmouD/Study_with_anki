#ifndef TASK_H
#define TASK_H

#include <QString>

/*
 * Task 是一个简单的数据类，用来表示 tasks 表中的一行记录。
 *
 * 这里故意没有使用复杂的 getter/setter，也没有引入 Model/View。
 * 对于这个学习项目来说，公开字段更直观：
 * 从数据库查出来 -> 组成 Task -> 交给界面显示；
 * 从弹窗读取输入 -> 组成 Task -> 交给数据库保存。
 */
class Task
{
public:
    // 默认构造用于“空任务”或“未查询到任务”的情况。
    Task();

    // 带参数构造用于从数据库查询结果快速创建 Task。
    Task(int id,
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
         const QString &createdAt);

    // 根据总课时和完成进度推算已经完成的课时数。
    int completedClassHours() const;

    // 生成适合界面显示的课时进度文本，例如“16/33课时”。
    QString classHoursProgressText() const;

    // 数据库主键。新任务尚未插入数据库前，id 通常为 -1。
    int id;

    // 用户填写的任务标题和内容。
    QString title;
    QString content;

    // 任务类型用于区分单日计划和跨天计划，取值为“每日任务”或“长期任务”。
    QString taskType;

    // 开始日期和结束日期使用字符串保存，日期格式为 yyyy-MM-dd。
    // 每日任务的 startDate 和 endDate 相同；长期任务可以横跨多天。
    QString startDate;
    QString endDate;

    // 课程总时长用于记录网课大概需要学习多久，例如“90分钟”或“2小时30分钟”。
    QString courseTime;

    // 课时数用于记录课程一共有多少课时，例如“高等数学：33课时”中的 33。
    int classHours;

    // 优先级取值：高、中、低。
    QString priority;

    // 完成进度，范围是 0 到 100。100 表示任务完成。
    int progress;

    // true 表示已完成，false 表示未完成；数据库中保存为 1 或 0。
    bool done;

    // 创建时间，使用 ISO 字符串保存。
    QString createdAt;
};

#endif // TASK_H
