//#include <DigiKeyboard.h>
#include <TinyWireS.h>
#define I2C_SLAVE_ADDRESS 0x4
#define BATTERY_READ_INTERVAL 1000
#define BATTERY_READ_DELAY 0
#define BATTERY_NUMREADINGS 5

typedef void(*TaskFunction)(); // Function pointer

int ADCPin = A1;

uint8_t vIndex = 1;
uint16_t voltages[BATTERY_NUMREADINGS] = { 0 };

// ----- STATE -----
typedef enum state {BOOTUP, RUNNING, SHUTDOWN} State;

typedef struct {
  State current_state;
  uint16_t battery_voltage;
} SystemState;

SystemState system_state;

// ----- TASKS -----
typedef struct {
  TaskFunction func;
  uint16_t interval_millis;
  uint64_t previous_millis;
} Task;

Task battery_task;

int createTask(TaskFunction function, int interval, int delay) {
  battery_task.func = function;
  battery_task.interval_millis = interval;
  battery_task.previous_millis = millis() - interval + delay;
  return 1; // Success
}

void executeTask() {
  if (battery_task.previous_millis + battery_task.interval_millis <= millis()) {
    battery_task.previous_millis = millis();
    battery_task.func();
  }
}

// ----- FUNCTIONS -----

/* Reads the pin voltage and stores
 * the average in system_state.battery_voltage
 */
void readBatteryVoltage() {
  // Read the voltage
  voltages[vIndex] = analogRead(ADCPin);
  float v = voltages[vIndex] * (5.00 / 1023.00);
  char buffer[16] = "Battery: ";
  char str[8];
  dtostrf(v, 4, 2, str);
  strcat(buffer, str);
//  DigiKeyboard.println(buffer);

  vIndex++;
  if (vIndex >= BATTERY_NUMREADINGS) {
    vIndex = 0;
  }
  // Total the values
  int sum = 0;
  for (int i = 1; i < BATTERY_NUMREADINGS; i++) {
    sum += voltages[i];
  }
  // Re-calculate the average
  system_state.battery_voltage = sum / BATTERY_NUMREADINGS;
}

// ----- I2C -----
/* Writes the SystemState struct to the I2C bus */
void tws_requestEvent() {
  // Copy the system_state struct into a byte array
  void* p = &system_state;
  uint8_t buffer[sizeof(SystemState)];
  memcpy(buffer, p, sizeof(SystemState));

  // Write buffer to I2C
  for (int i = 0; i < sizeof(buffer); i++) {
    TinyWireS.send(buffer[i]);
  }
}
/* Used to take instructions from the I2C master python script
 * eg. change polling frequency of battery Reads
 * eg. enable/disable power switch
 */
void tws_receiveEvent(uint8_t howMany) {
  while(TinyWireS.available()) {
    int data = TinyWireS.receive();
  }
}

// ----- START -----
void setup() {
//  DigiKeyboard.println("Running...");
  system_state.current_state = BOOTUP;

  // Initialise the pins
  pinMode(ADCPin, INPUT);
//  DigiKeyboard.println("Pins OK");

  // Create task(s)
  int task_result = createTask(readBatteryVoltage, BATTERY_READ_INTERVAL, BATTERY_READ_DELAY);

  if (task_result == 1) {
//    DigiKeyboard.println("Task OK");
  }
  else {
//    DigiKeyboard.println("Problem creating task!");
  }

  // Setup the I2C bus
  TinyWireS.begin(I2C_SLAVE_ADDRESS);
  TinyWireS.onReceive(tws_receiveEvent);
  TinyWireS.onRequest(tws_requestEvent);
//  DigiKeyboard.println("I2C OK");

}

void loop() {
  executeTask();
  TinyWireS_stop_check();
}
