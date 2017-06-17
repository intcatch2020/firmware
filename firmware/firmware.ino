#include "Platypus.h"
#include "Components.h"
#include <adk.h>

// Arduino headers used in Platypus.h
// (informs the IDE to link these libraries)
#include <Servo.h>
#include <Scheduler.h>

// JSON parsing library
#include <ArduinoJson.h>

// TODO: remove me
#include "Board.h"

//Typedefs
typedef platypus::SerialState SerialState;



// ADK USB Host configuration
/* Make server accept version 4.x.x */
char applicationName[] = "Platypus Server"; // the app on Android
char accessoryName[] = "Platypus Control Board"; // your Arduino board
char companyName[] = "Platypus LLC";
char versionNumber[] = "3.0";
char serialNumber[] = "3";
char url[] = "http://senseplatypus.com";

// Board parameters
char firmwareVersion[] = "4.2.0";
char boardVersion[] = "4.2.0";


#define PMTK_SET_NMEA_OUTPUT_RMCONLY "$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29"
#define PMTK_SET_NMEA_UPDATE_5HZ	"$PMTK220,200*2C"
#define PMTK_API_SET_FIX_CTL_5HZ	"$PMTK300,200,0,0,0,0*2F"


// ADK USB Host
USBHost Usb;
ADK adk(&Usb, companyName, applicationName, accessoryName, versionNumber, url, serialNumber);

// Android send/receive buffers
const size_t INPUT_BUFFER_SIZE = 512;
char input_buffer[INPUT_BUFFER_SIZE+1];
char debug_buffer[INPUT_BUFFER_SIZE+1];

const size_t OUTPUT_BUFFER_SIZE = 576;
char output_buffer[OUTPUT_BUFFER_SIZE+3];

static unsigned long last_command_time = 0;
static unsigned long time_at_connected = 0;

// System state enumeration
SerialState serial_state = SerialState::STANDBY;

// Time betweeen commands before we consider the Android
// server to be unresponsive.
const size_t RESPONSE_TIMEOUT_MS = 500;

// Time to wait before dropping into DISCONNECTED state when USB cable is disconnected
// Deals with dodgy USB connections and improves USB C support for all cables
const size_t CONNECTION_TIMEOUT_MS = 500;
const size_t CONNECT_STANDBY_TIMEOUT = 3000;

// Define the systems on this board
// TODO: move this board.h?
platypus::Led rgb_led;

/**
 * Wrapper for ADK send command that copies data to debug port.
 * Requires a null-terminated char* pointer.
 */
void send(char *str)
{
	// Add newline termination
	// TODO: Make sure we don't buffer overflow
	size_t len = strlen(str);
	str[len++] = '\r';
	str[len++] = '\n';
	str[len] = '\0';

	// Write string to USB.
	if (adk.isReady()) adk.write(len, (uint8_t*)str);
	// Copy string to debugging console.
	Serial.print("-> ");
	Serial.print(str);
}

/**
 * Returns a JSON error message to the connected USB device and the
 * serial debugging console.
 */
void reportError(const char *error_message, const char *buffer)
{
	// Construct a JSON error message.
	snprintf(output_buffer, OUTPUT_BUFFER_SIZE,
					 "{"
					 "\"error\": \"%s\","
					 "\"args\": \"%s\""
					 "}",
					 error_message, buffer);
	send(output_buffer);
}

/**
 * Handler to respond to incoming JSON commands and dispatch them to
 * configurable hardware components.
 */
void handleCommand(char *buffer)
{
	// Allocate buffer for JSON parsing
	StaticJsonBuffer<200> jsonBuffer;

	// Attempt to parse JSON in buffer
	JsonObject& command = jsonBuffer.parseObject(buffer);

	// Check for parsing error
	if (!command.success())
		{
			// Parsing Failure
			reportError("Failed to parse JSON command.", buffer);
			return;
		}

	for (JsonObject::iterator it=command.begin(); it!=command.end(); ++it)
		{
			const char * key = it->key;

			platypus::Configurable * target_object;
			size_t object_index;

			// Determine target object
			switch (key[0]){
			case 'm': // Motor command
				object_index = key[1] - '0';

				if (object_index >= board::NUM_MOTORS){
					reportError("Invalid motor index.", buffer);
					return;
				}

				target_object = platypus::motors[object_index];
				break;

			case 's': // Sensor command
				object_index = key[1] - '0';

				if (object_index >= board::NUM_SENSORS){
					reportError("Invalid sensor index.", buffer);
					return;
				}
				target_object = platypus::sensors[object_index];
				break;

			case 'e': //eboard command
				target_object = platypus::eboard;
				break;

			default: // Unrecognized target
				reportError("Unknown command target.", buffer);
				return;
			}

			// Extract JsonObject with param:value pairs
			JsonObject& params = it->value;
			// Todo: Move this parsing to specific components and pass ref to params instead
			// Iterate over and set parameter:value pairs on target object
			for (JsonObject::iterator paramIt=params.begin(); paramIt!=params.end(); ++paramIt)
				{

					const char * param_name = paramIt->key;
					const char * param_value = paramIt->value;

					/* Serial.print("Sending command to "); */
					/* Serial.print(key); */
					/* Serial.print(": "); */
					/* Serial.print(param_name); */
					/* Serial.print(" : "); */
					/* Serial.println(param_value); */

					if (!target_object->set(param_name, param_value)) {
						reportError("Invalid parameter set.", buffer);
						continue; // Todo: Should we return or continue?
					}
				}
		}
}

