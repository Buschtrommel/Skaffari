#include "skapptestobject.h"
#include <QTest>
#include <QLocalSocket>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSqlDatabase>
#include <QEventLoop>
#include <QTimer>
#include <QSqlError>
#include <QUuid>
#include <QCryptographicHash>

#define DOCKER_SOCKET "/var/run/docker.sock"
#define DOCKER_IMAGE_NAME "skaffaritestenv"
#define DOCKER_IMAGE_TAG "skaffaritestenv:latest"

SkAppTestObject::SkAppTestObject(QObject *parent) : QObject(parent)
{
    m_containerName = QString::fromLatin1(QCryptographicHash::hash(QUuid::createUuid().toByteArray(), QCryptographicHash::Sha1).toHex()).left(16);
}

SkAppTestObject::~SkAppTestObject()
{

}

bool SkAppTestObject::startContainer(const QMap<QString, QString> &config) const
{
    QLocalSocket docker;
    docker.connectToServer(QStringLiteral(DOCKER_SOCKET));
    if (!docker.waitForConnected(5000)) {
        qCritical() << "Failed to connect to docker socket.";
        return false;
    }

    QString cmd = QStringLiteral("GET /v1.32/images/json HTTP/1.1\r\nHost:\r\n\r\n");
    if (docker.write(cmd.toLatin1()) != cmd.length()) {
        qCritical() << "Failed to write command to docker socket.";
        return false;
    }

    if (!docker.waitForReadyRead()) {
        qCritical() << "Timeout while waiting for response from docker.";
        return false;
    }

    QTextStream stream(docker.readAll());
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.isEmpty()) {
            break;
        }
    }

    const QJsonArray images = QJsonDocument::fromJson(stream.readLine().toUtf8()).array();
    if (images.isEmpty()) {
        qCritical() << "No docker images available.";
        return false;
    }

    bool containsImage = false;
    auto it = images.constBegin();
    while (it != images.constEnd()) {
        const QJsonObject o = it->toObject();
        const QVariantList repoTags = o.value(QStringLiteral("RepoTags")).toArray().toVariantList();
        if (repoTags.contains(QVariant(QStringLiteral(DOCKER_IMAGE_TAG)))) {
            containsImage = true;
            break;
        }
        ++it;
    }

    if (!containsImage) {
        return false;
    }

    QJsonObject o;
    o.insert(QStringLiteral("Image"), QStringLiteral(DOCKER_IMAGE_NAME));
    QJsonObject hc;
    QJsonObject pb;

    QJsonArray imapPortsArray;
    QJsonObject imapHostPort{
        {QStringLiteral("HostPort"), QString::number(m_imapPort)}
    };
    imapPortsArray.append(imapHostPort);
    pb.insert(QStringLiteral("143/tcp"), imapPortsArray);

    QJsonArray mysqlPortsArray;
    QJsonObject mysqlHostPort{
        {QStringLiteral("HostPort"), QString::number(m_mysqlPort)}
    };
    mysqlPortsArray.append(mysqlHostPort);
    pb.insert(QStringLiteral("3306/tcp"), mysqlPortsArray);

    QJsonArray sievePortsArry;
    QJsonObject sieveHostPort{
        {QStringLiteral("HostPort"), QString::number(m_sievePort)}
    };
    sievePortsArry.append(sieveHostPort);
    pb.insert(QStringLiteral("4190/tcp"), sievePortsArry);

    hc.insert(QStringLiteral("PortBindings"), pb);
    o.insert(QStringLiteral("HostConfig"), hc);

    QStringList containerEnv;
    auto cit = config.constBegin();
    while (cit != config.constEnd()) {
        if (!cit.value().isEmpty()) {
            const QString envVar = cit.key() + QLatin1Char('=') + cit.value();
            containerEnv << envVar;
        }
        ++cit;
    }
    if (!containerEnv.empty()) {
        o.insert(QStringLiteral("Env"), QJsonArray::fromStringList(containerEnv));
    }
    QJsonDocument po{o};

    cmd = QLatin1String("POST /v1.32/containers/create?name=") + m_containerName + QLatin1String(" HTTP/1.1\r\nHost:\r\nContent-Type: application/json\r\nContent-Length:");
    const auto content = po.toJson(QJsonDocument::Compact);
    cmd.append(QString::number(content.length()));
    cmd.append(QLatin1String("\r\n\r\n"));
    cmd.append(content);
    cmd.append(QLatin1String("\r\n"));

    if (docker.write(cmd.toLatin1()) != cmd.length()) {
        qCritical() << "Failed to write command to docker socket.";
        return false;
    }

    if (!docker.waitForReadyRead()) {
        qCritical() << "Timeout while waiting for response from docker.";
        return false;
    }

    cmd = QLatin1String("POST /v1.32/containers/") + m_containerName + QLatin1String("/start HTTP/1.1\r\nHost:\r\n\r\n");

    if (docker.write(cmd.toLatin1()) != cmd.length()) {
        qCritical() << "Failed to write command to docker socket.";
        return false;
    }

    if (!docker.waitForReadyRead()) {
        qCritical() << "Timeout while waiting for response from docker.";
        return false;
    }

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"), QStringLiteral("amarsch"));
        db.setHostName(QStringLiteral("127.0.0.1"));
        db.setPort(m_mysqlPort);
        db.setDatabaseName(config.value(QStringLiteral("SKAFFARI_DB_NAME")));
        db.setUserName(config.value(QStringLiteral("SKAFFARI_DB_USER")));
        db.setPassword(config.value(QStringLiteral("SKAFFARI_DB_PASS")));
        int tries = 0;
        while (!db.open() && tries < 5) {
            QEventLoop loop;
            QTimer::singleShot(5000, &loop, &QEventLoop::quit);
            loop.exec();
            tries++;
        }

        if (!db.isOpen()) {
            qCritical() << "Failed to connect to docker container.";
            return false;
        }
    }

    QSqlDatabase::removeDatabase(QStringLiteral("amarsch"));

    return true;
}

