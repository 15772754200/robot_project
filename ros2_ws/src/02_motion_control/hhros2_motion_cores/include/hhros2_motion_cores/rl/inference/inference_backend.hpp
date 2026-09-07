#ifndef HHROS2_MOTION_CORES_RL_INFERENCE_BACKEND_HPP_
#define HHROS2_MOTION_CORES_RL_INFERENCE_BACKEND_HPP_

#include <vector>
#include <string>

namespace hhros2_motion_cores::rl_inference {

class InferenceBackend {
 public:
  virtual ~InferenceBackend() = default;
  virtual bool LoadModel(const std::string& path) = 0;
  virtual std::vector<float> Infer(
    const std::vector<float>& observation) = 0;
};

}  // namespace hhros2_motion_cores::rl_inference

#endif  // HHROS2_MOTION_CORES_RL_INFERENCE_BACKEND_HPP_