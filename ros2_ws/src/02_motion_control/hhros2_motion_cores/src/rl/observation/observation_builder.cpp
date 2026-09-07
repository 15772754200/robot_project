#include "hhros2_motion_cores/rl/observation/observation_builder.hpp"

#include <cstddef>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include "hhros2_motion_cores/rl/observation/observation_utils.hpp"

namespace hhros2_motion_cores::rl_observation {
namespace {

constexpr std::size_t kBaseVectorSize = 3;
constexpr std::size_t kPhaseSize = 2;
constexpr std::size_t kPeriodSize = 1;

void AppendJointPosition(
    std::vector<float>& obs,
    const std::vector<float>& joint_pos,
    const ObservationConfig& config)
{
    if (config.default_joint_pos.empty()) { // 如果没有设置默认关节角，就直接把关节角乘scale放进observation
        AppendScaled(obs, joint_pos, config.joint_pos_scale);
        return;
    }

    AppendJointPosRelative( // 如果设置了关节默认角，学的是偏离默认站姿的量
        obs, joint_pos, config.default_joint_pos, config.joint_pos_scale);
}

void AppendElementwiseScaled(
    std::vector<float>& obs,
    const std::vector<float>& values,
    const std::vector<float>& scales,
    float fallback_scale)
{
    if (scales.empty()) {   // 如果没有单独scale，就所有元素同乘一个scale
        AppendScaled(obs, values, fallback_scale);
        return;
    }
    if (values.size() != scales.size()) {   // 如果scale个数和关节角个数不匹配，就报错
        throw std::runtime_error("Observation value and scale size mismatch.");
    }
    for (std::size_t i = 0; i < values.size(); ++i) {
        obs.push_back(values[i] * scales[i]);   // 就每个元素乘自己的 scale
    }
}

void ClipObservation(std::vector<float>& obs, float limit)
{
    for (auto& value : obs) {   // 对观测量做统一限制吗
        if (!std::isfinite(value)) {
            value = 0.0f;
        }
        if (limit > 0.0f) {
            value = std::clamp(value, -limit, limit);
        }
    }
}

std::vector<std::size_t> ObservationTermWidths(
    const ObservationConfig& config)
{
    std::vector<std::size_t> widths;
    widths.reserve(config.observations.size());
    for (const auto& name : config.observations) {
        if (name == "base_ang_vel" || name == "projected_gravity") {
            widths.push_back(kBaseVectorSize);
        } else if (name == "command") {
            widths.push_back(config.use_command ? kBaseVectorSize : 0U);
        } else if (name == "joint_pos" || name == "joint_pos_raw" ||
                   name == "joint_vel") {
            widths.push_back(config.num_joints);
        } else if (name == "last_action") {
            widths.push_back(config.use_last_action ? config.num_joints : 0U);
        } else if (name == "phase") {
            widths.push_back(kPhaseSize);
        } else if (name == "period") {
            widths.push_back(kPeriodSize);
        } else {
            throw std::runtime_error("Unknown observation item: " + name);
        }
    }
    return widths;
}

}  // namespace

ObservationBuilder::ObservationBuilder(const ObservationConfig& config)
: config_(config)
{
}

std::vector<float> ObservationBuilder::BuildSingle(
    const ObservationInput& input) const
{
    const std::size_t n = config_.num_joints;   // 读取关节数量

    std::vector<float> obs;
    obs.reserve(3 + 3 + 3 + n + n + n + 2 + 1); // 提前申请内存

    for (const auto& name : config_.observations) { // 根据写的不同来变化，根据配置文件顺序决定
        if (name == "base_ang_vel") {
            CheckSize(input.base_ang_vel, kBaseVectorSize, "base_ang_vel");
            AppendScaled(obs, input.base_ang_vel, config_.angl_vel_scale);
        } else if (name == "projected_gravity") {
            CheckSize(input.projected_gravity, kBaseVectorSize, "projected_gravity");
            AppendScaled(
                obs, input.projected_gravity, config_.projected_gravity_scale);
        } else if (name == "command") {
            if (config_.use_command) {
                CheckSize(input.command, kBaseVectorSize, "command");
                AppendElementwiseScaled(
                    obs, input.command, config_.command_scales,
                    config_.command_scale);
            }
        } else if (name == "joint_pos") {
            CheckSize(input.joint_pos, n, "joint_pos");
            AppendJointPosition(obs, input.joint_pos, config_);
        } else if (name == "joint_pos_raw") {
            CheckSize(input.joint_pos, n, "joint_pos");
            AppendScaled(obs, input.joint_pos, config_.joint_pos_scale);
        } else if (name == "joint_vel") {
            CheckSize(input.joint_vel, n, "joint_vel");
            AppendScaled(obs, input.joint_vel, config_.joint_vel_scale);
        } else if (name == "last_action") {
            if (config_.use_last_action) {
                CheckSize(input.last_action, n, "last_action");
                AppendScaled(obs, input.last_action, config_.last_action_scale);
            }
        } else if (name == "phase") {
            CheckSize(input.phase, kPhaseSize, "phase");
            AppendScaled(obs, input.phase, config_.phase_scale);
        } else if (name == "period") {
            CheckSize(input.period, kPeriodSize, "period");
            AppendScaled(obs, input.period, config_.period_scale);
        } else {
            throw std::runtime_error("Unknown observation item: " + name);
        }
    }

    ClipObservation(obs, config_.clip_obs); // clip observation
    if (config_.num_observations != 0 &&
        obs.size() != config_.num_observations) {
        throw std::runtime_error(
            "Observation size mismatch. Expected " +
            std::to_string(config_.num_observations) + ", got " +
            std::to_string(obs.size()) + ".");
    }
    return obs;
}

std::vector<float> ObservationBuilder::Build(const ObservationInput& input)
{
    const auto frame = BuildSingle(input);
    const std::size_t stack = config_.observation_stack;

    if (stack <= 1) {   // 如果不需要stack就直接返回frame
        return frame;
    }

    if (config_.observation_stack_mode == 0) {
        // The AMP policies were exported from an environment that stacks each
        // observation term independently: [ang_vel history][gravity history]
        // ... rather than [whole frame history]. The shapes are identical, so
        // preserving this positional contract is essential.
        const auto widths = ObservationTermWidths(config_);
        if (observation_term_history_.empty()) {
            observation_term_history_.resize(widths.size());
        } else if (observation_term_history_.size() != widths.size()) {
            throw std::runtime_error("Observation term history size mismatch.");
        }

        std::size_t offset = 0;
        for (std::size_t i = 0; i < widths.size(); ++i) {
            const auto width = widths[i];
            if (offset + width > frame.size()) {
                throw std::runtime_error("Observation term exceeds frame size.");
            }
            const std::vector<float> term(
                frame.begin() + static_cast<std::ptrdiff_t>(offset),
                frame.begin() + static_cast<std::ptrdiff_t>(offset + width));
            auto& history = observation_term_history_[i];
            if (history.empty()) {
                history.assign(stack, term);
            } else {
                history.push_back(term);
                while (history.size() > stack) history.pop_front();
            }
            offset += width;
        }
        if (offset != frame.size()) {
            throw std::runtime_error("Observation terms do not cover frame.");
        }

        std::vector<float> stacked;
        stacked.reserve(frame.size() * stack);
        for (const auto& term_history : observation_term_history_) {
            for (const auto& term : term_history) {
                stacked.insert(stacked.end(), term.begin(), term.end());
            }
        }
        return stacked;
    }

    if (observation_history_.empty()) {
        observation_history_.assign(
            stack - 1, std::vector<float>(frame.size(), 0.0f));
        observation_history_.push_back(frame);
    } else {
        observation_history_.push_back(frame);
        while (observation_history_.size() > stack) {
            observation_history_.pop_front();
        }
    }

    std::vector<float> stacked;
    stacked.reserve(frame.size() * stack);
    for (const auto& item : observation_history_) {
        stacked.insert(stacked.end(), item.begin(), item.end());
    }
    return stacked;
}

void ObservationBuilder::Reset()
{
    observation_history_.clear();
    observation_term_history_.clear();
}
} 
