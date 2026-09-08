#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include "DeviceManager.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onAddClicked();
    void onExportClicked();
    void onClearClicked();
    void onRemoveSelectedClicked();
    void onSearchChanged();
    void onBulkApplyClicked();
    void onBulkAddSelectedClicked();
    void onBulkRefreshClicked();
    void onBulkPasteClicked();
    void onBulkAssembleClicked();
    void onBulkExportClicked();

private:
    void setupUi();
    void refreshTable(const QString &filter = QString());
    void refreshBulkTable();
    void refreshBulkFilterOptions();
    void updateBulkTypeLabel();
    QComboBox *createFilterComboBox();
    void setupTableFilterRow(QTableWidget *table, QVector<QComboBox*> &filters);
    void refreshColumnFilterOptions(QTableWidget *table, QVector<QComboBox*> &filters);
    void applyTableFilters(QTableWidget *table, QVector<QComboBox*> &filters);
    void copySelectedRowsToClipboard(QTableWidget *table, const QString &separator = "\t");
    void saveBulkUndoState();
    void restoreBulkUndoState();
    void redoBulkUndoState();
    void pasteClipboardToBulkTable(bool selectedOnly = false);
    bool validateInput(QString &errorMessage);
    static bool parseTagParts(const QString &tag, QString &prefix, QString &number, QString &index);
    static QString buildTag(const QString &prefix, const QString &number, const QString &index);

    QTabWidget *m_tabWidget;
    QComboBox *m_typeCombo;
    QLineEdit *m_numberEdit;
    QLineEdit *m_indexEdit;
    QLineEdit *m_searchEdit;
    QLineEdit *m_bulkPrefixEdit;
    QLineEdit *m_bulkNumberEdit;
    QLineEdit *m_bulkIndexEdit;
    QLabel *m_bulkTypeLabel;
    QPushButton *m_addButton;
    QPushButton *m_exportButton;
    QPushButton *m_clearButton;
    QPushButton *m_removeButton;
    QPushButton *m_bulkApplyButton;
    QPushButton *m_bulkAddSelectedButton;
    QPushButton *m_bulkRefreshButton;
    QPushButton *m_bulkPasteButton;
    QPushButton *m_bulkAssembleButton;
    QPushButton *m_bulkExportButton;
    QTableWidget *m_table;
    QTableWidget *m_bulkTable;
    QVector<QComboBox*> m_tableColumnFilters;
    QVector<QComboBox*> m_bulkColumnFilters;
    QVector<QString> m_bulkImportedTags;
    QVector<int> m_filteredDeviceIndexes;
    QVector<QVector<QString>> m_bulkUndoHistory;
    QVector<QVector<QString>> m_bulkRedoHistory;

    DeviceManager m_manager;
};

#endif // MAINWINDOW_H