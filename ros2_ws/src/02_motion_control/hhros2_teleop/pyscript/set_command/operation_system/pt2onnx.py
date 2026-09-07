#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ONNX-only deployment notice.

The deploy workspace does not load or convert non-ONNX policy formats.
"""

def main():
    message = """
================================================================
                     重要提示
================================================================

当前部署框架只支持 ONNX 模型。

部署端要求：
1. 策略文件必须是 .onnx。
2. config.yaml 中 model_name 必须指向 .onnx 文件。
3. ONNX 输入维度必须与 num_observations * observation_stack 匹配。
4. ONNX 输出维度必须与 num_of_dofs 匹配。

训练侧如需导出或转换模型，请在训练工程内完成。部署工程不再提供转换入口，
也不再携带转换依赖。

================================================================
        请将已导出的 ONNX 模型放入对应 policy 目录后再运行
================================================================
"""
    print(message)


if __name__ == "__main__":
    main()
