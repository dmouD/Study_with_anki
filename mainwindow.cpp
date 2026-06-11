#include "mainwindow.h"

#include "deepseekclient.h"
#include "reviewwidget.h"
#include "taskdialog.h"

#include <QAbstractAnimation>
#include <QAbstractItemView>
#include <QBrush>
#include <QCalendarWidget>
#include <QColor>
#include <QDate>
#include <QEasingCurve>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QSizePolicy>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTimer>
#include <QtGlobal>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_deepSeekClient(new DeepSeekClient(this)),
      m_stackedWidget(nullptr),
      m_plannerWidget(nullptr),
      m_reviewWidget(nullptr),
      m_calendar(nullptr),
      m_taskTable(nullptr),
      m_addButton(nullptr),
      m_editButton(nullptr),
      m_deleteButton(nullptr),
      m_toggleButton(nullptr),
      m_showAllButton(nullptr),
      m_reviewButton(nullptr),
      m_detailTitleLabel(nullptr),
      m_detailMetaLabel(nullptr),
      m_detailProgressBar(nullptr),
      m_detailContentEdit(nullptr),
      m_apiKeyEdit(nullptr),
      m_aiAnalyzeButton(nullptr),
      m_aiStatusLabel(nullptr),
      m_aiResultEdit(nullptr),
      m_showingAllTasks(true)
{
    setWindowTitle("DStudy");

    setupUi();
    connectSignals();

    if (!m_databaseManager.initialize()) {
        QMessageBox::critical(this, "数据库错误", "数据库初始化失败，请查看调试输出。");
        return;
    }

    m_calendar->setSelectedDate(QDate::currentDate());
    loadTodayTasks();

    QTimer::singleShot(0, this, [this]() {
        playStartupAnimation();
    });
}

