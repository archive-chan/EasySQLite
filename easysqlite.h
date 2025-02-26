#ifndef EASYSQLITE_H
#define EASYSQLITE_H

#include <QObject>
#include <QSqlTableModel>

typedef struct EasySQLiteConfig{
private:
    QString m_databasePath="./appdata.db";
    QList<QPair<QString,QString>> tables;
    QList<QPair<QString,QString>> records;

public:
    void setDatabasePath(const QString& path){
        m_databasePath = path;
    }

    QString databasePath(){
        return m_databasePath;
    }

    void newTable(const QString &tableName, const QString &definition) {
        tables.append(qMakePair(tableName, definition));
    }

    void newRecord(const QString &tableName, const QString &recordValues) {
        records.append(qMakePair(tableName, recordValues));
    }

    int tableNum(){
        return tables.size();
    }

    int recordNum(){
        return records.size();
    }

    QString createTablename(int tableIndex){
        return tables.at(tableIndex).first;
    }

    QString createDefinition(int tableIndex){
        return tables.at(tableIndex).second;
    }

    QString insertTablename(int recordIndex){
        return records.at(recordIndex).first;
    }

    QString insertRecordvalues(int recordIndex){
        return records.at(recordIndex).second;
    }
}ESConfig;

class EasySQLite : public QObject{
    Q_OBJECT

public:
    explicit EasySQLite(QObject *parent = nullptr);
    ~EasySQLite();

private:
    QSqlDatabase m_database;
    QString m_errorInfo;
    QSqlTableModel* m_tableModel;


    bool databaseOpen();
    void databaseClose();
    bool tableCreate(const QString& tableName, const QString& definition);
    bool tableInsert(const QString& tableName, const QString& recordValues);
    bool tablePrint(const QString& tableName);

    QString value2SqlFormat(const QVariant& value);
    QString values2SqlFormat(const QVariantList &values);
    QStringList valuesList2SqlFormat(const QList<QVariantList>& valuesList);
    QString primarykeyName(const QString& tableName);
    bool isFieldNameMatch(const QString& tableName, const QString& fieldName, bool& isMatch);
    bool isFieldValueMatch(const QString& tableName, const QString& fieldName,
                           const QVariant& fieldValue, bool& isMatch);

public:
    enum class Condition{
        Equal,
        NotEqual,
        Greater,
        GreaterEqual,
        Less,
        LessEqual,
        Between,
        In,
        And,
        Or,
        LikeStart,
        LikeEnd,
        NotLikeStart,
        NotLikeEnd
    };

    enum class SortPolicy{
        ASC,
        DESC
    };

    bool databaseInit(ESConfig *config= nullptr);
    bool recordInsert(const QString& tableName, const QVariantList& values);
    bool recordsInsert(const QString& tableName, const QList<QVariantList>& valuesList);
    bool recordDelete(const QString& tableName, const QVariant& primarykeyValue);
    bool recordsDelete(const QString& tableName, const QVariantList& primarykeyValueList);
    bool recordSelectTableAll(const QString& tableName);
    bool recordSelect(const QString& tableName, const QStringList& fieldNameList,const QString& condition,
                      const QString& sortFieldName, const SortPolicy& sortPolicy);
    bool fieldUpdateValue(const QString& tableName, const QString& fieldName, const QVariant& fieldValue,
                     const QString& condiFieldName, const QVariant& condiFieldValue);
    bool fieldUpdate(const QString& tableName, const QString& fieldName,
                     const QVariant& fieldValue,const QString& condition);


    QString errorInfo();
    QSqlTableModel* tableModel();
    QVariant value(const QString& tableName, const QVariant& primarykeyValue, const QString& fieldName);
    bool isValueExist(const QString& tableName, const QVariant& primarykeyValue,const QString& fieldName,const QVariant& inputValue);
    QString singleConditionCreate(const QString& tableName,const Condition& condition,
                                  const QString& condiFieldName, const QVariant& condiFieldValue);
    QString singleConditionCreate(const QString& tableName,const Condition& condition,
                                  const QString& condiFieldName, const QVariantList& condiFieldValueList);
    QString mutiConditionCreate(const Condition& condition,const QStringList& conditionList);


};

#endif // EASYSQLITE_H