void setup()
{
	delay(1000);

	// Latch power shutdown line high to keep board from turning off.
	pinMode(board::PWR_KILL, OUTPUT);
	digitalWrite(board::PWR_KILL, HIGH);

	// Initialize debugging serial console.
	Serial.begin(115200);
	// Start the system in the disconnected state
	serial_state = SerialState::STANDBY;

	// Set ADC Precision:
	analogReadResolution(12);

	/*
	// Set GPS Settings
	Serial1.begin(9600);
	Serial1.setTimeout(250);
	// Set output to RMC only
	Serial1.println(PMTK_SET_NMEA_OUTPUT_RMCONLY);
	Serial1.flush();
	// Set output rate to 5Hz
	Serial1.println(PMTK_API_SET_FIX_CTL_5HZ);
	// Set fix rate to 5Hz
	Serial1.println(PMTK_SET_NMEA_UPDATE_5HZ);
	Serial1.flush();
	*/

	// Initialize EBoard object
	platypus::eboard = new platypus::EBoard();

	// Initialize and power all peripherals (WiFi & Pump)
	platypus::peripherals[0] = new platypus::Peripheral(0, true);
	platypus::peripherals[1] = new platypus::Peripheral(1, true);

	// Initialize External sensors
	platypus::sensors[0] = new platypus::AdafruitGPS(0, 0);
	platypus::sensors[1] = new platypus::AdafruitGPS(1, 1);
	platypus::sensors[2] = new platypus::AdafruitGPS(2, 2);
	platypus::sensors[3] = new platypus::EmptySensor(3, 3);

	// Initialize Internal sensors
	platypus::sensors[4] = new platypus::BatterySensor(4);
	// platypus::sensors[5] = new platypus::IMU(5);


	// Initialize motors
	platypus::motors[0] = new platypus::Dynamite(0);
	platypus::motors[1] = new platypus::Dynamite(1);

	// Make the ADK buffers into null terminated string.
	debug_buffer[INPUT_BUFFER_SIZE] = '\0';
	input_buffer[INPUT_BUFFER_SIZE] = '\0';
	output_buffer[OUTPUT_BUFFER_SIZE] = '\0';
	// Create secondary tasks for system.
	Scheduler.startLoop(motorUpdateLoop);
	Scheduler.startLoop(serialLoop);
	Scheduler.startLoop(ADKLoop);

	// Initialize Platypus library.
	platypus::init();

	// Print header indicating that board successfully initialized

	Serial.println(F("------------------------------"));
	Serial.println(companyName);
	Serial.println(url);
	Serial.println(accessoryName);
	Serial.println(versionNumber);
	Serial.println(F("------------------------------"));

	// Turn LED to startup state.
	rgb_led.set(255, 0, 255);
	delay(1000);
}

void loop()
{
	/* Serial.print("last cmd recvd: "); */
	/* Serial.println(last_command_time); */

	//Serial.println(serial_state);
	unsigned long current_time = millis();
	if (serial_state == SerialState::ACTIVE) //if youre in active drop to connected
		{
			if (millis() - last_command_time >= RESPONSE_TIMEOUT_MS)
				{
					serial_state = SerialState::CONNECTED;
					Serial.println("STATE: CONNECTED");
					last_command_time = millis();
				}
		}
	else if (serial_state == SerialState::CONNECTED)
		{
			/* Serial.println("time diff");	 */
			/* Serial.println(millis() - last_command_time); */
			if (millis() - last_command_time >= CONNECT_STANDBY_TIMEOUT)
				{
					serial_state = SerialState::STANDBY;
		Serial.println("STATE: STANDBY");
				}
		}
	yield();
}

