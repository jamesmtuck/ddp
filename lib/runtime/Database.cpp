#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

#ifdef __cplusplus
extern "C" {
#endif


struct profiler_common {
  int refid;
  int *gv;
  int total;
  int *totcnt;
  int *extra;
  unsigned int *population;
};


//extern struct ddpref ddp_refids[];
//static int ddp_already_ran=0;


  static int create_table(sqlite3 *db, const char *tableName) {

  const char * command = "create table if not exists %s (filename text, fileid integer not null, refid integer not null, count integer,total integer, totcnt integer, extra integer, population integer, primary key (fileid,refid))";

  char cmd[1024];
  sprintf(cmd,command,tableName);

  sqlite3_stmt * stmt;
  int result;
  do {
    result = sqlite3_prepare_v2(db, cmd, strlen(cmd)+1, &stmt, NULL);
  }  while ( SQLITE_LOCKED == result || SQLITE_BUSY == result);

  if (result) {
    fprintf(stderr,"sqlite3_prepare_v2 - Can't create table (%s): %s - %d\n",tableName,sqlite3_errmsg(db),result);
    return 0;
  }

  int ret;
  do {
     ret=sqlite3_step(stmt);
  } while( SQLITE_BUSY==ret || SQLITE_LOCKED ==ret);

  if (ret!=SQLITE_DONE) {
    fprintf(stderr,"sqlite3_step - Can't create table (%s): %s\n",tableName,sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return 0;
  }

  sqlite3_finalize(stmt);
  return 1;
}


  static char *db_env=NULL;

  void update_sqlite_database(const char *path,
			      const char *dbName,
			      const char *tableName,
			      const char *fileName,
			      int fileid,
			      struct profiler_common *array,
			      int size)
  {
    // open the data base
    sqlite3 *db;
    const char * db_path;
    
    if (db_env==NULL) // only do this once
      db_env = getenv("PROFILING_DB_OVERRIDE");

    if (db_env)
      db_path = db_env;
    else
      db_path = path;
    
    char name[1024];
    char *sErrMsg;
    
    // clean this up!!!
    sprintf(name,"%s/%s",db_path,dbName);
    
    //fprintf(stderr,"Open Database: %s\n",name);
    int rc = sqlite3_open(name,&db);
    if( rc ){
      fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
      sqlite3_close(db);
      return;
    }
    
    if(!create_table(db,tableName)) {
      fprintf(stderr,"Error dumping profile to database: %s.",db_path);
      fprintf(stderr,"Dumping info to screen:\n");
      fprintf(stderr,"\tRefid   :Count   \n");
      for (int i=0; i<size; i++)
	printf("\t%8d:%8d\n",array[i].refid, *(array[i].gv));
      return;
    }
    
    // Don't wait for transaction to complete; more risky, but okay for
    // profile data
    sqlite3_exec(db, "PRAGMA synchronous = OFF", NULL, NULL, &sErrMsg);
    sqlite3_exec(db, "PRAGMA journal_mode = MEMORY", NULL, NULL, &sErrMsg);
    
    int i;
    sqlite3_stmt *stmt;
    
    char sql[] = "insert or replace into %s (filename,fileid,refid,count,total,totcnt,extra,population) values (?,?,?,?,?,?,?,?)";
    char format[1024];
    sprintf(format,sql,tableName);

    int result = sqlite3_prepare_v2(db, format, strlen(format)+1, &stmt, NULL) ;
    
    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &sErrMsg);
    
    for (i=0; i<size; i++) {
//      fprintf(stderr,"filename=%s:refid=%d:total=%d:count=%d:&count=%lx:totcnt=%d:extra=%d:population=%d\n",fileName,array[i].refid,array[i].total,*array[i].gv,(unsigned long)array[i].gv,array[i].totcnt?*array[i].totcnt:-1,array[i].extra?*array[i].extra:-1, array[i].population?*array[i].population:-1);
      //if (*array[i].gv > 0) {
	sqlite3_bind_text(stmt, 1, fileName, strlen(fileName)+1 ,SQLITE_STATIC);
	sqlite3_bind_int(stmt, 2, fileid);
	sqlite3_bind_int(stmt, 3, array[i].refid);
	sqlite3_bind_int(stmt, 4, *array[i].gv);
	sqlite3_bind_int(stmt, 5, array[i].total);
	sqlite3_bind_int(stmt, 6, array[i].totcnt?*array[i].totcnt:-1);
	sqlite3_bind_int(stmt, 7, array[i].extra?*array[i].extra:-1);
	sqlite3_bind_int(stmt, 8, array[i].population?*array[i].population:0);
	while( SQLITE_BUSY==(result=sqlite3_step(stmt)) );
	if(result!=SQLITE_DONE) {
	  fprintf(stderr,"Something went wrong!!! %s\n",format);
	  fprintf(stderr, "Error message: %s\n", sqlite3_errmsg(db));
	}  
	sqlite3_reset(stmt);	
	//}
    }
    sqlite3_exec(db, "END TRANSACTION", NULL, NULL, &sErrMsg);
    
    sqlite3_finalize(stmt);
    
    sqlite3_close(db);    
  }

#ifdef __cplusplus
}
#endif
