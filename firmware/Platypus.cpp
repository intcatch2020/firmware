#include "Platypus.h"

using namespace platypus;

USARTClass *platypus::SERIAL_PORTS[4] = {
  NULL,
  &Serial1,
  &Serial2,
  &Serial3,
};

SerialHandler_t platypus::SERIAL_HANDLERS[4] = {
  {NULL, NULL},
  {NULL, NULL},
  {NULL, NULL},
  {NULL, NULL}
};

void serialEvent1() 
{
  if (SERIAL_HANDLERS[1].handler != NULL) 
  {
    (*SERIAL_HANDLERS[1].handler)(SERIAL_HANDLERS[1].data);
  }
}

void serialEvent2() 
{
  if (SERIAL_HANDLERS[2].handler != NULL) 
  {
    (*SERIAL_HANDLERS[2].handler)(SERIAL_HANDLERS[2].data);
  }
}

void serialEvent3() 
{ 
  if (SERIAL_HANDLERS[3].handler != NULL) 
  {
    (*SERIAL_HANDLERS[3].handler)(SERIAL_HANDLERS[3].data);
  }
}

Led::Led()
  : r_(0), g_(0), b_(0)
{
  pinMode(board::LED.R, OUTPUT);
  digitalWrite(board::LED.R, HIGH);

  pinMode(board::LED.G, OUTPUT);
  digitalWrite(board::LED.G, HIGH);

  pinMode(board::LED.B, OUTPUT);
  digitalWrite(board::LED.B, HIGH);
}

Led::~Led()
{
  pinMode(board::LED.R, INPUT);
  pinMode(board::LED.G, INPUT);
  pinMode(board::LED.B, INPUT);
}

void Led::set(int red, int green, int blue)
{
  R(red);
  G(green);
  B(blue);
}

void Led::R(int red)
{
  r_ = red;
  digitalWrite(board::LED.R, !r_);
}

int Led::R()
{
  return r_;
}

void Led::G(int green)
{
  g_ = green;
  digitalWrite(board::LED.G, !g_);
}

int Led::G()
{
  return g_;
}

void Led::B(int blue)
{
  b_ = blue;
  digitalWrite(board::LED.B, !b_);
}

int Led::B()
{
  return b_;
}

Motor::Motor(int channel)
  : enable_(board::MOTOR[channel].ENABLE), enabled_(false), velocity_(0)
{
  servo_.attach(board::MOTOR[channel].SERVO);
  pinMode(enable_, OUTPUT);
  digitalWrite(enable_, LOW);
}

Motor::~Motor()
{
  pinMode(enable_, INPUT);
  digitalWrite(enable_, LOW);
  servo_.detach();
}

void Motor::velocity(float velocity)
{
  if (velocity > 1.0) {
    velocity = 1.0;
  }
  if (velocity < -1.0) {
    velocity = -1.0;
  }
  velocity_ = velocity;

  float command = (velocity * 600) + 1500;
  servo_.writeMicroseconds(command);
}

float Motor::velocity()
{
  return velocity_;
}

void Motor::enable(bool enabled)
{
  enabled_ = enabled;
  digitalWrite(enable_, enabled_);

  if (!enabled_)
  {
    velocity(0.0);
  }
}

bool Motor::enabled()
{
  return enabled_;
}

void Motor::enable()
{
  enable(true);
}

void Motor::disable()
{
  enable(false);
}

float Motor::current()
{
  // TODO: fill me in.
  return 0.0;
}

bool Motor::set(char *param, char *value)
{
  // Set motor velocity.
  if (!strncmp("v", param, 2))
  {
    float v = atof(value);
    velocity(v);
    return true;
  }
  // Return false for unknown command.
  else
  {
    return false;
  }
}

Sensor::Sensor(int channel) 
: channel_(channel)
{  
  // Disable RSxxx receiver
  pinMode(board::SENSOR[channel].RX_DISABLE, OUTPUT);
  digitalWrite(board::SENSOR[channel].RX_DISABLE, HIGH);

  // Disable RSxxx transmitter
  pinMode(board::SENSOR[channel].TX_ENABLE, OUTPUT);
  digitalWrite(board::SENSOR[channel].TX_ENABLE, LOW);

  // Disable RS485 termination resistor
  pinMode(board::SENSOR[channel].RS485_TE, OUTPUT);
  digitalWrite(board::SENSOR[channel].RS485_TE, LOW);

  // Select RS232 (deselect RS485)
  pinMode(board::SENSOR[channel].RS485_232, OUTPUT);
  digitalWrite(board::SENSOR[channel].RS485_232, LOW);

  // TODO: deconflict this!
  if (channel < 2)
  {
    // Disable half-duplex
    pinMode(board::HALF_DUPLEX01, OUTPUT);
    digitalWrite(board::HALF_DUPLEX01, LOW);
  } 
  else 
  {
    // Disable half-duplex
    pinMode(board::HALF_DUPLEX23, OUTPUT);
    digitalWrite(board::HALF_DUPLEX23, LOW);
  }
  
  // Disable loopback test
  pinMode(board::LOOPBACK, OUTPUT);
  digitalWrite(board::LOOPBACK, LOW);
  
  // Disable 12V output
  pinMode(board::SENSOR[channel].PWR_ENABLE, OUTPUT);
  digitalWrite(board::SENSOR[channel].PWR_ENABLE, LOW);
  
  // Register serial event handler
  SerialHandler_t handler = {Sensor::onSerial_, this}; 
  SERIAL_HANDLERS[channel] = handler;
}

Sensor::~Sensor()
{
  // TODO: fill me in
}

bool Sensor::set(char* param, char* value)
{
  return false;
}

void Sensor::onSerial() 
{
  // Default to doing nothing on serial events. 
}

void Sensor::onSerial_(void *data)
{
  // Resolve self-reference and call member function.
  Sensor *self = (Sensor*)data;
  self->onSerial();
}
