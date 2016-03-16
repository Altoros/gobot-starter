/*
 * Apple System Management Control (SMC) Tool
 * Copyright (C) 2006 devnull 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __SMC_H__
#define __SMC_H__
#endif

#define KERNEL_INDEX_SMC      2

#define SMC_CMD_READ_BYTES    5
#define SMC_CMD_WRITE_BYTES   6
#define SMC_CMD_READ_KEYINFO  9

#define DATATYPE_SP78         "sp78"

// key values
#define SMC_KEY_CPU_TEMP      "TC0P"

#include <stdio.h>
#include <string.h>
#include <IOKit/IOKitLib.h>

typedef struct {
    char                  major;
    char                  minor;
    char                  build;
    char                  reserved[1]; 
    UInt16                release;
} SMCKeyData_vers_t;

typedef struct {
    UInt16                version;
    UInt16                length;
    UInt32                cpuPLimit;
    UInt32                gpuPLimit;
    UInt32                memPLimit;
} SMCKeyData_pLimitData_t;

typedef struct {
    UInt32                dataSize;
    UInt32                dataType;
    char                  dataAttributes;
} SMCKeyData_keyInfo_t;

typedef char              SMCBytes_t[32]; 

typedef struct {
  UInt32                  key; 
  SMCKeyData_vers_t       vers; 
  SMCKeyData_pLimitData_t pLimitData;
  SMCKeyData_keyInfo_t    keyInfo;
  char                    result;
  char                    status;
  char                    data8;
  UInt32                  data32;
  SMCBytes_t              bytes;
} SMCKeyData_t;

typedef char              UInt32Char_t[5];

typedef struct {
  UInt32Char_t            key;
  UInt32                  dataSize;
  UInt32Char_t            dataType;
  SMCBytes_t              bytes;
} SMCVal_t;

static io_connect_t conn;

UInt32 _strtoul(char *str, int size, int base)
{
    UInt32 total = 0;
    int i;

    for (i = 0; i < size; i++)
    {
        if (base == 16)
            total += str[i] << (size - 1 - i) * 8;
        else
           total += (unsigned char) (str[i] << (size - 1 - i) * 8);
    }
    return total;
}

void _ultostr(char *str, UInt32 val)
{
    str[0] = '\0';
    sprintf(str, "%c%c%c%c", 
            (unsigned int) val >> 24,
            (unsigned int) val >> 16,
            (unsigned int) val >> 8,
            (unsigned int) val);
}

float _strtof(char *str, int size, int e)
{
    float total = 0;
    int i;
    
    for (i = 0; i < size; i++)
    {
        if (i == (size - 1))
            total += (str[i] & 0xff) >> e;
        else
            total += str[i] << (size - 1 - i) * (8 - e);
    }
    
	total += (str[size-1] & 0x03) * 0.25;
    
    return total;
}

kern_return_t SMCOpen(void)
{
    kern_return_t result;
    io_iterator_t iterator;
    io_object_t   device;

    CFMutableDictionaryRef matchingDictionary = IOServiceMatching("AppleSMC");
    result = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDictionary, &iterator);
    if (result != kIOReturnSuccess)
    {
        printf("Error: IOServiceGetMatchingServices() = %08x\n", result);
        return 1;
    }

    device = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (device == 0)
    {
        printf("Error: no SMC found\n");
        return 1;
    }

    result = IOServiceOpen(device, mach_task_self(), 0, &conn);
    IOObjectRelease(device);
    if (result != kIOReturnSuccess)
    {
        printf("Error: IOServiceOpen() = %08x\n", result);
        return 1;
    }

    return kIOReturnSuccess;
}

kern_return_t SMCClose()
{
    return IOServiceClose(conn);
}


kern_return_t SMCCall(int index, SMCKeyData_t *inputStructure, SMCKeyData_t *outputStructure)
{
    size_t   structureInputSize;
    size_t   structureOutputSize;

    structureInputSize = sizeof(SMCKeyData_t);
    structureOutputSize = sizeof(SMCKeyData_t);

    #if MAC_OS_X_VERSION_10_5
    return IOConnectCallStructMethod( conn, index,
                            // inputStructure
                            inputStructure, structureInputSize,
                            // ouputStructure
                            outputStructure, &structureOutputSize );
    #else
    return IOConnectMethodStructureIStructureO( conn, index,
                                                structureInputSize, /* structureInputSize */
                                                &structureOutputSize,   /* structureOutputSize */
                                                inputStructure,        /* inputStructure */
                                                outputStructure);       /* ouputStructure */
    #endif

}

