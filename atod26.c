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
//--------------------------------------------------------------------------
//
//  atod26.c - Reads the voltage on the 1-Wire of the DS2438.
//  version 1.00
// Major rewrite by Robin Gilks Copyright (c)2009
//

// Include Files
#include <stdio.h>
#include "ownet.h"
#include "atod26.h"

extern int16_t debug;

int ReadPage(int portnum, int page_num, uint8_t *rec_buf);
int WritePage(int portnum, int page_num, uint8_t *tx_buf, int tx_len);


/**
 * Sets the DS2438 to trigger all readable values
 *
 * portnum  the port number of the port being used for the
 *          1-Wire Network.
 * Trigger  OR'ed bit flags to indicate which of Volts & Temp to start conversion on
 * SNum     the serial number for the part that the read is
 *          to be done on.
 *
 * @return 'true' if the read was complete
 */

int TriggerAll(int portnum, int Trigger, uint8_t *SNum)
{
   int busybyte;

   owSerialNum(portnum, SNum, FALSE);

   if (Trigger & ConvertT)
   {
      if (owAccess(portnum))
      {
         // Convert Temperature command
         if (!owWriteByte(portnum, 0x44))
         {
            OWERROR(OWERROR_WRITE_BYTE_FAILED);
            return FALSE;
         }

         busybyte = owReadByte(portnum);

         while (busybyte == 0)
            busybyte = owReadByte(portnum);
      }
   }
   if (Trigger & ConvertV)
   {
      if (owAccess(portnum))
      {
         if (!owWriteByte(portnum, 0xB4))
         {
            OWERROR(OWERROR_WRITE_BYTE_FAILED);
            return FALSE;
         }

         busybyte = owReadByte(portnum);

         while (busybyte == 0)
            busybyte = owReadByte(portnum);
      }
   }

   return TRUE;
}


/**
 * Sets the DS2438 to trigger a voltage read from the Vad 'tap' pin
 *
 * portnum  the port number of the port being used for the
 *          1-Wire Network.
 * SNum     the serial number for the part that the read is
 *          to be done on.
 *
 * @return voltage value if the read was completed OK else 0
 */
double ReadVad (int portnum, uint8_t *SNum)
{
   uint8_t page_data[10];
   int busybyte;
   short signed int V;

   owSerialNum(portnum, SNum, FALSE);
   // Get the Status/Configuration page
   if (!ReadPage(portnum, 0, page_data))
      return 0;

   // if there is no change required in the config reigster, then leave it alone
   if ((page_data[0] & CONF2438_AD) != 0)
   {
      if (owAccess(portnum))
      {
         // Write the Status/Configuration byte
         page_data[0] &= ~CONF2438_AD;
         if (!WritePage(portnum, 0, page_data, 1))
            return 0;
      }//Access
   }

   if (owAccess(portnum))
   {
      if (!owWriteByte(portnum, 0xB4))
      {
         OWERROR(OWERROR_WRITE_BYTE_FAILED);
         return FALSE;
      }

      busybyte = owReadByte(portnum);

      while (busybyte == 0)
         busybyte = owReadByte(portnum);
   }
   if (!ReadPage(portnum, 0, page_data))
      return FALSE;

   // accuracy is only guaranteed to within 50mV so adjust away from zero
   V = ((((page_data[4] << 8) | page_data[3])) + 3) & 0x3ff;	
   return (double) V * 0.01;
}

/**
 * Sets the DS2438 to trigger a temperature read
 *
 * portnum  the port number of the port being used for the
 *          1-Wire Network.
 * SNum     the serial number for the part that the read is
 *          to be done on.
 *
 * @return temperature value in degrees C if the read was completed OK else return -999
 */
