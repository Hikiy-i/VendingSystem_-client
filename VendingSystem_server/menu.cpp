#include "menu.h"

Menu::Menu(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Menu)
{
    prepareTheTable();
    ui->setupUi(this);
    Menu::on_refresh_btn_clicked();

    QThread *thDBHandler = new QThread;    //创建处理数据库的线程
    DBHandler *iDBHandler = new  DBHandler;
    iDBHandler->moveToThread(thDBHandler);
    connect(this, SIGNAL(GetAdvTable()), iDBHandler, SLOT(handle_getAdvTable()));
    connect(iDBHandler, SIGNAL(ShowAdvTable(QSortFilterProxyModel*)), this, SLOT(show_adv_table(QSortFilterProxyModel*)));
    thDBHandler->start();

    emit GetAdvTable();
//    thDBHandler->stop();
}

Menu::~Menu()
{
    delete ui;
    delete ui_reg;
    delete userRegWidget;
    delete model;
    delete m_buttonDelegate;
    delete sqlproxy;
}

void Menu::on_newUser_btn_clicked()
{
    userRegWidget = new QWidget();
    ui_reg = new Ui::UserReg();
    ui_reg->setupUi(userRegWidget);
    connect(ui_reg->back_btn, SIGNAL(clicked(bool)), this, SLOT(back_btn_clicked()));
    connect(ui_reg->add_btn, SIGNAL(clicked(bool)), this, SLOT(add_btn_clicked()));
    userRegWidget->show();
}

void Menu::back_btn_clicked()
{
    userRegWidget->close();
}

void Menu::add_btn_clicked()
{
    QString name = ui_reg->name_le->text();
    QString username = ui_reg->username_le->text();
    QString password = ui_reg->password_le->text();
    QString contact = ui_reg->contact_le->text();

    QButtonGroup sexSelect;
    sexSelect.addButton(ui_reg->man);
    sexSelect.addButton(ui_reg->woman);
    QString sex = sexSelect.checkedButton()->objectName();
    if(!QString::compare(sex, "man"))
        sex = "男";
    else
        sex = "女";

    if(name.isEmpty() || username.isEmpty() || password.isEmpty() || contact.isEmpty() || contact.size() != 11)
    {
        ui_reg->tips_lb->setText("请检查输入！");
        return;
    }

    qDebug() << "------------add------------";
    qDebug() << "Name:" << name
             << "Sex:" << sex
             << "Contact" << contact;
    qDebug() << "---------------------------";


    if(!db.open())
    {
        qDebug()<<"no open";
        QMessageBox::critical(this, "Error", "无法打开数据库！");
        return;
    }
    else
    {
        qDebug()<<"open";
        QString insert_sql_user = "insert into user(ID,Name,Username,Password,Sex,Contact,RegDate) values(:ID,:Name,:Username,:Password,:Sex,:Contact,:RegDate)";
        QSqlQuery sql_query = QSqlQuery(db);
        sql_query.prepare(insert_sql_user);

        sql_query.bindValue(":Name",name);
        sql_query.bindValue(":Username", username);
        sql_query.bindValue(":Password", password);
        sql_query.bindValue(":Sex", sex);
        sql_query.bindValue(":Contact", contact);
        sql_query.bindValue(":RegDate", QDate::currentDate().toString("yyyy-MM-dd"));

        if(!sql_query.exec())
        {
            qDebug() << QObject::tr("User table failed to insert!");
            qDebug() << sql_query.lastError();
            QMessageBox::critical(this, "Error", "添加失败！");

        }
        else
        {
            qDebug() << "User insert!";
            Menu::on_refresh_btn_clicked();
            QMessageBox::information(this, "Tip", "添加成功！");
            userRegWidget->close();
        }
        db.close();
    }

}


void Menu::prepareTheTable(void)
{
    QString dbFilePath(DBFILE);
    QFileInfo fileinfo(dbFilePath);
    if(QSqlDatabase::contains(DBCONNECTION))
    {
        qDebug() << "Connection:dbUser existed.";
        db = QSqlDatabase::database(DBCONNECTION);
    }
    else
    {
        qDebug() << "Connection:dbUser not existed.";
        db = QSqlDatabase::addDatabase("QSQLITE", DBCONNECTION);
        db.setDatabaseName(DBNAME);
        if(!fileinfo.isFile())
        {
            if(db.open())
            {
                QString create_sql_user = "create table user(ID INTEGER PRIMARY KEY AUTOINCREMENT,Name varchar(24),Username varchar(24),Password varchar(24),Sex varchar(5),Contact varchar(11),RegDate date)";
                QSqlQuery sql_query = QSqlQuery(db);
                sql_query.prepare(create_sql_user);
                if(!sql_query.exec())
                {
                    qDebug() << QObject::tr("User table failed to create!");
                    qDebug() << sql_query.lastError();
                    QFile::remove(DBFILE);
                }
                else
                {
                    qDebug() << "User table created!";
                }
            }
            else
            {
                QMessageBox::information(NULL, "Error", "Fail to connect the SQLite!",
                                         QMessageBox::Yes);
            }
        }
    }
    db.close();
}

