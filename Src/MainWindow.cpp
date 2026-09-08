#include "MainWindow.h"
#include "DeviceFactory.h"
#include "Valve.h"
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QCloseEvent>
#include <QFile>
#include <QHeaderView>
#include <QShortcut>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QStringConverter>
#include <algorithm>
#include <limits>

namespace {
QComboBox *makeFilterComboBox()
{
    auto *filterCombo = new QComboBox();
    filterCombo->addItem("Все");
    filterCombo->setToolTip("Фильтр по столбцу");
    filterCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    filterCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    filterCombo->setMinimumContentsLength(12);
    filterCombo->setMinimumWidth(100);
    filterCombo->setStyleSheet(
        "QComboBox { min-height: 30px; max-height: 30px; width: 100%; padding-left: 8px; padding-right: 18px; background: #f4fbfc; }"
        "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right; width: 18px; border: none; }"
    );
    return filterCombo;
}

QString decodeCsvText(const QByteArray &data)
{
    if (data.isEmpty()) {
        return {};
    }

    if (data.startsWith("\xEF\xBB\xBF")) {
        return QString::fromUtf8(data.mid(3));
    }
    if (data.startsWith("\xFF\xFE")) {
        return QString::fromUtf16(reinterpret_cast<const char16_t*>(data.constData() + 2), (data.size() - 2) / 2);
    }
    if (data.startsWith("\xFE\xFF")) {
        const QByteArray swapped = data.mid(2);
        const int length = swapped.size() / 2;
        QString text;
        text.resize(length);
        auto *out = reinterpret_cast<char16_t*>(text.data());
        const auto *in = reinterpret_cast<const char16_t*>(swapped.constData());
        for (int i = 0; i < length; ++i) {
            const quint16 value = in[i];
            out[i] = static_cast<char16_t>((value >> 8) | ((value & 0xFF) << 8));
        }
        return text;
    }

    bool looksLikeUtf16LE = false;
    bool looksLikeUtf16BE = false;
    int nullCount = 0;
    for (int i = 0; i < data.size() - 1; i += 2) {
        if (data[i] == '\0' && data[i + 1] != '\0') {
            ++nullCount;
        }
        if (data[i] != '\0' && data[i + 1] == '\0') {
            ++nullCount;
        }
    }
    looksLikeUtf16LE = (data.size() >= 4 && (data.size() / 2) > 0 && (nullCount * 2) > data.size() / 3);
    looksLikeUtf16BE = looksLikeUtf16LE;

    if (looksLikeUtf16LE || looksLikeUtf16BE) {
        return QString::fromUtf16(reinterpret_cast<const char16_t*>(data.constData()), data.size() / 2);
    }

    QString utf8Text = QString::fromUtf8(data);
    if (!utf8Text.contains(QChar(0xFFFD))) {
        return utf8Text;
    }

    const QString cp1251Text = QString::fromLocal8Bit(data);
    const QString latin1Text = QString::fromLatin1(data);
    if (cp1251Text.contains(QRegularExpression("[А-Яа-яЁёA-Za-z0-9_./\\-]"))) {
        return cp1251Text;
    }

    return latin1Text;
}

QStringList parseCsvCells(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    QStringList parts = trimmed.split(';', Qt::KeepEmptyParts);
    if (parts.size() <= 1) {
        parts = trimmed.split(',', Qt::KeepEmptyParts);
    }

    for (QString &part : parts) {
        part = part.trimmed();
        if (part.startsWith('"') && part.endsWith('"') && part.size() >= 2) {
            part = part.mid(1, part.size() - 2);
        }
        part.replace("\"\"", "\"");
    }
    return parts;
}
}

QComboBox *MainWindow::createFilterComboBox()
{
    return makeFilterComboBox();
}

void MainWindow::setupTableFilterRow(QTableWidget *table, QVector<QComboBox*> &filters)
{
    table->setRowCount(1);
    table->setRowHeight(0, 32);
    filters.clear();

    for (int col = 0; col < table->columnCount(); ++col) {
        auto *filterCombo = createFilterComboBox();
        filters.append(filterCombo);
        table->setCellWidget(0, col, filterCombo);
    }
}

void MainWindow::refreshColumnFilterOptions(QTableWidget *table, QVector<QComboBox*> &filters)
{
    for (int col = 0; col < table->columnCount(); ++col) {
        auto *combo = filters.value(col, nullptr);
        if (!combo) {
            continue;
        }

        const QString currentText = combo->currentText();
        combo->blockSignals(true);
        combo->clear();
        combo->addItem("Все");

        QSet<QString> uniqueValues;
        for (int row = 1; row < table->rowCount(); ++row) {
            auto *item = table->item(row, col);
            if (!item) {
                continue;
            }
            const QString value = item->text().trimmed();
            if (!value.isEmpty()) {
                uniqueValues.insert(value);
            }
        }

        QStringList values = QStringList(uniqueValues.values());
        std::sort(values.begin(), values.end());
        for (const QString &value : values) {
            combo->addItem(value);
        }

        const int index = combo->findText(currentText, Qt::MatchExactly);
        combo->setCurrentIndex(index >= 0 ? index : 0);
        combo->blockSignals(false);
        table->setCellWidget(0, col, combo);
    }
}