void batteryUpdateLoop()
{
	int rawVoltage = analogRead(board::V_BATT);
	double voltageReading = 0.008879*rawVoltage + 0.09791;

	char output_str[128];
	snprintf(output_str, 128,
					 "{"
					 "\"s4\":{"
					 "\"type\":\"battery\","
					 "\"data\":\"%.3f %f %f\""
					 "}"
					 "}",
					 voltageReading,
					 platypus::motors[0]->velocity(),
					 platypus::motors[1]->velocity()
					 );
	send(output_str);
	yield();
}


/**
 * Periodically sends motor velocity updates.
 */
void motorUpdateLoop()
{
	// Wait for a fixed time period.
	delay(100);

	// Set the LED for current system state.
	unsigned c = (millis() >> 8) & 1;
	if (c > 128) c = 255 - c;

	/* REPLACE WITH SERIAL STATE */

	switch (serial_state)
		{
		case SerialState::STANDBY:
			// Red pulse
			rgb_led.set(c, 0, 0);
			break;
		case SerialState::CONNECTED:
			// Yellow pulse
			rgb_led.set(c, c/4, 0);
			break;
		case SerialState::ACTIVE:
			// Green pulse
			rgb_led.set(0, c, 0);
			break;
		}

	// Handle the motors appropriately for each system state.
	switch (serial_state)
		{
		case SerialState::STANDBY:
			// Turn off motors.
			for (size_t motor_idx = 0; motor_idx < board::NUM_MOTORS; ++motor_idx)
				{
					platypus::Motor* motor = platypus::motors[motor_idx];
					if (motor->enabled())
						{
							Serial.print("Disabling motor "); Serial.println(motor_idx);
							motor->disable();
						}
				}
			break;
		case SerialState::CONNECTED:
			// Decay all motors exponentially towards zero speed.
			for (size_t motor_idx = 0; motor_idx < board::NUM_MOTORS; ++motor_idx)
				{
					platypus::Motor* motor = platypus::motors[motor_idx];
					motor->set("v", "0.0"); 
				}
			// NOTE: WE DO NOT BREAK OUT OF THE SWITCH HERE!
		case SerialState::ACTIVE:
			// Rearm motors if necessary.
			for (size_t motor_idx = 0; motor_idx < board::NUM_MOTORS; ++motor_idx)
				{
					platypus::Motor* motor = platypus::motors[motor_idx];
					if (!motor->enabled())
						{
							Serial.print("Arming motor "); Serial.print(motor_idx);
							motor->arm();
							Serial.println(F("Motor Armed"));
						}
				}
			break;
		}

	// Send status updates while connected to server.
	if (serial_state == SerialState::ACTIVE)
		{
			// TODO: move this to another location (e.g. Motor)
			// Send motor status update over USB
			snprintf(output_buffer, OUTPUT_BUFFER_SIZE,
							 "{"
							 "\"m0\":{"
							 "\"v\":%f"
							 "},"
							 "\"m1\":{"
							 "\"v\":%f"
							 "}"
							 "}",
							 platypus::motors[0]->velocity(),
							 platypus::motors[1]->velocity()
							 );
			send(output_buffer);
		}
	yield();
}

/**
 * Reads from serial debugging console and attempts to execute commands.
 */

void serialLoop()
{
	unsigned long current_command_time = millis();
	static size_t debug_buffer_idx = 0;
	// Wait until characters are received.

	if (Serial.available())
		{
			// Put the new character into the buffer, ignore \n and \r
			char c = Serial.read();
			if (c != '\n' && c != '\r'){
				debug_buffer[debug_buffer_idx++] = c;
			}
			// If it is the end of a line, or we are out of space, parse the buffer.
			if (debug_buffer_idx >= INPUT_BUFFER_SIZE || c == '\n' || c == '\r')
				{
					// Properly null-terminate the buffer.
					debug_buffer[debug_buffer_idx] = '\0';
					debug_buffer_idx = 0;
		
					last_command_time = current_command_time;
					handleCommand(debug_buffer);
				}
		}
	yield();
}
void ADKLoop()
{
	uint32_t bytes_read;
	Usb.Task();
	if (adk.isReady())
		{
			//read from adk usb
			if (serial_state != SerialState::ACTIVE)
	{
		serial_state = SerialState::ACTIVE;
		Serial.println("STATE: ACTIVE");
	}
			unsigned long current_command_time = millis();
			adk.read(&bytes_read, INPUT_BUFFER_SIZE, (uint8_t*)input_buffer);

			if (bytes_read <= 0)
				{
					yield();
					return;
				}
			else
				{
					last_command_time = current_command_time;
					input_buffer[bytes_read] = '\0';
					handleCommand(input_buffer);
				}
		}
	yield();
}
