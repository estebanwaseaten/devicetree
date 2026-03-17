// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * class for unsigned integer values larger than 64bits
 * (basic functionality - probably should add a few more overloads)
 *
 * Copyright (C) 2025 Daniel Wegkamp
 */


#ifndef UINT_LARGE_H
#define UINT_LARGE_H

#include <cstdint>
#include <iostream>

using namespace std;

#define UINT_LARGE_MAX_CELLS 8                      //maximum of 8 * 32 = 256 bits. Can be increased here --> needs more memory.
#define UINT_LARGE_OVERFLOW 0x100000000             //when to shift to next uint32_t

class uint_large
{
private:

    uint32_t value[UINT_LARGE_MAX_CELLS] = {0};       //stores the actual values
    //uint32_t wordsInUse;        //needed?

public:
    uint_large(){ value[0] = 0; };
    uint_large( uint32_t v ){ value[0] = v; }
    uint_large( uint32_t v1, uint32_t v0 ){ value[0] = v0; value[1] = v1; }
    uint_large( uint32_t v2, uint32_t v1, uint32_t v0 ){ value[0] = v0; value[1] = v1; value[2] = v2;}
    uint_large( uint32_t v3, uint32_t v2, uint32_t v1,uint32_t v0 ){ value[0] = v0; value[1] = v1; value[2] = v2; value[3] = v3; }

    bool set_word( uint32_t v, uint8_t index ){ if( index < UINT_LARGE_MAX_CELLS){ value[index]= v; return true; } return false; };

    bool operator==( const uint_large &obj ) //self < obj
    {
        bool result = true;
        for (size_t i = 0; i < UINT_LARGE_MAX_CELLS; i++)
        {
            if(value[i] != obj.value[i])
                return false;
        }
        return true;
    }

    bool operator<( const uint_large &obj ) //self < obj
    {
        //start at most significant:
        for (size_t i = UINT_LARGE_MAX_CELLS; i > 0; i--)
        {
            if( value[i-1] < obj.value[i-1]  )
            {
                return true;
            }
        }
        return false;
    }

    bool operator>( const uint_large &obj ) //self < obj
    {
        //start at most significant:
        for (size_t i = UINT_LARGE_MAX_CELLS; i > 0; i--)
        {
            //cout << value[i-1] << " < " << obj.value[i-1] << "?  ";
            if( value[i-1] < obj.value[i-1]  )
            {
                //cout << "return true" << endl;
                return false;
            }
        }
        return true;
    }

    bool operator<=( const uint_large &obj ) //self < obj
    {
        return !( *this > obj );
    }

    bool operator>=( const uint_large &obj ) //self < obj
    {
        return !( *this < obj );
    }

    uint_large operator+(const uint_large &obj)
    {
        uint_large result;
        uint64_t carry = 0;
        uint64_t temp = 0;

        for (size_t i = 0; i < UINT_LARGE_MAX_CELLS; i++)
        {
            temp = (uint64_t)value[i] + (uint64_t)obj.value[i] + carry;
            if( temp >= UINT_LARGE_OVERFLOW )   //overflowing
            {
                temp -= UINT_LARGE_OVERFLOW;
                carry = 1;
            }
            else
            {
                carry = 0;
            }

            result.value[i] = temp;
        }
        return result;
    }

    uint_large operator-(uint_large const& obj)
    {
        uint_large result;
        uint64_t carry = 0;
        uint64_t temp = 0;
        uint64_t a, b;

        for (size_t i = 0; i < UINT_LARGE_MAX_CELLS; i++)
        {
            a = value[i];
            b = obj.value[i];

            if ( a < ( b + carry ) )  //negative result
            {
                temp = UINT_LARGE_OVERFLOW + a - b - carry;
                carry = 1;
            }
            else
            {
                temp = a - b - carry;
                carry = 0;
            }
            result.value[i] = temp;
        }
        return result;
    }

    bool fits_uint64()
    {
        for (size_t i = 2; i < UINT_LARGE_MAX_CELLS; i++)
        {
            if ( value[i] != 0 )
            {
                return false;
            }
        }
        return true;
    }

    operator uint64_t() const
    {
        uint64_t temp = value[0];
        return temp | (uint64_t)value[1] << 32;
    }

    friend ostream& operator<<(ostream& os, const uint_large& obj)
    {
        bool nonzero = false;
        string space = "";
        for (size_t i = UINT_LARGE_MAX_CELLS; i > 0; i--)   //we print backwards
        {
            if( obj.value[i-1] != 0 || nonzero )
            {
                os << space << "0x" << hex << setw(8) << setfill('0') << obj.value[i-1];
                space = " ";
                nonzero = true;
            }
        }

        if( !nonzero )
        {
            os << "0x" << hex << setw(8) << setfill('0') << 0;
        }
        return os;
    }
};



#endif
