一、代码架构
ros2_ws/src/
├── 02_motion_control/                # 02运动控制层
│   ├── hhros2_motion_cores           # RL policy core / WBC core / fsm


二、代码分析
1：motion_core_base.cpp/hpp:
该类是一个抽象基类，创建了一个虚函数virtual bool compute(),由其派生类去实现细节，他是所有运动核心的骨架，实现了通用且可复制的输入订阅、仲裁模式监控、定时触发和指令发布机制，而将核心控制算法留给派生类（RlStandCore、RlWalkCore、RlRunCore、wbcCore等）去实现。
2：rl_policy_core.cpp/hpp：
RlPolicyCore是一个完整的强化学习策略部署引擎，继承自MotionCoreBase，专门负责将离线训练好的策略网络（onnx模型）加载到真实的机器人上，并以固定频率运行推理，生成关节控制指令
2.1：核心功能模块
（1）初始化与参数配置：
1.读取策略路径，观测配置路径，关节限位文件，增益参数，默认姿态等
2.加载观测配置，加载关节限位，初始化策略所需的关节位置/速度向量
3.订阅反馈话题/joint_states，绑定on_joint_feedback回调
4.加载ONNX策略模型（load_poliy）,创建RlRuntime实例
5.状态估计可选支持Mujoco仿真数据源
（2）关节反馈处理（on_joint_feeedback）
1.仅在核心激活时处理（is_active()）
2.从信息中提取位置和速度，存入platform_pos_和platform_vel_
3.根据观测配置中的state_joint_mapping将数据映射到策略需要的joint_pos_/joint_vel_
4.当所有策略关节数据都有效时，标记have_joint_feedback_ = true 并记录当前仲裁模式版本
（3）核心推理循环（compute）
1.有基类定时器调用，执行一下步骤：
1.1.检查策略是否加载且就绪，是否收到完整关节反馈，当前仲裁模式是否匹配
1.2.组装四元数，角速度，遥控指令，关节状态，时间
支持两种数据来源：状态估计器（state_estimator）或Mujoco仿真
1.3.检查四元数是否含NaN/Inf，范数是否在合理范围
1.4.模式切换检测：若仲裁模式版本变化，重置runtime_(清空观测历史)
1.5.准备阶段：在切换模式的初期，根据配置文件中的过渡时间prepare_duration_sec_，进行位置和增益（pk,kd等参数）的线性插值，输出平滑过渡指令
1.6.策略推理：通过runtime->Step()执行推理，得到RuntimeOutput,通过fill_joint_motor将结果填充到JointMotor消息
1.7.异常处理：若推理失败，输出阻尼指令
三、整体流程总结：
[定时器触发] 
    → 基类 on_timer 
    → 检查 active_mode_ == served_mode_ 
    → 获取最新 cmd_vel 和 state 
    → 调用派生类的 compute 
        → RlPolicyCore::compute 
            → 安全前置检查 
            → 构建 RuntimeInput 
            → 验证基座姿态 
            → 模式切换重置 
            → 准备阶段插值（若需要）
            → runtime_->Step 推理 
            → 填充 JointMotor 
    → 基类发布 reference 话题
四、Rl策略通过导出接口，统一在control_layer.launch.py中进行创建和传递相应模型的插件
RCLCPP_COMPONENTS_REGISTER_NODE(hhros2_motion_cores::RlStandCore)
RCLCPP_COMPONENTS_REGISTER_NODE(hhros2_motion_cores::RlWalkCore)
RCLCPP_COMPONENTS_REGISTER_NODE(hhros2_motion_cores::RlRunCore)