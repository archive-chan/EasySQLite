#include "easysqlite.h"
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStringList>

EasySQLite::EasySQLite(QObject *parent):QObject{parent}{}

EasySQLite::~EasySQLite(){
    delete m_tableModel;
    databaseClose();
}

/*
 *  @brief  初始化数据库
 *  @param  初始化配置结构体
 *  @retval 是否成功初始化
 */
bool EasySQLite::databaseInit(ESConfig *config){

    //根据配置结构体是否为空来决定如何初始化数据库连接
    if(config== nullptr){
        //直接使用创建过的默认连接
        if(QSqlDatabase::contains("qt_sql_default_connection")){
            //已找到创建过的默认连接
            m_database = QSqlDatabase::database("qt_sql_default_connection");
        }else{
            //未找到默认连接
            m_errorInfo = "[EasySQLite/Error]数据库初始化报错: 连接时未找到默认连接";
            return false;
        }
    }else{
        //通过结构体来创建连接
        if(!QSqlDatabase::contains("qt_sql_default_connection")){
            //未创建过默认连接
            m_database = QSqlDatabase::addDatabase("QSQLITE");
            m_database.setDatabaseName(config->databasePath());
        }else{
            //默认连接已存在
            m_errorInfo = "[EasySQLite/Error]数据库初始化报错: 默认连接已存在";
            return false;
        }
    }

    //已连接 打开本地数据库
    if(!databaseOpen()){
        //打开失败
        m_errorInfo = "[EasySQLite/Error]数据库初始化报错: 数据库打开失败";
        return false;
    }

    //打开成功 查询数据库中表格数量
    int tableNum;
    //法1 直接用tables()
    QStringList tables=m_database.tables();
    tableNum=tables.size();

    // //法2 使用命令查询表格数量
    // QSqlQuery query;
    // if(!query.exec(QString("SELECT count(*) FROM sqlite_master WHERE type = 'table'"))){
    //     //查询失败
    //     m_errorInfo = "[EasySQLite/Error]数据库初始化报错: 执行SQL语句查询表格数量错误" + query.lastError().text();
    //     return false;
    // }
    // //查询成功 获取结果
    // query.next();
    // tableNum=query.value(0).toInt();

    if(!tableNum){
        //表格数量为0 重新建表 插入默认数据
        //根据配置结构体建表
        for (int tableIndex = 0; tableIndex < config->tableNum(); ++tableIndex){
            QString tableName = config->createTablename(tableIndex);
            QString definition = config->createDefinition(tableIndex);
            if(!tableCreate(tableName,definition)){
                //建表失败
                m_errorInfo = "[EasySQLite/Error]数据库初始化报错: 表格创建失败";
                return false;
            }
        }

        //建表成功 根据配置结构体插入数据
        for (int recordIndex = 0; recordIndex < config->recordNum(); ++recordIndex) {
            QString tableName = config->insertTablename(recordIndex);
            QString recordValues = config->insertRecordvalues(recordIndex);
            if(!tableInsert(tableName,recordValues)){
                //插入失败
                m_errorInfo = "[EasySQLite/Error]数据库初始化报错: 数据插入失败";
                return false;
            }
        }

        //插入成功 用配置结构体的数据打印表格
        for (int tableIndex = 0; tableIndex < config->tableNum(); ++tableIndex) {
            QString tableName = config->createTablename(tableIndex);
            if(!tablePrint(tableName)){
                //打印失败
                m_errorInfo = "[EasySQLite/Error]数据库初始化报错: 表格打印失败";
                return false;
            }
        }
    }else{
        //表格数量不为0 打印所有表格
        //查询获取每个索引对应的表名再打印
        //法1 直接用tables()
        for (int tableIndex = 0; tableIndex < tableNum; ++tableIndex) {
            if(!tablePrint(tables.at(tableIndex))){
                //打印失败
                m_errorInfo = "[EasySQLite/Error]数据库初始化报错: 表格打印失败";
                return false;
            }
        }

        // //法2 使用命令查询表名
        // QSqlQuery query;
        // if(!query.exec(QString("SELECT name FROM sqlite_master WHERE type = 'table'"))){
        //     //查询失败
        //     m_errorInfo = "[EasySQLite/Error]数据库初始化报错: 执行SQL语句查询表名错误" + query.lastError().text();
        //     return false;
        // }
        // //查询成功 遍历打印表格
        // for (int tableIndex = 0; tableIndex < tableNum; ++tableIndex) {
        //     query.next();
        //     if(!TablePrint(query.value(0).toString())){
        //         //打印失败
        //         m_errorInfo = "[EasySQLite/Error]数据库初始化报错: 表格打印失败";
        //         return false;
        //     }
        // }
    }
    //打印成功 关闭数据库
    databaseClose();
    //初始化成功
    return true;
}

/*
 *  @brief  打开数据库
 *  @param  无
 *  @retval 是否成功打开数据库
 */
