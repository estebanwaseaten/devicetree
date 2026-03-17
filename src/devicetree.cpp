// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Extract device information from linux device tree
 *
 * Copyright (C) 2025 Daniel Wegkamp
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>    //remove_if
#include <iomanip>
#include <cstring>
#include <bitset>

#include <devicetree/devicetree.h>

device::device( string devName, bool verbose, uint32_t type, bool checkStatus )
{
    cout << "default device constructor" << endl;
    if( !findDevicePath( devName ) )
    {
        cout << "did not find device: " << devName << endl;
        return;
    }

    found = true;

    if( checkStatus && !checkOkay() )
    {
        cout << "device status is not okay!" << endl;
        return;
    }
    deviceType = type;      //do we need this?

    //1. generate nodes from path
    //2. traverse the path from device-tree on and save the '#address-cells' and '#size-cells'. parameters, ranges and reg are also extracted within constructor pathNode().
    generateNodes( verbose );
    traverseNodes( verbose );

    //after traverseNodes myRegisterCount is valid
    backupRegs = new uint32_t[myRegisterCount];

}

device::~device()
{
    if( virt_reg != MAP_FAILED )
    {
        munmap( (void*)virt_reg, (size_t)mySize );
    }
}


int device::pioctl( uint64_t cmd, void *arg )
{
    cout << "generic device ioctl - function not supported." << endl;

    return 0;
}


/*
 * tries to finde the device name (devName) within the device tree's alias directory
 * if found, the path to the device is read and stored in myPath
 */
bool device::findDevicePath( string devName )
{
    string base_path = DEVICE_TREE_BASE_PATH;
    string aliasPath;
    bool found = false;

    myPath = "";
    for( const auto &entry : filesystem::directory_iterator( base_path + "/aliases") )
    {
        filesystem::path tempPath = entry.path();
        string temp = tempPath.filename();

        if( devName == temp )
        {
            aliasPath = tempPath;   //converted to string
            myPath = base_path + getFileContentString( aliasPath );
            found = true;
        }
    }

    return found;
}

/*
 * extracts the status from the device tree
 */
bool device::checkOkay()
{
    okay = false;
    if( myPath != "" )
    {
        string statusString = getFileContentString( myPath.string() + "/status" );
        if( statusString.find( "okay" ) != string::npos )
        {
            okay = true;
            status = status | k_dev_ok;
        }
    }
    return okay;
}

void device::generateNodes( bool verbose )
{
    uint32_t prevAddrCount = 0;
    uint32_t prevCellCount = 0;
    string current_path;
    for (auto it = myPath.begin(); it != myPath.end(); ++it)
    {
        string directory = (*it).string();
        if ( directory != "/" && directory != "" )
        {
            current_path += "/" + directory;
            if ( verbose )
            {
                cout << current_path << endl;
            }
            if ( myNodes.size() > 0)
            {
                prevAddrCount = myNodes.back().child_address_cell_count;
                prevCellCount = myNodes.back().child_size_cell_count;
                //cout << "previous: " << prevAddrCount << ", " << prevCellCount << endl;
            }
            myNodes.push_back( pathNode(current_path, prevAddrCount, prevCellCount ) ); //e.g. ranges need to know own address_cell_count
        }
    }
    status |= k_dev_nodes_generated;
}

void device::traverseNodes( bool verbose )
{
    //traverse backwards to find the memory addresses with respect to each node... (could store this within the node... maybe.)
    //bus address
    uint_large myCurrentOffset;     //offset *accumulated?*
    uint_large myCurrentAddress;
    reg        myCurrentReg;

    bool registeredDevice = false;

    int nodeCount = myNodes.size();
    //cout << "node count: " << nodeCount << endl;
    for (size_t i = nodeCount; i > 0; i--)
    {
        pathNode    *currentNode = &myNodes[i-1];

        //cout << currentNode->myPath << "(" << currentNode->address_cell_count << "/" << currentNode->size_cell_count << ")" << ": " << endl;
        if ( verbose )
        {
            currentNode->printRanges();
            currentNode->printReg();
        }

        if( !registeredDevice && currentNode->hasReg )          //really only needed for the lowest node (the device itself)
        {
            deviceNode = currentNode;
            //cout << "hasReg!" << deviceNode->address_cell_count << endl;
            myBusAddr = deviceNode->myReg.addr;
            mySize = deviceNode->myReg.size;
            myRegisterCount = mySize / 4;

            myCurrentReg = deviceNode->myReg;
            compatible = deviceNode->compatible;
            compatible.erase( remove_if( compatible.begin(), compatible.end(), [](unsigned char c){ return !isprint(c); }), compatible.end() );

            //cout << "Device bus address: " << myCurrentReg.addr << endl;
            registeredDevice = true;     //so this does not get overwritten
        }

        //1. check if myAddr within ranges:
        bool isInAnyRange;
        if( currentNode->hasRanges )
        {
            isInAnyRange = false;
            for( range currentRange : currentNode->myRanges )
            {
                if( isInRange( &currentRange, &myCurrentReg ) )
                {
                    //cout << "yeah, " << myCurrentReg.addr << " in range: " << currentRange.childAddr << " - " << currentRange.parentAddr << " - " << currentRange.childSize << endl;
                    myCurrentOffset = myCurrentReg.addr - currentRange.childAddr;
                    myCurrentAddress = currentRange.parentAddr + myCurrentOffset;
                    myCurrentReg.addr = myCurrentAddress;
                    isInAnyRange = true;
                    //cout << "Device current address now: " << myCurrentAddress << endl;
                }
            }
            if ( !isInAnyRange )
            {
                cout << "no range was found for previous address... probably failed?!" << endl;
            }
        }

        //cout << endl;
    }
    //cout << "device final Address: " << myCurrentAddress << endl << endl;
    myBaseAddr = myCurrentAddress;
    status |= k_dev_traversed_tree;
}