void MainWindow::applyTableFilters(QTableWidget *table, QVector<QComboBox*> &filters)
{
    for (int row = 1; row < table->rowCount(); ++row) {
        bool visible = true;
        for (int col = 0; col < table->columnCount(); ++col) {
            auto *combo = filters.value(col, nullptr);
            if (!combo || combo->currentText() == "Все") {
                continue;
            }
            auto *item = table->item(row, col);
            if (!item || item->text().trimmed() != combo->currentText().trimmed()) {
                visible = false;
                break;
            }
        }
        table->setRowHidden(row, !visible);
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("TagBuilder");
    resize(1200, 700);
    setMinimumSize(950, 550);
    setWindowIcon(QIcon(":/TagBuilder_Icon.png"));
    setStyleSheet(
        "QMainWindow { background: rgb(43, 116, 130); }"
        "QWidget { color: #1f2328; }"
        "QTabWidget::pane { border: 1px solid #2b7482; border-radius: 8px; background: #ffffff; }"
        "QTabBar::tab { background: #edf3f4; color: #1f2328; border: 1px solid #2b7482; border-bottom: none; border-top-left-radius: 8px; border-top-right-radius: 8px; padding: 8px 18px; margin-right: 2px; min-height: 30px; }"
        "QTabBar::tab:selected { background: #ffffff; color: #1f2328; font-weight: 600; }"
        "QTabBar::tab:hover { background: #dfeef1; }"
        "QPushButton { background: #f4f4f4; color: #1f2328; border: 1px solid #b9b9b9; border-radius: 6px; padding: 7px 14px; min-height: 30px; }"
        "QPushButton:hover { background: #e8e8e8; }"
        "QPushButton:pressed { background: #d9d9d9; }"
        "QLineEdit, QComboBox, QTableWidget { background: #ffffff; border: 1px solid #c8d5d8; border-radius: 6px; }"
        "QComboBox { padding: 4px 8px; min-height: 28px; }"
        "QHeaderView::section { background: rgb(43, 116, 130); color: #ffffff; font-weight: 600; padding: 8px; border: 1px solid #1d5a66; }"
        "QTableWidget::item { border: 1px solid #e5ebed; }"
        "QTableWidget::item:selected { background: #3d8bfd; color: white; }"
        "QTableWidget { selection-background-color: #3d8bfd; selection-color: white; alternate-background-color: #f9fbfb; background: #ffffff; }"
    );
    setupUi();

    m_manager.loadFromFile(DeviceManager::storageFilePath());
    refreshTable(m_searchEdit->text());
    refreshBulkTable();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    const auto reply = QMessageBox::question(this,
        "Выход",
        "Вы уверены, что хотите завершить работу?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        event->accept();
    } else {
        event->ignore();
    }
}

bool MainWindow::parseTagParts(const QString &tag, QString &prefix, QString &number, QString &index)
{
    prefix.clear();
    number.clear();
    index.clear();

    const QString trimmed = tag.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    const int firstSeparator = trimmed.indexOf('_');
    if (firstSeparator < 0) {
        prefix = trimmed;
        return true;
    }

    prefix = trimmed.left(firstSeparator);
    const QString tail = trimmed.mid(firstSeparator + 1);
    const int secondSeparator = tail.indexOf('_');
    if (secondSeparator < 0) {
        number = tail;
        return true;
    }

    number = tail.left(secondSeparator);
    index = tail.mid(secondSeparator + 1);
    return true;
}

QString MainWindow::buildTag(const QString &prefix, const QString &number, const QString &index)
{
    QString result;
    if (!prefix.isEmpty()) {
        result = prefix;
    }
    if (!number.isEmpty()) {
        if (!result.isEmpty()) {
            result += "_";
        }
        result += number;
    }
    if (!index.isEmpty()) {
        if (!result.isEmpty()) {
            result += "_";
        }
        result += index;
    }
    return result;
}

void MainWindow::setupUi() {
    auto *central = new QWidget(this);
    setCentralWidget(central);

    m_tabWidget = new QTabWidget(this);

    QWidget *listPage = new QWidget(this);
    auto *listLayout = new QVBoxLayout(listPage);

    auto *inputLayout = new QHBoxLayout();
    inputLayout->setContentsMargins(10, 10, 10, 6);
    inputLayout->setSpacing(10);

    m_typeCombo = new QComboBox();
    m_typeCombo->addItems(DeviceFactory::availableTypes());
    for (int i = 0; i < m_typeCombo->count(); ++i) {
        const QString typeName = m_typeCombo->itemText(i);
        if (typeName == "Valve") {
            m_typeCombo->setItemText(i, "Задвижка (Valve)");
        }
        m_typeCombo->setItemData(i, typeName);
    }
    m_typeCombo->setToolTip("Тип объекта");

    m_numberEdit = new QLineEdit();
    m_numberEdit->setPlaceholderText("Номер");
    m_numberEdit->setToolTip("Номер объекта, например: 1, 1V, A12");
    m_numberEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("^[A-Za-z0-9_-]*$"), this));

    m_indexEdit = new QLineEdit();
    m_indexEdit->setPlaceholderText("Дополнительный индекс");
    m_indexEdit->setToolTip("До 16 символов: A-Z, a-z, 0-9, -, _");

    m_addButton = new QPushButton("Добавить");
    m_addButton->setToolTip("Добавить объект в список");

    inputLayout->addWidget(m_typeCombo);
    inputLayout->addWidget(m_numberEdit);
    inputLayout->addWidget(m_indexEdit);
    inputLayout->addWidget(m_addButton);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(10, 0, 10, 6);
    buttonLayout->setSpacing(10);
    m_exportButton = new QPushButton("Экспорт CSV");
    m_exportButton->setToolTip("Сохранить список в CSV файл");
    m_clearButton = new QPushButton("Очистить список");
    m_clearButton->setToolTip("Удалить все объекты");
    m_removeButton = new QPushButton("Удалить выбранное");
    m_removeButton->setToolTip("Удалить выделенные строки");

    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addWidget(m_clearButton);
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addStretch();

    auto *searchLayout = new QHBoxLayout();
    searchLayout->setContentsMargins(10, 0, 10, 6);
    searchLayout->setSpacing(10);
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("Поиск по тегу, типу или описанию");
    m_searchEdit->setClearButtonEnabled(true);
    searchLayout->addWidget(new QLabel("Поиск:"));
    searchLayout->addWidget(m_searchEdit, 1);

    m_table = new QTableWidget();
    m_table->setColumnCount(3);
    QStringList headers = {"Тег объекта", "Тип", "Описание объекта"};
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_table->setStyleSheet(
        "QTableWidget::item:selected { background: #3d8bfd; color: white; }"
        "QTableWidget { selection-background-color: #3d8bfd; selection-color: white; }"
    );
    m_table->horizontalHeader()->setSectionsClickable(true);

    setupTableFilterRow(m_table, m_tableColumnFilters);

    listLayout->addLayout(inputLayout);
    listLayout->addLayout(buttonLayout);
    listLayout->addLayout(searchLayout);
    listLayout->addWidget(m_table, 1);

    QWidget *bulkPage = new QWidget(this);
    auto *bulkLayout = new QVBoxLayout(bulkPage);

    m_bulkTypeLabel = new QLabel("Тип объекта: Задвижка");
    m_bulkTypeLabel->setStyleSheet("QLabel { font-weight: bold; }");

    auto *bulkFieldsLayout = new QVBoxLayout();
    bulkFieldsLayout->setContentsMargins(10, 10, 10, 6);
    bulkFieldsLayout->setSpacing(10);
    m_bulkPrefixEdit = new QLineEdit();
    m_bulkPrefixEdit->setPlaceholderText("Короткое имя (например: VLV)");
    m_bulkPrefixEdit->setToolTip("Пустое поле = оставить как есть; $ = удалить короткое имя");
    m_bulkNumberEdit = new QLineEdit();
    m_bulkNumberEdit->setPlaceholderText("Новый номер");
    m_bulkNumberEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("^[$A-Za-z0-9_-]*$"), this));
    m_bulkNumberEdit->setToolTip("Пустое поле = оставить как есть; $ = удалить номер. Поддерживаются значения вроде 1V, A12.");
    m_bulkIndexEdit = new QLineEdit();
    m_bulkIndexEdit->setPlaceholderText("Новый индекс");
    m_bulkIndexEdit->setToolTip("Пустое поле = оставить как есть; $ = удалить индекс");

    m_bulkApplyButton = new QPushButton("Применить ко всем");
    m_bulkAddSelectedButton = new QPushButton("Добавить к выделенным");
    m_bulkRefreshButton = new QPushButton("Обновить");
    m_bulkPasteButton = new QPushButton("Вставить из буфера");
    m_bulkAssembleButton = new QPushButton("Собрать теги");
    m_bulkExportButton = new QPushButton("Экспорт CSV");

    auto *bulkActionsLayout = new QHBoxLayout();
    bulkActionsLayout->setContentsMargins(0, 0, 0, 0);
    bulkActionsLayout->setSpacing(10);
    bulkActionsLayout->addWidget(m_bulkApplyButton);
    bulkActionsLayout->addWidget(m_bulkAddSelectedButton);
    bulkActionsLayout->addWidget(m_bulkRefreshButton);
    bulkActionsLayout->addWidget(m_bulkPasteButton);
    bulkActionsLayout->addWidget(m_bulkAssembleButton);
    bulkActionsLayout->addWidget(m_bulkExportButton);
    bulkActionsLayout->addStretch();

    auto *bulkReplaceLayout = new QHBoxLayout();
    bulkReplaceLayout->setSpacing(10);
    bulkReplaceLayout->addWidget(new QLabel("Короткое имя:"));
    bulkReplaceLayout->addWidget(m_bulkPrefixEdit, 2);
    bulkReplaceLayout->addWidget(new QLabel("Номер:"));
    bulkReplaceLayout->addWidget(m_bulkNumberEdit, 1);
    bulkReplaceLayout->addWidget(new QLabel("Индекс:"));
    bulkReplaceLayout->addWidget(m_bulkIndexEdit, 2);

    bulkFieldsLayout->addLayout(bulkActionsLayout);
    bulkFieldsLayout->addLayout(bulkReplaceLayout);

    m_bulkTable = new QTableWidget();
    m_bulkTable->setColumnCount(5);
    m_bulkTable->setHorizontalHeaderLabels({"Вставленное название", "Короткое имя", "Номер", "Индекс", "Готовый тег"});
    m_bulkTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_bulkTable->verticalHeader()->setVisible(false);
    m_bulkTable->setEditTriggers(QAbstractItemView::AllEditTriggers);
    m_bulkTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_bulkTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_bulkTable->setAlternatingRowColors(true);
    m_bulkTable->setStyleSheet(
        "QTableWidget::item:selected { background: #3d8bfd; color: white; }"
        "QTableWidget { selection-background-color: #3d8bfd; selection-color: white; }"
    );
    m_bulkTable->horizontalHeader()->setSectionsClickable(true);

    setupTableFilterRow(m_bulkTable, m_bulkColumnFilters);

    bulkLayout->addWidget(m_bulkTypeLabel);
    bulkLayout->addLayout(bulkFieldsLayout);
    bulkLayout->addWidget(m_bulkTable, 1);

    m_tabWidget->addTab(listPage, "Список объектов");
    m_tabWidget->addTab(bulkPage, "Массовое изменение");

    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    auto *topBarLayout = new QHBoxLayout();
    topBarLayout->setContentsMargins(10, 8, 10, 0);
    topBarLayout->addStretch();
    auto *helpButton = new QPushButton("Подробнее");
    topBarLayout->addWidget(helpButton);
    mainLayout->addLayout(topBarLayout);
    mainLayout->addWidget(m_tabWidget);

    connect(helpButton, &QPushButton::clicked, this, [this]() {
        QString helpText =
            "<h3>Горячие клавиши</h3>"
            "<p><b>Ctrl+C</b> — копировать выделенные строки<br>"
            "<b>Ctrl+Shift+C</b> — копировать в CSV/TSV<br>"
            "<b>Ctrl+V</b> — вставить список из буфера обмена<br>"
            "<b>Ctrl+Shift+V</b> — вставить только выделенные строки<br>"
            "<b>Ctrl+Z</b> — отменить последнее действие<br>"
            "<b>Ctrl+Shift+Z</b> — повторить действие<br>"
            "<b>Ctrl+A</b> — выделить всё<br>"
            "<b>Delete</b> — удалить выделенное<br>"
            "<b>Esc</b> — снять выделение<br>"
            "<b>Ctrl+F</b> — перейти к поиску<br>"
            "<b>Ctrl+L</b> — очистить фильтр</p>"
            "<h3>Как пользоваться программой</h3>"
            "<p>1. Введи тип объекта, номер и индекс и добавь его в список.<br>"
            "2. Используй поиск по тегу, типу или описанию.<br>"
            "3. На вкладке 'Массовое изменение' вставь список тегов из буфера обмена.<br>"
            "4. Заполни поля для замены: короткое имя, номер или индекс. Чтобы удалить часть, укажи $.<br>"
            "5. Нажми 'Применить ко всем' или 'Добавить к выделенным'.<br>"
            "6. Экспортируй результат в CSV.</p>";
        QMessageBox::information(this, "Подробнее", helpText);
    });

    connect(m_addButton, &QPushButton::clicked, this, &MainWindow::onAddClicked);
    connect(m_exportButton, &QPushButton::clicked, this, &MainWindow::onExportClicked);
    connect(m_clearButton, &QPushButton::clicked, this, &MainWindow::onClearClicked);
    connect(m_removeButton, &QPushButton::clicked, this, &MainWindow::onRemoveSelectedClicked);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchChanged);
    connect(m_bulkApplyButton, &QPushButton::clicked, this, &MainWindow::onBulkApplyClicked);
    connect(m_bulkAddSelectedButton, &QPushButton::clicked, this, &MainWindow::onBulkAddSelectedClicked);
    connect(m_bulkRefreshButton, &QPushButton::clicked, this, &MainWindow::onBulkRefreshClicked);
    connect(m_bulkPasteButton, &QPushButton::clicked, this, &MainWindow::onBulkPasteClicked);
    connect(m_bulkAssembleButton, &QPushButton::clicked, this, &MainWindow::onBulkAssembleClicked);
    connect(m_bulkExportButton, &QPushButton::clicked, this, &MainWindow::onBulkExportClicked);
    for (auto *combo : m_tableColumnFilters) {
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
            refreshTable(m_searchEdit->text());
        });
    }
    for (auto *combo : m_bulkColumnFilters) {
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
            refreshBulkTable();
        });
    }
    connect(m_bulkPrefixEdit, &QLineEdit::textChanged, this, [this](const QString &) {
        updateBulkTypeLabel();
    });
    connect(m_bulkNumberEdit, &QLineEdit::textChanged, this, [this](const QString &) {
        updateBulkTypeLabel();
    });
    connect(m_bulkIndexEdit, &QLineEdit::textChanged, this, [this](const QString &) {
        updateBulkTypeLabel();
    });
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        updateBulkTypeLabel();
    });
    connect(m_table->horizontalHeader(), &QHeaderView::sectionClicked, this, [this](int logicalIndex) {
        m_table->selectColumn(logicalIndex);
    });
    connect(m_bulkTable->horizontalHeader(), &QHeaderView::sectionClicked, this, [this](int logicalIndex) {
        m_bulkTable->selectColumn(logicalIndex);
    });

    auto *copyShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_C), this);
    auto *copyCsvShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), this);
    auto *pasteShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_V), this);
    auto *pasteSelectedShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V), this);
    auto *undoShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z), this);
    auto *redoShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z), this);
    auto *selectAllShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_A), this);
    auto *deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), this);
    auto *escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    auto *findShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this);
    auto *clearFilterShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), this);

    connect(copyShortcut, &QShortcut::activated, this, [this]() {
        if (m_tabWidget->currentIndex() == 0) {
            copySelectedRowsToClipboard(m_table, "\t");
        } else {
            copySelectedRowsToClipboard(m_bulkTable, "\t");
        }
    });
    connect(copyCsvShortcut, &QShortcut::activated, this, [this]() {
        if (m_tabWidget->currentIndex() == 0) {
            copySelectedRowsToClipboard(m_table, ";");
        } else {
            copySelectedRowsToClipboard(m_bulkTable, ";");
        }
    });
    connect(pasteShortcut, &QShortcut::activated, this, [this]() {
        pasteClipboardToBulkTable(false);
    });
    connect(pasteSelectedShortcut, &QShortcut::activated, this, [this]() {
        pasteClipboardToBulkTable(true);
    });
    connect(undoShortcut, &QShortcut::activated, this, [this]() {
        restoreBulkUndoState();
    });
    connect(redoShortcut, &QShortcut::activated, this, [this]() {
        redoBulkUndoState();
    });
    connect(selectAllShortcut, &QShortcut::activated, this, [this]() {
        if (m_tabWidget->currentIndex() == 0) {
            m_table->selectAll();
        } else {
            m_bulkTable->selectAll();
        }
    });
    connect(deleteShortcut, &QShortcut::activated, this, [this]() {
        if (m_tabWidget->currentIndex() == 0) {
            onRemoveSelectedClicked();
        } else {
            if (m_bulkTable->selectedRanges().isEmpty()) {
                return;
            }
            for (const auto &range : m_bulkTable->selectedRanges()) {
                for (int row = range.bottomRow(); row >= range.topRow(); --row) {
                    m_bulkTable->removeRow(row);
                }
            }
        }
    });
    connect(escapeShortcut, &QShortcut::activated, this, [this]() {
        if (m_tabWidget->currentIndex() == 0) {
            m_table->clearSelection();
        } else {
            m_bulkTable->clearSelection();
        }
    });
    connect(findShortcut, &QShortcut::activated, this, [this]() {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    });
    connect(clearFilterShortcut, &QShortcut::activated, this, [this]() {
        m_searchEdit->clear();
        refreshTable();
    });
}

