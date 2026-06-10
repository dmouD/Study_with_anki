#include "taskdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTextEdit>
#include <QtGlobal>
#include <QVBoxLayout>

TaskDialog::TaskDialog(QWidget *parent)
    : QDialog(parent),
      m_titleEdit(new QLineEdit(this)),
      m_contentEdit(new QTextEdit(this)),
      m_taskTypeCombo(new QComboBox(this)),
      m_startDateEdit(new QDateEdit(this)),
      m_endDateEdit(new QDateEdit(this)),
      m_courseTimeEdit(new QLineEdit(this)),
      m_classHoursSpinBox(new QSpinBox(this)),
      m_priorityCombo(new QComboBox(this)),
      m_progressSlider(new QSlider(Qt::Horizontal, this)),
      m_progressSpinBox(new QSpinBox(this)),
      m_doneCheckBox(new QCheckBox("已完成", this)),
      m_taskId(-1)
{
    setWindowTitle("任务");
    setMinimumSize(540, 600);

    /*
     * 这些占位文本不会保存到数据库，只是帮助用户理解每个输入框该填什么。
     * 初学者也可以把它们看作 QLineEdit/QTextEdit 的常见用法示例。
     */
    m_titleEdit->setPlaceholderText("例如：复习高数第三章");
    m_contentEdit->setPlaceholderText("可以记录任务说明、资料位置或复习重点。");
    m_contentEdit->setMinimumHeight(130);

    m_taskTypeCombo->addItems({"每日任务", "长期任务"});
    m_taskTypeCombo->setCurrentText("每日任务");

    /*
     * 每日任务只需要一个日期；长期任务才启用结束日期。
     * 保存时每日任务会自动把 endDate 设置成 startDate。
     */
    m_startDateEdit->setCalendarPopup(true);
    m_startDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_startDateEdit->setDate(QDate::currentDate());

    m_endDateEdit->setCalendarPopup(true);
    m_endDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_endDateEdit->setDate(QDate::currentDate());
    m_endDateEdit->setMinimumDate(m_startDateEdit->date());
    m_endDateEdit->setEnabled(false);

    m_courseTimeEdit->setPlaceholderText("例如：45分钟 / 90分钟 / 2小时30分钟");

    m_classHoursSpinBox->setRange(0, 999);
    m_classHoursSpinBox->setSuffix(" 课时");
    m_classHoursSpinBox->setSpecialValueText("未填写");

    m_priorityCombo->addItems({"高", "中", "低"});
    m_priorityCombo->setCurrentText("中");

    /*
     * 完成进度使用“滑块 + 数字框”组合：
     * - 滑块适合快速拖动。
     * - 数字框适合精确输入 0-100。
     */
    m_progressSlider->setRange(0, 100);
    m_progressSlider->setTickInterval(25);
    m_progressSlider->setSingleStep(5);

    m_progressSpinBox->setRange(0, 100);
    m_progressSpinBox->setSuffix("%");
    m_progressSpinBox->setSingleStep(5);

    auto *progressWidget = new QWidget(this);
    auto *progressLayout = new QHBoxLayout(progressWidget);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    progressLayout->setSpacing(10);
    progressLayout->addWidget(m_progressSlider, 1);
    progressLayout->addWidget(m_progressSpinBox);

    auto *titleLabel = new QLabel("任务信息", this);
    titleLabel->setObjectName("dialogTitle");

    auto *subtitleLabel = new QLabel("填写任务类型、日期范围和 0-100% 完成进度", this);
    subtitleLabel->setObjectName("dialogSubtitle");

    auto *formLayout = new QFormLayout;
    formLayout->setContentsMargins(0, 12, 0, 10);
    formLayout->setSpacing(12);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    formLayout->addRow("标题:", m_titleEdit);
    formLayout->addRow("内容:", m_contentEdit);
    formLayout->addRow("任务类型:", m_taskTypeCombo);
    formLayout->addRow("开始日期:", m_startDateEdit);
    formLayout->addRow("结束日期:", m_endDateEdit);
    formLayout->addRow("课程总时长:", m_courseTimeEdit);
    formLayout->addRow("课时数:", m_classHoursSpinBox);
    formLayout->addRow("优先级:", m_priorityCombo);
    formLayout->addRow("完成进度:", progressWidget);
    formLayout->addRow("状态:", m_doneCheckBox);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText("确定");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("取消");
    buttonBox->button(QDialogButtonBox::Ok)->setObjectName("okButton");
    buttonBox->button(QDialogButtonBox::Cancel)->setObjectName("cancelButton");
    buttonBox->button(QDialogButtonBox::Ok)->setCursor(Qt::PointingHandCursor);
    buttonBox->button(QDialogButtonBox::Cancel)->setCursor(Qt::PointingHandCursor);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &TaskDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &TaskDialog::reject);

    connect(m_startDateEdit, &QDateEdit::dateChanged, this, [this](const QDate &date) {
        m_endDateEdit->setMinimumDate(date);
        if (m_taskTypeCombo->currentText() == "每日任务" || m_endDateEdit->date() < date) {
            m_endDateEdit->setDate(date);
        }
    });

    connect(m_taskTypeCombo, &QComboBox::currentTextChanged, this, [this](const QString &taskType) {
        const bool isLongTermTask = taskType == "长期任务";
        m_endDateEdit->setEnabled(isLongTermTask);
        if (!isLongTermTask) {
            m_endDateEdit->setDate(m_startDateEdit->date());
        }
    });

    /*
     * 让滑块、数字框和“已完成”复选框保持同步：
     * - 进度到 100 时自动勾选已完成。
     * - 勾选已完成时自动把进度设为 100。
     * - 取消已完成时，如果当前是 100，就回到 0。
     */
    connect(m_progressSlider, &QSlider::valueChanged, m_progressSpinBox, &QSpinBox::setValue);
    connect(m_progressSpinBox, qOverload<int>(&QSpinBox::valueChanged), m_progressSlider, &QSlider::setValue);
    connect(m_progressSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        m_doneCheckBox->setChecked(value >= 100);
    });
    connect(m_doneCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && m_progressSpinBox->value() < 100) {
            m_progressSpinBox->setValue(100);
        } else if (!checked && m_progressSpinBox->value() == 100) {
            m_progressSpinBox->setValue(0);
        }
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

