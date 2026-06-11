#include "carddialog.h"

#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPair>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {
QString normalizedDialogGroup(const QString &group)
{
    const QString trimmedGroup = group.trimmed();
    return trimmedGroup.isEmpty() || trimmedGroup == "全部分组" ? QString("默认分组") : trimmedGroup;
}

QString normalizedDialogDeck(const QString &deck)
{
    const QString trimmedDeck = deck.trimmed();
    return trimmedDeck.isEmpty() || trimmedDeck == "全部牌组" ? QString("默认牌组") : trimmedDeck;
}

QStringList uniqueDecks(const QStringList &decks)
{
    QStringList result;
    for (const QString &deck : decks) {
        const QString deckName = normalizedDialogDeck(deck);
        if (!result.contains(deckName)) {
            result.append(deckName);
        }
    }

    return result;
}
}

CardDialog::CardDialog(QWidget *parent)
    : QDialog(parent),
      m_frontEdit(new QLineEdit(this)),
      m_backEdit(new QTextEdit(this)),
      m_tagEdit(new QLineEdit(this)),
      m_groupCombo(new QComboBox(this)),
      m_deckCombo(new QComboBox(this)),
      m_colorCombo(new QComboBox(this))
{
    setWindowTitle("添加复习卡片");
    setMinimumSize(520, 420);

    /*
     * 正面和背面是复习卡片最核心的内容。
     * 标签是可选字段，用来给卡片做简单分类。
     */
    m_frontEdit->setPlaceholderText("例如：Qt 中信号和槽的作用是什么？");
    m_backEdit->setPlaceholderText("写下答案、解释或需要回忆的重点。");
    m_backEdit->setMinimumHeight(150);
    m_tagEdit->setPlaceholderText("例如：Qt 基础 / 高等数学 / 英语单词");
    m_groupCombo->setEditable(true);
    m_groupCombo->addItem("默认分组");
    if (m_groupCombo->lineEdit() != nullptr) {
        m_groupCombo->lineEdit()->setPlaceholderText("例如：高等数学 / 英语");
    }
    m_deckCombo->setEditable(true);
    m_deckCombo->addItem("默认牌组");
    if (m_deckCombo->lineEdit() != nullptr) {
        m_deckCombo->lineEdit()->setPlaceholderText("例如：英语四级 / 高等数学");
    }
    setupColorOptions();

    auto *titleLabel = new QLabel("复习卡片", this);
    titleLabel->setObjectName("dialogTitle");

    auto *subtitleLabel = new QLabel("正面和背面必填，保存后会立即进入今日复习。", this);
    subtitleLabel->setObjectName("dialogSubtitle");

    auto *formLayout = new QFormLayout;
    formLayout->setContentsMargins(0, 12, 0, 10);
    formLayout->setSpacing(12);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    formLayout->addRow("正面问题:", m_frontEdit);
    formLayout->addRow("背面答案:", m_backEdit);
    formLayout->addRow("标签:", m_tagEdit);
    formLayout->addRow("分组:", m_groupCombo);
    formLayout->addRow("牌组:", m_deckCombo);
    formLayout->addRow("卡片颜色:", m_colorCombo);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText("确定");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("取消");
    buttonBox->button(QDialogButtonBox::Ok)->setObjectName("okButton");
    buttonBox->button(QDialogButtonBox::Cancel)->setObjectName("cancelButton");
    buttonBox->button(QDialogButtonBox::Ok)->setCursor(Qt::PointingHandCursor);
    buttonBox->button(QDialogButtonBox::Cancel)->setCursor(Qt::PointingHandCursor);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &CardDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &CardDialog::reject);
    connect(m_groupCombo, &QComboBox::currentTextChanged, this, [this]() {
        refreshDeckComboForCurrentGroup();
    });

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 22, 24, 20);
    mainLayout->setSpacing(8);
    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(subtitleLabel);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttonBox);

    setupStyleSheet();
}

QString CardDialog::getFront() const
{
    return m_frontEdit->text().trimmed();
}

QString CardDialog::getBack() const
{
    return m_backEdit->toPlainText().trimmed();
}

QString CardDialog::getTag() const
{
    return m_tagEdit->text().trimmed();
}

QString CardDialog::getGroup() const
{
    return normalizedDialogGroup(m_groupCombo->currentText());
}

QString CardDialog::getDeck() const
{
    return normalizedDialogDeck(m_deckCombo->currentText());
}

QString CardDialog::getCardColor() const
{
    const QString color = m_colorCombo->currentData().toString().trimmed();
    return color.isEmpty() ? QString("#ffffff") : color;
}

void CardDialog::setGroups(const QStringList &groups, const QString &currentGroup)
{
    m_groupCombo->clear();

    QStringList normalizedGroups;
    normalizedGroups.append("默认分组");
    for (const QString &group : groups) {
        const QString trimmedGroup = group.trimmed();
        if (!trimmedGroup.isEmpty() && !normalizedGroups.contains(trimmedGroup)) {
            normalizedGroups.append(trimmedGroup);
        }
    }

    m_groupCombo->addItems(normalizedGroups);

    const QString selectedGroup = normalizedDialogGroup(currentGroup);
    const int index = m_groupCombo->findText(selectedGroup);
    if (index >= 0) {
        m_groupCombo->setCurrentIndex(index);
    } else {
        m_groupCombo->setCurrentText(selectedGroup);
    }
}