void MainWindow::setupUi()
{
    /*
     * 主窗口使用上下结构：
     * - 上方是 QSplitter，左侧放日历，右侧放任务表格。
     * - 下方是按钮栏，集中放任务操作按钮。
     *
     * QSplitter 可以让用户拖动左右区域宽度，比固定布局更舒服。
     */
    m_stackedWidget = new QStackedWidget(this);
    m_plannerWidget = new QWidget(this);
    m_plannerWidget->setObjectName("mainBackground");

    auto *mainLayout = new QVBoxLayout(m_plannerWidget);
    mainLayout->setContentsMargins(18, 18, 18, 14);
    mainLayout->setSpacing(14);

    auto *leftPanel = new QFrame(this);
    leftPanel->setObjectName("sidePanel");
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(18, 18, 18, 18);
    leftLayout->setSpacing(14);

    m_calendar = new QCalendarWidget(this);
    m_calendar->setGridVisible(true);
    m_calendar->setMinimumWidth(280);
    m_calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);

    leftLayout->addWidget(createSectionHeader("日历筛选", "点击日期后显示覆盖当天的任务"));
    leftLayout->addWidget(m_calendar);
    leftLayout->addStretch();

    auto *rightPanel = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(14);

    auto *taskListPanel = new QFrame(this);
    taskListPanel->setObjectName("taskListPanel");
    auto *taskListLayout = new QVBoxLayout(taskListPanel);
    taskListLayout->setContentsMargins(18, 18, 18, 18);
    taskListLayout->setSpacing(14);

    m_taskTable = new QTableWidget(this);
    m_taskTable->setColumnCount(8);
    m_taskTable->setHorizontalHeaderLabels({"ID", "类型", "标题", "日期", "总课时", "完成课时", "优先级", "完成进度"});
    m_taskTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_taskTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_taskTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_taskTable->setAlternatingRowColors(true);
    m_taskTable->setShowGrid(false);
    m_taskTable->setWordWrap(false);
    m_taskTable->setFocusPolicy(Qt::NoFocus);
    m_taskTable->verticalHeader()->setVisible(false);
    m_taskTable->verticalHeader()->setDefaultSectionSize(42);
    m_taskTable->horizontalHeader()->setFixedHeight(42);
    m_taskTable->horizontalHeader()->setStretchLastSection(true);
    m_taskTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_taskTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_taskTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_taskTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    taskListLayout->addWidget(createSectionHeader("任务列表", "点击任务后在下方查看完整内容"));
    taskListLayout->addWidget(m_taskTable);

    auto *detailPanel = new QFrame(this);
    detailPanel->setObjectName("detailPanel");
    auto *detailLayout = new QVBoxLayout(detailPanel);
    detailLayout->setContentsMargins(18, 16, 18, 18);
    detailLayout->setSpacing(10);

    m_detailTitleLabel = new QLabel("请选择一条任务", this);
    m_detailTitleLabel->setObjectName("detailTitle");
    m_detailTitleLabel->setWordWrap(true);

    m_detailMetaLabel = new QLabel("点击上方任务列表中的任意一行，即可查看任务内容。", this);
    m_detailMetaLabel->setObjectName("detailMeta");
    m_detailMetaLabel->setWordWrap(true);

    m_detailProgressBar = new QProgressBar(this);
    m_detailProgressBar->setObjectName("detailProgressBar");
    m_detailProgressBar->setRange(0, 100);
    m_detailProgressBar->setValue(0);
    m_detailProgressBar->setFormat("%p%");
    m_detailProgressBar->setTextVisible(true);

    m_detailContentEdit = new QTextEdit(this);
    m_detailContentEdit->setObjectName("detailContentEdit");
    m_detailContentEdit->setReadOnly(true);
    m_detailContentEdit->setPlaceholderText("这里会显示任务的详细内容。");
    m_detailContentEdit->setMinimumHeight(105);

    detailLayout->addWidget(createSectionHeader("任务详情", "查看当前选中任务的完整内容"));
    detailLayout->addWidget(m_detailTitleLabel);
    detailLayout->addWidget(m_detailMetaLabel);
    detailLayout->addWidget(m_detailProgressBar);
    detailLayout->addWidget(m_detailContentEdit);

    auto *aiPanel = new QFrame(this);
    aiPanel->setObjectName("aiPanel");
    auto *aiLayout = new QVBoxLayout(aiPanel);
    aiLayout->setContentsMargins(18, 16, 18, 18);
    aiLayout->setSpacing(10);

    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setObjectName("apiKeyEdit");
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText("输入 DeepSeek API Key，仅本次运行使用");
    QString environmentApiKey = qEnvironmentVariable("DEEPSEEK_API_KEY");
    if (environmentApiKey.isEmpty()) {
        environmentApiKey = qEnvironmentVariable("DEESEEK_API_KEY");
    }
    if (!environmentApiKey.isEmpty()) {
        m_apiKeyEdit->setText(environmentApiKey);
    }

    m_aiAnalyzeButton = new QPushButton("AI 分析计划", this);
    m_aiAnalyzeButton->setProperty("buttonRole", "primary");
    m_aiAnalyzeButton->setMinimumHeight(36);
    m_aiAnalyzeButton->setCursor(Qt::PointingHandCursor);

    auto *aiInputLayout = new QHBoxLayout;
    aiInputLayout->setContentsMargins(0, 0, 0, 0);
    aiInputLayout->setSpacing(8);
    aiInputLayout->addWidget(m_apiKeyEdit, 1);
    aiInputLayout->addWidget(m_aiAnalyzeButton);

    m_aiStatusLabel = new QLabel("AI 会根据当前全部任务给出安排建议，不会自动修改任务。", this);
    m_aiStatusLabel->setObjectName("aiStatusLabel");
    m_aiStatusLabel->setWordWrap(true);

    m_aiResultEdit = new QTextEdit(this);
    m_aiResultEdit->setObjectName("aiResultEdit");
    m_aiResultEdit->setReadOnly(true);
    m_aiResultEdit->setPlaceholderText("AI 分析结果会显示在这里。");
    m_aiResultEdit->setMinimumHeight(105);

    aiLayout->addWidget(createSectionHeader("AI 计划助手", "使用 DeepSeek 分析当前任务安排"));
    aiLayout->addLayout(aiInputLayout);
    aiLayout->addWidget(m_aiStatusLabel);
    aiLayout->addWidget(m_aiResultEdit);

    auto *bottomPanels = new QWidget(this);
    auto *bottomLayout = new QHBoxLayout(bottomPanels);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(14);
    bottomLayout->addWidget(detailPanel, 1);
    bottomLayout->addWidget(aiPanel, 1);

    rightLayout->addWidget(taskListPanel, 3);
    rightLayout->addWidget(bottomPanels, 2);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName("mainSplitter");
    splitter->setHandleWidth(12);
    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    m_addButton = new QPushButton("添加任务", this);
    m_editButton = new QPushButton("修改任务", this);
    m_deleteButton = new QPushButton("删除任务", this);
    m_toggleButton = new QPushButton("标记完成/未完成", this);
    m_showAllButton = new QPushButton("显示全部", this);
    m_reviewButton = new QPushButton("Anki复习", this);

    m_addButton->setProperty("buttonRole", "primary");
    m_editButton->setProperty("buttonRole", "normal");
    m_deleteButton->setProperty("buttonRole", "danger");
    m_toggleButton->setProperty("buttonRole", "accent");
    m_showAllButton->setProperty("buttonRole", "ghost");
    m_reviewButton->setProperty("buttonRole", "primary");

    const QList<QPushButton *> buttons = {
        m_addButton,
        m_editButton,
        m_deleteButton,
        m_toggleButton,
        m_showAllButton,
        m_reviewButton
    };

    for (QPushButton *button : buttons) {
        button->setMinimumHeight(40);
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }

    auto *buttonBar = new QFrame(this);
    buttonBar->setObjectName("buttonBar");
    auto *buttonLayout = new QHBoxLayout(buttonBar);
    buttonLayout->setContentsMargins(14, 12, 14, 12);
    buttonLayout->setSpacing(10);
    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_editButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addWidget(m_toggleButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_reviewButton);
    buttonLayout->addWidget(m_showAllButton);

    mainLayout->addWidget(splitter);
    mainLayout->addWidget(buttonBar);

    m_reviewWidget = new ReviewWidget(&m_databaseManager, this);
    m_stackedWidget->addWidget(m_plannerWidget);
    m_stackedWidget->addWidget(m_reviewWidget);

    setCentralWidget(m_stackedWidget);
    setupStyleSheet();
    statusBar()->showMessage("准备加载今日任务");
}