bool EasySQLite::databaseOpen(){
    if(!m_database.open()){
        //打开失败
        m_errorInfo = "[EasySQLite/Error]数据库打开报错: " + m_database.lastError().text();
        return false;
    }
    //打开成功
    return true;
}

/*
 *  @brief  关闭数据库
 *  @param  无
 *  @retval 无
 */
void EasySQLite::databaseClose(){
    m_database.close();
}

/*
 *  @brief  创建表格
 *  @param  表格名
 *  @param  创建表格的字段参数
 *  @retval 是否建表成功
 */
bool EasySQLite::tableCreate(const QString& tableName,const QString& definition){
    //默认数据库已打开 判断表格是否存在
    if (m_database.tables().contains(tableName)) {
        m_errorInfo = "[EasySQLite/Error]表格创建报错: 表格已存在, 无法重复建表";
        return false;
    }

    //表格不存在 可以建表 判断是否指定主键
    if(!definition.contains("PRIMARY KEY", Qt::CaseInsensitive)){
        //创建时未指定主键
        m_errorInfo = "[EasySQLite/Error]表格创建报错: 未指定主键";
        return false;
    }

    //已指定主键 开始建表
    QSqlQuery query;
    if(!query.exec(QString("CREATE TABLE %1(%2)").arg(tableName).arg(definition))){
        //建表失败
        m_errorInfo = "[EasySQLite/Error]表格创建报错: 执行SQL语句建表错误" + query.lastError().text();
        return false;
    }

    //建表成功
    return true;
}

bool EasySQLite::tableInsert(const QString &tableName, const QString &recordValues){
    //默认数据库已打开 判断表格是否存在
    if (!m_database.tables().contains(tableName)) {
        m_errorInfo = "[EasySQLite/Error]表格插入报错: 表格不存在, 无法插入数据";
        return false;
    }

    //表格存在 开始插入数据
    QSqlQuery query;
    if(!query.exec(QString("INSERT INTO %1 VALUES(%2)").arg(tableName).arg(recordValues))){
        //插入失败
        m_errorInfo = "[EasySQLite/Error]表格插入报错: 执行SQL语句插入数据错误" + query.lastError().text();
        return false;
    }

    //插入成功
    return true;
}

/*
 *  @brief  select * from 表格名
 *  @param  表格名
 *  @retval 是否打印成功
 */
bool EasySQLite::tablePrint(const QString& tableName){
    //默认数据库已打开 判断表格是否存在
    if (!m_database.tables().contains(tableName)) {
        m_errorInfo = "[EasySQLite/Error]表格打印报错: 表格不存在";
        return false;
    }

    //表格存在 开始打印
    qDebug().noquote()<<"[EasySQLite/Info]数据库打印begin";

    //打印表格名
    qDebug().noquote()<<QString("-------------- %1 --------------").arg(tableName);

    //查询各字段的名称
    QSqlQuery query;
    if(!query.exec(QString("PRAGMA table_info(%1)").arg(tableName))){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]表格打印报错: 执行SQL语句查询表头错误" + query.lastError().text();
        return false;
    }

    //打印表头
    QString heading;
    while(query.next()){
        heading+=(query.value(1).toString()+"\t");
    }
    heading.removeLast();
    qDebug().noquote()<<heading;

    //打印表头分界线
    qDebug().noquote()<<"-------------------------------------";

    //查询数据
    if(!query.exec(QString("SELECT * FROM %1").arg(tableName))){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]表格打印报错: 执行SQL语句查询数据错误" + query.lastError().text();
        return false;
    }

    //获取一条记录里的字段数量
    QSqlRecord record = query.record();
    int fieldNum = record.count();

    //打印数据
    QString str;
    while(query.next()){
        for (int fieldIndex = 0; fieldIndex < fieldNum; fieldIndex++) {
            str += (query.value(fieldIndex).toString()+"\t");
        }
        str.removeLast();
        qDebug().noquote()<<str;
        str="";
    }

    //打印表尾
    qDebug().noquote()<<"-------------------------------------";
    qDebug().noquote()<<"[EasySQLite/Info]数据库打印end\n";

    //打印成功
    return true;
}





/*
 *  @brief  对单个数值转换为SQL格式
 *  @param  数值 例: test233 38.9 250
 *  @retval SQL格式的数值字符串 例: "'test233'" "38.9" "250"
 */
QString EasySQLite::value2SqlFormat(const QVariant &value){
    //为空或未初始化
    if(value.isNull()||!value.isValid()){
        return "";
    }

    //转换为字符串
    QString strValue = value.toString();

    //检查是否为数字（整数或小数）
    bool isNumber = false;
    strValue.toDouble(&isNumber);

    if (isNumber) {
        //数字直接返回字符串形式
        return strValue;
    } else {
        //非数字加上单引号
        return QString("'%1'").arg(strValue);
    }
}

/*
 *  @brief  对数值容器逐个转换为SQL格式并用逗号拼接
 *  @param  数值容器 例. {test, 1, 28.899}
 *  @retval SQL格式的拼接s字符串 例. "'test', 1, 28.899"
 */
