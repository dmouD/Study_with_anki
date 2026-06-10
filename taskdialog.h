#ifndef TASKDIALOG_H
#define TASKDIALOG_H

#include "task.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLineEdit;
class QSlider;
class QSpinBox;
class QTextEdit;

/*
 * TaskDialog 是添加/修改任务时弹出的对话框。
 *
 * 同一个对话框同时服务“添加”和“修改”两种场景：
 * - 添加任务时：保持默认值，用户输入后通过 getTask() 取出。
 * - 修改任务时：先调用 setTask() 把旧数据填进去，再让用户编辑。
 *
 * 这个类只负责表单输入，不直接访问数据库。
 */
class TaskDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TaskDialog(QWidget *parent = nullptr);

    // 将当前控件里的内容组装成 Task，供 MainWindow 保存到数据库。
    Task getTask() const;

    // 修改已有任务时使用：把数据库中的任务内容回填到表单控件。
    void setTask(const Task &task);

protected:
    // 重写 accept()，在用户点击“确定”时做最基本的输入检查。
    void accept() override;

private:
    void setupStyleSheet();

    // 表单控件。指针由 Qt 父子对象机制自动释放。
    QLineEdit *m_titleEdit;
    QTextEdit *m_contentEdit;
    QComboBox *m_taskTypeCombo;
    QDateEdit *m_startDateEdit;
    QDateEdit *m_endDateEdit;
    QLineEdit *m_courseTimeEdit;
    QSpinBox *m_classHoursSpinBox;
    QComboBox *m_priorityCombo;
    QSlider *m_progressSlider;
    QSpinBox *m_progressSpinBox;
    QCheckBox *m_doneCheckBox;

    // 修改任务时保存原任务的 id 和 createdAt；添加任务时 id 为 -1。
    int m_taskId;
    QString m_createdAt;
};

#endif // TASKDIALOG_H