kern_return_t SMCReadKey(UInt32Char_t key, SMCVal_t *val)
{
    kern_return_t result;
    SMCKeyData_t  inputStructure;
    SMCKeyData_t  outputStructure;

    memset(&inputStructure, 0, sizeof(SMCKeyData_t));
    memset(&outputStructure, 0, sizeof(SMCKeyData_t));
    memset(val, 0, sizeof(SMCVal_t));

    inputStructure.key = _strtoul(key, 4, 16);
    inputStructure.data8 = SMC_CMD_READ_KEYINFO;

    result = SMCCall(KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
    if (result != kIOReturnSuccess)
        return result;

    val->dataSize = outputStructure.keyInfo.dataSize;
    _ultostr(val->dataType, outputStructure.keyInfo.dataType);
    inputStructure.keyInfo.dataSize = val->dataSize;
    inputStructure.data8 = SMC_CMD_READ_BYTES;

    result = SMCCall(KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
    if (result != kIOReturnSuccess)
        return result;

    memcpy(val->bytes, outputStructure.bytes, sizeof(outputStructure.bytes));

    return kIOReturnSuccess;
}

kern_return_t SMCReadKey2(UInt32Char_t key, SMCVal_t *val,io_connect_t conn)
{
    kern_return_t result;
    SMCKeyData_t  inputStructure;
    SMCKeyData_t  outputStructure;
    
    memset(&inputStructure, 0, sizeof(SMCKeyData_t));
    memset(&outputStructure, 0, sizeof(SMCKeyData_t));
    memset(val, 0, sizeof(SMCVal_t));
    
    inputStructure.key = _strtoul(key, 4, 16);
    sprintf(val->key, key);
    
    result = SMCGetKeyInfo(inputStructure.key, &outputStructure.keyInfo, conn);
    if (result != kIOReturnSuccess)
    {
        return result;
    }
    
    val->dataSize = outputStructure.keyInfo.dataSize;
    _ultostr(val->dataType, outputStructure.keyInfo.dataType);
    inputStructure.keyInfo.dataSize = val->dataSize;
    inputStructure.data8 = SMC_CMD_READ_BYTES;
    
    result = SMCCall2(KERNEL_INDEX_SMC, &inputStructure, &outputStructure,conn);
    if (result != kIOReturnSuccess)
    {
        return result;
    }
    
    memcpy(val->bytes, outputStructure.bytes, sizeof(outputStructure.bytes));
    
    return kIOReturnSuccess;
}

kern_return_t SMCCall2(int index, SMCKeyData_t *inputStructure, SMCKeyData_t *outputStructure,io_connect_t conn)
{
    size_t   structureInputSize;
    size_t   structureOutputSize;
    structureInputSize = sizeof(SMCKeyData_t);
    structureOutputSize = sizeof(SMCKeyData_t);
    
    return IOConnectCallStructMethod(conn, index, inputStructure, structureInputSize, outputStructure, &structureOutputSize);
}

kern_return_t SMCGetKeyInfo(UInt32 key, SMCKeyData_keyInfo_t* keyInfo, io_connect_t conn)
{
    SMCKeyData_t inputStructure;
    SMCKeyData_t outputStructure;
    kern_return_t result = kIOReturnSuccess;
    int i = 0;
    
    memset(&inputStructure, 0, sizeof(inputStructure));
    memset(&outputStructure, 0, sizeof(outputStructure));

    inputStructure.key = key;
    inputStructure.data8 = SMC_CMD_READ_KEYINFO;

    result = SMCCall2(KERNEL_INDEX_SMC, &inputStructure, &outputStructure, conn);
    if (result == kIOReturnSuccess)
    {
      *keyInfo = outputStructure.keyInfo;
    }
    
    return result;
}

kern_return_t SMCWriteKey2(SMCVal_t writeVal, io_connect_t conn)
{
    kern_return_t result;
    SMCKeyData_t  inputStructure;
    SMCKeyData_t  outputStructure;
    
    SMCVal_t      readVal;
    
    result = SMCReadKey2(writeVal.key, &readVal,conn);
    if (result != kIOReturnSuccess)
        return result;
    
    if (readVal.dataSize != writeVal.dataSize)
        return kIOReturnError;
    
    memset(&inputStructure, 0, sizeof(SMCKeyData_t));
    memset(&outputStructure, 0, sizeof(SMCKeyData_t));
    
    inputStructure.key = _strtoul(writeVal.key, 4, 16);
    inputStructure.data8 = SMC_CMD_WRITE_BYTES;
    inputStructure.keyInfo.dataSize = writeVal.dataSize;
    memcpy(inputStructure.bytes, writeVal.bytes, sizeof(writeVal.bytes));
    result = SMCCall2(KERNEL_INDEX_SMC, &inputStructure, &outputStructure,conn);
    
    if (result != kIOReturnSuccess)
        return result;
    return kIOReturnSuccess;
}

kern_return_t SMCWriteKey(SMCVal_t writeVal)
{
    return SMCWriteKey2(writeVal, conn);
}


double SMCGetTemperature()
{
    SMCVal_t val;
    kern_return_t result;
    double res = 0.0;

    SMCOpen();
    result = SMCReadKey(SMC_KEY_CPU_TEMP, &val);
    if (result == kIOReturnSuccess) {
        // read succeeded - check returned value
        if (val.dataSize > 0) {
            if (strcmp(val.dataType, DATATYPE_SP78) == 0) {
                // convert sp78 value to temperature
                int intValue = val.bytes[0] * 256 + (unsigned char)val.bytes[1];
                res = intValue / 256.0;
            }
        }
    }
    SMCClose();
    return res;
}

int SMCGetTotalFansCount() 
{
   SMCVal_t      val;
   int res = -1;
    
   SMCOpen();
   if (SMCReadKey("FNum", &val) == kIOReturnSuccess)
     res = _strtoul((char *)val.bytes, val.dataSize, 10);
   SMCClose();
   return res;
}

typedef struct {
  float min;
  float current;
  float max;
} SMCFanInfo;


SMCFanInfo SMCGetFanInfo(int i) 
{
   SMCVal_t      val;
   UInt32Char_t  key;
   SMCFanInfo res;
    
   SMCOpen();
   sprintf(key, "F%dMn", i); 
   SMCReadKey(key, &val); 
   res.min = _strtof(val.bytes, val.dataSize, 2);
   sprintf(key, "F%dAc", i); 
   SMCReadKey(key, &val); 
   res.current = _strtof(val.bytes, val.dataSize, 2);
   sprintf(key, "F%dMx", i); 
   SMCReadKey(key, &val); 
   res.max = _strtof(val.bytes, val.dataSize, 2);
   SMCClose();
   return res;
}

kern_return_t SMCSetFanSpeed(int i, int speed) 
{
   SMCVal_t val;
   kern_return_t r;
    
   SMCOpen();
   sprintf(val.key, "F%dMn", i); 
   speed <<= 2;
   val.bytes[0] = speed >> 8;
   val.bytes[1] = speed && 0xFF;
   val.dataSize = 2;
   r = SMCWriteKey(val);
   SMCClose();
   return r;
}
