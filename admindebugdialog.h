#ifndef ADMINDEBUGDIALOG_H
#define ADMINDEBUGDIALOG_H

#include <QDialog>

class DatabaseManager;
class QPushButton;

/*
 * AdminDebugDialog 是管理员调试界面。
 *
 * 这里集中放高风险调试功能，避免它们混在普通任务/复习操作里。
 * 当前只提供“清除数据库”功能。
 */
class AdminDebugDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AdminDebugDialog(DatabaseManager *databaseManager, QWidget *parent = nullptr);

signals:
    // 清库成功后通知 MainWindow 刷新任务表和复习页面。
    void databaseCleared();

private:
    void setupUi();
    void setupStyleSheet();
    void clearDatabase();

    DatabaseManager *m_databaseManager;
    QPushButton *m_clearDatabaseButton;
    QPushButton *m_closeButton;
};

#endif // ADMINDEBUGDIALOG_H
