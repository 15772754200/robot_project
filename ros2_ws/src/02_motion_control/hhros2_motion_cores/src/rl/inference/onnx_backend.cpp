#include "hhros2_motion_cores/rl/inference/onnx_backend.hpp"

#include <algorithm>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <string>

namespace hhros2_motion_cores::rl_inference {

OnnxBackend::OnnxBackend(int device_id)
: device_id_(device_id),
    env_(ORT_LOGGING_LEVEL_WARNING, "hhros2_rl_onnx")
{
    session_options_.SetIntraOpNumThreads(1);
    session_options_.SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_ALL);
    
#ifdef USE_ONNXRUNTIME_CUDA
    OrtCUDAProviderOptions cuda_options{};
    cuda_options.device_id = device_id_;
    session_options_.AppendExecutionProvider_CUDA(cuda_options);
#endif
}

bool OnnxBackend::LoadModel(const std::string &path)
{
    try {
        session_ = std::make_unique<Ort::Session>(  // 加载模型
            env_, path.c_str(), session_options_);
        
        Ort::AllocatorWithDefaultOptions allocator; // 创建默认内存分配器

        input_names_str_.clear();
        output_names_str_.clear();
        input_names_.clear();
        output_names_.clear();

        const size_t num_inputs  = session_ -> GetInputCount();     // 查看模型输入数量
        const size_t num_outputs = session_ -> GetOutputCount();    // 查看模型输出数量

        for (size_t i = 0; i < num_inputs; ++i)
        {
            auto name = session_ -> GetInputNameAllocated(i, allocator);    // 读取第i个输入的名字放到name里
            input_names_str_.emplace_back(name.get());  // 将name放到input_names_str_里

            auto type_info = session_ -> GetInputTypeInfo(i);   // 读取输入的类型信息
            auto tensor_info = type_info.GetTensorTypeAndShapeInfo(); // 读取tensor的shape信息
            input_shape_ = tensor_info.GetShape();  // 将shape信息放到input_shape_里
        }

        for (size_t i = 0; i < num_outputs; ++i)
        {
            auto name = session_ -> GetOutputNameAllocated(i, allocator);    // 读取第i个输出的名字放到name里
            output_names_str_.emplace_back(name.get());  // 将name放到output_names_str_里

            auto type_info = session_ -> GetOutputTypeInfo(i);
            auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
            output_shape_ = tensor_info.GetShape();
        }

        for (const auto& str : input_names_str_)
        {
            input_names_.push_back(str.c_str());    // 转换成c风格的字符串指针，然后放到input_names_的后面
        }
        for (const auto& str : output_names_str_)
        {
            output_names_.push_back(str.c_str());   // 转换成c风格的字符串指针，然后放到output_names_的后面
        }
        return true;
    }
    catch (const std::exception &e) {
        return false;
    }
}

std::vector<float> OnnxBackend::Infer(const std::vector<float>& observation)
{
    if (!session_)  {   // 检查模型有没有加载
        throw std::runtime_error("ONNX model is not loaded. ");
    }
    if (input_names_.empty() || output_names_.empty()) {
        throw std::runtime_error("ONNX model input/output names are empty.");
    }

    std::vector<int64_t> input_shape = input_shape_;
    if (input_shape.empty()) {
        throw std::runtime_error("ONNX model input shape is empty.");
    }

    std::size_t dynamic_count = 0;
    std::size_t last_dynamic_index = 0;
    int64_t known_size = 1;
    for (std::size_t i = 0; i < input_shape.size(); ++i) {
        if (input_shape[i] < 0) {
            ++dynamic_count;
            last_dynamic_index = i;
        } else {
            known_size *= input_shape[i];
        }
    }

    if (dynamic_count == 1) {
        if (known_size <= 0 ||
            observation.size() % static_cast<std::size_t>(known_size) != 0) {
            throw std::runtime_error(
                "Observation size does not fit ONNX dynamic input shape.");
        }
        input_shape[last_dynamic_index] =
            static_cast<int64_t>(
                observation.size() / static_cast<std::size_t>(known_size));
    } else if (dynamic_count > 1) {
        for (auto& dim : input_shape) {
            if (dim < 0) {
                dim = 1;
            }
        }
    }

    const int64_t input_size = std::accumulate(
        input_shape.begin(), input_shape.end(),
        int64_t{1}, std::multiplies<int64_t>()
    );

    if (static_cast<size_t>(input_size) != observation.size())
    {
        throw std::runtime_error(
            "Observation size " + std::to_string(observation.size()) +
            " does not match ONNX input size " + std::to_string(input_size) + ".");
    }

    auto memory_info = Ort::MemoryInfo::CreateCpu(  // 告诉ONNX Runtime: 输入数据现在CPU内存里，即使后面用GPU推理，也可以用CPU tensor。ONNX Runtime会自动拷贝到GPU内
        OrtArenaAllocator, OrtMemTypeDefault);
    
    // 把observation包装成ONNX Runtime的输入tensor
    auto input_tensor = Ort::Value::CreateTensor<float>(
        memory_info,
        const_cast<float*>(observation.data()),
        observation.size(),
        input_shape.data(),
        input_shape.size());
    
    // 执行推理
    auto outputs = session_->Run(
        Ort::RunOptions{nullptr},
        input_names_.data(),
        &input_tensor,
        1,
        output_names_.data(),
        output_names_.size());

    // 检查输出是否为空或不是tensor
    if (outputs.empty() || !outputs.front().IsTensor()) {
        throw std::runtime_error("ONNX model did not return a tensor output.");
    }

    // 拿到输出数据的指针
    auto output_info = outputs.front().GetTensorTypeAndShapeInfo();
    const auto output_size = output_info.GetElementCount(); // 计算输出的维度
    const float* output_data = outputs.front().GetTensorData<float>();    //

    return std::vector<float>(output_data, output_data + output_size);  // 复制这个区间里的所有元素
}

}   // namespace hhros2_motion_cores::rl_inference