bool SkAppTestObject::stopContainer() const
{
    QLocalSocket docker;
    docker.connectToServer(QStringLiteral(DOCKER_SOCKET));
    if (!docker.waitForConnected()) {
        return false;
    }

    QString cmd = QLatin1String("DELETE /v1.32/containers/") + m_containerName + QLatin1String("?force=true HTTP/1.1\r\nHost:\r\n\r\n");

    if (docker.write(cmd.toLatin1()) != cmd.length()) {
        qCritical() << "Failed to write command to docker socket.";
        return false;
    }

    if (!docker.waitForReadyRead()) {
        qCritical() << "Timeout while waiting for response from docker.";
        return false;
    }

    return true;
}

//bool SkAppTestObject::startMysql()
//{
//    if (!m_mysqlWorkingDir.isValid()) {
//        qCritical() << "Can not create mysql working directory.";
//        return false;
//    }
//    chown(m_mysqlWorkingDir.path(), QStringLiteral(MYSQL_UG));

//    if (!m_mysqlDataDir.isValid()) {
//        qCritical() << "Can not create mysql data directory.";
//        return false;
//    }
//    chown(m_mysqlDataDir.path(), QStringLiteral(MYSQL_UG), 750);

//    if (!m_mysqlSocketDir.isValid()) {
//        qCritical() << "Can not create mysql socket directory.";
//        return false;
//    }
//    chown(m_mysqlSocketDir.path(), QStringLiteral(MYSQL_UG));

//    if (!m_mysqlLogDir.isValid()) {
//        qCritical() << "Can not create mysql log directory.";
//        return false;
//    }
//    chown(m_mysqlLogDir.path(), QStringLiteral(MYSQL_UG), 750);

//    if (!m_mysqlConfigFile.open()) {
//        qCritical() << "Can not open mysql config file.";
//        return false;
//    }

//    {
//        QTextStream mysqlConfOut(&m_mysqlConfigFile);
//        mysqlConfOut.setCodec("UTF-8");
//        mysqlConfOut << "[mysqld]" << endl;
//        mysqlConfOut << "bind-address=127.0.0.1" << endl;
//        mysqlConfOut << "log-error=" << m_mysqlLogDir.filePath(QStringLiteral("mysql.log")) << endl;
//        mysqlConfOut << "innodb_file_format=Barracuda" << endl;
//        mysqlConfOut << "innodb_file_per_table=ON" << endl;
//        mysqlConfOut << "datadir=" << m_mysqlDataDir.path() << endl;
//        mysqlConfOut << "port=" << m_mysqlPort << endl;
//        mysqlConfOut << "socket=" << m_mysqlSocketDir.filePath(QStringLiteral("mysql.sock")) << endl;
//        mysqlConfOut << "server-id=1" << endl;
//        mysqlConfOut.flush();
//    }

