Runtime Instrumentation
=======================

The files in this forlder are devoted to runtime instrumentation.  For the most part, it is self contained. All the files
get linked into a single archive.  

Some rules of use
-----------------

* Do not include any file within LLVM here.
* Try to keep code self contained so that we don't need to link to other libraries.
* Carefully weigh the benefits of instrumentation versus library code.  
    - Library code must be called, and as such, should be infrequently used or used in conjunction with LTO.  
    - Don't rely too much on LTO. Library code that's called too frequently will significantly degrade LTO performance and should be avoided.

Building
-------

The library will be built as part of the standard LLVM project makefile rules.  But, we want to configure the library a few different
ways so we provide a Makefile.multilib also.  This allows customization along a few dimensions:

- Link Time Optimization
- 32 bit versus 64 bit