void CardDialog::setDecks(const QStringList &decks, const QString &currentDeck)
{
    m_decksByGroup.clear();
    m_decksByGroup.insert(getGroup(), uniqueDecks(decks));
    refreshDeckComboForCurrentGroup(currentDeck);
}

void CardDialog::setDecksByGroup(const QMap<QString, QStringList> &decksByGroup,
                                 const QString &currentGroup,
                                 const QString &currentDeck)
{
    m_decksByGroup.clear();

    QStringList groups;
    for (auto it = decksByGroup.constBegin(); it != decksByGroup.constEnd(); ++it) {
        const QString groupName = normalizedDialogGroup(it.key());
        groups.append(groupName);
        m_decksByGroup.insert(groupName, uniqueDecks(it.value()));
    }

    if (!groups.contains(normalizedDialogGroup(currentGroup))) {
        groups.append(normalizedDialogGroup(currentGroup));
    }

    setGroups(groups, currentGroup);
    refreshDeckComboForCurrentGroup(currentDeck);
}

void CardDialog::refreshDeckComboForCurrentGroup(const QString &preferredDeck)
{
    const QString groupName = getGroup();
    QStringList decks = m_decksByGroup.value(groupName);
    if (decks.isEmpty()) {
        QSignalBlocker blocker(m_deckCombo);
        m_deckCombo->clear();
        m_deckCombo->addItem("请先新建牌组");
        m_deckCombo->setEnabled(false);
        return;
    }

    m_deckCombo->setEnabled(true);
    const QString previousDeck = m_deckCombo->currentText();
    QString selectedDeck = preferredDeck.trimmed().isEmpty()
                               ? normalizedDialogDeck(previousDeck)
                               : normalizedDialogDeck(preferredDeck);
    if (!decks.contains(selectedDeck)) {
        selectedDeck = decks.first();
    }

    QSignalBlocker blocker(m_deckCombo);
    m_deckCombo->clear();
    m_deckCombo->addItems(decks);
    const int index = m_deckCombo->findText(selectedDeck);
    if (index >= 0) {
        m_deckCombo->setCurrentIndex(index);
    } else {
        m_deckCombo->setCurrentText(selectedDeck);
    }
}

bool CardDialog::currentGroupHasDecks() const
{
    return !m_decksByGroup.value(getGroup()).isEmpty();
}

void CardDialog::setupColorOptions()
{
    /*
     * 使用固定柔和色板，避免颜色过深影响文字阅读。
     * 如果想增加颜色，只需要在这里继续 append 即可。
     */
    const QList<QPair<QString, QString>> colors = {
        {"珍珠白", "#ffffff"},
        {"天空蓝", "#eef6ff"},
        {"薄荷绿", "#eefbf4"},
        {"奶油黄", "#fff8dd"},
        {"浅桃粉", "#fff1f3"},
        {"淡紫色", "#f5f0ff"}
    };

    for (const auto &color : colors) {
        QPixmap pixmap(18, 18);
        pixmap.fill(QColor(color.second));
        m_colorCombo->addItem(QIcon(pixmap), color.first, color.second);
    }
}

void CardDialog::accept()
{
    if (!currentGroupHasDecks()) {
        QMessageBox::warning(this, "提示", "当前分组还没有牌组，请先在牌库页面新建牌组。");
        return;
    }

    if (getFront().isEmpty()) {
        QMessageBox::warning(this, "提示", "正面问题不能为空。");
        return;
    }

    if (getBack().isEmpty()) {
        QMessageBox::warning(this, "提示", "背面答案不能为空。");
        return;
    }

    QDialog::accept();
}

void CardDialog::setupStyleSheet()
{
    /*
     * 使用和 TaskDialog 接近的浅色表单风格，
     * 让“添加任务”和“添加卡片”的体验保持一致。
     */
    setStyleSheet(R"(
        QDialog {
            background: #f5f7fb;
            color: #263244;
            font-size: 14px;
        }

        QLabel#dialogTitle {
            color: #1f2d3d;
            font-size: 20px;
            font-weight: 700;
        }

        QLabel#dialogSubtitle {
            color: #7b8798;
            font-size: 12px;
        }

        QLineEdit,
        QTextEdit,
        QComboBox {
            background: #ffffff;
            border: 1px solid #dfe6f1;
            border-radius: 8px;
            padding: 8px 10px;
            selection-background-color: #2f6fed;
        }

        QTextEdit {
            padding: 10px;
        }

        QLineEdit:focus,
        QTextEdit:focus,
        QComboBox:focus {
            border-color: #2f6fed;
        }

        QPushButton {
            min-height: 34px;
            border-radius: 8px;
            border: 1px solid #cfd8e6;
            background: #ffffff;
            color: #263244;
            padding: 7px 16px;
            font-weight: 600;
        }

        QPushButton:hover {
            background: #f0f5ff;
            border-color: #adc3ea;
        }

        QPushButton#okButton {
            background: #2f6fed;
            border-color: #2f6fed;
            color: #ffffff;
        }

        QPushButton#okButton:hover {
            background: #255fd0;
            border-color: #255fd0;
        }

        QPushButton#cancelButton {
            background: #f8fafd;
            color: #475569;
        }
    )");
}
