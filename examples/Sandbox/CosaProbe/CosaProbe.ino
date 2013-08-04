/**
 * @file CosaProbe.ino
 * @version 1.0
 *
 * @section License
 * Copyright (C) 2013, Mikael Patel
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA  02111-1307  USA
 *
 * @section Description
 * Probe to sample pulse sequence for analysis.
 *
 * This file is part of the Arduino Che Cosa project.
 */

#include "Cosa/Types.h"
#include "Cosa/ExternalInterrupt.hh"
#include "Cosa/RTC.hh"
#include "Cosa/Power.hh"
#include "Cosa/Watchdog.hh"
#include "Cosa/IOStream/Driver/UART.hh"
#include "Cosa/Trace.hh"

/**
 * Probe to collect samples using an external interrupt pin.
 * Interrupt handler captures and records the pulse with until
 * max number of samples or illegal pulse. Sample request/await
 * starts interrupt handler and stops on max time.
 */
class Probe : public ExternalInterrupt {
private:
  static const uint8_t SAMPLE_MAX = 128;

  volatile uint16_t m_sample[SAMPLE_MAX];
  volatile uint8_t m_sampling;
  volatile uint32_t m_start;
  volatile uint8_t m_ix;
  
  const uint16_t LOW_THRESHOLD;
  const uint16_t HIGH_THRESHOLD;

  virtual void on_interrupt(uint16_t arg = 0);

public:
  Probe(Board::ExternalInterruptPin pin, uint16_t low, uint16_t high) : 
    ExternalInterrupt(pin, ExternalInterrupt::ON_CHANGE_MODE),
    m_sampling(false),
    m_start(0L),
    m_ix(0),
    LOW_THRESHOLD(low),
    HIGH_THRESHOLD(high)
  {
  }

  void sample_request();
  void sample_await(uint32_t ms);

  friend IOStream& operator<<(IOStream& outs, Probe& probe);
};

IOStream& operator<<(IOStream& outs, Probe& probe)
{
  outs << probe.m_ix << ':';
  for (uint8_t i = 0; i < probe.m_ix; i++)
    outs << ' ' << probe.m_sample[i];
  return (outs);
}

void 
Probe::on_interrupt(uint16_t arg) 
{ 
  if (m_start == 0L) {
    m_start = RTC::micros();
    m_ix = 0;
    return;
  }
  uint32_t stop = RTC::micros();
  uint16_t us = (stop - m_start);
  m_start = stop;
  m_sample[m_ix++] = us;
  if (us < LOW_THRESHOLD || us > HIGH_THRESHOLD) goto exception;
  if (m_ix != SAMPLE_MAX) return;
  
 exception:
  m_sampling = false;
  disable();
}

void 
Probe::sample_request()
{
  m_sampling = true;
  m_start = 0L;
  enable();
}

void 
Probe::sample_await(uint32_t ms)
{
  uint32_t start = RTC::millis();
  while (m_sampling && RTC::since(start) < ms) Power::sleep(SLEEP_MODE_IDLE);
  disable();
}

Probe probe(Board::EXT0, 20, 100);

void setup()
{
  uart.begin(9600);
  trace.begin(&uart, PSTR("CosaProbe: started"));
  Watchdog::begin();
  RTC::begin();
}

void loop()
{
  SLEEP(2);

  // Make a request (DHT)
  probe.set_mode(IOPin::OUTPUT_MODE);
  probe.set();
  probe.clear();
  Watchdog::delay(32);
  probe.set();
  probe.set_mode(IOPin::INPUT_MODE);
  DELAY(40);

  // Wait for the response
  probe.sample_request();
  probe.sample_await(1000);

  // Print samples
  trace << probe << endl;
}
