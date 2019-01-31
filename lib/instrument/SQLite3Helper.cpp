#include <sqlite3.h>
#include "SQLite3Helper.h"
#include <string>
#include <iostream>
#include <string.h>

using namespace llvm;

const char* llvm::TableColumn::column_kind_name[4] = {
  "integer",
  "integer",
  "double",
  "text"
};

bool llvm::SQLite3Open(sqlite3 **db, std::string filename, bool create) {
  int status = SQLITE_OPEN_READWRITE;
  if (create)
    status |= SQLITE_OPEN_CREATE;

  int rc = sqlite3_open_v2(filename.c_str(),db, status, NULL);
  if( rc ){
    std::cerr << "Can't open database: %s\n" << sqlite3_errmsg(*db);
    sqlite3_close(*db);
    return false;
  }
  return true;
}

bool llvm::SQLite3EndTransaction(sqlite3 *db) {
  std::string command;
  command =  "END TRANSACTION";
  sqlite3_stmt * stmt;
  int result;
  do {
     result = sqlite3_prepare_v2(db, command.c_str(), command.size()+1, &stmt, NULL);
  } while ( SQLITE_LOCKED == result || SQLITE_BUSY == result);

  if (result) {
    std::cerr << "sqlite3_prepare_v2 - Can't end transaction: %s\n"
              << sqlite3_errmsg(db);
    return false;
  }

  do {
     result=sqlite3_step(stmt);
  } while ( SQLITE_LOCKED == result || SQLITE_BUSY == result);

  if (result!=SQLITE_DONE) {
    std::cerr << "sqlite3_step - Can't end transaction: %s\n" << sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    return false;
  }

  sqlite3_finalize(stmt);
  return true;
}

bool llvm::SQLite3BeginImmediate(sqlite3 *db) {
  std::string command;
  command =  "BEGIN IMMEDIATE";
  sqlite3_stmt * stmt;
  int result;
  do {
     result = sqlite3_prepare_v2(db, command.c_str(), command.size()+1, &stmt, NULL);
  } while ( SQLITE_LOCKED == result || SQLITE_BUSY == result);

  if (result) {
    std::cerr << "sqlite3_prepare_v2 - Can't begin transaction: %s\n" << sqlite3_errmsg(db);
    return false;
  }

  do {
     result=sqlite3_step(stmt);
  } while ( SQLITE_LOCKED == result || SQLITE_BUSY == result);

  if (result!=SQLITE_DONE) {
    std::cerr << "sqlite3_step - Can't begin transaction: %s\n" << sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    return false;
  }

  sqlite3_finalize(stmt);

  return true;
}

bool llvm::SQLite3CreateTable(sqlite3 *db, ProfilerTable &table) {
  std::string command;
  command =  "create table if not exists " + table.getName() + " ";
  command += table.getCreateTableCommand();
  sqlite3_stmt * stmt;
  int result = sqlite3_prepare_v2(db, command.c_str(), command.size()+1, &stmt, NULL);
  if (result) {
    std::cerr << "Can't create table: %s\n" << sqlite3_errmsg(db);
    return false;
  }

  int ret;
  while(SQLITE_BUSY == (ret=sqlite3_step(stmt)));

  if (ret!=SQLITE_DONE) {
    std::cerr << "Can't create table: %s\n" << sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    return false;
  }

  sqlite3_finalize(stmt);
  return true;
}

bool llvm::SQLite3Insert(sqlite3 *db, ProfilerTable &table, TableRow &values) {
  std::string command;
  command =  "insert or replace into  " + table.getName() + " ";
  command += table.getInsertCommand();
  command += " values " + table.getBindList();
  sqlite3_stmt * stmt;
  int result = sqlite3_prepare_v2(db, command.c_str(), command.size()+1, &stmt, NULL);
  if (result) {
    std::cerr << "Can't insert into table: " << sqlite3_errmsg(db);
    return false;
  }

  TableRow::iterator it;
  int i = 1;
  for(it=values.begin(); it!=values.end(); it++) {
    switch ( (*it).getKind() ) {
      case SQL_INT: sqlite3_bind_int(stmt, i, (*it).getInt());
                    break;
      case SQL_INT64: sqlite3_bind_int64(stmt, i, (sqlite3_int64)(*it).getInt64());
                      break;
      case SQL_DOUBLE: sqlite3_bind_double(stmt, i, (*it).getDouble());
                        break;
      case SQL_TEXT: sqlite3_bind_text(stmt, i, (*it).getText(), -1,
                                     SQLITE_TRANSIENT); break;
    }
    i++;
  }

  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return true;
}

std::vector<TableRow> llvm::SQLite3Select(sqlite3 *db, ProfilerTable &table,
                                          TableRow &select, TableRow &constraints) {
  std::vector<TableRow> v;
  std::string command;
  command =  "select " + select.toString(false) + " from ";
  command += table.getName();
  std::string constraint = constraints.toConstraint();
  if (constraints.size()>0)
    command += " where " + constraints.toConstraint();

  //std::cerr << "Command: " << command;

  sqlite3_stmt * stmt;
  int result = sqlite3_prepare_v2(db, command.c_str(), command.size()+1, &stmt, NULL);
  if (result) {
    std::cerr << "Can't perform selection: " << sqlite3_errmsg(db);
    return v;
  }

  int s;
  do {
    s = sqlite3_step(stmt);
    TableRow row;
    TableRow &trow = select;
    if (SQLITE_ROW == s) {
      TableRow::iterator it;
      int i=0;
      for(it=trow.begin(); it!=trow.end(); it++) {
        switch ( (*it).getKind() ) {
        case SQL_INT:
          row.add(TableColumn(sqlite3_column_int(stmt, i)));
          break;
        case SQL_INT64:
          row.add(TableColumn((int64_t)sqlite3_column_int64(stmt, i)));
          break;
        case SQL_DOUBLE:
          row.add(TableColumn((double)sqlite3_column_double(stmt, i)));
          break;
        case SQL_TEXT:
          row.add(TableColumn((const char*)strdup(
                          (const char*)sqlite3_column_text(stmt, i))));
          break;
        }
        i++;
      }
      v.push_back(row);
    }
  } while (s == SQLITE_ROW) ;

  sqlite3_finalize(stmt);
  return v;
}