QString EasySQLite::values2SqlFormat(const QVariantList &values){
    QString ret;
    for (int i = 0; i < values.size(); ++i) {
        ret += (value2SqlFormat(values.at(i))+",");
    }
    ret.removeLast();
    return ret;
}



/*
 *  @brief  对多个数值容器逐个转换为SQL格式并用逗号拼接
 *  @param  数值容器的容器 例. {{test, 1, 28.899}, {test2, 9, 59.899}}
 *  @retval SQL格式的拼接s字符串 例. ->  {"'test', 1, 28.899", "'test2', 9, 59.899"}
 */
QStringList EasySQLite::valuesList2SqlFormat(const QList<QList<QVariant>>& valuesList){
    QStringList ret;
    for (int i = 0; i < valuesList.size(); ++i) {
        QString strValues = values2SqlFormat(valuesList.at(i));
        ret.append(strValues);
    }
    return ret;
}


QString EasySQLite::singleConditionCreate(const QString& tableName,const Condition &condition, const QString &condiFieldName, const QVariant &condiFieldValue){
    //检查数据库是否打开
    if(!m_database.isOpen()){
        //未打开 执行打开数据库
        if(!databaseOpen()){
            //打开失败
            m_errorInfo = "[EasySQLite/Error]单个条件创建报错: 数据库打开失败";
            return "";
        }
    }

    //数据库已打开 判断表格是否存在
    if (!m_database.tables().contains(tableName)) {
        //表格不存在
        m_errorInfo = "[EasySQLite/Error]单个条件创建报错: 表格不存在";
        return "";
    }

    //判断字段名是否存在
    bool isExist = false;
    if(!isFieldNameMatch(tableName,condiFieldName,isExist)){
        //查询错误
        m_errorInfo = "[EasySQLite/Error]单个条件创建报错: 字段名查询错误";
        return "";
    }
    //查询成功 判断
    if(!isExist){
        //不存在
        m_errorInfo = "[EasySQLite/Error]单个条件创建报错: 字段名不存在";
        return "";
    }

    //字段名存在 开始创建
    QString ret;
    switch (condition) {
    case Condition::Equal://=
        ret = condiFieldName + " = "+ value2SqlFormat(condiFieldValue);
        break;
    case Condition:: NotEqual://!=
        ret = condiFieldName + " != "+ value2SqlFormat(condiFieldValue);
        break;
    case Condition::Greater://>
        ret = condiFieldName + " > "+ value2SqlFormat(condiFieldValue);
        break;
    case Condition::GreaterEqual://>=
        ret = condiFieldName + " >= "+ value2SqlFormat(condiFieldValue);
        break;
    case Condition::Less://<
        ret = condiFieldName + " < "+ value2SqlFormat(condiFieldValue);
        break;
    case Condition::LessEqual://<=
        ret = condiFieldName + " <= "+ value2SqlFormat(condiFieldValue);
        break;
    case Condition::LikeStart://like 'v%'
        ret = condiFieldName + " like '"+condiFieldValue.toString()+"%'";
        break;
    case Condition::LikeEnd://like '%v'
        ret = condiFieldName + " like '%"+condiFieldValue.toString()+"'";
        break;
    case Condition::NotLikeStart://not like 'v%'
        ret = condiFieldName + " not like '"+condiFieldValue.toString()+"%'";
        break;
    case Condition::NotLikeEnd://not like '%v'
        ret = condiFieldName + " not like '%"+condiFieldValue.toString()+"'";
        break;
    }

    //创建完成 关闭数据库
    databaseClose();
    return ret;
}

QString EasySQLite::singleConditionCreate(const QString& tableName,const Condition &condition, const QString &condiFieldName, const QVariantList &condiFieldValueList){
    //检查数据库是否打开
    if(!m_database.isOpen()){
        //未打开 执行打开数据库
        if(!databaseOpen()){
            //打开失败
            m_errorInfo = "[EasySQLite/Error]单个条件创建报错: 数据库打开失败";
            return "";
        }
    }

    //数据库已打开 判断表格是否存在
    if (!m_database.tables().contains(tableName)) {
        //表格不存在
        m_errorInfo = "[EasySQLite/Error]单个条件创建报错: 表格不存在";
        return "";
    }

    //判断字段名是否存在
    bool isExist = false;
    if(!isFieldNameMatch(tableName,condiFieldName,isExist)){
        //查询错误
        m_errorInfo = "[EasySQLite/Error]单个条件创建报错: 字段名查询错误";
        return "";
    }
    //查询成功 判断
    if(!isExist){
        //不存在
        m_errorInfo = "[EasySQLite/Error]单个条件创建报错: 字段名不存在";
        return "";
    }

    //字段名存在 开始创建
    QString ret;
    switch (condition) {
    case Condition::Between:// v1 ~ v2
        ret = condiFieldName + " between "+value2SqlFormat(condiFieldValueList.at(0))+" and "+value2SqlFormat(condiFieldValueList.at(1));
        break;
    case Condition::In://in v1 v2 v3...
        ret = condiFieldName+" in (";
        for (int var = 0; var < condiFieldValueList.size(); ++var) {
            ret += (value2SqlFormat(condiFieldValueList.at(var))+",");
        }
        ret.removeLast();
        ret += ")";
        break;
    }
    //创建完成 关闭数据库
    databaseClose();
    return ret;
}

