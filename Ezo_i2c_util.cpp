#include "Ezo_i2c_util.h"


static const char *TAG = "EZO_UTIL";

// prints the boards name and I2C address
void print_device_info(Ezo_board &Device) {
  ESP_LOGI(TAG, "%s at address 0x%02X", Device.get_name(), Device.get_address());
}

// used for printing either a success_string message if a command was successful or the error type if it wasnt
void print_success_or_error(Ezo_board &Device, const char* success_string) {
  switch (Device.get_error()) {             //switch case based on what the response code is.
    case Ezo_board::SUCCESS:
      ESP_LOGI(TAG, "%s", success_string);   //the command was successful, print the success string
      break;

    case Ezo_board::FAIL:
      ESP_LOGW(TAG, "Failed");        //means the command has failed.
      break;

    case Ezo_board::NOT_READY:
      ESP_LOGW(TAG, "Pending");       //the command has not yet been finished calculating.
      break;

    case Ezo_board::NO_DATA:
      ESP_LOGW(TAG, "No Data");       //the sensor has no data to send.
      break;
    case Ezo_board::NOT_READ_CMD:
      ESP_LOGW(TAG, "Not Read Cmd");  //the sensor has not received a read command before user requested reading.
      break;
  }
}

void receive_and_print_response(Ezo_board &Device) {
  char receive_buffer[32];                  //buffer used to hold each boards response
  Device.receive_cmd(receive_buffer, 32);   //put the response into the buffer

  print_success_or_error(Device, " - ");          //print if our response is an error or not
  
  if (Device.get_error() == Ezo_board::SUCCESS) {
    ESP_LOGI(TAG, "%s (0x%02X): %s", Device.get_name(), Device.get_address(), receive_buffer);
  } else {
    ESP_LOGW(TAG, "%s (0x%02X): Error code %d", Device.get_name(), Device.get_address(), Device.get_error());
  }
}

void receive_and_print_reading(Ezo_board &Device) {              // function to decode the reading after the read command was issued
  Device.receive_read_cmd();              //get the response data and put it into the [Device].reading variable if successful
  
  if (Device.get_error() == Ezo_board::SUCCESS) {
    ESP_LOGI(TAG, "%s: %.2f", Device.get_name(), Device.get_last_received_reading());
  } else {
    const char* error_type;
    switch (Device.get_error()) {
      case Ezo_board::FAIL:
        error_type = "Failed";
        break;
      case Ezo_board::NOT_READY:
        error_type = "Pending";
        break;
      case Ezo_board::NO_DATA:
        error_type = "No Data";
        break;
      case Ezo_board::NOT_READ_CMD:
        error_type = "Not Read Cmd";
        break;
      default:
        error_type = "Unknown Error";
        break;
    }
    ESP_LOGW(TAG, "%s: %s", Device.get_name(), error_type);
  }
}