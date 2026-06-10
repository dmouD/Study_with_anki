#ifndef REVIEWWIDGET_H
#define REVIEWWIDGET_H

#include <QList>
#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;
class QTextEdit;

/*
 * ReviewWidget 是一个轻量版 Anki 复习子界面。
 *
 * 第一阶段只做纯 UI：
 * - 不连接数据库。
 * - 不修改任务管理逻辑。
 * - 使用几张假数据演示“正面 -> 显示答案 -> 反馈 -> 下一张”的流程。
 */
class ReviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ReviewWidget(QWidget *parent = nullptr);

signals:
    // 用户点击“返回计划”时通知 MainWindow 切回原来的计划软件页面。
    void backRequested();

private:
    struct ReviewCard
    {
        QString front;
        QString back;
        QString tag;
    };

    void setupUi();
    void setupStyleSheet();
    void connectSignals();
    void loadFakeCards();
    void showCurrentCard();
    void showEmptyState();
    void showAnswer();
    void submitRating(int rating);
    void setReviewButtonsVisible(bool visible);

    QList<ReviewCard> m_cards;
    int m_currentIndex;

    QLabel *m_titleLabel;
    QLabel *m_statusLabel;
    QLabel *m_tagLabel;
    QTextEdit *m_frontEdit;
    QTextEdit *m_backEdit;
    QPushButton *m_showAnswerButton;
    QPushButton *m_backToPlannerButton;
    QPushButton *m_forgetButton;
    QPushButton *m_blurryButton;
    QPushButton *m_rememberButton;
    QPushButton *m_easyButton;
};

#endif // REVIEWWIDGET_H