void MainWindow::onAddClicked() {
    QString error;
    if (!validateInput(error)) {
        QMessageBox::critical(this, "Ошибка ввода", error);
        return;
    }

    const QString number = m_numberEdit->text().trimmed();
    const QString index = m_indexEdit->text().trimmed();

    QString type = m_typeCombo->currentData().toString();
    if (type.isEmpty()) {
        type = m_typeCombo->currentText();
    }
    if (type.startsWith("Задвижка")) {
        type = "Valve";
    }

    auto device = DeviceFactory::createDevice(type, number, index, "VLV");
    if (!device) {
        QMessageBox::critical(this, "Ошибка", "Неизвестный тип объекта");
        return;
    }

    if (m_manager.containsTag(device->tag())) {
        QMessageBox::warning(this, "Дубликат", "Такой тег уже существует в списке.");
        return;
    }

    m_manager.addDevice(device);
    m_manager.saveToFile(DeviceManager::storageFilePath());
    refreshTable(m_searchEdit->text());
    refreshBulkTable();

    m_numberEdit->clear();
    m_indexEdit->clear();
    m_numberEdit->setFocus();
}

void MainWindow::onExportClicked() {
    if (m_manager.devices().isEmpty()) {
        QMessageBox::warning(this, "Предупреждение", "Список объектов пуст");
        return;
    }

    const QString fileName = QFileDialog::getSaveFileName(this,
        "Экспорт в CSV", "", "CSV files (*.csv)");
    if (fileName.isEmpty()) {
        return;
    }

    const QString csv = m_manager.exportToCsv();

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось открыть файл для записи");
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream.setGenerateByteOrderMark(true);
    stream << csv;
    file.close();
}