void MainWindow::setupStyleSheet()
{
    /*
     * Qt Widgets 可以用类似 CSS 的样式表快速改善界面。
     * 这里把颜色、边框、按钮状态集中写在一个函数里，
     * 以后想换风格时只改这一处即可。
     */
    setStyleSheet(R"(
        QWidget#mainBackground {
            background: #f5f7fb;
            color: #263244;
            font-size: 14px;
        }

        QFrame#sidePanel,
        QFrame#taskListPanel,
        QFrame#detailPanel,
        QFrame#aiPanel,
        QFrame#buttonBar {
            background: #ffffff;
            border: 1px solid #dfe6f1;
            border-radius: 10px;
        }

        QFrame#detailPanel,
        QFrame#aiPanel {
            background: #fbfdff;
        }

        QLabel#sectionTitle {
            color: #1f2d3d;
            font-size: 18px;
            font-weight: 700;
        }

        QLabel#sectionSubtitle {
            color: #7b8798;
            font-size: 12px;
        }

        QLabel#detailTitle {
            color: #162033;
            font-size: 17px;
            font-weight: 700;
        }

        QLabel#detailMeta {
            color: #64748b;
            font-size: 13px;
        }

        QLabel#aiStatusLabel {
            color: #64748b;
            font-size: 13px;
        }

        QSplitter::handle {
            background: transparent;
        }

        QCalendarWidget {
            background: #ffffff;
            border: 1px solid #dfe6f1;
            border-radius: 8px;
            selection-background-color: #2f6fed;
            selection-color: #ffffff;
        }

        QCalendarWidget QToolButton {
            color: #263244;
            background: #eef3fb;
            border: 1px solid #dfe6f1;
            border-radius: 6px;
            padding: 5px 8px;
            margin: 2px;
        }

        QCalendarWidget QToolButton:hover {
            background: #e0ebff;
            border-color: #b8ccf2;
        }

        QCalendarWidget QMenu {
            background: #ffffff;
            border: 1px solid #dfe6f1;
        }

        QCalendarWidget QSpinBox {
            border: 1px solid #dfe6f1;
            border-radius: 6px;
            padding: 3px;
            background: #ffffff;
        }

        QTableWidget {
            background: #ffffff;
            alternate-background-color: #f8fafd;
            border: 1px solid #dfe6f1;
            border-radius: 8px;
            gridline-color: transparent;
            selection-background-color: #dce8ff;
            selection-color: #1f2d3d;
        }

        QTableWidget::item {
            border-bottom: 1px solid #eef2f7;
            padding: 8px;
        }

        QTableWidget::item:hover {
            background: #f0f6ff;
        }

        QTableWidget::item:selected {
            background: #dce8ff;
            color: #1f2d3d;
        }

        QProgressBar#taskProgressBar {
            min-height: 22px;
            max-height: 22px;
            border: 1px solid #dfe6f1;
            border-radius: 11px;
            background: #f1f5f9;
            color: #1f2d3d;
            text-align: center;
            font-weight: 700;
        }

        QProgressBar#taskProgressBar::chunk {
            border-radius: 10px;
            background: #2f6fed;
        }

        QProgressBar#detailProgressBar {
            min-height: 20px;
            max-height: 20px;
            border: 1px solid #dfe6f1;
            border-radius: 10px;
            background: #edf2f7;
            color: #1f2d3d;
            text-align: center;
            font-weight: 700;
        }

        QProgressBar#detailProgressBar::chunk {
            border-radius: 9px;
            background: #1f7a55;
        }

        QTextEdit#detailContentEdit {
            background: #ffffff;
            border: 1px solid #dfe6f1;
            border-radius: 8px;
            padding: 10px;
            color: #263244;
            selection-background-color: #dce8ff;
        }

        QLineEdit#apiKeyEdit,
        QTextEdit#aiResultEdit {
            background: #ffffff;
            border: 1px solid #dfe6f1;
            border-radius: 8px;
            padding: 9px 10px;
            color: #263244;
            selection-background-color: #dce8ff;
        }

        QLineEdit#apiKeyEdit:focus,
        QTextEdit#aiResultEdit:focus {
            border-color: #2f6fed;
        }

        QHeaderView::section {
            background: #edf2f7;
            color: #475569;
            border: none;
            border-right: 1px solid #dfe6f1;
            padding: 9px;
            font-weight: 700;
        }

        QPushButton {
            border-radius: 8px;
            border: 1px solid #cfd8e6;
            background: #ffffff;
            color: #263244;
            padding: 8px 14px;
            font-weight: 600;
        }

        QPushButton:hover {
            background: #f0f5ff;
            border-color: #adc3ea;
        }

        QPushButton:pressed {
            background: #e4edff;
            padding-top: 9px;
            padding-bottom: 7px;
        }

        QPushButton[buttonRole="primary"] {
            background: #2f6fed;
            border-color: #2f6fed;
            color: #ffffff;
        }

        QPushButton[buttonRole="primary"]:hover {
            background: #255fd0;
            border-color: #255fd0;
        }

        QPushButton[buttonRole="accent"] {
            background: #eef8f4;
            border-color: #a9d8c4;
            color: #1f7a55;
        }

        QPushButton[buttonRole="accent"]:hover {
            background: #def1e9;
            border-color: #86c8ac;
        }

        QPushButton[buttonRole="danger"] {
            background: #fff1f1;
            border-color: #f1b8b8;
            color: #c73d3d;
        }

        QPushButton[buttonRole="danger"]:hover {
            background: #ffe3e3;
            border-color: #e69a9a;
        }

        QPushButton[buttonRole="ghost"] {
            background: #f8fafd;
            color: #475569;
        }

        QStatusBar {
            background: #f5f7fb;
            color: #64748b;
            border: none;
        }
    )");
}

