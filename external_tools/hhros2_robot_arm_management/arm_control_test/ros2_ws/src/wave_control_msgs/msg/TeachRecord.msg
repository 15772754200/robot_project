# 示教记录状态消息
string record_id      # 记录ID
string state          # 状态: "idle", "recording", "paused", "completed"
int32 frame_count     # 已记录帧数
float32 duration      # 记录时长(秒)
float32 frequency     # 记录频率(Hz)
string description    # 记录描述