void MainWindow::onClearClicked() {
    m_manager.clear();
    m_manager.saveToFile(DeviceManager::storageFilePath());
    refreshTable(m_searchEdit->text());
    refreshBulkTable();
}

void MainWindow::onRemoveSelectedClicked() {
    const QList<QTableWidgetSelectionRange> ranges = m_table->selectedRanges();
    if (ranges.isEmpty()) {
        QMessageBox::warning(this, "Удаление", "Сначала выберите строку в таблице.");
        return;
    }

    QList<int> indexesToRemove;
    for (const auto& range : ranges) {
        for (int row = range.topRow(); row <= range.bottomRow(); ++row) {
            auto *item = m_table->item(row, 0);
            if (!item) {
                continue;
            }
            const int deviceIndex = item->data(Qt::UserRole).toInt();
            if (!indexesToRemove.contains(deviceIndex)) {
                indexesToRemove.append(deviceIndex);
            }
        }
    }

    std::sort(indexesToRemove.begin(), indexesToRemove.end(), std::greater<int>());
    for (int index : indexesToRemove) {
        m_manager.removeDeviceAt(index);
    }

    m_manager.saveToFile(DeviceManager::storageFilePath());
    refreshTable(m_searchEdit->text());
    refreshBulkTable();
}

void MainWindow::onSearchChanged() {
    refreshTable(m_searchEdit->text());
}

