// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Extract device information from linux device tree
 *
 * Copyright (C) 2025 Daniel Wegkamp
 */

#ifndef devicetree_h
#define devicetree_h

#include <filesystem>
#include <string>
#include <vector>
#include <sys/mman.h>   //mmap()

#include "uint_large.h"

#define DEVICE_TREE_BASE_PATH "/proc/device-tree"
#define DEVICE_TYPE_GENERIC 0

#define REGCTL_GENERIC_RREG 0x4321
#define REGCTL_GENERIC_WREG 0x1234


using namespace std;

enum device_status
{
    k_dev_status_unknown = 0b0,
    k_dev_ok = 0b1,
    k_dev_nodes_generated = 0b10,
    k_dev_traversed_tree = 0b100
};

struct range    //support 4 cells (uint32_t) for now
{
    uint_large childAddr;
    uint_large parentAddr;
    uint_large childSize;
};

struct reg
{
    uint_large addr;
    uint_large size;
};


string getFileContentString( string path );
vector<byte> getFileContentBinBytes( string path );
vector<uint32_t> getFileContentBinFourBytes( string path );
bool isInRange( range *myRange, reg *a );

struct pathNode;

class device
{
public:
    device(){};
    device( string devName, bool verbose, uint32_t type, bool checkStatus );
    ~device();

    bool findDevicePath( string devName );
    bool checkOkay();
    void generateNodes( bool verbose );
    void traverseNodes( bool verbose );

    void mapMemory( int mem_handle );

    //access full 32 bit register:
    int regWrite( uint32_t offset, uint32_t value );
    int regRead( uint32_t offset, uint32_t *value );

    virtual void regBackup(){};
    virtual void regRestore(){};
    virtual void setSettings(){};
    virtual int pioctl( uint64_t cmd, void *arg );      //so the childs class method is called if available

    //debug:
    void dumpRegs();
    void dumpRegs( uint32_t offset, uint32_t count );
    void printBin32( uint32_t data );
    string getBinString( uint8_t byte );

    string   getCompatible(){ return compatible; };
    uint64_t getBusAdress(){ return (uint64_t)myBusAddr; };
    uint64_t getBaseAdress(){ return (uint64_t)myBaseAddr; };
    uint64_t getSizeBytes(){ return (uint64_t)mySize; };
    string   getPathString(){ return myPath.string(); };
    uint32_t getType(){ return deviceType; }
    void    *getVirtualReg(){ return (void*)virt_reg; };

    void printMemPtr(){cout << "printMemPtr: " << (uint32_t*)virt_reg << endl;}


protected:
    uint8_t             status = 0;
    bool                okay = false;
    bool                found = false;

    uint32_t            deviceType = DEVICE_TYPE_GENERIC;

    filesystem::path    myPath;
    vector<pathNode>    myNodes;
    pathNode            *deviceNode = nullptr;
    string              compatible;

    uint_large          myBusAddr = 0;
    uint_large          mySize = 0;         //should be in bytes
    uint_large          myBaseAddr = 0;

    volatile void       *virt_reg = MAP_FAILED;
    uint32_t            myRegisterCount = 0;    //is in 4bytes
    uint32_t            *backupRegs = nullptr;
};


struct pathNode
{
    pathNode( string path, uint32_t my_cell_count, uint32_t my_size_cell_count );   //inherits counts from parent node

    void extractCellCounts( string path );
    void extractRanges( string path );
    void extractReg( string path );

    uint32_t         address_cell_count = 0;
    uint32_t         size_cell_count = 0;

    uint32_t         child_address_cell_count = 0;
    uint32_t         child_size_cell_count = 0;

    vector<range>   myRanges;
    reg             myReg;
    string          myPath;
    string          compatible;

    bool            valid = true;

    bool            hasReg = false;
    bool            hasRanges = false;
    bool            hasCounts = false;

    void printRanges();
    void printReg();
};


#endif