QString EasySQLite::mutiConditionCreate(const Condition& condition,const QStringList& conditionList){
    QString ret;
    switch (condition) {
    case Condition::And://and
        for (int var = 0; var < conditionList.size(); ++var) {
            ret += (conditionList.at(var));
            if(var!=(conditionList.size()-1)){
                ret += " and ";
            }
        }
        break;
    case Condition::Or://or
        for (int var = 0; var < conditionList.size(); ++var) {
            ret += (conditionList.at(var));
            if(var!=(conditionList.size()-1)){
                ret += " or ";
            }
        }
        break;
    }
    return ret;
}


QString EasySQLite::primarykeyName(const QString &tableName){
    //默认数据库已打开 判断表格是否存在
    if (!m_database.tables().contains(tableName)) {
        m_errorInfo = "[EasySQLite/Error]查询主键名报错: 表格不存在";
        return "";
    }

    //查询主键名
    QSqlQuery query;
    if(!query.exec(QString("PRAGMA TABLE_INFO(%1)").arg(tableName))){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]查询主键名报错: 执行SQL语句查询主键错误" + query.lastError().text();
        return "";
    }

    //查询成功 获取主键名
    QString primarykeyName;
    while(query.next()){
        if(query.value(5).toInt()){
            primarykeyName= query.value(1).toString();
        }
    }

    //获取成功
    return primarykeyName;
}

bool EasySQLite::isFieldNameMatch(const QString& tableName, const QString& fieldName, bool& isMatch){
    //默认数据库已打开 判断表格是否存在
    if (!m_database.tables().contains(tableName)) {
        m_errorInfo = "[EasySQLite/Error]查询字段名是否存在报错: 表格不存在";
        return false;
    }

    //表格存在 开始查询
    QSqlQuery query;
    if(!query.exec(QString("PRAGMA TABLE_INFO(%1)").arg(tableName))){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]查询字段名是否存在报错: 执行SQL语句查询表格信息错误";
        return false;
    }

    //查询成功
    isMatch = false;
    while(query.next()){
        if(query.value(1).toString()==fieldName){
            isMatch = true;
        }
    }
    //执行成功
    return true;
}

bool EasySQLite::isFieldValueMatch(const QString &tableName, const QString &fieldName, const QVariant &fieldValue, bool& isMatch){
    //默认数据库已打开 判断表格是否存在
    if (!m_database.tables().contains(tableName)) {
        m_errorInfo = "[EasySQLite/Error]查询字段值是否存在报错: 表格不存在";
        return false;
    }

    //判断是否存在字段名
    bool isFieldNameExixt=false;
    if(!isFieldNameMatch(tableName,fieldName,isFieldNameExixt)){
        //执行失败
        m_errorInfo = "[EasySQLite/Error]查询字段值是否存在报错: 查询字段名是否存在错误";
        return false;
    }

    //执行成功
    if(!isFieldNameExixt){
        //字段名不存在
        m_errorInfo = "[EasySQLite/Error]查询字段值是否存在报错: 字段名不存在";
        return false;
    }

    //字段名存在
    QSqlQuery query;
    if(!query.exec(QString("SELECT %1 FROM %2"))){
        m_errorInfo = "[EasySQLite/Error]查询字段值是否存在报错:: 执行SQL语句查询数据错误" + query.lastError().text();
        return false;
    }

    //判断
    isMatch = false;
    while(query.next()){
        if(query.value(0)==fieldValue){
            isMatch = true;
        }
    }

    //执行成功
    return true;
}















/*
 *  @brief  向表格插入整行记录
 *  @param  表格名
 *  @param  插入数据参数
 *  @retval 是否插入成功
 */
bool EasySQLite::recordInsert(const QString& tableName,const QVariantList& values){
    //检查数据库是否打开
    if(!m_database.isOpen()){
        //未打开 执行打开数据库
        if(!databaseOpen()){
            //打开失败
            m_errorInfo = "[EasySQLite/Error]整行记录插入报错: 数据库打开失败";
            return false;
        }
    }

    //数据库已打开 判断表格是否存在
    if (!m_database.tables().contains(tableName)) {
        //表格不存在
        m_errorInfo = "[EasySQLite/Error]整行记录插入报错: 表格不存在";
        return false;
    }

    //表格存在 准备将数据转为sql格式
    QString strValues = values2SqlFormat(values);

    //开始插入数据
    QSqlQuery query;
    if(!query.exec(QString("INSERT INTO %1 VALUES(%2)").arg(tableName).arg(strValues))){
        //插入失败
        m_errorInfo = "[EasySQLite/Error]整行记录插入报错: 执行SQL语句插入数据错误" + query.lastError().text();
        return false;
    }

    //select * from tableName 赋值变量model
    if(!recordSelectTableAll(tableName)){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]整行记录插入报错: 查询TableModel错误";
        return false;
    }

    //插入成功 关闭数据库
    databaseClose();
    return true;
}

