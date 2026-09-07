#include "hhros2_log/log.h"
#include <optional>
#include "internal/cia402_power_controller.hpp"
#include "internal/bit_pattern.hpp"

MotorControlMode ti5_motor_control_mode = MotorControlMode::HybridForcePositionMode; // 默认力位混控模式，如果需要调整电机控制模式，修改改变量即可

enum class cia402_state
{
  none = -1,
  not_ready_to_switch_on = 0,
  switch_on_disabled,
  ready_to_switch_on,
  switched_on,
  operation_enabled,
  quick_stop_active,
  fault_reaction_active,
  fault,
};

const char *to_string(cia402_state state)
{
  switch (state)
  {
  case cia402_state::none:
    return "none";
  case cia402_state::not_ready_to_switch_on:
    return "not ready to switch on";
  case cia402_state::switch_on_disabled:
    return "switch on disabled";
  case cia402_state::ready_to_switch_on:
    return "ready to switch on";
  case cia402_state::switched_on:
    return "switched on";
  case cia402_state::operation_enabled:
    return "operation enabled";
  case cia402_state::quick_stop_active:
    return "quick stop active";
  case cia402_state::fault_reaction_active:
    return "fault reaction active";
  case cia402_state::fault:
    return "fault";
  default:
    return "invalid";
  }
}

using u16_bit_pattern = basic_bit_pattern<std::uint16_t>;

static constexpr std::array<std::pair<u16_bit_pattern, cia402_state>, 8>
    state_patterns = {{
        {{0x004f, 0x0040}, cia402_state::switch_on_disabled},
        {{0x006f, 0x0021}, cia402_state::ready_to_switch_on},
        {{0x006f, 0x0023}, cia402_state::switched_on},
        {{0x006f, 0x0027}, cia402_state::operation_enabled},
        {{0x006f, 0x0007}, cia402_state::quick_stop_active},
        {{0x004f, 0x000f}, cia402_state::fault_reaction_active},
        {{0x004f, 0x0008}, cia402_state::fault},
        {{0x0000, 0x0000}, cia402_state::not_ready_to_switch_on},
    }};

constexpr cia402_state to_cia402_state(std::uint16_t status_word)
{
  for (auto [pattern, state] : state_patterns)
    if (pattern == status_word)
      return state;
  return cia402_state::fault;
}

static constexpr std::array<std::pair<cia402_state, u16_bit_pattern>, 8>
    enable_transition_commands = {{
        {cia402_state::not_ready_to_switch_on, {0x0081 | 0x0006, 0x0006}},
        {cia402_state::switch_on_disabled, {0x0081 | 0x0006, 0x0006}},
        {cia402_state::ready_to_switch_on, {0x0088 | 0x0007, 0x0007}},
        {cia402_state::switched_on, {0x0080 | 0x000F, 0x000F}},
        {cia402_state::operation_enabled, {0, 0}},
        {cia402_state::quick_stop_active, {0, 0}},
        {cia402_state::fault_reaction_active, {0, 0}},
        {cia402_state::fault, {0x0080, 0x0080}},
    }};

static constexpr std::array<std::pair<cia402_state, u16_bit_pattern>, 7>
    disable_transition_commands = {{
        {cia402_state::not_ready_to_switch_on, {0x0081 | 0x0006, 0x0006}},
        {cia402_state::switch_on_disabled, {0x0081 | 0x0006, 0x0006}},
        {cia402_state::ready_to_switch_on, {0x0088 | 0x0007, 0x0007}},
        {cia402_state::switched_on, {0x0080 | 0x000F, 0x0007}},
        {cia402_state::operation_enabled, {0x0080 | 0x000F, 0x0007}},
        {cia402_state::fault_reaction_active, {0x0080 | 0x000F, 0x0080}},
        {cia402_state::fault, {0x0080 | 0x0008, 0x0080}},
    }};

auto get_enable_command(cia402_state state) -> std::optional<u16_bit_pattern>
{
  for (auto [s, c] : enable_transition_commands)
  {
    if (s == state)
      return c;
  }
  return std::nullopt;
}

auto get_disable_command(cia402_state state) -> std::optional<u16_bit_pattern>
{
  for (auto [s, c] : disable_transition_commands)
  {
    if (s == state)
      return c;
  }
  return std::nullopt;
}

enum mode_of_operation_type : std::int8_t
{
  op_mode_no = 0,
  op_mode_pp = 1,
  op_mode_vl = 2,
  op_mode_pv = 3,
  op_mode_hm = 6,
  op_mode_ip = 7,
  op_mode_csp = 8,
  op_mode_csv = 9,
  op_mode_cst = 10,
  op_mode_pt = -1 // 力位混控模式
};

void Cia402PowerController::on_cycle()
{
  if (axis == nullptr ||
      axis->status_word == nullptr ||
      axis->control_word == nullptr ||
      axis->mode_of_operation == nullptr)
  {
    valid = false;
    error = true;
    errorid = -2;
    return;
  }

  if (!status)
  {
    if (ti5_motor_control_mode == MotorControlMode::PositionMode)
    {
      // if (axis->target_position == nullptr ||
      //     axis->position_actual_value == nullptr)
      // {
      //   valid = false;
      //   error = true;
      //   errorid = -3;
      //   return;
      // }
      //   *axis->target_position = *axis->position_actual_value;
    }
    else if (ti5_motor_control_mode == MotorControlMode::HybridForcePositionMode)
    {
      if (axis->target_pos == nullptr || axis->actual_pos == nullptr)
      {
        valid = false;
        error = true;
        errorid = -4;
        return;
      }
        *axis->target_pos = *axis->actual_pos;
    }
    
  }

  //将状态字转换成状态
  auto state = to_cia402_state(*axis->status_word);

  std::optional<u16_bit_pattern> command;
  //针对不同的enable状态，获取对应的比特位指令
  if (enable)
  {
    command = get_enable_command(state);
  }
  else
  {
    command = get_disable_command(state);
  }

  if (!command)
  {
    error = true;
    errorid = -1;
  }
  else
  {
    //用上面获取的比特位指令设置对应的控制字
    *axis->control_word << *command;
    error = false;
    errorid = 0;
    
    //在switched_on的状态下设置运行模式
    if (state == cia402_state::switched_on)
    {
      if (axis->mode_of_operation)
      {
        if (ti5_motor_control_mode == MotorControlMode::PositionMode)
        {// 位置控制模式
            // *axis->mode_of_operation = op_mode_csp;
        }
        else if (ti5_motor_control_mode == MotorControlMode::HybridForcePositionMode)
        {// 力位混控模式
            *axis->mode_of_operation = op_mode_pt;
        }
      }
    }
  }

  valid = true;

  auto old_state = to_cia402_state(axis->old_status_word);

  if (old_state != state)
  {
    LOG_DEBUG(LogType::MOTORLOG, "Axis %d changed from '%s' to '%s', control_word=0x%04X",
          axis->axis_id,
          to_string(old_state),
          to_string(state),
          *axis->control_word);

    axis->old_status_word = *axis->status_word;
  }
  
  //只有当状态为operation_enabled的时候才将status设置为true
  status = state == cia402_state::operation_enabled;
}
 