void MainWindow::connectSignals()
{
    /*
     * 信号槽连接集中放在这里，方便初学者查找“哪个按钮触发哪个函数”。
     * clicked 是按钮点击信号，selectionChanged 是日历日期变化信号。
     */
    connect(m_addButton, &QPushButton::clicked, this, &MainWindow::addTask);
    connect(m_editButton, &QPushButton::clicked, this, &MainWindow::editTask);
    connect(m_deleteButton, &QPushButton::clicked, this, &MainWindow::deleteTask);
    connect(m_toggleButton, &QPushButton::clicked, this, &MainWindow::toggleTaskDone);
    connect(m_showAllButton, &QPushButton::clicked, this, &MainWindow::showAllTasks);
    connect(m_reviewButton, &QPushButton::clicked, this, &MainWindow::openReviewWidget);
    connect(m_reviewWidget, &ReviewWidget::backRequested, this, &MainWindow::showPlannerWidget);
    connect(m_calendar, &QCalendarWidget::selectionChanged, this, &MainWindow::filterTasksBySelectedDate);
    connect(m_taskTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::showSelectedTaskDetails);
    connect(m_aiAnalyzeButton, &QPushButton::clicked, this, &MainWindow::analyzePlanWithAi);
    connect(m_deepSeekClient, &DeepSeekClient::analysisFinished, this, &MainWindow::showAiResult);
    connect(m_deepSeekClient, &DeepSeekClient::analysisFailed, this, &MainWindow::showAiError);
}