/*
 *  @brief  向单个表格插入多行记录
 *  @param  表格名
 *  @param  插入数据参数列表
 *  @retval 是否插入成功
 */
bool EasySQLite::recordsInsert(const QString& tableName,const QList<QVariantList>& valuesList){
    //检查数据库是否打开
    if(!m_database.isOpen()){
        //未打开 执行打开数据库
        if(!databaseOpen()){
            //打开失败
            m_errorInfo = "[EasySQLite/Error]多行记录插入报错: 数据库打开失败";
            return false;
        }
    }

    //数据库已打开 判断表格是否存在
    if (!m_database.tables().contains(tableName)) {
        m_errorInfo = "[EasySQLite/Error]多行记录插入报错: 表格不存在";
        return false;
    }

    //表格存在 准备将数据转为sql格式
    QStringList strValuesList = valuesList2SqlFormat(valuesList);

    //开始插入数据
    QSqlQuery query;
    for (int recordIndex = 0; recordIndex < strValuesList.size(); ++recordIndex) {
        if(!query.exec(QString("INSERT INTO %1 VALUES(%2)").arg(tableName).arg(strValuesList.at(recordIndex)))){
            //插入失败
            m_errorInfo = "[EasySQLite/Error]多行记录插入报错: 执行SQL语句插入数据错误" + query.lastError().text();
            return false;
        }
    }

    //select * from tableName 赋值变量model
    if(!recordSelectTableAll(tableName)){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]多行记录插入报错: 查询TableModel错误";
        return false;
    }

    //插入成功 关闭数据库
    databaseClose();
    return true;
}

bool EasySQLite::recordDelete(const QString& tableName,const QVariant& primarykeyValue){
    //检查数据库是否打开
    if(!m_database.isOpen()){
        //未打开 执行打开数据库
        if(!databaseOpen()){
            //打开失败
            m_errorInfo = "[EasySQLite/Error]整行记录删除报错: 数据库打开失败";
            return false;
        }
    }

    //数据库已打开 判断表格是否存在
    if (!m_database.tables().contains(tableName)) {
        m_errorInfo = "[EasySQLite/Error]整行记录删除报错: 表格不存在";
        return false;
    }

    //表格存在 查询表格信息获取主键名
    QString primaryKeyName = primarykeyName(tableName);
    if(primaryKeyName==""){
        m_errorInfo = "[EasySQLite/Error]整行记录删除报错: 查询主键名错误";
        return false;
    }

    //判断主键值是否存在
    bool isPrimarykeyValueExist = false;
    if(!isFieldValueMatch(tableName,primaryKeyName,primarykeyValue,isPrimarykeyValueExist)){
        //查询主键值错误
        m_errorInfo = "[EasySQLite/Error]整行记录删除报错: 查询主键值是否存在错误";
        return false;
    }

    //判断
    if(!isPrimarykeyValueExist){
        //主键值不存在
        m_errorInfo = "[EasySQLite/Error]整行记录删除报错: 主键值不存在";
        return false;
    }

    //主键值存在 转换主键值为sql格式
    QString strPrimarykeyValue = value2SqlFormat(primarykeyValue);

    //开始删除记录
    QSqlQuery query;
    if(!query.exec(QString("DELETE FROM %1 WHERE %2 = %3").arg(tableName).arg(primaryKeyName).arg(strPrimarykeyValue))){
        //删除失败
        m_errorInfo = "[EasySQLite/Error]整行记录删除报错: 执行SQL语句删除记录错误" + query.lastError().text();
        return false;
    }

    //select * from tableName赋值变量 model
    if(!recordSelectTableAll(tableName)){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]整行记录删除报错: 查询TableModel错误";
        return false;
    }

    //删除成功 关闭数据库
    databaseClose();
    return true;
}

