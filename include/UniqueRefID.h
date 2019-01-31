#ifndef UNIQUE_REF_ID_H
#define UNIQUE_REF_ID_H

#include <iostream>
#include <fstream>
#include <string>

namespace llvm {

  class UniqueRefID {
  protected:
    unsigned long long start;
    unsigned long long count;

  public:
    UniqueRefID(unsigned long long aCount)
      :start(aCount),count(aCount) {}

    unsigned long get() {
      return count;
    }

    unsigned long inc() {
      return count++;
    }

    virtual ~UniqueRefID() {}
  };


  class RefIDFromFile : public UniqueRefID {
  protected:
    std::string filename;
  public:
    RefIDFromFile(std::string afilename)
      :UniqueRefID(0)
      ,filename(afilename) {
      std::ifstream ifs;
      ifs.open(filename.c_str());
      if (ifs.good()) {
        unsigned long l=0;
        ifs >> l;
        count = (unsigned long long)l;
	      start = count;
	//std::cout << "Got " << l << "from file.\n";
      } else {
        count = 0;
	      start = 0;
        //std::cout << "Error opening " << filename << "\n";
      }
      ifs.close();
    }

  ~RefIDFromFile() {
      if (start!=count) {
        std::ofstream ofs;
        ofs.open(filename.c_str());
        ofs << count;
        ofs.close();
	      //std::cerr << "Write result to " << filename << "\n";
      }
    }
  };

}

#endif //UNIQUE_REF_ID_H