//    chown(m_mysqlConfigFile.fileName(), QStringLiteral("root:mysql"), 640);

//    const QString mysqlDefaultsArg = QLatin1String("--defaults-file=") + m_mysqlConfigFile.fileName();
//    const QStringList mysqlInstallArgs{QStringLiteral("--force"), QStringLiteral("--skip-auth-anonymous-user"), QStringLiteral("--auth-root-authentication-method=normal"), mysqlDefaultsArg, QStringLiteral("--user=mysql")};

//    if (QProcess::execute(QStringLiteral("/usr/bin/mysql_install_db"), mysqlInstallArgs) != 0) {
//        qCritical() << "Can not install initial mysql databases.";
//        return false;
//    }

//    QFileInfo socketFi(m_mysqlSocketDir.filePath(QStringLiteral("mysql.sock")));

//    m_mysqlProcess.setArguments({mysqlDefaultsArg, QStringLiteral("--user=mysql")});
//    m_mysqlProcess.setProgram(QStringLiteral("/usr/sbin/mysqld"));
//    m_mysqlProcess.setWorkingDirectory(m_mysqlWorkingDir.path());
//    m_mysqlProcess.start();

//    auto cur = QDateTime::currentDateTime();
//    while (!socketFi.exists() && (cur.msecsTo(QDateTime::currentDateTime()) < 5000)) {

//    }

//    if (!socketFi.exists()) {
//        qCritical() << "Failed to start mysql server within 5 seconds.";
//        qDebug() << "MySQL startup params:" << m_mysqlProcess.arguments().join(QChar(QChar::Space));
//        return false;
//    }

//    return true;
//}

//bool SkAppTestObject::createDatabase()
//{
//    {
//        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"), QStringLiteral("skaffaridb"));
//        db.setDatabaseName(QStringLiteral("mysql"));
//        db.setUserName(QStringLiteral("root"));
//        db.setConnectOptions(QStringLiteral("UNIX_SOCKET=%1").arg(m_mysqlSocketDir.filePath(QStringLiteral("mysql.sock"))));
//        if (Q_UNLIKELY(!db.open())) {
//            qCritical() << "Failed to open database connection:" << db.lastError().text();
//            return false;
//        }

//        QSqlQuery q(db);

//        if (!q.exec(QStringLiteral("CREATE DATABASE %1").arg(m_dbName))) {
//            qCritical() << "Failed to create database:" << q.lastError().text();
//            return false;
//        }

//        if (!q.exec(QStringLiteral("CREATE USER '%1'@'localhost' IDENTIFIED BY '%2'").arg(m_dbUser, m_dbPass))) {
//            qCritical() << "Failed to create database user:" << q.lastError().text();
//            return false;
//        }

//        if (!q.exec(QStringLiteral("GRANT ALL ON %1.* TO '%2'@'localhost'").arg(m_dbName, m_dbUser))) {
//            qCritical() << "Failed to grant access to db user:" << q.lastError().text();
//            return false;
//        }

//        db.close();
//    }

//    QSqlDatabase::removeDatabase(QStringLiteral("skaffaridb"));

//    return true;
//}

SkCmdProc::SkCmdProc(QObject *parent) : QProcess(parent)
{
    setProgram(QStringLiteral(SKAFFARI_CMD));
    setProcessChannelMode(QProcess::MergedChannels);
}

SkCmdProc::~SkCmdProc()
{

}

bool SkCmdProc::enterString(const QString &str)
{
    const QByteArray ba = str.toUtf8();
    if (write(ba) == ba.length()) {
        if (write("\n") == 1) {
            return true;
        }
    }
    return false;
}

qint64 SkCmdProc::enterNumber(int number)
{
    const QByteArray ba = QByteArray::number(number);
    if (write(ba) == ba.length()) {
        if (write("\n") == 1) {
            return true;
        }
    }
    return false;
}

qint64 SkCmdProc::enterBool(bool b)
{
    if (b) {
        return write("y\n") == 2;
    } else {
        return write("n\n") == 2;
    }
}

void SkCmdProc::setShowOutput(bool show)
{
    m_showOutput = show;
}

bool SkCmdProc::waitForOutput()
{
    if (waitForReadyRead(m_waitForOutputTimeOut)) {
        if (m_showOutput) {
            qDebug() << readAll();
        }
        return true;
    } else {
        return false;
    }
}