bool EasySQLite::recordsDelete(const QString &tableName, const QVariantList &primarykeyValueList){
    //检查数据库是否打开
    if(!m_database.isOpen()){
        //未打开 执行打开数据库
        if(!databaseOpen()){
            //打开失败
            return false;
        }
    }

    //数据库已打开 判断表格是否存在
    if (!m_database.tables().contains(tableName)) {
        m_errorInfo = "[EasySQLite/Error]多行记录删除报错: 表格不存在";
        return false;
    }

    //表格存在 查询表格信息获取主键名
    QString primaryKeyName = primarykeyName(tableName);
    if(primaryKeyName==""){
        m_errorInfo = "[EasySQLite/Error]多行记录删除报错: 查询主键名错误";
        return false;
    }


    //判断主键值是否存在
    bool isPrimarykeyValueExist = false;
    for (int var = 0; var < primarykeyValueList.size(); ++var) {
        if(!isFieldValueMatch(tableName,primaryKeyName,primarykeyValueList.at(var),isPrimarykeyValueExist)){
            //查询主键值错误
            m_errorInfo = "[EasySQLite/Error]多行记录删除报错: 查询主键值是否存在错误";
            return false;
        }
        //查询成功 判断
        if(!isPrimarykeyValueExist){
            //主键值不存在
            m_errorInfo = "[EasySQLite/Error]多行记录删除报错: 主键值不存在";
            return false;
        }else{
            //主键值存在 继续循环
            isPrimarykeyValueExist = false;
        }
    }

    //主键值存在 转换主键值为sql格式
    QStringList strPrimarykeyValueList;
    for (int var = 0; var < primarykeyValueList.size(); ++var) {
        strPrimarykeyValueList.append(value2SqlFormat(primarykeyValueList.at(var)));
    }

    //开始删除数据
    QSqlQuery query;
    for (int var = 0; var < strPrimarykeyValueList.size(); ++var) {
        if(!query.exec(QString("DELETE FROM %1 WHERE %2 = %3").arg(tableName).arg(primaryKeyName).arg(strPrimarykeyValueList.at(var)))){
            //删除失败
            m_errorInfo = "[EasySQLite/Error]多行记录删除报错: 执行SQL语句删除记录错误" + query.lastError().text();
            return false;
        }
    }

    //select * from tableName 赋值变量model
    if(!recordSelectTableAll(tableName)){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]多行记录删除报错: 查询TableModel错误";
        return false;
    }

    //删除成功 关闭数据库
    databaseClose();
    return true;
}

bool EasySQLite::recordSelectTableAll(const QString &tableName){
    //检查数据库是否打开
    if(!m_database.isOpen()){
        //未打开 执行打开数据库
        if(!databaseOpen()){
            //打开失败
            m_errorInfo = "[EasySQLite/Error]表格全查询报错: 数据库打开失败";
            return false;
        }
    }

    //数据库已打开 判断表格是否存在
    if (!m_database.tables().contains(tableName)) {
        m_errorInfo = "[EasySQLite/Error]表格全查询报错: 表格不存在";
        return false;
    }

    //表格存在 开始查询
    QSqlQuery query;
    if(!query.exec(QString("SELECT * FROM %1").arg(tableName))){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]表格全查询报错: 执行SQL语句查询表格错误" + query.lastError().text();
        return false;
    }

    //select * from tableName 赋值变量model
    m_tableModel= new QSqlTableModel(this,m_database);
    m_tableModel->setQuery(query);

    //查询成功 关闭数据库
    databaseClose();
    return true;
}

bool EasySQLite::recordSelect(const QString &tableName, const QStringList &fieldNameList, const QString& condition,
                              const QString &sortFieldName, const SortPolicy &sortPolicy){
    //检查数据库是否打开
    if(!m_database.isOpen()){
        //未打开 执行打开数据库
        if(!databaseOpen()){
            //打开失败
            m_errorInfo = "[EasySQLite/Error]记录查询报错: 数据库打开失败";
            return false;
        }
    }

    //数据库已打开 判断表格是否存在
    if (!m_database.tables().contains(tableName)) {
        m_errorInfo = "[EasySQLite/Error]记录查询报错: 表格不存在";
        return false;
    }

    //判断查询字段名是否存在 拼接查询字段名
    QString fieldName;
    for (int var = 0; var < fieldNameList.size(); ++var) {
        bool isFieldNameExist=false;
        QString currentFieldName = fieldNameList.at(var);
        if(!isFieldNameMatch(tableName,currentFieldName,isFieldNameExist)){
            //查询失败
            m_errorInfo = "[EasySQLite/Error]记录查询报错: 查询字段名是否存在错误";
            return false;
        }
        //查询成功
        if(!isFieldNameExist){
            //字段名不存在
            m_errorInfo = "[EasySQLite/Error]记录查询报错: 查询字段名不存在";
            return false;
        }

        //字段名存在 拼接
        fieldName += (currentFieldName+",");
    }
    fieldName.removeLast();

    //检查排序字段名是否存在
    bool isSortFieldNameExist=false;
    if(!isFieldNameMatch(tableName,sortFieldName,isSortFieldNameExist)){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]记录查询报错: 查询排序字段名是否存在错误";
        return false;
    }
    //查询成功
    if(!isSortFieldNameExist){
        //字段名不存在
        m_errorInfo = "[EasySQLite/Error]记录查询报错: 排序字段名不存在";
        return false;
    }

    //制作条件字符串
    QString strCondition = "WHERE "+condition;

    //转换排序策略字符串
    QString policy;
    if(sortPolicy==SortPolicy::ASC){
        policy = "ASC";
    }else{
        policy = "DESC";
    }

    //执行查询
    QSqlQuery query;
    if(!query.exec(QString("SELECT %1 FROM %2 %3 ORDER BY %4 %5").arg(fieldName).arg(tableName).arg(strCondition).arg(sortFieldName).arg(policy))){
        m_errorInfo = "[EasySQLite/Error]记录查询报错: 执行SQL语句查询错误";
        return false;
    }

    //查询成功 赋值变量model
    m_tableModel= new QSqlTableModel(this,m_database);
    m_tableModel->setQuery(query);

    //查询成功 关闭数据库
    databaseClose();
    return true;
}