double ReadTemp(int portnum, uint8_t *SNum)
{
   signed short int t;
   int busybyte;
   uint8_t page_data[10];
   
   owSerialNum(portnum, SNum, FALSE);
   if (owAccess(portnum))
   {
      // Convert Temperature command
      if (!owWriteByte(portnum, 0x44))
      {
         OWERROR(OWERROR_WRITE_BYTE_FAILED);
         return -999;
      }

      busybyte = owReadByte(portnum);

      while (busybyte == 0)
         busybyte = owReadByte(portnum);
   }

   if (!ReadPage(portnum, 0, page_data))
      return -999;

   // if sign bit set then invert
   t = (((page_data[2] << 8) | page_data[1]) >> 3);
   if (page_data[2] & 128)
      return (double) (-(8192 - t))  * 0.03125;
   return (double) t  * 0.03125;
}


/**
 * Sets the DS2438 to do a current read
 *
 * portnum  the port number of the port being used for the
 *          1-Wire Network.
 * SNum     the serial number for the part that the read is
 *          to be done on.
 *
 * @return current as a signed integer if the read was completed OK else return 0
 */
signed short int ReadCurrent(int portnum, uint8_t *SNum)
{
   uint8_t page_data[10];
   signed short int t, result;

   owSerialNum(portnum, SNum, FALSE);

   if (!ReadPage(portnum, 0, page_data))
      return FALSE;

   // another case of the sign bit in a strange place...
   t = ((page_data[6]  << 8) | page_data[5]) & 0x3ff;
   result = t;
   if (page_data[6] & 4)
      result = -(1024 - t);
   return result;

}

/**
 * Reads a page of EEPROM/SRAM via the scratchpad
 *          assumes the serial num has already been set
 * portnum  the port number of the port being used for the
 *          1-Wire Network.
 * page_num page number to read fromn the 2438
 * rec_buf  8 byte (minimum) buffer for the page contents
 *
 * @return TRUE if all OK else return FALSE
 */

int ReadPage(int portnum, int page_num, uint8_t *rec_buf)
{
   uint8_t send_block[50];
   int send_cnt = 0;
   int i;
   uint16_t lastcrc8 = 0;

   if (owAccess(portnum))
   {
      // Recall the page
      // Recall command
      send_block[send_cnt++] = 0xB8;

      // Page to Recall
      send_block[send_cnt++] = page_num;

      if (!owBlock(portnum, FALSE, send_block, send_cnt))
         return FALSE;
   }

   if (owAccess(portnum))
   {
      send_cnt = 0;
      // Read the  byte
      // Read scratchpad command
      send_block[send_cnt++] = 0xBE;

      // Page to read
      send_block[send_cnt++] = page_num;

      for (i = 0; i < 9; i++)
         send_block[send_cnt++] = 0xFF;

      if (owBlock(portnum, FALSE, send_block, send_cnt))
      {
         setcrc8(portnum, 0);

         for (i = 2; i < send_cnt; i++)
            lastcrc8 = docrc8(portnum, send_block[i]);

         if (lastcrc8 != 0x00)
            return FALSE;
      }//Block
      else
         return FALSE;
   }

   for (i = 0; i < 8; i++)
      rec_buf[i] = send_block[i + 2];

   if (debug > 2)
   {
      printf("Block %d\n", page_num);
      for (i = 0; i < 8; i++)
         printf("Offset %d = %02x\n", i, rec_buf[i]);
      printf("\n");
   }

   return TRUE;
}

/**
 * Writes a page of EEPROM/SRAM via the scratchpad
 *          assumes the serial num has already been set
 * portnum  the port number of the port being used for the
 *          1-Wire Network.
 * page_num page number to write to the 2438
 * tx_buf   8 byte (minimum) buffer for the page contents
 *
 * @return TRUE if all OK else return FALSE
 */

int WritePage(int portnum, int page_num, uint8_t *tx_buf, int tx_len)
{
   uint8_t send_block[20];
   int send_cnt = 0;
   int i, busybyte;
   
   if (owAccess(portnum))
   {
      // Write the page back
      // Write scratchpad command
      send_block[send_cnt++] = 0x4E;

      // Write page
      send_block[send_cnt++] = page_num;

      for (i = 0; i < tx_len; i++)
         send_block[send_cnt++] = tx_buf[i];

      if (!owBlock(portnum, FALSE, send_block, send_cnt))
         return FALSE;
   }
   if (owAccess(portnum))
   {
      send_cnt = 0;
      // Copy the page
      // Copy scratchpad command
      send_block[send_cnt++] = 0x48;

      // Copy page
      send_block[send_cnt++] = page_num;

      if (owBlock(portnum, FALSE, send_block, send_cnt))
      {
          busybyte = owReadByte(portnum);

          while (busybyte == 0)
             busybyte = owReadByte(portnum);

      }//Block
   }//Access
   else
      return FALSE;

   return TRUE;
}


