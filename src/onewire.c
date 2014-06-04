#include <stdint.h>
#include <util/delay.h>
#include <avr/io.h>
#include "onewire.h"
#include "pinmanip.h"

/*
 * Reccomended 480us Minimum time for ready pulse time blocks.
 * The  util/delay.h _delay_us function isn't very precise, so 485us is default.
 */
#define ready_PULSE_TIME 485
#define ready_RESPONSE_TIME 30

/* 
 * The 1-wire bus should be pulled low for as short as possible when 
 * initiating a read slot, with a minimum time is 1us. The master must  
 * release the bus and sample the bus within 15us from the start of the time
 * slot. The optimal time for sampling is towards the end of the 15us block.
 */
#define TIME_SLOT_DURATION 60
#define READ_SAMPLE_DELAY 10 

bool onewire_setup_device(struct OnewireDevice *device, 
                            volatile uint8_t *bus_register, uint8_t bus_pin)
{
  device->bus_port = bus_register;
  device->bus_pin = bus_pin;
  // Check if a device is connected on the line
  if (onewire_send_reset_pulse(device)) {
      return onewire_get_ROM_code(device);
  } else {
      return false;
  }
}

void onewire_send_byte(struct OnewireDevice *device, uint8_t byte)
{
  // 1-Wire bus protocol calls for sending least-significant bit first
  for (uint8_t i = 0; i < 8; i++) {
      set_pin_mode(device->bus_port, device->bus_pin, OUTPUT);
      clear_pin(device->bus_port, device->bus_pin);
      _delay_us(1);
      if (get_bit(byte, i)) {
          set_pin_mode(device->bus_port, device->bus_pin, INPUT);
      }
      _delay_us(TIME_SLOT_DURATION);
      set_pin_mode(device->bus_port, device->bus_pin, INPUT);
  }
}

uint8_t onewire_read_byte(struct OnewireDevice *device)
{
  int8_t byte = 0;
  // 1-Wire protocol sends LSB first
  for (uint8_t i = 0; i < 8; i++) {
      set_pin_mode(device->bus_port, device->bus_pin, OUTPUT);
      clear_pin(device->bus_port, device->bus_pin);
      _delay_us(1);
      set_pin_mode(device->bus_port, device->bus_pin, INPUT);
      _delay_us(READ_SAMPLE_DELAY);
      byte |= (!get_bit_pu(*port_to_pin_reg(device->bus_port), device->bus_pin)) << i;
      _delay_us(TIME_SLOT_DURATION - READ_SAMPLE_DELAY);
  }
  return byte;
}

bool onewire_send_reset_pulse(struct OnewireDevice *device)
{
  int8_t presence_detected;

  // Initiate ready sequence by pulling the bus line low for 480us
  set_pin(device->bus_port, device->bus_pin + 1);
  set_pin_mode(device->bus_port, device->bus_pin, OUTPUT);
  clear_pin(device->bus_port, device->bus_pin);
  _delay_us(ready_PULSE_TIME); 

  // Release the bus line
  set_pin(device->bus_port, device->bus_pin);
  _delay_us(ready_RESPONSE_TIME); 

  // Read PRESENSE signal
  set_pin_mode(device->bus_port, device->bus_pin, INPUT);
  presence_detected = get_bit_pu(*port_to_pin_reg(device->bus_port), device->bus_pin);
  _delay_us(ready_PULSE_TIME - ready_RESPONSE_TIME);
  return presence_detected;
}

bool onewire_get_ROM_code(struct OnewireDevice *device)
{
  if (!onewire_send_reset_pulse(device)) {
      return false;
  }
  onewire_send_byte(device, READ_ROM);
  device->family_code = onewire_read_byte(device);
  uint8_t bytes[7];
  device->serial_no = 0;
  for (int8_t i = 0; i < 7; i++) {
      bytes[i] = onewire_read_byte(device);
  }

  for (int8_t i = 6; i >= 0; i--) {
      device->serial_no <<= 8;
      device->serial_no |= bytes[i];
  }
  
  device->crc = onewire_read_byte(device);
  return true;
}