bool EasySQLite::fieldUpdateValue(const QString &tableName, const QString &fieldName, const QVariant &fieldValue,
                             const QString &condiFieldName, const QVariant &condiFieldValue){
    //检查数据库是否打开
    if(!m_database.isOpen()){
        //未打开 执行打开数据库
        if(!databaseOpen()){
            //打开失败
            m_errorInfo = "[EasySQLite/Error]字段更新数值报错: 数据库打开失败";
            return false;
        }
    }

    //数据库已打开 判断表格是否存在
    if (!m_database.tables().contains(tableName)) {
        m_errorInfo = "[EasySQLite/Error]字段更新数值报错: 表格不存在";
        return false;
    }

    //判断更新字段名是否存在
    bool isExist = false;
    if(!isFieldNameMatch(tableName,fieldName,isExist)){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]字段更新数值报错: 查询更新字段名是否存在错误";
        return false;
    }
    //查询成功 判断
    if(!isExist){
        //不存在
        m_errorInfo = "[EasySQLite/Error]字段更新数值报错: 更新字段名不存在";
        return false;
    }
    isExist =false;

    //判断条件字段名是否存在
    if(!isFieldNameMatch(tableName,condiFieldName,isExist)){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]字段更新数值报错: 查询条件字段名是否存在错误";
        return false;
    }
    //查询成功 判断
    if(!isExist){
        //不存在
        m_errorInfo = "[EasySQLite/Error]字段更新数值报错: 条件字段名不存在";
        return false;
    }
    isExist =false;

    //判断条件字段值是否存在
    if(!isFieldValueMatch(tableName,condiFieldName,condiFieldValue,isExist)){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]字段更新数值报错: 查询条件字段值是否存在错误";
        return false;
    }
    //查询成功 判断
    if(!isExist){
        //不存在
        m_errorInfo = "[EasySQLite/Error]字段更新数值报错: 条件字段值不存在";
        return false;
    }

    //把更新字段值和条件字段值转为SQL格式
    QString strFieldValue=  value2SqlFormat(fieldValue);
    QString strCondiFieldValue=  value2SqlFormat(condiFieldValue);

    //开始更新
    QSqlQuery query;
    if(!query.exec(QString("UPDATE %1 SET %2 = %3 WHERE %4 = %5").arg(tableName).arg(fieldName).arg(strFieldValue).arg(condiFieldName).arg(strCondiFieldValue))){
        m_errorInfo = "[EasySQLite/Error]字段更新数值报错: 执行SQL语句更新数据失败";
        return false;
    }

    //select * from tableName 赋值变量model
    if(!recordSelectTableAll(tableName)){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]字段更新数值报错: 查询TableModel错误";
        return false;
    }

    //更新成功 关闭数据库
    databaseClose();
    return true;
}

bool EasySQLite::fieldUpdate(const QString &tableName, const QString &fieldName, const QVariant &fieldValue, const QString &condition){
    //检查数据库是否打开
    if(!m_database.isOpen()){
        //未打开 执行打开数据库
        if(!databaseOpen()){
            //打开失败
            m_errorInfo = "[EasySQLite/Error]字段更新报错: 数据库打开失败";
            return false;
        }
    }

    //数据库已打开 判断表格是否存在
    if (!m_database.tables().contains(tableName)) {
        m_errorInfo = "[EasySQLite/Error]字段更新报错: 表格不存在";
        return false;
    }

    //判断更新字段名是否存在
    bool isExist = false;
    if(!isFieldNameMatch(tableName,fieldName,isExist)){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]字段更新报错: 查询更新字段名是否存在错误";
        return false;
    }
    //查询成功 判断
    if(!isExist){
        //不存在
        m_errorInfo = "[EasySQLite/Error]字段更新报错: 更新字段名不存在";
        return false;
    }

    //把更新字段值转为SQL格式
    QString strFieldValue = value2SqlFormat(fieldValue);

    //制作条件字符串
    QString strCondition = "WHERE "+condition;

    //开始更新
    QSqlQuery query;
    if(!query.exec(QString("UPDATE %1 SET %2 = %3 %4").arg(tableName).arg(fieldName).arg(strFieldValue).arg(strCondition))){
        m_errorInfo = "[EasySQLite/Error]字段更新报错: 更新字段名不存在";
        return false;
    }

    //select * from tableName 赋值变量model
    if(!recordSelectTableAll(tableName)){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]字段更新报错: 查询TableModel错误";
        return false;
    }

    //更新成功 关闭数据库
    databaseClose();
    return true;
}








