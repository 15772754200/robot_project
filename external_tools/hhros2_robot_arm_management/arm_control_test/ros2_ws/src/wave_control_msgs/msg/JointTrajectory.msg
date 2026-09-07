# 关节轨迹消息
string[] joint_names           # 关节名称列表
float32[] positions           # 扁平化的位置数据 [joint0_t0, joint1_t0, ..., jointN_t0, joint0_t1, ...]
int32 num_joints              # 关节数量
int32 num_points              # 轨迹点数量
float32[] timestamps          # 时间戳列表
string frame_id               # 坐标系
string description            # 轨迹描述