/**
 * Sets the DS2438 to read all values
 *
 * portnum  the port number of the port being used for the
 *          1-Wire Network.
 * pResults pointer to a structure of results
 * SNum     the serial number for the part that the read is
 *          to be done on.
 *
 * @return 'true' if the read was complete
 */

int ReadAll(int portnum, Result_t *pResult, uint8_t *SNum)
{
   uint8_t page_data[10];
   signed short int t;

   owSerialNum(portnum, SNum, FALSE);

   if (!ReadPage(portnum, 0, page_data))
      return FALSE;

   // if sign bit set then invert
   t = (((page_data[2] << 8) | page_data[1]) >> 3);
   pResult->Temp = t;
   if (page_data[2] & 128)
      pResult->Temp = -(8192 - t);
   
   pResult->Volts = ((page_data[4] << 8) | page_data[3]);

   // another case of the sign bit in a strange place...
   t = ((page_data[6]  << 8) | page_data[5]) & 0x3ff;
   pResult->Current = t;
   if (page_data[6] & 4)
      pResult->Current = -(1024 - t);
   

   if (!ReadPage(portnum, 1, page_data))
      return FALSE;
   pResult->ICA = page_data[4];

   if (!ReadPage(portnum, 7, page_data))
      return FALSE;
   pResult->CCA = (page_data[5] << 8) | page_data[4];
   pResult->DCA = (page_data[7] << 8) | page_data[6];

   return TRUE;
}

/**
 * Sets the DS2438 configuration register
 *
 * portnum  the port number of the port being used for the
 *          1-Wire Network.
 * config   value to write to the config register to set the mode
 * SNum     the serial number for the part that the read is
 *          to be done on.
 *
 * @return 'true' if the read was complete
 */
int SetupConfig(int portnum, int config, uint8_t *SNum)
{
   uint8_t send_block[50];
   int send_cnt = 0;

   owSerialNum(portnum, SNum, FALSE);
   // Get the Status/Configuration page
   if (!ReadPage(portnum, 0, send_block))
      return FALSE;

   // if there is no change required in the config reigster, then leave it alone
   if ((send_block[0] & CONF2438_ALL) == config)
      return TRUE;

   if (owAccess(portnum))
   {
      send_cnt = 0;
      // Write the Status/Configuration byte

      // set config byte
      send_block[send_cnt++] = config;
      if (WritePage(portnum, 0, send_block, send_cnt))
         return TRUE;

   }//Access

   return FALSE;
}

/**
 * Sets the offset register and clears the threshold
 * portnum  the port number of the port being used for the
 *          1-Wire Network.
 * SNum     the serial number for the part that the read is
 *          to be done on.
 *
 * @return TRUE if all OK else return FALSE
 */