void device::mapMemory( int mem_handle )    //maps memory only for this device!
{
    cout << "mapping memory at: " << hex << myBaseAddr << " size: " << mySize << endl;
    virt_reg = mmap( 0, (size_t)mySize,  PROT_READ|PROT_WRITE, MAP_SHARED, mem_handle, (off_t)myBaseAddr);
    if ( virt_reg == MAP_FAILED )
    {
        cout << "mmap failed!" << endl;
    }
    else
    {
        cout << "mmap success: " << (uint32_t*)virt_reg << endl;
    }
}

int device::regWrite( uint32_t offset, uint32_t value )       // regRead( 0x044, 128 );
{
    if( virt_reg != MAP_FAILED && offset < mySize )
    {
        volatile uint32_t *device_reg = (uint32_t*)(virt_reg + offset);
        *device_reg = value;

        cout << "regWrite: 0x" << hex << offset << " virt: " << hex << (uint32_t*)virt_reg << " value: 0x" << hex << value << endl;
        return 0;
    }
    return -1;
}

int device::regRead( uint32_t offset, uint32_t *value )
{
    if( virt_reg != MAP_FAILED && offset < mySize )
    {
        volatile uint32_t *device_reg = (uint32_t*)(virt_reg + offset);    //this does not work!!!! the offset is 10 instead of 4
        *value = device_reg[0];

        cout << "regRead: 0x" << hex << offset << " virt: " << hex << (uint32_t*)device_reg << " value: 0x" << hex << *value << endl;
        return 0;
    }
    return -1;
}

void device::dumpRegs()
{   //loop through all registers
    volatile uint32_t *regs = (uint32_t*)virt_reg;      //no offset
    for (size_t i = 0; i < myRegisterCount; i++ )//getSizeBytes()/4; i++)
    {
        cout << "register 0x" << hex << setw(4) << setfill('0') << i*4 << ": ";
        printBin32( (uint32_t)regs[i] );
        cout << " hex: " << hex << (uint32_t)regs[i];
        cout << endl;
    }
}

void device::dumpRegs( uint32_t offset, uint32_t count )
{
    volatile uint32_t *regs = (uint32_t*)((uint32_t*)virt_reg + offset);

    for (size_t i = 0; i < count; i++ )
    {
        cout << "register 0x" << hex << setw(4) << setfill('0') << offset + i*4 << ": ";
        printBin32( (uint32_t)regs[i] );
        cout << " hex: " << hex << (uint32_t)regs[i];
        cout << endl;
    }
}

void device::printBin32( uint32_t data )
{
    uint8_t *bytes = (uint8_t*)&data;

    cout << getBinString( bytes[3] ) << " ";
    cout << getBinString( bytes[2] ) << " ";
    cout << getBinString( bytes[1] ) << " ";
    cout << getBinString( bytes[0] );
}

string device::getBinString( uint8_t byte )
{
    stringstream stream;
    std::bitset<8> bits{byte};
    //cout << dec << " " << (uint32_t)byte << " ";
    stream << "0b";
    stream << bits;

    //cout << " 0x" << hex << setw(2) << setfill('0') << (uint32_t)byte;
    return stream.str();
}

pathNode::pathNode( string path, uint32_t my_address_cell_count, uint32_t my_size_cell_count )
{
    myPath = path;

    address_cell_count = my_address_cell_count;     //needed for subroutines
    size_cell_count = my_size_cell_count;

    //cout << "generate pathNode for " << path << " with address cell count: " << my_address_cell_count << " and size cell count: " << my_size_cell_count << endl;

    //extract things:
    //1. try #address-cells and #size-cells
    extractCellCounts( path );

    //2. try ranges
    extractRanges( path );

    //3. try reg
    extractReg( path );

    //4. extract compatible
    compatible = getFileContentString( path + "/compatible");
    //could extract more data here...
}