void MainWindow::loadAllTasks()
{
    // 显示全部任务时，不再受日历当前日期限制。
    m_showingAllTasks = true;
    refreshTaskTable(m_databaseManager.getAllTasks());
    statusBar()->showMessage("显示全部任务");
}

void MainWindow::loadTodayTasks()
{
    /*
     * 程序启动时默认显示“今天”的任务，避免历史每日任务干扰当前计划。
     * 长期任务只要日期范围覆盖今天，仍然会显示出来。
     */
    m_showingAllTasks = false;
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    refreshTaskTable(m_databaseManager.getTasksByDate(today));
    statusBar()->showMessage("今日任务: " + today);
}

void MainWindow::refreshTaskTable(const QList<Task> &tasks)
{
    /*
     * 每次刷新表格时先清空旧行，再根据查询结果重新插入。
     * 这种做法对小型本地计划软件足够直观，也避免复杂 Model/View 架构。
     */
    m_taskTable->setRowCount(0);
    clearTaskDetails();

    for (const Task &task : tasks) {
        const int row = m_taskTable->rowCount();
        m_taskTable->insertRow(row);

        /*
         * 第一列显示给用户看的连续编号，而不是数据库真实主键。
         * SQLite 的 AUTOINCREMENT 主键删除后不会回收，这是正常设计。
         * 为了界面看起来连续，把真实 task.id 存到 UserRole 里，后续修改/删除仍然用真实 ID。
         */
        auto *idItem = new QTableWidgetItem(QString::number(row + 1));
        idItem->setData(Qt::UserRole, task.id);
        auto *typeItem = new QTableWidgetItem(task.taskType);
        auto *titleItem = new QTableWidgetItem(task.title);
        auto *dateItem = new QTableWidgetItem(taskDateRangeText(task));
        auto *classHoursItem = new QTableWidgetItem(task.classHours > 0 ? QString::number(task.classHours) + "课时" : "-");
        auto *completedClassHoursItem = new QTableWidgetItem(task.classHoursProgressText());
        auto *priorityItem = new QTableWidgetItem(task.priority);
        auto *progressBar = createProgressBar(task);

        idItem->setTextAlignment(Qt::AlignCenter);
        typeItem->setTextAlignment(Qt::AlignCenter);
        dateItem->setTextAlignment(Qt::AlignCenter);
        classHoursItem->setTextAlignment(Qt::AlignCenter);
        completedClassHoursItem->setTextAlignment(Qt::AlignCenter);
        priorityItem->setTextAlignment(Qt::AlignCenter);

        const QList<QTableWidgetItem *> items = {
            idItem,
            typeItem,
            titleItem,
            dateItem,
            classHoursItem,
            completedClassHoursItem,
            priorityItem
        };

        for (QTableWidgetItem *item : items) {
            styleTaskTableItem(item, task);
        }

        if (task.priority == "高") {
            priorityItem->setForeground(QBrush(QColor("#c73d3d")));
        } else if (task.priority == "中") {
            priorityItem->setForeground(QBrush(QColor("#a15c00")));
        } else {
            priorityItem->setForeground(QBrush(QColor("#1f7a55")));
        }

        if (task.taskType == "长期任务") {
            typeItem->setForeground(QBrush(QColor("#2563eb")));
            typeItem->setBackground(QBrush(QColor("#eaf2ff")));
        } else {
            typeItem->setForeground(QBrush(QColor("#1f7a55")));
            typeItem->setBackground(QBrush(QColor("#e9f8ef")));
        }

        m_taskTable->setItem(row, 0, idItem);
        m_taskTable->setItem(row, 1, typeItem);
        m_taskTable->setItem(row, 2, titleItem);
        m_taskTable->setItem(row, 3, dateItem);
        m_taskTable->setItem(row, 4, classHoursItem);
        m_taskTable->setItem(row, 5, completedClassHoursItem);
        m_taskTable->setItem(row, 6, priorityItem);
        m_taskTable->setCellWidget(row, 7, progressBar);
        m_taskTable->setRowHeight(row, 42);
    }

    if (m_taskTable->rowCount() > 0) {
        m_taskTable->selectRow(0);
    }
}

