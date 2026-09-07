#ifndef HHROS2_MOTION_CORES_RL_ONNX_BACKEND_HPP_
#define HHROS2_MOTION_CORES_RL_ONNX_BACKEND_HPP_

#include "inference_backend.hpp"
#include <cstdint>
#include <memory>
#include <vector>
#include <onnxruntime_cxx_api.h>

namespace hhros2_motion_cores::rl_inference {

class OnnxBackend : public InferenceBackend {
 public:
  explicit OnnxBackend(int device_id = 0);
  ~OnnxBackend() override = default;

  bool LoadModel(const std::string& path) override;
  std::vector<float> Infer(const std::vector<float>& observation) override;

 private:
  int device_id_{0};

  Ort::Env env_;
  Ort::SessionOptions session_options_;
  std::unique_ptr<Ort::Session> session_;
  
  std::vector<std::string> input_names_str_;
  std::vector<std::string> output_names_str_;
  std::vector<const char*> input_names_;
  std::vector<const char*> output_names_;

  std::vector<int64_t> input_shape_;
  std::vector<int64_t> output_shape_;
};

}  // namespace hhros2_motion_cores::rl_inference

#endif  // HHROS2_MOTION_CORES_RL_ONNX_BACKEND_HPP_