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

#ifndef SKVALIDATORUNIQUEDB_H
#define SKVALIDATORUNIQUEDB_H

#include <Cutelyst/Plugins/Utils/ValidatorRule>

class SkValidatorUniqueDb : public Cutelyst::ValidatorRule
{
public:
    enum ColumnType {
        General         = 0,
        DomainName      = 1,
        EmailAddress    = 2
    };

    SkValidatorUniqueDb(const QString &field, const QString &table, const QString &column, ColumnType colType = General, const QString &label = QString(), const QString &customError = QString());

    ~SkValidatorUniqueDb();

    QString validate() const override;

protected:
    QString genericValidationError() const override;

private:
    QString m_table;
    QString m_column;
    ColumnType m_columnType;
};

#endif // SKVALIDATORUNIQUEDB_H