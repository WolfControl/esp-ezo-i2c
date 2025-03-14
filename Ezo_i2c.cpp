
#include "Ezo_i2c.h"
#include <stdlib.h>

static const char *TAG = "EZO_I2C";

Ezo_board::Ezo_board(uint8_t address){
	this->i2c_address = address;
}

Ezo_board::Ezo_board(uint8_t address, const char* name){
	this->i2c_address = address;
	this->name = name;
}

Ezo_board::Ezo_board(uint8_t address, i2c_master_bus_handle_t bus_handle) : Ezo_board(address) {
  this->bus_handle = bus_handle;
}

Ezo_board::Ezo_board(uint8_t address, const char* name, i2c_master_bus_handle_t bus_handle) : Ezo_board(address, name) {
  this->bus_handle = bus_handle;
}

esp_err_t Ezo_board::init() {
  if (this->bus_handle == nullptr) {
      ESP_LOGE(TAG, "I2C bus handle is null, can't initialize device");
      return ESP_ERR_INVALID_ARG;
  }
  
  if (this->device_initialized) {
      ESP_LOGI(TAG, "Device already initialized");
      return ESP_OK;
  }
  
  // Configure the device
  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = this->i2c_address,
      .scl_speed_hz = 100000, // 100KHz is standard for most EZO sensors
  };
  
  // Add the device to the bus
  esp_err_t ret = i2c_master_bus_add_device(this->bus_handle, &dev_cfg, &this->dev_handle);
  if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to add device to I2C bus: %s", esp_err_to_name(ret));
      return ret;
  }
  
  this->device_initialized = true;
  return ESP_OK;
}

const char* Ezo_board::get_name(){
	return this->name;
}

void Ezo_board::set_name(const char* name){
	this->name = name;
}

uint8_t Ezo_board::get_address(){
    return i2c_address;
}

void Ezo_board::set_address(uint8_t address){
    this->i2c_address = address;
}


void Ezo_board::set_bus_handle(i2c_master_bus_handle_t new_handle) {
    this->bus_handle = new_handle;
}

void Ezo_board::send_cmd(const char* command) {
  if (!this->device_initialized) {
      ESP_LOGE(TAG, "Device not initialized, can't send command");
      return;
  }
  
  size_t cmd_len = strlen(command);
  esp_err_t ret;
  
  // Use i2c_master_transmit for IDFv5
  ret = i2c_master_transmit(this->dev_handle, (const uint8_t*)command, cmd_len, -1);
  
  if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Error sending command: %s", esp_err_to_name(ret));
  }
  
  this->issued_read = false;
}

void Ezo_board::send_read_cmd(){
	send_cmd("r");
	this->issued_read = true;
}

void Ezo_board::send_cmd_with_num(const char* cmd, float num, uint8_t decimal_amount) {
  char buffer[32];
  char format[10];
  
  // Create format string based on decimal_amount (e.g., "%.3f")
  snprintf(format, sizeof(format), "%%.%df", decimal_amount);
  
  // Format the command with the number
  snprintf(buffer, sizeof(buffer), "%s", cmd);
  
  // Append the formatted number
  char num_str[16];
  snprintf(num_str, sizeof(num_str), format, num);
  strncat(buffer, num_str, sizeof(buffer) - strlen(buffer) - 1);
  
  send_cmd(buffer);
}

void Ezo_board::send_read_with_temp_comp(float temperature){
	send_cmd_with_num("rt,", temperature, 3);
	this->issued_read = true;
}


enum Ezo_board::errors Ezo_board::receive_read_cmd(){
	
	char _sensordata[this->bufferlen];
	this->error = receive_cmd(_sensordata, bufferlen);
	
	if(this->error == SUCCESS){
		if(this->issued_read == false){
			this->error = NOT_READ_CMD;
		}
		else{
			this->reading = atof(_sensordata);
		}
	}
	return this->error;
}

bool Ezo_board::is_read_poll(){
	return this->issued_read;
}

float Ezo_board:: get_last_received_reading(){  
	return this->reading;
}

enum Ezo_board::errors Ezo_board::get_error(){
	return this->error;
}

enum Ezo_board::errors Ezo_board::receive_cmd(char* sensordata_buffer, uint8_t buffer_len) {
  if (!this->device_initialized) {
      ESP_LOGE(TAG, "Device not initialized, can't receive command");
      this->error = FAIL;
      return this->error;
  }
  
  // Buffer to hold the raw response (status code + data)
  uint8_t read_buffer[buffer_len + 1]; // +1 for status code
  memset(read_buffer, 0, buffer_len + 1);
  
  // Clear the output buffer
  memset(sensordata_buffer, 0, buffer_len);
  
  // Read data from the device
  esp_err_t ret = i2c_master_receive(this->dev_handle, read_buffer, buffer_len, -1);
  
  if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Error receiving data: %s", esp_err_to_name(ret));
      this->error = FAIL;
      return this->error;
  }
  
  // The first byte is the status code
  uint8_t code = read_buffer[0];
  
  // Copy the data (excluding status code) to the output buffer
  uint8_t sensor_bytes_received = 0;
  for (int i = 1; i < buffer_len + 1 && read_buffer[i] != 0; i++) {
      sensordata_buffer[sensor_bytes_received++] = read_buffer[i];
  }
  
  // Parse status code
  switch (code) {
      case 1:
          this->error = SUCCESS;
          break;
      case 2:
          this->error = FAIL;
          break;
      case 254:
          this->error = NOT_READY;
          break;
      case 255:
          this->error = NO_DATA;
          break;
      default:
          ESP_LOGW(TAG, "Unknown status code: %d", code);
          this->error = FAIL;
          break;
  }
  
  return this->error;
}