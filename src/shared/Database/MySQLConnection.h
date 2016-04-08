/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MySQLConnection_H
#define __MySQLConnection_H

#ifdef _WIN32
#include <winsock2.h>
#endif

#include <mysql.h>
#include "QueryResult.h"

struct MySQLConnectionInfo
{
    MySQLConnectionInfo() : host(""), port_or_socket(""), database(""), user(""), password("") { }
    explicit MySQLConnectionInfo(std::string host, std::string port, std::string database, std::string username, std::string password)
    {
        this->host.assign(host);
        this->port_or_socket.assign(port);
        this->user.assign(username);
        this->password.assign(password);
        this->database.assign(database);
    }

    std::string user;
    std::string password;
    std::string database;
    std::string host;
    std::string port_or_socket;
};

class MySQLConnection
{
    public:
        MySQLConnection();                               //! Constructor for synchronous connections.
        ~MySQLConnection();

        bool Open(MySQLConnectionInfo connectInfo);
        void Close();
    public:
        bool Execute(const char* sql);
        QueryResult Query(const char* sql);
        bool _Query(const char *sql, MYSQL_RES **pResult, MYSQL_FIELD **pFields, uint64* pRowCount, uint32* pFieldCount);
\
        operator bool () const { return m_Mysql != NULL; }
        void Ping() { mysql_ping(m_Mysql); }
        uint32 GetLastError() { return mysql_errno(m_Mysql); }
        MYSQL* GetHandle()  { return m_Mysql; }
    protected:
        bool                                 m_reconnecting;  //! Are we reconnecting?
        bool                                 m_prepareError;  //! Was there any error while preparing statements?
    private:
        bool _HandleMySQLErrno(uint32 errNo);
        MYSQL *               m_Mysql;                      //! MySQL Handle.
        MySQLConnectionInfo   m_connectionInfo;             //! Connection info (used for logging)

        MySQLConnection(MySQLConnection const& right) = delete;
        MySQLConnection& operator=(MySQLConnection const& right) = delete;
};

#endif
