/*
* Copyright (c) Ryabikov Aleksandr <a.ryabikov@afteri.ru> <rsashka@mail.ru>
* All rights reserved.
*
* This code is released under Apache License 2.0. Please see the
* file at : https://github.com/rsashka/SCI2C/blob/master/LICENSE
*/

#include <Wire.h>

#include <vector>

// I2C address on chip A7001
#define ADDR_A7001 static_cast<uint16_t>(0x48)


using namespace std;
typedef std::vector<uint8_t> vect;


//--------------------------------------------------------------------------
// Output dump data by serial port
void vect_dump(const char * prefix, const vect & v, const size_t start = 0, const size_t count = 0)
{
  if(prefix)
  {
    Serial.print(prefix);
  }
  if(v.size() < start)
  {
    Serial.println("Empty");
    return;
  }
  for(size_t i=0; i < (v.size()-start) && (count == 0 || i < count); i++)
  {
    uint8_t b = v[start + i];

    // Format output HEX data
    if(i) Serial.print(" ");
    if(b < 0x0F) Serial.print("0");
 
    Serial.print(b, HEX);
  }
  Serial.println("");
}

//--------------------------------------------------------------------------
// Send array bytes by I2C to address A7001 and read response result_size bytes 
vect sci2c_exchange(const vect data, const uint8_t result_size)
{
  Wire.beginTransmission(ADDR_A7001);
  Wire.write(data.data(), data.size()); 
  Wire.endTransmission(false);
  Wire.requestFrom(ADDR_A7001, result_size, true);
  //delay(1);
  
  vect result(result_size, 0);
  if(result_size >= 2)
  {
    result[0] = Wire.read(); // Data size CDB
    result[1] = Wire.read(); // PCB 

    for(size_t i=2; i<result.size()-2 && Wire.available(); i++)
    {
      result[i+2] = Wire.read();
    }
  }
  return result;
}


//--------------------------------------------------------------------------
// Read Status Code
uint8_t sci2c_status(const char * msg = nullptr)
{
  vect v = sci2c_exchange({0b0111}, 2);
  uint8_t status = v[1] >> 4;
  if(msg)
  {
    Serial.print(msg); // Prefix
  
    switch(status)
    {
      case 0b0000: Serial.println("OK (Ready)"); break;
      case 0b0001: Serial.println("OK (Busy)"); break;

      case 0b1000: Serial.println("ERROR (Exception raised)"); break;
      case 0b1001: Serial.println("ERROR (Over clocking)"); break;
      case 0b1010: Serial.println("ERROR (Unexpected Sequence)"); break;
      case 0b1011: Serial.println("ERROR (Invalid Data Length)"); break;
      case 0b1100: Serial.println("ERROR (Unexpected Command)"); break;
      case 0b1101: Serial.println("ERROR (Invalid EDC)"); break;
      default: 
        Serial.print("ERROR (Other Exception ");
        Serial.print(status, BIN);
        Serial.println("b)");
        break;
    }
  }
  return status;
}

static uint8_t apdu_master_sequence_counter = 0; // Sequence Counter Master, Master to Slave

//--------------------------------------------------------------------------
// Send APDU
void sci2c_apdu_send(const vect apdu)
{
  vect_dump("C-APDU => ", apdu);
  vect data(2, 0); // 0x00 - Master to Slave Data Transmission command + reserve to length
  data.insert(data.end(), std::begin(apdu), std::end(apdu));

  data[0] |= (apdu_master_sequence_counter << 4);
  if(++apdu_master_sequence_counter > 0b111)
  {
    apdu_master_sequence_counter = 0;
  }

  data[1] = data.size() - 2;
  sci2c_exchange(data, 2);
  delay(10);
  sci2c_status("");
}


//--------------------------------------------------------------------------
// Receive APDU
vect sci2c_apdu_recv(uint8_t result_size)
{
  Wire.beginTransmission(ADDR_A7001);
  Wire.write(0b0010); // 0010b - Slave to Master Data Transmission command
  Wire.endTransmission(false);
  Wire.requestFrom(ADDR_A7001, result_size, true);
  
  vect result(result_size, 0);
  for(size_t i=0; i<result.size() && Wire.available(); i++)
  {
    result[i] = Wire.read();
  }

  vect_dump("R-APDU <= ", result, 2);

  return result;
}


//--------------------------------------------------------------------------
void setup(){
    Wire.begin();

    Serial.begin(9600);
    while (!Serial);
    Serial.println("");
    Serial.println("Smart Card I2C Protocol Arduino demo on A7001");
    Serial.println("");


    sci2c_exchange({0b00001111}, 2); //The bits b0 to b5 set to 001111b indicate the Wakeup command.
    sci2c_exchange({0b00001111}, 2); //The bits b0 to b5 set to 001111b indicate the Wakeup command.

    // Soft Reset
    sci2c_exchange({0b00011111}, 2); //The bits b0 to b5 set to 011111b indicate the Soft Reset command.
    delay(5); // Wait at least tRSTG  (time, ReSeT Guard)
    sci2c_status("Status SoftReset: ");

                  
    // Read ATR
    vect ATR = sci2c_exchange({0b101111}, 29+2); //The bits b0 to b5 set to 101111b indicate the Read Answer to Reset command.
    sci2c_status("Status ATR: ");
    vect_dump("ATR: ", ATR);
    delay(1);
    
    // Parameter Exchange
    // The bits b0 to b5 set to 111111b of the PCB send by the master device indicate the Parameter Exchange command.
    // The bits b6 and b7 of the PCB send by the master device code the CDBIsm,max(Command Data Bytes Integer, Slave to Master, MAXimum)
    vect CDB = sci2c_exchange({0b11111111}, 2); 
    sci2c_status("Status CDB: ");
    vect_dump("CDB: ", CDB, 1);


    // Further examples of the exchange of APDU

    // Exchanges APDU from exmaple chapter
    sci2c_apdu_send({0x00, 0xA4, 0x04, 0x04, 0x04, 0x54, 0x65, 0x73, 0x74, 0x00});
    sci2c_status("Status Test send: ");
    sci2c_apdu_recv(3+1); // R-APDU size + 1 byte PBC
    sci2c_status("Status Test recv: ");
    

    // Read Card Production Life Cycle
    sci2c_apdu_send({0x80, 0xCA, 0x9F, 0x7F, 0x00});
    sci2c_status("Status card LC send: ");
    sci2c_apdu_recv(0x30+1); // R-APDU size + 1 byte PBC
    sci2c_status("Status card LC recv: ");

    // Read Card Info
    sci2c_apdu_send({0x80, 0xCA, 0x00, 0x66, 0x00});
    sci2c_status("Status card info send: ");
    sci2c_apdu_recv(0x51+1); // R-APDU size + 1 byte PBC
    sci2c_status("Status card info recv: ");


    // Read Key Info
    sci2c_apdu_send({0x80, 0xCA, 0x00, 0xE0, 0x00});
    sci2c_status("Status key send: ");
    sci2c_apdu_recv(0x17+1); // R-APDU size + 1 byte PBC
    sci2c_status("Status key recv: ");


    // Again exchanges APDU from exmaple chapter
    sci2c_apdu_send({0x00, 0xA4, 0x04, 0x04, 0x04, 0x54, 0x65, 0x73, 0x74, 0x00});
    sci2c_status("Status Test send: ");
    sci2c_apdu_recv(3+1); // R-APDU size + 1 byte PBC
    sci2c_status("Status Test recv: ");
    
    
    Serial.println("Done!\n");

} 

//--------------------------------------------------------------------------
void loop() 
{
  delay(100);
}
