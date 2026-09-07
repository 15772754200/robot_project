#pragma once

#include "internal/motor_axis_data.hpp"

extern MotorControlMode ti5_motor_control_mode;
class Cia402PowerController
{
public:
  // in out
  MotorAxisData *axis;
  
  // in
  bool enable;

  // out
  bool status;
  bool valid;
  bool error;
  int errorid;

  void on_cycle();

};
