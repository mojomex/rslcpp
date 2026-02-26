// Copyright 2026 Simon Sagmeister
#pragma once

#include <memory>

#include <rclcpp/time.hpp>

namespace rslcpp
{
/// Interface for nodes that want to inject work when the executor is drained.
class ExecutorIdleStepper
{
public:
  using SharedPtr = std::shared_ptr<ExecutorIdleStepper>;

  virtual ~ExecutorIdleStepper() = default;

  /// Called after the executor finished processing all currently-ready callbacks.
  /// Returns true if new work was injected and the job loop should spin again immediately.
  virtual bool step_on_executor_idle(rclcpp::Time & sim_time) = 0;
};
}  // namespace rslcpp
