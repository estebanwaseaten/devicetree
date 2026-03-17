// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * just testing the device tree classes
 *
 * Copyright (C) 2025 Daniel Wegkamp
 */

#include "devicetree.h"



int main(int argc, char** argv)
{
    cout << "Hello devicetree 0.1 main()" << endl;

    string argument1 = "spi0";          //default
    if ( argc > 1)
    {
         argument1 = argv[1];
    }
    cout << "looking for device " << argument1 << endl;


    //create new device by name
    device myDevice( argument1 );

    myDevice.printDeviceInfo();

    //uint32_t addr[2];
    uint64_t addr = myDevice.getBaseAdress();
    cout << "The base address is: 0x" << hex << setw(16) << setfill('0') << addr << endl;


    return 0;
}