void pathNode::extractCellCounts( string path )
{
    vector addressCells = getFileContentBinFourBytes( path + "/#address-cells");
    if (addressCells.size() > 0)
    {
        hasCounts = true;
        //cout << "address: ";
        uint32_t chunk = addressCells[0];
        //cout << " 0x" << hex << setw(8) << setfill('0') << chunk << endl;
        if( chunk > UINT_LARGE_MAX_CELLS )
        {
            child_address_cell_count = UINT_LARGE_MAX_CELLS;
            cout << "ERROR: address cell counts larger than " << UINT_LARGE_MAX_CELLS << " are not supported" << endl;
            valid = false;
        }
        else
        {
            child_address_cell_count = chunk;
        }
    }

    //2. try #size-cells
    vector sizeCells = getFileContentBinFourBytes( path + "/#size-cells");
    if (sizeCells.size() > 0)
    {
        //cout << "size: ";
        uint32_t chunk = sizeCells[0];
        //cout << " 0x" << hex << setw(8) << setfill('0') << chunk << endl;
        if( chunk > UINT_LARGE_MAX_CELLS )
        {
            child_size_cell_count = UINT_LARGE_MAX_CELLS;
            cout << "ERROR: size cell counts larger than " << UINT_LARGE_MAX_CELLS << " are not supported" << endl;
            valid = false;
        }
        else
        {
            child_size_cell_count = chunk;
        }
    }
}

void pathNode::extractRanges( string path )
{
    vector rangesBytes = getFileContentBinFourBytes( path + "/ranges");      //needs own size-cells which is defined one layer above and the child sizes as well.

    if (rangesBytes.size() > 0)
    {
        hasRanges = true;
        uint32_t cells_per_range = child_address_cell_count + address_cell_count + child_size_cell_count;
        uint32_t range_count = rangesBytes.size() / cells_per_range;

        // 1 CELL = 4bytes = uint32_t
        int counter = 0;
        for (size_t i = 0; i < range_count; i++)
        {
            range temp;
            for( uint32_t a = child_address_cell_count; a > 0; a--)
            {
                temp.childAddr.set_word(rangesBytes[counter], a-1);
                counter++;
            }
            for (uint32_t a = address_cell_count; a > 0; a--)
            {
                temp.parentAddr.set_word(rangesBytes[counter], a-1);
                counter++;
            }
            for (uint32_t a = child_size_cell_count; a > 0; a--)
            {
                temp.childSize.set_word(rangesBytes[counter], a-1);
                counter++;
            }
            myRanges.push_back(temp);
        }
    }
}

void pathNode::extractReg( string path )
{
    vector regBytes = getFileContentBinFourBytes( path + "/reg");
    int counter = 0;

    //make sure arrays we stay in the array bounds: done when writing the cell counts
    if (regBytes.size() > 0)
    {
        hasReg = true;
        for (uint32_t a = address_cell_count; a > 0; a--)
        {
            myReg.addr.set_word(regBytes[counter], a-1);
            counter++;
        }

        for (uint32_t a = size_cell_count; a > 0; a--)
        {
            myReg.size.set_word(regBytes[counter], a-1);
            counter++;
        }
    }
}

void pathNode::printReg()
{
    if( hasReg )
    {
        cout << "reg: addr: " << myReg.addr << " size: " << myReg.size << endl;
    }
}

void pathNode::printRanges()
{
    if( hasRanges )
    {
        for( range thisRange : myRanges )
        {
            cout << "range: " << thisRange.childAddr << " - " << thisRange.parentAddr << " - " << thisRange.childSize << endl;
        }
    }
}

bool isInRange( range *myRange, reg *a )
{
    //cout << "is " << a->addr << "/" << a->size << " between " << myRange->childAddr << " and " << myRange->childAddr + myRange->childSize << "?" << endl;
    return ((a->addr >= myRange->childAddr) && ((a->addr + a->size) <= (myRange->childAddr + myRange->childSize)));
}

string getFileContentString( string path )
{
    //remove null characters
    path.erase( remove_if( path.begin(), path.end(), [](unsigned char c){ return !isprint(c); }), path.end() );
    ifstream file(path);

    stringstream buffer;

    if( file )
    {
        buffer << file.rdbuf();
        file.close();
    }
    else
    {
        //cout << "file not open" << endl;
        //file.close();
    }
    return buffer.str();
}


vector<byte> getFileContentBinBytes( string path )
{
    //remove null characters
    path.erase( remove_if( path.begin(), path.end(), [](unsigned char c){ return !isprint(c); }), path.end() );
    filesystem::path filepath{path};
    ifstream file( path, std::ios::binary );
    vector<byte> buffer;

    if( file )
    {
        byte x;
        while( !file.fail() )
        {
            file.read( (char*)&x, 1);
            if( !file.fail() )
            {
                buffer.push_back( x );
            }
        }
        file.close();
    }
    else
    {
        //cout << "file not open" << endl;
    }
    return buffer;
}

vector<uint32_t> getFileContentBinFourBytes( string path )
{
    vector<byte> bytes = getFileContentBinBytes( path );

    uint32_t temp;
    vector<uint32_t> result;
    for (size_t i = 0; i < bytes.size(); i+=4 )
    {
        temp = ((uint32_t)bytes[i+0] << 24) | ((uint32_t)bytes[i+1] << 16) | ((uint32_t)bytes[i+2] << 8) | ((uint32_t)bytes[i+3]);
        result.push_back( temp );
    }
    return result;
}