void MainWindow::onBulkRefreshClicked() {
    refreshBulkTable();
}

void MainWindow::onBulkPasteClicked() {
    const QString clipboardText = QGuiApplication::clipboard()->text(QClipboard::Clipboard);
    if (clipboardText.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Буфер обмена", "Буфер обмена пуст. Вставьте список тегов.");
        return;
    }

    saveBulkUndoState();

    const QStringList lines = clipboardText.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        QMessageBox::warning(this, "Буфер обмена", "Не удалось прочитать данные из буфера обмена.");
        return;
    }

    m_bulkImportedTags.clear();
    while (m_bulkTable->rowCount() > 1) {
        m_bulkTable->removeRow(m_bulkTable->rowCount() - 1);
    }
    updateBulkTypeLabel();

    for (const QString &line : lines) {
        QString rawTag = line.trimmed();
        if (rawTag.isEmpty()) {
            continue;
        }

        const QStringList cells = line.split(QRegularExpression("[;\\t,]"), Qt::SkipEmptyParts);
        if (!cells.isEmpty()) {
            rawTag = cells.first().trimmed();
        }
        if (rawTag.isEmpty()) {
            continue;
        }

        QString prefix, numberText, indexText;
        const bool parsed = parseTagParts(rawTag, prefix, numberText, indexText);
        if (!parsed) {
            prefix.clear();
            numberText.clear();
            indexText.clear();
        }

        const int row = m_bulkTable->rowCount();
        m_bulkTable->insertRow(row);

        auto *originalItem = new QTableWidgetItem(rawTag);
        originalItem->setFlags(originalItem->flags() & ~Qt::ItemIsEditable);
        m_bulkTable->setItem(row, 0, originalItem);
        m_bulkTable->setItem(row, 1, new QTableWidgetItem(prefix));
        m_bulkTable->setItem(row, 2, new QTableWidgetItem(numberText));
        m_bulkTable->setItem(row, 3, new QTableWidgetItem(indexText));
        m_bulkTable->setItem(row, 4, new QTableWidgetItem(buildTag(prefix, numberText, indexText)));

        m_bulkImportedTags.append(rawTag);
    }

    if (m_bulkTable->rowCount() <= 1) {
        QMessageBox::warning(this, "Буфер обмена", "Из буфера не удалось выделить корректные теги.");
        return;
    }

    refreshBulkFilterOptions();
}

void MainWindow::onBulkAssembleClicked() {
    if (m_bulkTable->rowCount() <= 1) {
        QMessageBox::warning(this, "Сборка тегов", "Сначала вставьте список тегов из буфера обмена.");
        return;
    }

    saveBulkUndoState();

    for (int row = 1; row < m_bulkTable->rowCount(); ++row) {
        QString prefix = m_bulkTable->item(row, 1) ? m_bulkTable->item(row, 1)->text().trimmed() : QString();
        QString numberText = m_bulkTable->item(row, 2) ? m_bulkTable->item(row, 2)->text().trimmed() : QString();
        QString indexText = m_bulkTable->item(row, 3) ? m_bulkTable->item(row, 3)->text().trimmed() : QString();

        const QString assembled = buildTag(prefix, numberText, indexText);
        QTableWidgetItem *resultItem = m_bulkTable->item(row, 4);
        if (!resultItem) {
            resultItem = new QTableWidgetItem();
            m_bulkTable->setItem(row, 4, resultItem);
        }
        resultItem->setText(assembled);
    }
}

