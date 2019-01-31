#include "ProfilerDatabase.h"
#include "SQLite3Helper.h"
#include <cmath>

using namespace llvm;

/*
 *
 * ProfilerDBManager
 *
 */

DBFileManager DBFileManager::singleton;
//std::vector<ProfilerDatabase*> DBFileManager::dbList;

void DBFileManager::init(std::string aprefix, std::string file, std::string appname) {
  fileHelper.setPrefix(aprefix);
  origin = file;
  AppName = appname;
}

DBFileManager::~DBFileManager() {
  size_t i=0;
  for(i = 0; i < dbList.size(); i++)
    delete dbList[i];
}

ProfilerDatabase* ProfilerDatabase::CreateOrFind(std::string profname) {
  DBFileManager &Manager = DBFileManager::getSingleton();
  std::string fullname = Manager.getFullPath(profname+".db");
  std::cerr << "Database name is " << fullname << "\n";
  sqlite3 *fileDB = NULL;
  bool noDBSupport = false;
  if(!SQLite3Open(&fileDB, fullname,true)) {
    int t=0;
    for(t=0; t<3; t++) {
      fileDB=NULL;
      if(SQLite3Open(&fileDB, fullname,true))
        break;
    }
    if (t==3) {
      std::cerr << "Could not open database " << fullname << "!\n";
      std::cerr << "Warning: Running without database support.\n";
      noDBSupport = true;
    }
  }

  if (noDBSupport ) {
    return new ProfilerDatabase(profname, fullname, NULL,
                                new RefIDFromFile( Manager.getFullPath(profname+".refID") ));
  }

  ProfilerTable files = createFileTable();
  TableRow fileValues;

  SQLite3CreateTable(fileDB,files);

  TableRow select;
  select.add(TableColumn("fileid",SQL_INT64)).add(TableColumn("name",SQL_TEXT));
  TableRow constraint;

  constraint.add(TableColumn(Manager.getOrigin().c_str(),"name"));

  std::vector<TableRow> result = SQLite3Select(fileDB,files,select,constraint);

  UniqueRefID * refID;
  if (result.size()>0) {
    // Pick the last one
    TableRow &row = result[ result.size() - 1 ];
    // Get refID out of it
    TableColumn &c = row.get(0);
    //assert(row.size()==3);
    //assert(c.getKind()==SQL_INT64);
    int64_t id = c.getInt64();
    return new ProfilerDatabase(profname,fullname,fileDB,id,0);
  } else {
     // First check if the app exists - If yes, get the max fileid for the app,
     // increment it and write it back.
     // If app does not exist, get the max fileid in the table and ceil it to
     // closest 1000. This is done to ensure that each app's fileids stay consecutive.
     // FIXME: Assumption: A benchmark will never have more than 1000 files in
     // it. Needs something more intelligent.
     TableRow resultRows;
     resultRows.add(TableColumn("count(fileid)",SQL_INT64));
     resultRows.add(TableColumn("max(fileid)",SQL_INT64));
     TableRow appSelector;
     appSelector.add(TableColumn(Manager.AppName.c_str(),"app"));
     int64_t id;
     if(!SQLite3BeginImmediate(fileDB)) {
       std::cerr<<"Unable to make new entry in files table - BEGIN IMMEDIATE\n";
       exit(-1);
     }
     std::vector<TableRow> result = SQLite3Select(fileDB,files,resultRows,appSelector);
     if(result.size() > 0 && result[0].get(0).getInt64() > 0) {
        id = result[0].get(1).getInt64() + 1;
        assert(id<1000);
     } else {
        TableRow noConstraints;
        std::vector<TableRow> allresult =
                            SQLite3Select(fileDB,files,resultRows,noConstraints);
        if(allresult.size() > 0) {
           int maxid = allresult[0].get(1).getInt64();
           id = 1000 * static_cast<int>(ceil((maxid + 1.0)/1000.0));
        }
     }
     TableRow row = createFileRow(id);
     SQLite3Insert(fileDB,files,row);
     if(!SQLite3EndTransaction(fileDB)) {
       std::cerr<<"Unable to make new entry in files table - END TRANSACTION\n";
       exit(-1);
     }
     return new ProfilerDatabase(profname,fullname,fileDB,id,0);
  }
}

void DBFileManager::Initialize(std::string prefix, std::string file,
                                                   std::string appname) {
  singleton.init(prefix, file, appname);
}

DBFileManager &DBFileManager::getSingleton() {
  return singleton;
}

unsigned long long ProfilerDatabase::feedbackValue(std::string profname,
						                                        unsigned long long refID) {
  if (feedbackMap.find(refID)!=feedbackMap.end())
    return feedbackMap[refID];

  sqlite3 *db =  NULL;
  if(!SQLite3Open(&db, DBFileManager::getSingleton().getFullPath(profname+".db"))) {
  // we don't know, so return -1
      return (unsigned long long)-1;
    }

  ProfilerTable fb("feedback");
  fb.add(TableColumn("filename",SQL_TEXT))
    .add(TableColumn("fileid",SQL_TEXT))
    .add(TableColumn("refid",SQL_TEXT))
    .add(TableColumn("count",SQL_INT64))
    .add(TableColumn("total",SQL_INT64))
    .add(TableColumn("totcnt",SQL_INT64))
    .add(TableColumn("extra",SQL_INT64))
    .add(TableColumn("population",SQL_INT64));

  TableRow select;
  select.add(TableColumn("refid",SQL_INT64)).add(TableColumn("count",SQL_INT64));

  TableRow constraint;
  constraint.add(TableColumn((int64_t)(getFileID()),"fileid"));
  constraint.add(TableColumn(DBFileManager::getSingleton().getOrigin().c_str(),"filename"));
  std::vector<TableRow> result = SQLite3Select(db,fb,select,constraint);
  sqlite3_close(db);

  for(size_t r=0; r < result.size(); r++) {
      // Pick the last one
      TableRow &row = result[ r ];
      // Get refID out of it
      TableColumn &rid = row.get(0);
      TableColumn &count = row.get(1);
      //assert(row.size()==3);
      //assert(c.getKind()==SQL_INT64);

      int64_t refidval = rid.getInt64();
      int64_t countval = count.getInt64();
      feedbackMap[refidval] = countval;
      //return (unsigned long long)countval;
  }
  if (feedbackMap.find(refID)!=feedbackMap.end())
    return feedbackMap[refID];

  // no matches, so it must not have executed
  return (unsigned long long) 0;
}

/*ProfilerDatabase& DBFileManager::findOrCreateDB(std::string DBname, ProfilerTable &table)
{
  std::string fullname = fileHelper.getFullPath(DBname);
  sqlite3 *db;

  if (SQLite3Open(&db, fullname)) {
    // Database was created successfully
    return *(new ProfilerDatabase(db));
  } else {
    // Something went wrong
    return *(new ProfilerDatabase(NULL));
  }
}*/

/*
 *
 * ProfilerDatabase
 *
 */

void ProfilerDatabase::addTable(ProfilerTable &table) {
  SQLite3CreateTable(fileDB,table);
}

void ProfilerDatabase::insertValues(ProfilerTable &table, TableRow &values) {
  SQLite3Insert(fileDB,table,values);
}

/*std::vector<TableRow> ProfilerDatabase::fetchRows(ProfilerTable &table, TableRow &select)
{
  std::vector<TableRow> v;
  return v;
  }*/
