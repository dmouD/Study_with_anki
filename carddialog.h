#ifndef CARDDIALOG_H
#define CARDDIALOG_H

#include <QDialog>
#include <QStringList>

class QComboBox;
class QLineEdit;
class QTextEdit;

/*
 * CardDialog 是添加复习卡片时使用的弹窗。
 *
 * 这个类只负责收集用户输入，不直接访问数据库。
 * ReviewWidget 会在用户点击确定后调用 getFront()/getBack()/getTag()，
 * 再把数据交给 DatabaseManager 保存。
 */
class CardDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CardDialog(QWidget *parent = nullptr);

    QString getFront() const;
    QString getBack() const;
    QString getTag() const;
    QString getDeck() const;
    QString getCardColor() const;

    // ReviewWidget 打开弹窗前传入已有牌组，用户也可以直接输入新牌组。
    void setDecks(const QStringList &decks, const QString &currentDeck);

protected:
    // 用户点击“确定”时检查正面和背面是否为空。
    void accept() override;

private:
    void setupStyleSheet();
    void setupColorOptions();

    QLineEdit *m_frontEdit;
    QTextEdit *m_backEdit;
    QLineEdit *m_tagEdit;
    QComboBox *m_deckCombo;
    QComboBox *m_colorCombo;
};

#endif // CARDDIALOG_H
