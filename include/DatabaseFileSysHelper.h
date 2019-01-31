#ifndef DATABASE_FILE_MANAGER_H
#define DATABASE_FILE_MANAGER_H

//using namespace llvm;

#include <string>
#include <iostream>

namespace llvm {

  class DatabaseFileSysHelper {
  protected:
    std::string prefix;
    
  public:

    DatabaseFileSysHelper() {}

  DatabaseFileSysHelper(const std::string &aPrefix) 
    :prefix(aPrefix) {}
    
  virtual std::string getFullPath(const std::string &name) 
    {
      if (prefix.size()>0)
	return prefix + "/" + name;
      else
	return name;
    }

  virtual std::string getRefIDFile() 
    {
      return getFullPath("refID");
    }

  void setPrefix(std::string aprefix) 
    {
      prefix = aprefix;
    }

  std::string getPrefix() 
    {
      return prefix;
    }
  };
}

#endif
 