void MainWindow::showSelectedTaskDetails()
{
    /*
     * 点击表格行时，先通过隐藏的真实 ID 查询完整任务，
     * 再把 title/content 等信息显示到下方详情区。
     */
    const int id = selectedTaskId();
    if (id < 0) {
        clearTaskDetails();
        return;
    }

    const Task task = m_databaseManager.getTaskById(id);
    if (task.id < 0) {
        clearTaskDetails();
        return;
    }

    m_detailTitleLabel->setText(task.title);
    m_detailMetaLabel->setText(taskMetaText(task));
    animateProgressBar(m_detailProgressBar, task.progress, 260);

    const QString content = task.content.trimmed();
    m_detailContentEdit->setPlainText(content.isEmpty() ? "暂无详细内容。" : content);
    fadeInWidget(m_detailTitleLabel, 160);
    fadeInWidget(m_detailMetaLabel, 180);
    fadeInWidget(m_detailContentEdit, 220);
}

void MainWindow::clearTaskDetails()
{
    m_detailTitleLabel->setText("请选择一条任务");
    m_detailMetaLabel->setText("点击上方任务列表中的任意一行，即可查看任务内容。");
    animateProgressBar(m_detailProgressBar, 0, 180);
    m_detailContentEdit->clear();
}

void MainWindow::analyzePlanWithAi()
{
    /*
     * AI 分析只读取当前数据库中的任务，不会直接改任务。
     * 这样用户可以先看建议，再自己决定是否调整计划。
     */
    const QString prompt = buildAiPrompt();
    if (prompt.isEmpty()) {
        QMessageBox::information(this, "提示", "当前没有任务可以分析。");
        return;
    }

    m_aiAnalyzeButton->setEnabled(false);
    m_aiStatusLabel->setText("正在请求 DeepSeek 分析，请稍等...");
    m_aiResultEdit->setPlainText("正在分析当前任务列表...");

    m_deepSeekClient->analyzePlan(m_apiKeyEdit->text(), prompt);
}

void MainWindow::showAiResult(const QString &result)
{
    m_aiAnalyzeButton->setEnabled(true);
    m_aiStatusLabel->setText("分析完成。建议仅供参考，请结合自己的实际时间调整。");
    m_aiResultEdit->setPlainText(result);
}

void MainWindow::showAiError(const QString &errorMessage)
{
    m_aiAnalyzeButton->setEnabled(true);
    m_aiStatusLabel->setText("AI 分析失败。");
    m_aiResultEdit->setPlainText(errorMessage);
}

int MainWindow::selectedTaskId() const
{
    // 表格第 0 列显示连续编号，真实数据库 ID 存在 UserRole 中。
    const int row = m_taskTable->currentRow();
    if (row < 0) {
        return -1;
    }

    QTableWidgetItem *idItem = m_taskTable->item(row, 0);
    if (!idItem) {
        return -1;
    }

    const QVariant idData = idItem->data(Qt::UserRole);
    if (!idData.isValid()) {
        return -1;
    }

    return idData.toInt();
}

QString MainWindow::currentSelectedDateText() const
{
    // 数据库里按 ISO 格式保存日期，例如 2026-06-01，便于查询和排序。
    return m_calendar->selectedDate().toString(Qt::ISODate);
}

QString MainWindow::taskDateRangeText(const Task &task) const
{
    /*
     * 单日任务显示一个日期；跨天任务显示“开始日期 至 结束日期”。
     * 这里仅负责显示格式，不改变数据库中保存的 ISO 日期字符串。
     */
    if (task.endDate.isEmpty() || task.endDate == task.startDate) {
        return task.startDate;
    }

    return task.startDate + " 至 " + task.endDate;
}

QString MainWindow::taskMetaText(const Task &task) const
{
    const QString statusText = task.progress >= 100 ? "已完成" : "进行中";
    const QString courseTimeText = task.courseTime.trimmed().isEmpty() ? "未填写" : task.courseTime.trimmed();
    const QString classHoursText = task.classHours > 0 ? QString::number(task.classHours) + "课时" : "未填写";
    return QString("类型：%1    日期：%2    课程总时长：%3    总课时：%4    完成课时：%5    优先级：%6    状态：%7")
        .arg(task.taskType,
             taskDateRangeText(task),
             courseTimeText,
             classHoursText,
             task.classHoursProgressText(),
             task.priority,
             statusText);
}

