#include "internal/ti5_motor_command_adapter.hpp"

#include "internal/motor_shared_state.hpp"

#include <cstdlib>
#include <iostream>

Motor_master motor_msg;      // 只在该测试单元中起作用
MotorControlMode ti5_motor_control_mode =
    MotorControlMode::HybridForcePositionMode;

namespace
{

#define EXPECT_TRUE(condition)                                                \
    do                                                                        \
    {                                                                         \
        if (!(condition))                                                     \
        {                                                                     \
            std::cerr << __FILE__ << ":" << __LINE__                         \
                      << ": expected " #condition "\n";                      \
            std::exit(1);                                                     \
        }                                                                     \
    } while (false)

void test_hybrid_command_writes_target_velocity()
{
    float target_kp = 0.0F;
    float target_kd = 0.0F;
    float target_pos = 0.0F;
    float target_vel = 0.0F;
    float target_tor = 0.0F;
    float actual_pos = 1.25F;
    float actual_vel = -2.5F;
    float actual_cur = 0.75F;

    MotorAxisData axis{};
    axis.master_id = 1;
    axis.axis_id = 0;
    axis.kp = &target_kp;
    axis.kd = &target_kd;
    axis.target_pos = &target_pos;
    axis.target_vel = &target_vel;
    axis.target_tor = &target_tor;
    axis.actual_pos = &actual_pos;
    axis.actual_vel = &actual_vel;
    axis.actual_cur = &actual_cur;

    auto &command = motor_msg.master_id[1].motor_id[0];
    command.kp_cmd = 10000;
    command.kd_cmd = 20000;
    command.position_cmd = 30000;
    command.velocity_cmd = 40000;
    command.Tor_cmd = 50000;

    Ti5MotorCommandAdapter adapter;
    adapter.axis = &axis;
    adapter.execute = true;
    adapter.on_cycle();

    EXPECT_TRUE(target_kp == 1.0F);
    EXPECT_TRUE(target_kd == 2.0F);
    EXPECT_TRUE(target_pos == 3.0F);
    EXPECT_TRUE(target_vel == 4.0F);
    EXPECT_TRUE(target_tor == 5.0F);
    EXPECT_TRUE(actual_vel == -2.5F);
    EXPECT_TRUE(command.position_actual == 12500);
    EXPECT_TRUE(command.velocity_actual == -25000);
    EXPECT_TRUE(command.current_actual == 7500);
}

} // namespace

int main()
{
    test_hybrid_command_writes_target_velocity();
    return 0;
}
