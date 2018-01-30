/*
 * Skaffari - a mail account administration web interface based on Cutelyst
 * Copyright (C) 2017 Matthias Fehring <kontakt@buschmann23.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "setup.h"
#include <QSettings>
#include <QDir>
#include <QVersionNumber>
#include <Cutelyst/Plugins/Authentication/credentialpassword.h>
#include <QCryptographicHash>
#include <QStringList>
#include <QTimeZone>

#include "database.h"
#include "imap.h"
#include "../common/password.h"
#include "../common/config.h"
#include "../common/global.h"

Setup::Setup(const QString &confFile, bool quiet) :
    ConfigInput(quiet), m_confFile(confFile, true, true, quiet)
{

}


int Setup::exec() const
{
    printMessage(tr("Start to configure Skaffari."));

    bool configExists = m_confFile.exists();

    const int configCheck = m_confFile.checkConfigFile();
    if (configCheck > 0) {
        return configCheck;
    }

    QSettings os(m_confFile.absoluteFilePath(), QSettings::IniFormat);

    QVariantHash dbparams;
    os.beginGroup(QStringLiteral("Database"));
    for (const QString &key : os.childKeys()) {
        dbparams.insert(key, os.value(key));
    }
    os.endGroup();

    insertParamsDefault(dbparams, QStringLiteral("host"), QStringLiteral("localhost"));
    insertParamsDefault(dbparams, QStringLiteral("type"), QStringLiteral("QMYSQL"));
    insertParamsDefault(dbparams, QStringLiteral("port"), 3306);

    Database db;

    bool dbaccess = false;
    if (configExists && !dbparams.value(QStringLiteral("password")).toString().isEmpty()) {
        printTable({
                       {tr("Type"), dbparams.value(QStringLiteral("type")).toString()},
                       {tr("Host"), dbparams.value(QStringLiteral("host")).toString()},
                       {tr("Port"), dbparams.value(QStringLiteral("port")).toString()},
                       {tr("Name"), dbparams.value(QStringLiteral("name")).toString()},
                       {tr("User"), dbparams.value(QStringLiteral("user")).toString()},
                       {tr("Password"), QStringLiteral("********")}
                   }, tr("Database settings"));
        printStatus(tr("Establishing database connection"));
        dbaccess = db.open(dbparams);
        if (dbaccess) {
            printDone();
        } else {
            printFailed();
        }
    }

    if (!dbaccess) {
        printDesc(tr("Please enter the data to connect to your database system."));

        dbparams = askDatabaseConfig(dbparams);

        printStatus(tr("Establishing database connection"));

        if (!db.open(dbparams)) {
            printFailed();
            return dbError(db.lastDbError());
        } else {
            printDone();
        }

        os.beginGroup(QStringLiteral("Database"));
        QVariantHash::const_iterator i = dbparams.constBegin();
        while (i != dbparams.constEnd()) {
            os.setValue(i.key(), i.value());
            ++i;
        }
        os.endGroup();
        os.sync();
    }

    printStatus(tr("Checking database layout"));

    const QVersionNumber installedVersion = db.installedVersion();
    if (!installedVersion.isNull()) {
        printDone(installedVersion.toString());
    } else {
        printFailed();
        printStatus(tr("Performing database installation"));
        if (!db.installDatabase()) {
            printFailed();
            return dbError(db.lastDbError());
        } else {
            printDone();
        }
    }

    printStatus(tr("Searching for available admin accounts"));
    const uint adminCount = db.checkAdmin();
    if (adminCount > 0) {
        printDone(tr("Found %1").arg(adminCount));
    } else {
        printFailed(tr("None"));
    }

    QVariantHash adminsParams;

    os.beginGroup(QStringLiteral("Admins"));
    adminsParams.insert(QStringLiteral("pwalgorithm"), os.value(QStringLiteral("pwalgorithm"), SK_DEF_ADM_PWALGORITHM));
    adminsParams.insert(QStringLiteral("pwrounds"), os.value(QStringLiteral("pwrounds"), SK_DEF_ADM_PWROUNDS));
    adminsParams.insert(QStringLiteral("pwminlength"), os.value(QStringLiteral("pwminlength"), SK_DEF_ADM_PWMINLENGTH));
    os.endGroup();

    if (adminCount == 0) {
        printDesc(tr("Please configure your admin password settings and create a new admin user."));
        printDesc(QString());

        adminsParams = askPbkdf2Config(adminsParams);

        printMessage(QString());

        printDesc(tr("Create a super user administrator account to login into your Skaffari installation. This account can create further super user accounts and domain administrators."));

        const QString adminUser = readString(tr("User name"), QStringLiteral("admin"));
        const QString adminPass = readString(tr("Password"), QString());

        const QByteArray pw = Cutelyst::CredentialPassword::createPassword(adminPass.toUtf8(),
                                                                           static_cast<QCryptographicHash::Algorithm>(adminsParams.value(QStringLiteral("pwalgorithm")).value<quint8>()),
                                                                           adminsParams.value(QStringLiteral("pwrounds")).toInt(),
                                                                           24,
                                                                           27);

        printStatus(tr("Creating new admin account in database"));
        if (!db.setAdmin(adminUser, pw)) {
            printFailed();
            return dbError(db.lastDbError());
        }

        printDone();

        os.beginGroup(QStringLiteral("Admins"));
        QVariantHash::const_iterator i = adminsParams.constBegin();
        while (i != adminsParams.constEnd()) {
            os.setValue(i.key(), i.value());
            ++i;
        }
        os.endGroup();
        os.sync();

    } else if (readBool(tr("Do you want to set the admin password settings?"), false)) {

        adminsParams = askPbkdf2Config(adminsParams);

        os.beginGroup(QStringLiteral("Admins"));
        QVariantHash::const_iterator i = adminsParams.constBegin();
        while (i != adminsParams.constEnd()) {
            os.setValue(i.key(), i.value());
            ++i;
        }
        os.endGroup();
        os.sync();
    }


    if (!configExists || readBool(tr("Do you want to set the user password settings?"), false)) {

        printDesc(tr("Skaffari is designed to work together with pam_mysql. For this reason, passwords for user accounts are stored in a different format than those for administrator accounts. The format of the user passwords must be compatible with the methods supported by pam_mysql. You can find out more about the supported methods in the README of your pam_mysql installation."));
        printDesc(QString());

        os.beginGroup(QStringLiteral("Accounts"));
        Password::Method accountsPwMethod = static_cast<Password::Method>(os.value(QStringLiteral("pwmethod"), SK_DEF_ACC_PWMETHOD).value<quint8>());
        Password::Algorithm accountsPwAlgo = static_cast<Password::Algorithm>(os.value(QStringLiteral("pwalgorithm"), SK_DEF_ACC_PWALGORITHM).value<quint8>());
        quint32 accountsPwRounds = os.value(QStringLiteral("pwrounds"), SK_DEF_ACC_PWROUNDS).value<quint32>();
        quint8 accountsPwMinLength = os.value(QStringLiteral("pwminlength"), SK_DEF_ACC_PWMINLENGTH).value<quint8>();
        os.endGroup();

        accountsPwMethod = static_cast<Password::Method>(readChar(tr("Encryption method"),
                                                              static_cast<quint8>(accountsPwMethod),
                                                              QStringList({
                                                                              tr("The basic method for encryption of user passwords. Some methods support additional settings and different algorithms which can be set in the next step. If possible, you should use crypt(3) because this method supports modern hash functions together with salts and an extensible storage format. The other methods are available for compatibility reasons."),
                                                                              tr("Supported methods:"),
                                                                              tr("0: no encryption - highly discouraged"),
                                                                              tr("1: crypt(3) function - recommended"),
                                                                              tr("2: MySQL password function"),
                                                                              tr("3: plain hex MD5 - not recommended"),
                                                                              tr("4: plain hex SHA1 - not recommended")
                                                                          }),
                                                              QList<quint8>({0,1,2,3,4})));
        if (accountsPwMethod == Password::Crypt) {
            accountsPwAlgo = static_cast<Password::Algorithm>(readChar(tr("Encryption algorithm"),
                                                                      static_cast<quint8>(accountsPwAlgo),
                                                                      QStringList({
                                                                                      tr("The method crypt(3) supports different algorithms to derive a key from a password. To find out which algorithms are supported on your system, use man crypt. Especially bcrypt, which uses Blowfish, is not available on all systems because it is not part of the standard distribution of crypt(3). The non-recommended algorithms are available for compatibility reasons and to store passwords across operating system boundaries."),
                                                                                      QString(),
                                                                                      tr("Supported algorithms:"),
                                                                                      tr("0: Default - points to SHA-256"),
                                                                                      tr("1: Traditional DES-based - not recommended"),
                                                                                      tr("2: FreeBSD-style MD5-based - not recommended"),
                                                                                      tr("3: SHA-256 based"),
                                                                                      tr("4: SHA-512 based"),
                                                                                      tr("5: OpenBSD-style Blowfish-based (bcrypt) - not supported everywhere")
                                                                                  }),
                                                                      QList<quint8>({0,1,2,3,4,5})));
        } else if (accountsPwMethod == Password::MySQL) {
            accountsPwAlgo = static_cast<Password::Algorithm>(readChar(tr("Encryption algorithm"),
                                                                      accountsPwAlgo,
                                                                      QStringList({
                                                                                      tr("MySQL supports two different hash functions, a new and an old one. If possible, you should use the new function. The old function is provided for compatibility reasons."),
                                                                                      QString(),
                                                                                      tr("Supported algorithms:"),
                                                                                      tr("0: default - points to MySQL new"),
                                                                                      tr("6: MySQL new"),
                                                                                      tr("7: MySQL old")
                                                                                  }),
                                                                      QList<quint8>({0,6,7})));
        }

        if (accountsPwMethod == Password::Crypt) {
            if ((accountsPwAlgo == Password::CryptSHA256) || (accountsPwAlgo == Password::CryptSHA512) || (accountsPwAlgo == Password::CryptBcrypt)) {
                QStringList accountsPwRoundsDesc = (accountsPwAlgo == Password::CryptBcrypt) ? QStringList(tr("Crypt(3) with bcrypt supports an iteration count to increase the time cost for creating the derived key. The iteration count passed to the crypt function is the base-2 logarithm of the actual iteration count. Supported values are between 4 and 31.")) : QStringList(tr("Crypt(3) with SHA-256 and SHA-512 supports an iteration count from 1000 to 999999999. The iterations are used to increase the time cost for creating the derived key."));
                if ((accountsPwAlgo == Password::CryptBcrypt) && (accountsPwRounds > 31)) {
                    accountsPwRounds = 12;
                }

                accountsPwRounds = readInt(tr("Encryption rounds"), accountsPwRounds, accountsPwRoundsDesc);
            }
        }

        accountsPwMinLength = readChar(tr("Password minimum length"), accountsPwMinLength, QStringList(tr("The required minimum length for user account passwords created or changed via Skaffari.")));

        os.beginGroup(QStringLiteral("Accounts"));
        os.setValue(QStringLiteral("pwmethod"), static_cast<quint8>(accountsPwMethod));
        os.setValue(QStringLiteral("pwalgorithm"), static_cast<quint8>(accountsPwAlgo));
        os.setValue(QStringLiteral("pwrounds"), accountsPwRounds);
        os.setValue(QStringLiteral("pwminlength"), accountsPwMinLength);
        os.endGroup();
        os.sync();
    }

    printStatus(tr("Checking for IMAP admin account"));
    QString cyrusAdmin = db.checkCyrusAdmin();
    if (!cyrusAdmin.isEmpty()) {
        printDone(cyrusAdmin);
    } else {
        //: status for not finding any IMAP admin users
        printFailed(tr("None"));

        printDesc(tr("The administrator user for the IMAP server is defined in the imapd.conf in the admins: key. The user name that you enter here must also be specified in the imapd.conf. The administrator is used to perform various tasks on the IMAP server, such as setting storage quotas and creating/deleting mailboxes and folders. The user is created in the database used for Skaffari."));

        cyrusAdmin = readString(tr("IMAP admin user"), QStringLiteral("cyrus"));
        Password cyrusAdminPw(readString(tr("IMAP admin password"), QString()));

        printStatus(tr("Creating IMAP admin user in database"));
        os.beginGroup(QStringLiteral("Accounts"));
        Password::Method accountsPwMethod = static_cast<Password::Method>(os.value(QStringLiteral("pwmethod"), SK_DEF_ACC_PWMETHOD).value<quint8>());
        Password::Algorithm accountsPwAlgo = static_cast<Password::Algorithm>(os.value(QStringLiteral("pwalgorithm"), SK_DEF_ACC_PWALGORITHM).value<quint8>());
        quint32 accountsPwRounds = os.value(QStringLiteral("pwrounds"), SK_DEF_ACC_PWROUNDS).value<quint32>();
        os.endGroup();
        QByteArray cyrusAdminPwEnc = cyrusAdminPw.encrypt(accountsPwMethod, accountsPwAlgo, accountsPwRounds);
        if (cyrusAdminPwEnc.isEmpty()) {
            printFailed();
            return configError(tr("Failed to encrypt Cyrus admin password."));
        }

        if (!db.setCryusAdmin(cyrusAdmin, cyrusAdminPwEnc)) {
            printFailed();
            return dbError(db.lastDbError());
        }

        printDone();
    }

    QVariantHash imapParams;

    os.beginGroup(QStringLiteral("IMAP"));
    for (const QString &key : os.childKeys()) {
        imapParams.insert(key, os.value(key));
    }
    os.endGroup();

    insertParamsDefault(imapParams, QStringLiteral("host"), QStringLiteral("localhost"));
    insertParamsDefault(imapParams, QStringLiteral("port"), 134);
    insertParamsDefault(imapParams, QStringLiteral("protocol"), SK_DEF_IMAP_PROTOCOL);
    insertParamsDefault(imapParams, QStringLiteral("encryption"), SK_DEF_IMAP_ENCRYPTION);

    bool imapaccess = false;
    Imap imap;

    if (configExists && !imapParams.value(QStringLiteral("password")).toString().isEmpty()) {
        printTable({
                       {tr("Host"), imapParams.value(QStringLiteral("host")).toString()},
                       {tr("Port"), imapParams.value(QStringLiteral("port")).toString()},
                       {tr("Protocol"), Imap::networkProtocolToString(imapParams.value(QStringLiteral("protocol")).value<quint8>())},
                       {tr("Encryption"), Imap::encryptionTypeToString(imapParams.value(QStringLiteral("encryption")).value<quint8>())},
                       {tr("User"), imapParams.value(QStringLiteral("user")).toString()},
                       {tr("Password"), QStringLiteral("********")},
                       {tr("Peer name"), imapParams.value(QStringLiteral("peername")).toString()}
                   }, tr("IMAP settings"));

        printStatus(tr("Establishing IMAP connection"));
        imap.setParams(imapParams);
        imapaccess = imap.login();
        if (Q_LIKELY(imapaccess)) {
            if (Q_UNLIKELY(!imap.logout())) {
                printFailed();
                return imapError(imap.lastError());
            }
            printDone();
        } else {
            printFailed();
        }
    }

    if (imapParams.value(QStringLiteral("password")).toString().isEmpty() || !imapaccess || readBool(tr("Do you want to set the IMAP connection settings?"), false)) {

        if (!imapaccess) {
            printDesc(tr("Connection to your IMAP server failed. Please reenter your connection data."));
        } else {
            printDesc(tr("Please enter the data to connect to your IMAP server."));
        }
        printDesc(QString());
        printDesc(tr("Connection to your IMAP server as admin is used to perform tasks like setting quotas and creating/deleting mailboxes and folders. The user account has to be defined as admin in the imapd.conf file in the admins: key."));
        printDesc(QString());

        imapParams = askImapConfig(imapParams);

        printStatus(tr("Establishing IMAP connection"));
        imap.setParams(imapParams);
        imapaccess = imap.login();
        if (Q_LIKELY(imapaccess)) {
            if (Q_UNLIKELY(!imap.logout())) {
                printFailed();
                return imapError(imap.lastError());
            }
            printDone();
        } else {
            printFailed();
            return imapError(imap.lastError());
        }

        os.beginGroup(QStringLiteral("IMAP"));
        QVariantHash::const_iterator i = imapParams.constBegin();
        while (i != imapParams.constEnd()) {
            os.setValue(i.key(), i.value());
            ++i;
        }
        os.endGroup();
        os.sync();

    }

    if (!configExists || readBool(tr("Do you want to set the IMAP behavior settings?"), false)) {

        printDesc(tr("Depending on the settings of your IMAP server, you can set different behavior of Skaffari when managing your email accounts."));
        printDesc(QString());

        insertParamsDefault(imapParams, QStringLiteral("unixhierarchysep"), SK_DEF_IMAP_UNIXHIERARCHYSEP);
        insertParamsDefault(imapParams, QStringLiteral("domainasprefix"), SK_DEF_IMAP_DOMAINASPREFIX);
        insertParamsDefault(imapParams, QStringLiteral("fqun"), SK_DEF_IMAP_FQUN);
        insertParamsDefault(imapParams, QStringLiteral("createmailbox"), SK_DEF_IMAP_CREATEMAILBOX);

        bool unixHierarchySep   = imapParams.value(QStringLiteral("unixhierarchysep")).toBool();
        bool domainAsPrefix     = imapParams.value(QStringLiteral("domainasprefix")).toBool();
        bool fqun               = imapParams.value(QStringLiteral("fqun")).toBool();
        quint8 createmailbox    = imapParams.value(QStringLiteral("createmailbox")).value<quint8>();

        unixHierarchySep = readBool(tr("UNIX hierarchy separator"),
                                    unixHierarchySep,
                                    QStringList({
                                                    tr("This setting should correspond to the value of the same setting in your imapd.conf(5) file and indicates that your imap server uses the UNIX separator character '/' for delimiting levels of mailbox hierarchy instead of the netnews separator character '.'. Up to Cyrus-IMAP 2.5.x the default value for this value in the IMAP server configuration is off, beginning with version 3.0.0 of Cyrus-IMAP the default has changed to on.")
                                                })
                                    );
        imapParams.insert(QStringLiteral("unixhierarchysep"), unixHierarchySep);

        if (unixHierarchySep) {

            domainAsPrefix = readBool(tr("Domain as prefix"),
                                      domainAsPrefix,
                                      QStringList({
                                                      tr("If enabled, usernames will be composed from the email local part and the domain name, separated by a dot instead of an @ sign. Like user.example.com. If you want to use real email addresses (fully qualified user names aka. fqun) like user@example.com as user names, you also have to set fqun to true in the next step."),
                                                      tr("For domains, the prefix will automatically be the same as the domain name when enabling this option."),
                                                      tr("NOTE: you have to set the following line in your imapd.conf file:"),
                                                      QStringLiteral("unixhierarchysep: yes")
                                                  })
                                      );
            imapParams.insert(QStringLiteral("domainasprefix"), domainAsPrefix);
        } else {
            imapParams.insert(QStringLiteral("domainasprefix"), false);
        }

        if (unixHierarchySep && domainAsPrefix) {

            fqun = readBool(tr("Fully qualified user name"),
                            fqun,
                            QStringList({
                                            tr("If you wish to use user names like email addresses (aka. fully qualified user name) you can activate this option."),
                                            tr("NOTE: you also have to add this lines to your imapd.conf file:"),
                                            QStringLiteral("unixhierarchysep: yes"),
                                            QStringLiteral("virtdomains: yes")
                                        })
                            );
            imapParams.insert(QStringLiteral("fqun"), fqun);
        } else {
            imapParams.insert(QStringLiteral("fqun"), false);
        }

        createmailbox = readChar(tr("Create mailboxes"),
                                 createmailbox,
                                 QStringList({
                                                 tr("Skaffari can create the mailboxes and all default folders on the IMAP server after creating a new user account. Alternatively the IMAP server can create default folders and account quotas on the first user login or first incoming email for the new account (has to be configured in your imapd.conf file). Skaffari is more flexible on creating different default folders for different domains."),
                                                 tr("Available behavior:"),
                                                 tr("0: Skaffari does nothing - all will be created by the IMAP server on first login or first incoming email"),
                                                 tr("1: Login after creation - Skaffari relies on the IMAP server to create folders and quotas, but will perform a login after account creation to initiate the creation by the IMAP server"),
                                                 tr("2: Only set quota - Skaffari will login to the new account after creation to let the IMAP server create the mailbox and will then set the quota"),
                                                 tr("3: Create by Skaffari - Skaffari will create the new mailbox and the default folders and will set the account quota after adding a new account")
                                             }),
                                 QList<quint8>({0,1,2,3})
                                 );
        imapParams.insert(QStringLiteral("createmailbox"), createmailbox);

        os.beginGroup(QStringLiteral("IMAP"));
        QVariantHash::const_iterator i = imapParams.constBegin();
        while (i != imapParams.constEnd()) {
            os.setValue(i.key(), i.value());
            ++i;
        }
        os.endGroup();
        os.sync();
    }

    printSuccess(tr("Skaffari setup was successful."));

    return 0;
}

void Setup::insertParamsDefault(QVariantHash &params, const QString &key, const QVariant &defVal)
{
    if (!params.value(key).isValid()) {
        params.insert(key, defVal);
    }
}