QString MainWindow::buildAiPrompt()
{
    const QList<Task> tasks = m_databaseManager.getAllTasks();
    if (tasks.isEmpty()) {
        return QString();
    }

    QString prompt;
    prompt += "请分析下面的学习任务，并给出一份实用的计划安排建议。\n";
    prompt += "请用中文回答，结构清晰，包含：\n";
    prompt += "1. 当前任务压力概览\n";
    prompt += "2. 优先处理顺序\n";
    prompt += "3. 未来几天的安排建议\n";
    prompt += "4. 可能的风险和调整建议\n";
    prompt += "不要编造不存在的任务，不要直接假设我每天都有大量空闲时间。\n\n";
    prompt += "任务列表：\n";

    int index = 1;
    for (const Task &task : tasks) {
        prompt += QString("%1. 标题：%2\n").arg(index).arg(task.title);
        prompt += QString("   类型：%1\n").arg(task.taskType);
        prompt += QString("   日期：%1\n").arg(taskDateRangeText(task));
        prompt += QString("   课程总时长：%1\n").arg(task.courseTime.trimmed().isEmpty() ? "无" : task.courseTime.trimmed());
        prompt += QString("   课时数：%1\n").arg(task.classHours > 0 ? QString::number(task.classHours) + "课时" : "无");
        prompt += QString("   已完成课时：%1\n").arg(task.classHoursProgressText());
        prompt += QString("   优先级：%1\n").arg(task.priority);
        prompt += QString("   进度：%1%\n").arg(task.progress);
        prompt += QString("   状态：%1\n").arg(task.progress >= 100 ? "已完成" : "进行中");
        prompt += QString("   内容：%1\n\n").arg(task.content.trimmed().isEmpty() ? "无" : task.content.trimmed());
        ++index;
    }

    return prompt;
}

QWidget *MainWindow::createSectionHeader(const QString &title, const QString &subtitle)
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 2);
    layout->setSpacing(3);

    auto *titleLabel = new QLabel(title, container);
    titleLabel->setObjectName("sectionTitle");

    auto *subtitleLabel = new QLabel(subtitle, container);
    subtitleLabel->setObjectName("sectionSubtitle");

    layout->addWidget(titleLabel);
    layout->addWidget(subtitleLabel);

    return container;
}

QProgressBar *MainWindow::createProgressBar(const Task &task)
{
    /*
     * QTableWidget 的单元格不只能放文字，也可以放 QWidget。
     * 这里把“状态”列升级成 QProgressBar，用 0-100% 表示完成进度。
     */
    auto *progressBar = new QProgressBar(this);
    progressBar->setObjectName("taskProgressBar");
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setFormat("%p%");
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setTextVisible(true);

    // 让鼠标点击进度条时仍然可以选中整行，而不是被进度条控件截走事件。
    progressBar->setAttribute(Qt::WA_TransparentForMouseEvents);
    animateProgressBar(progressBar, task.progress, 520);

    return progressBar;
}

void MainWindow::styleTaskTableItem(QTableWidgetItem *item, const Task &task) const
{
    /*
     * 表格单元格的基础样式集中处理：
     * - 已完成任务整体文字变浅，降低视觉权重。
     * - 每个单元格都禁止被单独编辑，保持表格只负责展示。
     */
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);

    if (task.done || task.progress >= 100) {
        item->setForeground(QBrush(QColor("#8a94a6")));
    }
}

void MainWindow::playStartupAnimation()
{
    /*
     * 启动动画只做轻量淡入，不移动布局，也不影响用户点击。
     * 每个面板错开一点点开始，界面会比瞬间出现更柔和。
     */
    const QList<QWidget *> panels = {
        findChild<QWidget *>("sidePanel"),
        findChild<QWidget *>("taskListPanel"),
        findChild<QWidget *>("detailPanel"),
        findChild<QWidget *>("aiPanel"),
        findChild<QWidget *>("buttonBar")
    };

    int delay = 0;
    for (QWidget *panel : panels) {
        if (!panel) {
            continue;
        }

        QTimer::singleShot(delay, this, [this, panel]() {
            fadeInWidget(panel, 260);
        });
        delay += 70;
    }
}