int  SetCalibration(int portnum, uint8_t *SNum, int offset)
{

   uint8_t rec_block[20];
   int i;


   // turn off current sensing
   // this also sets the serial number which read/write page rely on
   SetupConfig(portnum, 0, SNum);
   // wait for the ADC to stop
   msDelay(40);

   // Get offset register from page 1
   if (!ReadPage(portnum, 1, rec_block))
      return FALSE;

   printf("Initial offset reg = %02x%02x\n", rec_block[6], rec_block[5]);
   // clear the offset reg
   rec_block[5] = 0;
   rec_block[6] = 0;
   
   if (!owAccess(portnum))
      return FALSE;

   // Write the page back
   if (!WritePage(portnum, 1, rec_block, 8))
      return FALSE;

   SetupConfig(portnum, CONF2438_IAD, SNum);
   // wait for the ADC to start
   msDelay(40);
   // Get Current register - it should read zero from page 0
   if (!ReadPage(portnum, 0, rec_block))
      return FALSE;

   printf("Initial Current reg = %02x%02x\n", rec_block[6], rec_block[5]);

   // turn off current sensing
   SetupConfig(portnum, 0, SNum);
   // wait for the ADC to stop
   msDelay(40);
   
   i = (rec_block[6] << 8) | rec_block[5];
   // get a clean copy of what is in page 1
   if (!ReadPage(portnum, 1, rec_block))
      return FALSE;

   if (offset)
      i = offset;
   i = (0 - i) << 3;
   rec_block[5] = i & 0xff;
   rec_block[6] = i >> 8;
   printf("Value going into offset = %02x%02x\n", rec_block[6], rec_block[5]);
   
   if (!owAccess(portnum))
      return FALSE;

   // Write the page back
   if (!WritePage(portnum, 1, rec_block, 8))
      return FALSE;

   // turn current sensing back on
   SetupConfig(portnum, CONF2438_IAD, SNum);
   msDelay(40);

   if (!ReadPage(portnum, 0, rec_block))
      return FALSE;

   printf("New Current reg = %02x%02x\n", rec_block[6], rec_block[5]);
   printf("Threshold reg = %02x\n", rec_block[7]);
  
   if (!ReadPage(portnum, 1, rec_block))
   return FALSE;

   printf("New offset reg = %02x%02x\n", rec_block[6], rec_block[5]);

   return TRUE;
    

}

/**
 * Saves a value into one of the non-volatile registers
 * portnum  the port number of the port being used for the
 *          1-Wire Network.
 * SNum     the serial number for the part that the read is
 *          to be done on.
 * location Offset in available non-volatile stores starting from 0
 * value    what to store
 *
 * @return TRUE if all OK else return FALSE
 */

int  SaveNVM(int portnum, uint8_t *SNum, uint8_t location, uint8_t value)
{
   uint8_t page_data[10], page, offset;
   
   page = (location / 8)  + 4;
   offset = location % 8; 

   owSerialNum(portnum, SNum, FALSE);
   if (!ReadPage(portnum, page, page_data))
      return FALSE;

   page_data[offset] = value;     // slip in the value to save

   // Write the page to back
   if (!WritePage(portnum, page, page_data, 8))
      return FALSE;

   return TRUE;
}


/**
 * Sets the ICA register
 * portnum  the port number of the port being used for the
 *          1-Wire Network.
 * SNum     the serial number for the part that the read is
 *          to be done on.
 * value    what to write to the ICA register
 *
 * @return TRUE if all OK else return FALSE
 */
int SetICA(int portnum, uint8_t *SNum, uint8_t value)
{
   uint8_t page_data[10];
   
   owSerialNum(portnum, SNum, FALSE);

   if (!owAccess(portnum))
      return FALSE;

   // Get ICA register from page 1
   if (!ReadPage(portnum, 1, page_data))
      return FALSE;

   if (debug)
      printf("ICA was = %02x\n", page_data[4]);
      
   page_data[4] = value;     // slip in the new value
   
   if (!owAccess(portnum))
      return FALSE;

   // Write the page back
   if (!WritePage(portnum, 1, page_data, 8))
      return FALSE;

   if (debug)
      printf("ICA now = %02x\n", page_data[4]);

   return TRUE;
   
}

int SetCCADCA(int portnum, uint8_t *SNum, int cca, int dca)
{
   uint8_t page_data[10];
   
   owSerialNum(portnum, SNum, FALSE);

   if (!owAccess(portnum))
      return FALSE;

   // Get registers from page 7
   if (!ReadPage(portnum, 7, page_data))
      return FALSE;

   printf("Setting CCA and DCA\n");
   // do the CCA/DCA regs
   if (cca >= 0)
   {
      page_data[4] = cca & 0xff;
      page_data[5] = cca >> 8;
   }
   if (dca >= 0)
   {
      page_data[6] = dca & 0xff;
      page_data[7] = dca >> 8;
   }
      
   if (!owAccess(portnum))
      return FALSE;

   // Write the page back
   if (!WritePage(portnum, 7, page_data, 8))
      return FALSE;

   return TRUE;
}