/*
 *  @brief  获取报错字符串成员变量
 *  @param  无
 *  @retval 报错字符串成员变量
 */
QString EasySQLite::errorInfo(){
    return m_errorInfo;
}

/*
 *  @brief  获取表格模型
 *  @param  无
 *  @retval 表格模型
 */
QSqlTableModel *EasySQLite::tableModel(){
    if(m_tableModel==nullptr){
        m_tableModel = new QSqlTableModel(this,m_database);
    }
    return m_tableModel;
}

QVariant EasySQLite::value(const QString &tableName, const QVariant &primarykeyValue, const QString &fieldName){
    //检查数据库是否打开
    if(!m_database.isOpen()){
        //未打开 执行打开数据库
        if(!databaseOpen()){
            //打开失败
            m_errorInfo = "[EasySQLite/Error]获取数值报错: 数据库打开失败";
            return false;
        }
    }

    //数据库已打开 判断表格是否存在
    if (!m_database.tables().contains(tableName)) {
        m_errorInfo = "[EasySQLite/Error]获取数值报错: 表格不存在";
        return false;
    }

    //表格存在 判断主键值是否存在
    //获取主键名
    QString primaryName = primarykeyName(tableName);
    //查询主键值是否存在
    bool isExist;
    if(!isFieldValueMatch(tableName,primaryName,primarykeyValue,isExist)){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]获取数值报错: 查询主键值是否存在错误";
        return false;
    }
    //查询成功
    if(!isExist){
        //不存在
        m_errorInfo = "[EasySQLite/Error]获取数值报错: 主键值不存在";
        return false;
    }
    isExist = false;

    //判断字段名是否存在
    if(!isFieldNameMatch(tableName,fieldName,isExist)){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]获取数值报错: 查询字段名是否存在错误";
        return false;
    }
    //查询成功
    if(!isExist){
        //不存在
        m_errorInfo = "[EasySQLite/Error]获取数值报错: 字段名不存在";
        return false;
    }

    //转换主键值为sql格式
    QString primaryValue= value2SqlFormat(primarykeyValue);

    //开始查询数值
    QSqlQuery query;
    if(!query.exec(QString("SELECT %1 FROM %2 WHERE %3 = %4").arg(fieldName).arg(tableName).arg(primaryName).arg(primaryValue))){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]获取数值报错: 执行SQL语句查询数值错误";
        return false;
    }

    //查询成功 获取数据
    query.next();

    //关闭数据库
    databaseClose();
    return query.value(0);
}

bool EasySQLite::isValueExist(const QString &tableName, const QVariant &primarykeyValue, const QString &fieldName, const QVariant &inputValue){
    //检查数据库是否打开
    if(!m_database.isOpen()){
        //未打开 执行打开数据库
        if(!databaseOpen()){
            //打开失败
            m_errorInfo = "[EasySQLite/Error]判断数值是否存在报错: 数据库打开失败";
            return false;
        }
    }

    //数据库已打开 判断表格是否存在
    if (!m_database.tables().contains(tableName)) {
        m_errorInfo = "[EasySQLite/Error]判断数值是否存在报错: 表格不存在";
        return false;
    }

    //表格存在 判断主键值是否存在
    //获取主键名
    QString primaryName = primarykeyName(tableName);
    //查询主键值是否存在
    bool isExist;
    if(!isFieldValueMatch(tableName,primaryName,primarykeyValue,isExist)){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]判断数值是否存在报错: 查询主键值是否存在错误";
        return false;
    }
    //查询成功
    if(!isExist){
        //不存在
        m_errorInfo = "[EasySQLite/Error]判断数值是否存在报错: 主键值不存在";
        return false;
    }
    isExist = false;

    //判断字段名是否存在
    if(!isFieldNameMatch(tableName,fieldName,isExist)){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]判断数值是否存在报错: 查询字段名是否存在错误";
        return false;
    }
    //查询成功
    if(!isExist){
        //不存在
        m_errorInfo = "[EasySQLite/Error]判断数值是否存在报错: 字段名不存在";
        return false;
    }

    //转换主键值为sql格式
    QString primaryValue= value2SqlFormat(primarykeyValue);

    //开始查询数值
    QSqlQuery query;
    if(!query.exec(QString("SELECT %1 FROM %2 WHERE %3 = %4").arg(fieldName).arg(tableName).arg(primaryName).arg(primaryValue))){
        //查询失败
        m_errorInfo = "[EasySQLite/Error]判断数值是否存在报错: 执行SQL语句查询数值错误";
        return false;
    }

    //查询成功 获取数据
    query.next();
    if(query.value(0)!=inputValue){
        return false;
    }

    //关闭数据库
    databaseClose();
    return true;
}