void MainWindow::fadeInWidget(QWidget *widget, int duration)
{
    if (!widget) {
        return;
    }

    if (widget->graphicsEffect()) {
        return;
    }

    auto *effect = new QGraphicsOpacityEffect(widget);
    widget->setGraphicsEffect(effect);
    effect->setOpacity(0.0);

    auto *animation = new QPropertyAnimation(effect, "opacity", widget);
    animation->setDuration(duration);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(animation, &QPropertyAnimation::finished, widget, [widget, effect]() {
        if (widget->graphicsEffect() == effect) {
            widget->setGraphicsEffect(nullptr);
        }
    });

    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::animateProgressBar(QProgressBar *progressBar, int targetValue, int duration)
{
    if (!progressBar) {
        return;
    }

    const int boundedValue = qBound(0, targetValue, 100);
    auto *animation = new QPropertyAnimation(progressBar, "value", progressBar);
    animation->setDuration(duration);
    animation->setStartValue(progressBar->value());
    animation->setEndValue(boundedValue);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::addTask()
{
    // 添加任务：打开空白对话框，用户确认后写入数据库，再刷新当前视图。
    TaskDialog dialog(this);
    dialog.setWindowTitle("添加任务");

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (!m_databaseManager.addTask(dialog.getTask())) {
        QMessageBox::warning(this, "提示", "添加任务失败，请查看调试输出。");
        return;
    }

    if (m_showingAllTasks) {
        loadAllTasks();
    } else {
        filterTasksBySelectedDate();
    }
}

void MainWindow::editTask()
{
    // 修改任务：先根据表格选中行拿到 ID，再从数据库读取完整任务内容回填到对话框。
    const int id = selectedTaskId();
    if (id < 0) {
        QMessageBox::information(this, "提示", "请先选择一条任务。");
        return;
    }

    const Task task = m_databaseManager.getTaskById(id);
    if (task.id < 0) {
        QMessageBox::warning(this, "提示", "未找到选中的任务。");
        return;
    }

    TaskDialog dialog(this);
    dialog.setWindowTitle("修改任务");
    dialog.setTask(task);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (!m_databaseManager.updateTask(dialog.getTask())) {
        QMessageBox::warning(this, "提示", "修改任务失败，请查看调试输出。");
        return;
    }

    if (m_showingAllTasks) {
        loadAllTasks();
    } else {
        filterTasksBySelectedDate();
    }
}

void MainWindow::deleteTask()
{
    // 删除任务前给用户一次确认，避免误删。
    const int id = selectedTaskId();
    if (id < 0) {
        QMessageBox::information(this, "提示", "请先选择一条任务。");
        return;
    }

    const QMessageBox::StandardButton result =
        QMessageBox::question(this, "确认删除", "确定要删除选中的任务吗？");

    if (result != QMessageBox::Yes) {
        return;
    }

    if (!m_databaseManager.deleteTask(id)) {
        QMessageBox::warning(this, "提示", "删除任务失败，请查看调试输出。");
        return;
    }

    if (m_showingAllTasks) {
        loadAllTasks();
    } else {
        filterTasksBySelectedDate();
    }
}

void MainWindow::toggleTaskDone()
{
    // 完成状态只需要传 ID 给数据库，由 SQL 在 0 和 1 之间切换。
    const int id = selectedTaskId();
    if (id < 0) {
        QMessageBox::information(this, "提示", "请先选择一条任务。");
        return;
    }

    if (!m_databaseManager.toggleTaskDone(id)) {
        QMessageBox::warning(this, "提示", "切换任务状态失败，请查看调试输出。");
        return;
    }

    if (m_showingAllTasks) {
        loadAllTasks();
    } else {
        filterTasksBySelectedDate();
    }
}

void MainWindow::filterTasksBySelectedDate()
{
    // 点击日历日期后，表格只显示该日期的任务。
    m_showingAllTasks = false;
    const QString date = currentSelectedDateText();
    refreshTaskTable(m_databaseManager.getTasksByDate(date));
    statusBar()->showMessage("当前筛选日期: " + date);
}

void MainWindow::showAllTasks()
{
    // “显示全部”按钮用于退出日期筛选模式。
    loadAllTasks();
}

void MainWindow::openReviewWidget()
{
    m_reviewWidget->reloadCards();
    m_stackedWidget->setCurrentWidget(m_reviewWidget);
    statusBar()->showMessage("Anki 复习");
}

void MainWindow::showPlannerWidget()
{
    m_stackedWidget->setCurrentWidget(m_plannerWidget);
    statusBar()->showMessage(m_showingAllTasks ? "显示全部任务" : "当前筛选日期: " + currentSelectedDateText());
}
