//---------------------------------------------------------------------------
// Copyright (C) 2000 Dallas Semiconductor Corporation, All Rights Reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL DALLAS SEMICONDUCTOR BE LIABLE FOR ANY CLAIM, DAMAGES
// OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name of Dallas Semiconductor
// shall not be used except as stated in the Dallas Semiconductor
// Branding Policy.
//---------------------------------------------------------------------------
//
//  atod26.h - Include file for Smart Battery Monitor demo.
//
//  Version: 2.00



#define SBATTERY_FAM  0x26
#define CONF2438_IAD  0x01
#define CONF2438_CA   0x02
#define CONF2438_EE   0x04
#define CONF2438_AD   0x08
#define CONF2438_TB   0x10
#define CONF2438_NVB  0x20
#define CONF2438_ADB  0x40
#define CONF2438_ALL  0x7f

#define ConvertT      0x01
#define ConvertV      0x02

typedef struct Result {
   int Temp;
   int Volts;
   int Current;
   int ICA;
   int CCA;
   int DCA;
} Result_t;

int SetupConfig(int portnum, int config, uint8_t *SNum);
int  SetCalibration(int portnum, uint8_t *SNum, int offset);
int TriggerAll(int portnum, int Trigger, uint8_t *SNum);
int ReadAll(int portnum, Result_t *pResult, uint8_t *SNum);
int  SaveNVM(int portnum, uint8_t *SNum, uint8_t location, uint8_t value);
int SetICA(int portnum, uint8_t *SNum, uint8_t value);
int SetCCADCA(int portnum, uint8_t *SNum, int cca, int dca);
double ReadTemp(int portnum, uint8_t *SNum);
double ReadVad(int portnum, uint8_t *SNum);
signed short int ReadCurrent(int portnum, uint8_t *SNum);