void Menu::on_refresh_btn_clicked()
{
    qDebug() << "refresh";
    if(!db.open())
    {
        qDebug()<<"no open";
        QMessageBox::critical(this, "Error", "无法打开数据库！");
        return;
    }
    else
    {
        model = new TableModel();
        m_buttonDelegate = new ButtonDelegate(ui->user_table);
        connect(m_buttonDelegate, SIGNAL(sig_editUser(QModelIndex)), this, SLOT(editUser_btn_clicked(QModelIndex)));
        connect(m_buttonDelegate, SIGNAL(sig_deleteUser(QModelIndex)), this, SLOT(deleteUser_btn_clicked(QModelIndex)));
        model->setQuery("select * from user", db);
        model->setHeaderData(0,Qt::Horizontal,QObject::tr("ID"));
        model->setHeaderData(1,Qt::Horizontal,QObject::tr("姓名"));
        model->setHeaderData(2,Qt::Horizontal,QObject::tr("用户名"));
        model->setHeaderData(3,Qt::Horizontal,QObject::tr("密码"));
        model->setHeaderData(4,Qt::Horizontal,QObject::tr("性别"));
        model->setHeaderData(5,Qt::Horizontal,QObject::tr("联系方式"));
        model->setHeaderData(6,Qt::Horizontal,QObject::tr("注册日期"));
        model->insertColumn(7);
        model->setHeaderData(7,Qt::Horizontal,QObject::tr("操作"));
        ui->user_table->setItemDelegateForColumn(7, m_buttonDelegate);
        sqlproxy = new QSortFilterProxyModel();
        sqlproxy->setSourceModel(model);
        ui->user_table->setModel(sqlproxy);
        ui->user_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    }
}





void Menu::on_batchDelete_btn_clicked()
{
    QMap<int,QPair<QVariant, Qt::CheckState> > checkedMap = model->check_state_map;
    QList<QPair<QVariant, Qt::CheckState> > valuesList = checkedMap.values();

    if(!checkedMap.size())
    {
        QMessageBox::information(this, "Tip", "请选择要删除的用户！");
        return;
    }
    else
    {
        for(int i = 0; i < checkedMap.size(); i++)
        {
            if(valuesList.at(i).second == Qt::Checked)
                break;
            else
            {
                QMessageBox::information(this, "Tip", "请选择要删除的用户！");
                return;
            }
        }
    }

    QMessageBox msg;
    msg.setText("确定要删除所选择的用户吗？");
    msg.setWindowTitle("Warning");
    QPushButton *yes_btn = msg.addButton("确认", QMessageBox::AcceptRole);
    QPushButton *no_btn = msg.addButton("取消", QMessageBox::RejectRole);
    msg.exec();
    if(msg.clickedButton() == yes_btn)
    {
        qDebug() << "yes";
        if(!db.open())
        {
            qDebug()<<"no open";
            QMessageBox::critical(this, "Error", "无法打开数据库！");
            return;
        }
        else
        {
            qDebug()<<"open";
            QVariantList GroupIDs;
            QString delete_sql_user = "delete from user where ID=?";
            QSqlQuery sql_query = QSqlQuery(db);
            sql_query.prepare(delete_sql_user);

            //QStringList idStringList;
            for(int i = 0; i< valuesList.size(); i++)
            {
                if(valuesList.at(i).second == Qt::Checked)
                    GroupIDs.append(valuesList.at(i).first.toInt());
            }
                //idStringList.append(valuesList.at(i).first.toString());

            //QString idString = idStringList.join(",");
            qDebug() << "sql:" << GroupIDs;
            //sql_query.bindValue(":ID",idString);
            sql_query.addBindValue(GroupIDs);
            if(!sql_query.execBatch())
            {
                qDebug() << QObject::tr("User table failed to delete!");
                qDebug() << sql_query.lastError();
                QMessageBox::critical(this, "Error", "删除失败！");

            }
            else
            {
                qDebug() << "User delete!";
                Menu::on_refresh_btn_clicked();
                QMessageBox::information(this, "Tip", "删除成功！");
            }
            db.close();
        }



    }
    else if(msg.clickedButton() == no_btn)
    {
        qDebug() << "no";
        model->check_state_map.clear();
        return;
    }
    return;
}

void Menu::show_adv_table(QSortFilterProxyModel *sqlproxy)
{
    qDebug()<<"Menu: show_adv_table";
    ui->adv_table->setModel(sqlproxy);
}

void Menu::editUser_btn_clicked(const QModelIndex &index)
{
    qDebug() << "edit-index:" << index;
}

void Menu::deleteUser_btn_clicked(const QModelIndex &index)
{
    qDebug() << "delete-index:" << index;
}