void MainWindow::pasteClipboardToBulkTable(bool selectedOnly) {
    const QString clipboardText = QGuiApplication::clipboard()->text(QClipboard::Clipboard);
    if (clipboardText.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Буфер обмена", "Буфер обмена пуст. Вставьте список тегов.");
        return;
    }

    while (m_bulkTable->rowCount() > 1) {
        m_bulkTable->removeRow(m_bulkTable->rowCount() - 1);
    }
    const QStringList rawLines = clipboardText.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
    if (rawLines.isEmpty()) {
        QMessageBox::warning(this, "Буфер обмена", "Не удалось прочитать данные из буфера обмена.");
        return;
    }

    QVector<QString> parsedTags;
    parsedTags.reserve(rawLines.size());
    for (const QString &line : rawLines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        const QStringList cells = trimmed.split(QRegularExpression("[;\\t,]"), Qt::SkipEmptyParts);
        const QString rawTag = cells.isEmpty() ? trimmed : cells.first().trimmed();
        if (!rawTag.isEmpty()) {
            parsedTags.push_back(rawTag);
        }
    }

    if (parsedTags.isEmpty()) {
        QMessageBox::warning(this, "Буфер обмена", "Не удалось выделить корректные теги из буфера обмена.");
        return;
    }

    saveBulkUndoState();

    if (selectedOnly) {
        const QList<QTableWidgetSelectionRange> ranges = m_bulkTable->selectedRanges();
        if (ranges.isEmpty()) {
            QMessageBox::warning(this, "Вставка", "Сначала выделите строки в таблице, куда нужно вставить данные.");
            return;
        }

        QSet<int> selectedRows;
        for (const auto &range : ranges) {
            for (int row = range.topRow(); row <= range.bottomRow(); ++row) {
                selectedRows.insert(row);
            }
        }

        const QVector<int> rows = selectedRows.values().toVector();
        if (rows.isEmpty()) {
            return;
        }

        const int replacementCount = std::min(static_cast<int>(rows.size()), static_cast<int>(parsedTags.size()));
        for (int i = 0; i < replacementCount; ++i) {
            const int row = rows.at(i);
            const QString rawTag = parsedTags.at(i);
            QString prefix, numberText, indexText;
            parseTagParts(rawTag, prefix, numberText, indexText);

            if (row < 0 || row >= m_bulkTable->rowCount()) {
                continue;
            }

            QTableWidgetItem *originalItem = m_bulkTable->item(row, 0);
            if (!originalItem) {
                originalItem = new QTableWidgetItem();
                m_bulkTable->setItem(row, 0, originalItem);
            }
            originalItem->setText(rawTag);

            QTableWidgetItem *prefixItem = m_bulkTable->item(row, 1);
            if (!prefixItem) {
                prefixItem = new QTableWidgetItem();
                m_bulkTable->setItem(row, 1, prefixItem);
            }
            prefixItem->setText(prefix);

            QTableWidgetItem *numberItem = m_bulkTable->item(row, 2);
            if (!numberItem) {
                numberItem = new QTableWidgetItem();
                m_bulkTable->setItem(row, 2, numberItem);
            }
            numberItem->setText(numberText);

            QTableWidgetItem *indexItem = m_bulkTable->item(row, 3);
            if (!indexItem) {
                indexItem = new QTableWidgetItem();
                m_bulkTable->setItem(row, 3, indexItem);
            }
            indexItem->setText(indexText);

            QTableWidgetItem *resultItem = m_bulkTable->item(row, 4);
            if (!resultItem) {
                resultItem = new QTableWidgetItem();
                m_bulkTable->setItem(row, 4, resultItem);
            }
            resultItem->setText(buildTag(prefix, numberText, indexText));
        }

        m_bulkImportedTags = parsedTags;
        for (int row = 1; row < m_bulkTable->rowCount(); ++row) {
            const QString rawTag = parsedTags.value(row - 1, QString());
            if (rawTag.isEmpty()) {
                continue;
            }
            QString prefix, numberText, indexText;
            parseTagParts(rawTag, prefix, numberText, indexText);
            m_bulkTable->item(row, 0)->setText(rawTag);
            if (m_bulkTable->item(row, 1)) m_bulkTable->item(row, 1)->setText(prefix);
            if (m_bulkTable->item(row, 2)) m_bulkTable->item(row, 2)->setText(numberText);
            if (m_bulkTable->item(row, 3)) m_bulkTable->item(row, 3)->setText(indexText);
            if (m_bulkTable->item(row, 4)) m_bulkTable->item(row, 4)->setText(buildTag(prefix, numberText, indexText));
        }
        refreshBulkFilterOptions();
        return;
    }

    while (m_bulkTable->rowCount() > 1) {
        m_bulkTable->removeRow(m_bulkTable->rowCount() - 1);
    }
    m_bulkImportedTags.clear();
    m_bulkImportedTags.reserve(parsedTags.size());
    for (const QString &rawTag : parsedTags) {
        QString prefix, numberText, indexText;
        parseTagParts(rawTag, prefix, numberText, indexText);

        const int row = m_bulkTable->rowCount();
        m_bulkTable->insertRow(row);

        auto *originalItem = new QTableWidgetItem(rawTag);
        originalItem->setFlags(originalItem->flags() & ~Qt::ItemIsEditable);
        m_bulkTable->setItem(row, 0, originalItem);
        m_bulkTable->setItem(row, 1, new QTableWidgetItem(prefix));
        m_bulkTable->setItem(row, 2, new QTableWidgetItem(numberText));
        m_bulkTable->setItem(row, 3, new QTableWidgetItem(indexText));
        m_bulkTable->setItem(row, 4, new QTableWidgetItem(buildTag(prefix, numberText, indexText)));

        m_bulkImportedTags.append(rawTag);
    }
    refreshBulkFilterOptions();
}

void MainWindow::onBulkExportClicked() {
    if (m_bulkTable->rowCount() <= 1) {
        QMessageBox::warning(this, "Экспорт CSV", "Нет данных для экспорта.");
        return;
    }

    const QString fileName = QFileDialog::getSaveFileName(this,
        "Экспорт тегов в CSV", QString(), "CSV files (*.csv)");
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Экспорт CSV", "Не удалось открыть файл для записи.");
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream.setGenerateByteOrderMark(true);
    stream << "ObjectTag\n";

    for (int row = 1; row < m_bulkTable->rowCount(); ++row) {
        const auto *item = m_bulkTable->item(row, 4);
        if (!item) {
            continue;
        }
        const QString tag = item->text().trimmed();
        if (!tag.isEmpty()) {
            stream << tag << "\n";
        }
    }

    file.close();
    QMessageBox::information(this, "Экспорт CSV", "Теги успешно сохранены в файл.");
}

