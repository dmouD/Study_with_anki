#include "admindebugdialog.h"

#include "databasemanager.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

AdminDebugDialog::AdminDebugDialog(DatabaseManager *databaseManager, QWidget *parent)
    : QDialog(parent),
      m_databaseManager(databaseManager),
      m_clearDatabaseButton(new QPushButton("清除数据库", this)),
      m_closeButton(new QPushButton("关闭", this))
{
    setupUi();
    setupStyleSheet();
}

void AdminDebugDialog::setupUi()
{
    setWindowTitle("管理员调试");
    setMinimumSize(460, 260);

    auto *titleLabel = new QLabel("管理员调试", this);
    titleLabel->setObjectName("debugTitle");

    auto *subtitleLabel = new QLabel("这里放置高风险调试功能，仅用于开发和测试。", this);
    subtitleLabel->setObjectName("debugSubtitle");
    subtitleLabel->setWordWrap(true);

    auto *warningLabel = new QLabel(
        "清除数据库会删除所有任务、Anki 卡片、分组和牌组，并重置自增 ID。这个操作不可撤销。",
        this);
    warningLabel->setObjectName("warningLabel");
    warningLabel->setWordWrap(true);

    m_clearDatabaseButton->setObjectName("clearDatabaseButton");
    m_clearDatabaseButton->setCursor(Qt::PointingHandCursor);
    m_clearDatabaseButton->setMinimumHeight(40);

    m_closeButton->setObjectName("closeButton");
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setMinimumHeight(36);

    connect(m_clearDatabaseButton, &QPushButton::clicked, this, &AdminDebugDialog::clearDatabase);
    connect(m_closeButton, &QPushButton::clicked, this, &AdminDebugDialog::accept);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 10, 0, 0);
    buttonLayout->addWidget(m_clearDatabaseButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_closeButton);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 22, 24, 20);
    mainLayout->setSpacing(12);
    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(subtitleLabel);
    mainLayout->addWidget(warningLabel);
    mainLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
}

void AdminDebugDialog::setupStyleSheet()
{
    setStyleSheet(R"(
        QDialog {
            background: #f5f7fb;
            color: #263244;
            font-size: 14px;
        }

        QLabel#debugTitle {
            color: #1f2d3d;
            font-size: 22px;
            font-weight: 800;
        }

        QLabel#debugSubtitle {
            color: #64748b;
            font-size: 13px;
        }

        QLabel#warningLabel {
            background: #fff1f1;
            border: 1px solid #f1b8b8;
            border-radius: 10px;
            color: #9b2c2c;
            padding: 14px;
            font-weight: 600;
        }

        QPushButton {
            border-radius: 8px;
            border: 1px solid #cfd8e6;
            background: #ffffff;
            color: #263244;
            padding: 8px 16px;
            font-weight: 700;
        }

        QPushButton:hover {
            background: #f0f5ff;
            border-color: #adc3ea;
        }

        QPushButton#clearDatabaseButton {
            background: #d93030;
            border-color: #d93030;
            color: #ffffff;
        }

        QPushButton#clearDatabaseButton:hover {
            background: #bd2424;
            border-color: #bd2424;
        }

        QPushButton#closeButton {
            background: #f8fafd;
            color: #475569;
        }
    )");
}

void AdminDebugDialog::clearDatabase()
{
    if (m_databaseManager == nullptr) {
        QMessageBox::warning(this, "提示", "数据库还没有初始化，不能执行清库。");
        return;
    }

    const QMessageBox::StandardButton firstConfirm =
        QMessageBox::warning(this,
                             "危险操作",
                             "确定要清除整个数据库吗？\n\n所有任务和 Anki 卡片都会被删除，此操作不可撤销。",
                             QMessageBox::Yes | QMessageBox::No,
                             QMessageBox::No);
    if (firstConfirm != QMessageBox::Yes) {
        return;
    }

    bool ok = false;
    const QString confirmation = QInputDialog::getText(this,
                                                       "再次确认",
                                                       "请输入 CLEAR 确认清除数据库:",
                                                       QLineEdit::Normal,
                                                       QString(),
                                                       &ok);
    if (!ok) {
        return;
    }

    if (confirmation != "CLEAR") {
        QMessageBox::information(this, "已取消", "输入内容不匹配，数据库没有被清除。");
        return;
    }

    if (!m_databaseManager->clearAllDataForDebug()) {
        QMessageBox::warning(this, "清除失败", "清除数据库失败，请查看调试输出。");
        return;
    }

    emit databaseCleared();
    QMessageBox::information(this, "清除完成", "数据库已清空，并已重建默认分组和默认牌组。");
}