void TaskDialog::setupStyleSheet()
{
    /*
     * 对话框使用和主窗口一致的浅色风格。
     * 所有表单控件的边框、圆角和选中颜色都集中在这里，方便学习和修改。
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
        QDateEdit,
        QComboBox,
        QSpinBox {
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
        QDateEdit:focus,
        QComboBox:focus,
        QSpinBox:focus {
            border-color: #2f6fed;
        }

        QSlider::groove:horizontal {
            height: 8px;
            background: #dfe6f1;
            border-radius: 4px;
        }

        QSlider::sub-page:horizontal {
            background: #2f6fed;
            border-radius: 4px;
        }

        QSlider::handle:horizontal {
            width: 18px;
            height: 18px;
            margin: -5px 0;
            border-radius: 9px;
            background: #ffffff;
            border: 2px solid #2f6fed;
        }

        QCheckBox {
            spacing: 8px;
            color: #263244;
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

Task TaskDialog::getTask() const
{
    /*
     * 从各个输入控件读取值，并转换成 Task 对象。
     * 开始/结束日期保存为 yyyy-MM-dd，和数据库查询格式保持一致。
     */
    Task task;
    task.id = m_taskId;
    task.title = m_titleEdit->text().trimmed();
    task.content = m_contentEdit->toPlainText();
    task.taskType = m_taskTypeCombo->currentText();
    task.startDate = m_startDateEdit->date().toString(Qt::ISODate);
    task.endDate = task.taskType == "每日任务"
                       ? task.startDate
                       : m_endDateEdit->date().toString(Qt::ISODate);
    task.courseTime = m_courseTimeEdit->text().trimmed();
    task.classHours = m_classHoursSpinBox->value();
    task.priority = m_priorityCombo->currentText();
    task.progress = m_progressSpinBox->value();
    task.done = task.progress >= 100;
    task.createdAt = m_createdAt;

    return task;
}

void TaskDialog::setTask(const Task &task)
{
    /*
     * 修改任务时，MainWindow 会先从数据库查询完整 Task，
     * 再调用 setTask() 把每个字段回填到对应控件。
     */
    m_taskId = task.id;
    m_createdAt = task.createdAt;

    m_titleEdit->setText(task.title);
    m_contentEdit->setPlainText(task.content);
    m_courseTimeEdit->setText(task.courseTime);
    m_classHoursSpinBox->setValue(task.classHours);

    const int typeIndex = m_taskTypeCombo->findText(task.taskType);
    m_taskTypeCombo->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);

    const QDate startDate = QDate::fromString(task.startDate, Qt::ISODate);
    if (startDate.isValid()) {
        m_startDateEdit->setDate(startDate);
        m_endDateEdit->setMinimumDate(startDate);
    }

    const QDate endDate = QDate::fromString(task.endDate, Qt::ISODate);
    if (endDate.isValid()) {
        m_endDateEdit->setDate(endDate);
    } else if (startDate.isValid()) {
        m_endDateEdit->setDate(startDate);
    }

    const int priorityIndex = m_priorityCombo->findText(task.priority);
    if (priorityIndex >= 0) {
        m_priorityCombo->setCurrentIndex(priorityIndex);
    }

    const int progress = qBound(0, task.done && task.progress < 100 ? 100 : task.progress, 100);
    m_progressSpinBox->setValue(progress);
    m_doneCheckBox->setChecked(progress >= 100);
}

void TaskDialog::accept()
{
    // 标题是 tasks 表里唯一 NOT NULL 的业务字段，因此这里做必填检查。
    if (m_titleEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "标题不能为空。");
        return;
    }

    if (m_endDateEdit->date() < m_startDateEdit->date()) {
        QMessageBox::warning(this, "提示", "结束日期不能早于开始日期。");
        return;
    }

    QDialog::accept();
}