void MainWindow::onBulkApplyClicked() {
    const QString newPrefix = m_bulkPrefixEdit->text().trimmed();
    const QString newNumberText = m_bulkNumberEdit->text().trimmed();
    const QString newIndex = m_bulkIndexEdit->text().trimmed();

    if (newPrefix.isEmpty() && newNumberText.isEmpty() && newIndex.isEmpty()) {
        QMessageBox::warning(this, "Массовое изменение", "Заполните хотя бы одно поле для замены. Для удаления используйте символ $.");
        return;
    }

    if (m_bulkTable->rowCount() <= 1) {
        QMessageBox::warning(this, "Массовое изменение", "Сначала вставьте список тегов из буфера обмена.");
        return;
    }

    saveBulkUndoState();

    auto applyField = [](QString &current, const QString &replacement) {
        if (replacement == "$") {
            current.clear();
        } else if (!replacement.isEmpty()) {
            current = replacement;
        }
    };

    if (m_bulkImportedTags.isEmpty()) {
        QVector<int> targetIndexes = m_filteredDeviceIndexes;
        if (targetIndexes.isEmpty()) {
            for (int i = 0; i < m_manager.devices().size(); ++i) {
                targetIndexes.append(i);
            }
        }

        for (int index : targetIndexes) {
            auto device = m_manager.devices().at(index);
            if (!device || device->type() != "Valve") {
                continue;
            }

            auto valve = std::dynamic_pointer_cast<Valve>(device);
            if (!valve) {
                continue;
            }

            QString prefix, numberText, indexText;
            parseTagParts(device->tag(), prefix, numberText, indexText);

            applyField(prefix, newPrefix);
            applyField(numberText, newNumberText);
            applyField(indexText, newIndex);

            valve->setPrefix(prefix);
            valve->setNumber(numberText);
            valve->setIndex(indexText);
        }

        m_manager.saveToFile(DeviceManager::storageFilePath());
        refreshTable(m_searchEdit->text());
        refreshBulkTable();
    } else {
        for (int row = 1; row < m_bulkTable->rowCount(); ++row) {
            QString prefix = m_bulkTable->item(row, 1) ? m_bulkTable->item(row, 1)->text().trimmed() : QString();
            QString numberText = m_bulkTable->item(row, 2) ? m_bulkTable->item(row, 2)->text().trimmed() : QString();
            QString indexText = m_bulkTable->item(row, 3) ? m_bulkTable->item(row, 3)->text().trimmed() : QString();

            applyField(prefix, newPrefix);
            applyField(numberText, newNumberText);
            applyField(indexText, newIndex);

            QTableWidgetItem *prefixItem = m_bulkTable->item(row, 1);
            if (!prefixItem) {
                prefixItem = new QTableWidgetItem();
                m_bulkTable->setItem(row, 1, prefixItem);
            }
            prefixItem->setText(prefix);

            QTableWidgetItem *numberItem = m_bulkTable->item(row, 2);
            if (!numberItem) {
                numberItem = new QTableWidgetItem();
                m_bulkTable->setItem(row, 2, numberItem);
            }
            numberItem->setText(numberText);

            QTableWidgetItem *indexItem = m_bulkTable->item(row, 3);
            if (!indexItem) {
                indexItem = new QTableWidgetItem();
                m_bulkTable->setItem(row, 3, indexItem);
            }
            indexItem->setText(indexText);

            QTableWidgetItem *resultItem = m_bulkTable->item(row, 4);
            if (!resultItem) {
                resultItem = new QTableWidgetItem();
                m_bulkTable->setItem(row, 4, resultItem);
            }
            resultItem->setText(buildTag(prefix, numberText, indexText));
        }
    }

    refreshBulkFilterOptions();
    m_bulkPrefixEdit->clear();
    m_bulkNumberEdit->clear();
    m_bulkIndexEdit->clear();
}

void MainWindow::onBulkAddSelectedClicked() {
    const QString newPrefix = m_bulkPrefixEdit->text().trimmed();
    const QString newNumberText = m_bulkNumberEdit->text().trimmed();
    const QString newIndex = m_bulkIndexEdit->text().trimmed();

    if (newPrefix.isEmpty() && newNumberText.isEmpty() && newIndex.isEmpty()) {
        QMessageBox::warning(this, "Массовое изменение", "Заполните хотя бы одно поле для замены. Для удаления используйте символ $.");
        return;
    }

    if (m_bulkTable->rowCount() <= 1) {
        QMessageBox::warning(this, "Массовое изменение", "Сначала вставьте список тегов из буфера обмена.");
        return;
    }

    const QList<QTableWidgetSelectionRange> ranges = m_bulkTable->selectedRanges();
    if (ranges.isEmpty()) {
        QMessageBox::warning(this, "Массовое изменение", "Сначала выделите строки, к которым нужно применить изменения.");
        return;
    }

    saveBulkUndoState();

    auto applyField = [](QString &current, const QString &replacement) {
        if (replacement == "$") {
            current.clear();
        } else if (!replacement.isEmpty()) {
            current = replacement;
        }
    };

    QSet<int> selectedRows;
    for (const auto &range : ranges) {
        for (int row = range.topRow(); row <= range.bottomRow(); ++row) {
            selectedRows.insert(row);
        }
    }

    for (int row : selectedRows.values()) {
        QString prefix = m_bulkTable->item(row, 1) ? m_bulkTable->item(row, 1)->text().trimmed() : QString();
        QString numberText = m_bulkTable->item(row, 2) ? m_bulkTable->item(row, 2)->text().trimmed() : QString();
        QString indexText = m_bulkTable->item(row, 3) ? m_bulkTable->item(row, 3)->text().trimmed() : QString();

        applyField(prefix, newPrefix);
        applyField(numberText, newNumberText);
        applyField(indexText, newIndex);

        QTableWidgetItem *prefixItem = m_bulkTable->item(row, 1);
        if (!prefixItem) {
            prefixItem = new QTableWidgetItem();
            m_bulkTable->setItem(row, 1, prefixItem);
        }
        prefixItem->setText(prefix);

        QTableWidgetItem *numberItem = m_bulkTable->item(row, 2);
        if (!numberItem) {
            numberItem = new QTableWidgetItem();
            m_bulkTable->setItem(row, 2, numberItem);
        }
        numberItem->setText(numberText);

        QTableWidgetItem *indexItem = m_bulkTable->item(row, 3);
        if (!indexItem) {
            indexItem = new QTableWidgetItem();
            m_bulkTable->setItem(row, 3, indexItem);
        }
        indexItem->setText(indexText);

        QTableWidgetItem *resultItem = m_bulkTable->item(row, 4);
        if (!resultItem) {
            resultItem = new QTableWidgetItem();
            m_bulkTable->setItem(row, 4, resultItem);
        }
        resultItem->setText(buildTag(prefix, numberText, indexText));
    }

    refreshBulkFilterOptions();
    m_bulkPrefixEdit->clear();
    m_bulkNumberEdit->clear();
    m_bulkIndexEdit->clear();
}

