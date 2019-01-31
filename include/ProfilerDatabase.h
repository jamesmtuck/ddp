#ifndef PROFILER_DATABASE_H
#define PROFILER_DATABASE_H

#include <string>
#include <iostream>
#include "DatabaseFileSysHelper.h"
#include "SQLite3Helper.h"
#include "UniqueRefID.h"
#include <map>
#include <cassert>

namespace llvm {

  class ProfilerDatabase;

  class DBFileManager {
  protected:
    std::string origin;
    DatabaseFileSysHelper fileHelper;
    std::vector<ProfilerDatabase*> dbList;

  private:
    DBFileManager()
      :origin("./"),
      AppName("unknown") {}

    ~DBFileManager();

    static DBFileManager singleton;
    void init(std::string aprefix, std::string file, std::string appname);

  public:
    std::string& getOrigin() { return origin; }

    std::string getFullPath(std::string dbfile) {
      return fileHelper.getFullPath(dbfile);
    }

    std::string getPrefix() {
      return fileHelper.getPrefix();
    }

    static DBFileManager &getSingleton();
    static void Initialize(std::string dbPrefix, std::string file, std::string appname);

    static void addProfiler(ProfilerDatabase *p) {
      singleton.dbList.push_back(p);
    }

  public:
    std::string AppName;
  };

  class ProfilerDatabase {
  private:
    std::string name;
    sqlite3* fileDB;

    unsigned long long startID;
    unsigned long long fileID;
    UniqueRefID *refID;
    bool updateFile;

    static ProfilerTable createFileTable() {
      ProfilerTable files("files");
      files.add(TableColumn("app",SQL_TEXT))
           .add(TableColumn("name",SQL_TEXT))
           .add(TableColumn("fileid",SQL_INT64));
      return files;
    }

    static TableRow createFileRow(unsigned long long fileid) {
      TableRow row;
      DBFileManager &Manager = DBFileManager::getSingleton();
      row.add(TableColumn(Manager.AppName.c_str()))
	       .add(TableColumn(Manager.getOrigin().c_str()))
         .add(TableColumn((int64_t)fileid));
      return row;
    }

  ProfilerDatabase(std::string aName, std::string fullname, sqlite3* db,
                    unsigned long long fileid, unsigned long long startid)
      :name(aName)
      ,fileDB(db)
      ,startID(startid)
      ,fileID(fileid)
      ,refID(NULL)
      ,updateFile(false)
      ,fullDBName(fullname) {
        refID = new UniqueRefID((unsigned long long)startID);
        DBFileManager::addProfiler(this);
    }

    std::string fullDBName;

  ProfilerDatabase(std::string aName, std::string fullName, sqlite3* db, UniqueRefID *ref)
      :name(aName)
      ,fileDB(db)
      ,refID(ref)
      ,fileID(0)
      ,updateFile(true)
      ,fullDBName(fullName) {
        startID = refID->get();
        DBFileManager::addProfiler(this);
    }

    std::map<unsigned long long, unsigned long long> feedbackMap;

  public:

    static ProfilerDatabase * CreateOrFind(std::string name);

    std::string getFullDBName() {
      return fullDBName;
    }

    void addTable(ProfilerTable &table);

    void insertValues(ProfilerTable &table, TableRow &row);

    //std::vector<TableRow> fetchRows(ProfilerTable &table, TableRow &select);

    bool isConnected() {
      return fileDB != NULL;
    }

    ~ProfilerDatabase() {
        if (isConnected()) {
          if(updateFile) {
            TableRow row = createFileRow(fileID);
            ProfilerTable table=createFileTable();
            SQLite3Insert(fileDB,table,row);
          }
          sqlite3_close(fileDB);
        }
      //  if(refID)
          //delete refID;
    }

    unsigned long long inc() {
      return refID->inc();
    }
    unsigned long long get() {
      return refID->get();
    }

    unsigned long long feedbackValue(std::string profname, unsigned long long refID);

    unsigned long long getFileID() {
      return fileID;
    }
  };

}

#endif //PROFILER_DATABASE_H
