#ifndef HHROS2_MOTION_CORES_RL_POLICY_UTILS_HPP_
#define HHROS2_MOTION_CORES_RL_POLICY_UTILS_HPP_

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>

#include "hhros2_motion_cores/rl/inference/inference_backend.hpp"
#include "hhros2_motion_cores/rl/inference/onnx_backend.hpp"

namespace hhros2_motion_cores::rl_policy {

inline std::string LowercaseFileExtension(const std::string& path)
{
  const auto slash = path.find_last_of("/\\");
  const auto dot = path.find_last_of('.');
  if (dot == std::string::npos ||
      (slash != std::string::npos && dot < slash)) {
    return {};
  }

  std::string ext = path.substr(dot);
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return ext;
}

inline std::unique_ptr<rl_inference::InferenceBackend>
CreateInferenceBackendForPolicyPath(const std::string& path)
{
  const auto ext = LowercaseFileExtension(path);
  if (ext == ".onnx") {
    return std::make_unique<rl_inference::OnnxBackend>();
  }
  throw std::runtime_error(
    "Unsupported policy file extension '" + ext +
    "'. Runtime inference currently supports .onnx only.");
}

}  // namespace hhros2_motion_cores::rl_policy

#endif  // HHROS2_MOTION_CORES_RL_POLICY_UTILS_HPP_
