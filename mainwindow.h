#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "databasemanager.h"

#include <QMainWindow>
#include <QString>

class DeepSeekClient;
class QCalendarWidget;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTableWidgetItem;
class QTableWidget;
class QTextEdit;
class QWidget;
class ReviewWidget;

/*
 * MainWindow 是整个程序的主窗口。
 *
 * 它只负责三件事：
 * 1. 创建主界面控件：左侧日历、右侧任务表格、底部操作按钮。
 * 2. 响应用户操作：添加、修改、删除、切换完成状态、日期筛选。
 * 3. 调用 DatabaseManager 读写数据。
 *
 * 注意：这里不直接写 SQL。这样界面逻辑和数据库逻辑就不会混在一起，
 * 对 Qt 初学者来说也更容易看懂每个类的职责。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    // 以下槽函数对应底部按钮和日历选择事件。
    void addTask();
    void editTask();
    void deleteTask();
    void toggleTaskDone();
    void filterTasksBySelectedDate();
    void showAllTasks();
    void openReviewWidget();
    void showPlannerWidget();
    void analyzePlanWithAi();
    void showAiResult(const QString &result);
    void showAiError(const QString &errorMessage);

private:
    // 界面创建、样式和信号槽连接。
    void setupUi();
    void setupStyleSheet();
    void connectSignals();

    // 数据加载和表格刷新。
    void loadAllTasks();
    void loadTodayTasks();
    void refreshTaskTable(const QList<Task> &tasks);
    void showSelectedTaskDetails();
    void clearTaskDetails();

    // 小工具函数：减少槽函数中的重复代码。
    int selectedTaskId() const;
    QString currentSelectedDateText() const;
    QString taskDateRangeText(const Task &task) const;
    QString taskMetaText(const Task &task) const;
    QString buildAiPrompt();
    QWidget *createSectionHeader(const QString &title, const QString &subtitle);
    QProgressBar *createProgressBar(const Task &task);
    void styleTaskTableItem(QTableWidgetItem *item, const Task &task) const;
    void playStartupAnimation();
    void fadeInWidget(QWidget *widget, int duration);
    void animateProgressBar(QProgressBar *progressBar, int targetValue, int duration);

    DatabaseManager m_databaseManager;
    DeepSeekClient *m_deepSeekClient;

    // 主界面控件指针由 Qt 父子对象机制自动释放。
    QStackedWidget *m_stackedWidget;
    QWidget *m_plannerWidget;
    ReviewWidget *m_reviewWidget;
    QCalendarWidget *m_calendar;
    QTableWidget *m_taskTable;
    QPushButton *m_addButton;
    QPushButton *m_editButton;
    QPushButton *m_deleteButton;
    QPushButton *m_toggleButton;
    QPushButton *m_showAllButton;
    QPushButton *m_reviewButton;
    QLabel *m_detailTitleLabel;
    QLabel *m_detailMetaLabel;
    QProgressBar *m_detailProgressBar;
    QTextEdit *m_detailContentEdit;
    QLineEdit *m_apiKeyEdit;
    QPushButton *m_aiAnalyzeButton;
    QLabel *m_aiStatusLabel;
    QTextEdit *m_aiResultEdit;

    // true 表示当前表格显示全部任务；false 表示按日历日期筛选。
    bool m_showingAllTasks;
};

#endif // MAINWINDOW_H