void MainWindow::refreshBulkTable() {
    m_bulkTable->setRowCount(1);
    updateBulkTypeLabel();

    if (!m_bulkImportedTags.isEmpty()) {
        for (int i = 0; i < m_bulkImportedTags.size(); ++i) {
            const QString tag = m_bulkImportedTags.at(i);
            QString prefix, numberText, indexText;
            const bool parsed = parseTagParts(tag, prefix, numberText, indexText);
            const QString resultTag = parsed ? buildTag(prefix, numberText, indexText) : tag;

            const int row = m_bulkTable->rowCount();
            m_bulkTable->insertRow(row);
            m_bulkTable->setItem(row, 0, new QTableWidgetItem(tag));
            m_bulkTable->setItem(row, 1, new QTableWidgetItem(prefix));
            m_bulkTable->setItem(row, 2, new QTableWidgetItem(numberText));
            m_bulkTable->setItem(row, 3, new QTableWidgetItem(indexText));
            m_bulkTable->setItem(row, 4, new QTableWidgetItem(resultTag));
        }
    }

    refreshColumnFilterOptions(m_bulkTable, m_bulkColumnFilters);
    applyTableFilters(m_bulkTable, m_bulkColumnFilters);
}

void MainWindow::refreshBulkFilterOptions() {
    refreshColumnFilterOptions(m_bulkTable, m_bulkColumnFilters);
    applyTableFilters(m_bulkTable, m_bulkColumnFilters);
}

void MainWindow::updateBulkTypeLabel() {
    QString typeText = "Задвижка";
    const QString currentText = m_typeCombo->currentText();
    if (!currentText.isEmpty()) {
        typeText = currentText;
        if (typeText.contains("Valve", Qt::CaseInsensitive)) {
            typeText = "Задвижка";
        }
    }
    m_bulkTypeLabel->setText("Тип объекта: " + typeText);
}

void MainWindow::saveBulkUndoState() {
    QVector<QString> snapshot = m_bulkImportedTags;
    if (!m_bulkUndoHistory.isEmpty() && m_bulkUndoHistory.last() == snapshot) {
        return;
    }
    m_bulkUndoHistory.append(snapshot);
    if (m_bulkUndoHistory.size() > 20) {
        m_bulkUndoHistory.removeFirst();
    }
    m_bulkRedoHistory.clear();
}

void MainWindow::restoreBulkUndoState() {
    if (m_bulkUndoHistory.isEmpty()) {
        return;
    }

    m_bulkRedoHistory.append(m_bulkImportedTags);
    m_bulkImportedTags = m_bulkUndoHistory.last();
    m_bulkUndoHistory.removeLast();
    refreshBulkTable();
}

void MainWindow::redoBulkUndoState() {
    if (m_bulkRedoHistory.isEmpty()) {
        return;
    }

    m_bulkUndoHistory.append(m_bulkImportedTags);
    m_bulkImportedTags = m_bulkRedoHistory.last();
    m_bulkRedoHistory.removeLast();
    refreshBulkTable();
}

void MainWindow::copySelectedRowsToClipboard(QTableWidget *table, const QString &separator) {
    if (!table || table->selectedRanges().isEmpty()) {
        return;
    }

    QSet<int> rows;
    for (const auto &range : table->selectedRanges()) {
        for (int row = range.topRow(); row <= range.bottomRow(); ++row) {
            rows.insert(row);
        }
    }

    if (rows.isEmpty()) {
        return;
    }

    QStringList lines;
    for (int row : rows.values()) {
        QStringList cells;
        for (int col = 0; col < table->columnCount(); ++col) {
            auto *item = table->item(row, col);
            cells << (item ? item->text() : QString());
        }
        lines << cells.join(separator);
    }

    QGuiApplication::clipboard()->setText(lines.join("\n"));
}

void MainWindow::refreshTable(const QString &filter) {
    m_table->setRowCount(1);
    m_filteredDeviceIndexes.clear();
    const auto& devices = m_manager.devices();
    const QString trimmedFilter = filter.trimmed();

    auto currentHeaderFilter = [&](int col) {
        if (col < 0 || col >= m_tableColumnFilters.size()) {
            return QString();
        }
        return m_tableColumnFilters.at(col)->currentText().trimmed();
    };

    for (int i = 0; i < devices.size(); ++i) {
        const auto& device = devices.at(i);
        const QString combined = (device->tag() + " " + device->type() + " " + device->description()).toLower();

        if (!trimmedFilter.isEmpty() && !combined.contains(trimmedFilter.toLower())) {
            continue;
        }

        const QString tagValue = currentHeaderFilter(0);
        if (!tagValue.isEmpty() && tagValue != "Все" && device->tag() != tagValue) {
            continue;
        }
        const QString typeValue = currentHeaderFilter(1);
        if (!typeValue.isEmpty() && typeValue != "Все" && device->type() != typeValue) {
            continue;
        }
        const QString descriptionValue = currentHeaderFilter(2);
        if (!descriptionValue.isEmpty() && descriptionValue != "Все" && device->description() != descriptionValue) {
            continue;
        }

        m_filteredDeviceIndexes.append(i);

        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto *tagItem = new QTableWidgetItem(device->tag());
        tagItem->setData(Qt::UserRole, i);
        m_table->setItem(row, 0, tagItem);
        m_table->setItem(row, 1, new QTableWidgetItem(device->type()));
        m_table->setItem(row, 2, new QTableWidgetItem(device->description()));
    }

    refreshColumnFilterOptions(m_table, m_tableColumnFilters);
}

bool MainWindow::validateInput(QString &errorMessage) {
    const QString numberStr = m_numberEdit->text().trimmed();
    if (numberStr.isEmpty()) {
        errorMessage = "Номер объекта не может быть пустым";
        return false;
    }

    const QRegularExpression numberRegExp(QStringLiteral("^[A-Za-z0-9_-]+$"));
    if (!numberRegExp.match(numberStr).hasMatch()) {
        errorMessage = "Номер содержит недопустимые символы. Разрешены: A-Z, a-z, 0-9, -, _";
        return false;
    }

    const QString index = m_indexEdit->text().trimmed();
    if (!index.isEmpty()) {
        if (index.length() > 16) {
            errorMessage = "Индекс содержит недопустимые символы. Разрешены: A-Z, a-z, 0-9, -, _";
            return false;
        }
        const QRegularExpression regExp(QStringLiteral("^[A-Za-z0-9_-]+$"));
        if (!regExp.match(index).hasMatch()) {
            errorMessage = "Индекс содержит недопустимые символы. Разрешены: A-Z, a-z, 0-9, -, _";
            return false;
        }
    }

    return true;
}