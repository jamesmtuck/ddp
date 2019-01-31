#ifndef SQLITE3_HELPER_H
#define SQLITE3_HELPER_H

#include <sqlite3.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <string>
#include <sstream>

namespace llvm {

  enum column_kind {
    SQL_INT=0,
    SQL_INT64,
    SQL_DOUBLE,
    SQL_TEXT
  };

  class TableColumn {
    static const char* column_kind_name[4];
    std::string name;
    column_kind kind;

    union {
      int       i;
      int64_t   i64;
      double    d;
      const char  *text;
    } val;

  public:
    int getInt()       { return val.i;    }
    int getInt64()       { return val.i64;    }
    double getDouble() { return val.d;    }
    const char *getText()    { return val.text; }

    TableColumn(std::string aname, column_kind k)
    :name(aname), kind(k) {}

    TableColumn(int i, std::string aname=""):name(aname) {
      val.i = i;
      kind = SQL_INT;
    }
    TableColumn(int64_t i, std::string aname=""):name(aname) {
      val.i64 = i;
      kind = SQL_INT;
    }
    TableColumn(double d, std::string aname=""):name(aname) {
      val.d = d;
      kind = SQL_DOUBLE;
    }
    TableColumn(const char *t, std::string aname=""):name(aname) {
      val.text = strdup(t);
      kind = SQL_TEXT;
    }
    static TableColumn &createInt64(std::string name) {
       return *(new TableColumn(name,SQL_INT64));
    }
    static TableColumn &createInt(std::string name) {
       return *(new TableColumn(name,SQL_INT));
    }
    static TableColumn &createDouble(std::string name) {
       return *(new TableColumn(name,SQL_DOUBLE));
    }
    static TableColumn &createText(std::string name) {
       return *(new TableColumn(name,SQL_TEXT));
    }

    column_kind getKind() {
      return kind;
    }

    std::string toConstraint() {
       std::string ret="";
       ret = name ;
       std::stringstream out;
       switch(kind) {
       case SQL_INT: out << " = " << val.i; break;
       case SQL_INT64: out<< " = " << val.i64; break;
       case SQL_DOUBLE: out << " = " << val.d; break;
       case SQL_TEXT: out << " like " << "\"" << val.text << "\""; break;
       }
       return ret + out.str();
    }

    std::string toString(bool withType=true) {
      if (!withType)
        return name;

      return name + " " + std::string(column_kind_name[kind]);
    }
  };

  class TableRow {
  protected:
    std::vector<TableColumn> row;

  public:

    typedef std::vector<TableColumn>::iterator iterator;
    iterator begin() { return row.begin(); }
    iterator end() { return row.end(); }

    size_t size() { return row.size(); }
    TableColumn &get(size_t i) {
       return row[i];
    }

    TableRow &add(TableColumn c) {
      row.push_back(c);
      return *this;
    }

    TableRow &addRefID() {
      row.push_back(TableColumn::createInt64("refID"));
      return *this;
    }

    std::string toString(bool withType=true) {
      std::vector<TableColumn>::iterator it;
      std::string ret="";
      for(it=row.begin(); it!=row.end(); it++) {
        if (it!=row.begin())
          ret = ret + ", ";
        ret = ret + (*it).toString(withType);
      }
      return ret;
    }

    std::string toConstraint() {
      std::vector<TableColumn>::iterator it;
      std::string ret="";
      for(it=row.begin(); it!=row.end(); it++) {
        if (it!=row.begin())
          ret = ret + " and ";
        ret = ret + (*it).toConstraint();
      }
       return ret;
    }

    std::string toBindString(bool withType=true) {
      std::vector<TableColumn>::iterator it;
      std::string ret="";
      for(it=row.begin(); it!=row.end(); it++) {
        if (it!=row.begin())
          ret = ret + ", ";
        ret = ret + "?";
      }
      return ret;
    }
  };

  class ProfilerTable {
  protected:
    std::string name;
    TableRow row;

  public:
    ProfilerTable(std::string aname) : name(aname) {}
    ProfilerTable(std::string aname, TableRow &arow)
    :name(aname),row(arow) {}

    std::string& getName() { return name; }

    TableRow& add(TableColumn col) {
      return row.add(col);
    }

    std::string getCreateTableCommand() {
      return "( " + row.toString() + " )";
    }

    std::string getInsertCommand() {
      return "( " + row.toString(false) + " )";
    }

    std::string getBindList() {
      return "( " + row.toBindString() + " )";
    }

    TableRow& getRow() { return row; }
  };

  /*
  class SQLite3Table {
  public:
    std::string name;
    std::list< SQLite3Column > table;
    void addColumn(std::string name, std::string type);
  };

  class SQLite3TableSelect {
    SQLite3TableSelect(SQLiteTable &table);
    void selectColumn(std::string);
  }
  */
  bool SQLite3Open(sqlite3 **db, std::string filename, bool create=false);
  bool SQLite3BeginImmediate(sqlite3 *db);
  bool SQLite3EndTransaction(sqlite3 *db);
  bool SQLite3CreateTable(sqlite3 *db, ProfilerTable &table);
  bool SQLite3Insert(sqlite3 *db, ProfilerTable &table, TableRow &values);
  std::vector<TableRow> SQLite3Select(sqlite3 *db, ProfilerTable &table,
                                  TableRow &select, TableRow &constraints);
}

#endif //SQLITE3_HELPER_H
