/*
 * Skaffari - a mail account administration web interface based on Cutelyst
 * Copyright (C) 2017-2018 Matthias Fehring <mf@huessenbergnetz.de>
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

#ifndef SKAFFARIIMAPERROR_H
#define SKAFFARIIMAPERROR_H

#include <QString>
#include <QSharedDataPointer>

class QSslError;

/*!
 * \ingroup skaffaricore
 * \brief Contains information about occurred IMAP errors.
 */
class SkaffariIMAPError
{
public:
    enum ErrorType {
        NoError,
        NoResponse,
        BadResponse,
        UndefinedResponse,
        EmptyResponse,
        ConnectionTimeout,
        EncryptionError,
        SocketError,
        ResponseError,
        InternalError,
        ConfigError,
        Unknown
    };

    explicit SkaffariIMAPError(ErrorType type = NoError, const QString &errorText = QString());
    explicit SkaffariIMAPError(const QSslError &sslError);
    SkaffariIMAPError(const SkaffariIMAPError &other);
    SkaffariIMAPError(SkaffariIMAPError &&other) noexcept;
    SkaffariIMAPError& operator=(const SkaffariIMAPError &other);
    SkaffariIMAPError& operator=(SkaffariIMAPError &&other) noexcept;
    ~SkaffariIMAPError();
    void swap(SkaffariIMAPError &other) noexcept;
    bool operator==(const SkaffariIMAPError &other) const;
    bool operator!=(const SkaffariIMAPError &other) const;

    ErrorType type() const;
    QString errorText() const;
    QString text() const;

    void clear();

private:
    class Data;
    QSharedDataPointer<Data> d;
};

#endif // SKAFFARIIMAPERROR_H
