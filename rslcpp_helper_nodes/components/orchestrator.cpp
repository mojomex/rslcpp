// Copyright 2026 Simon Sagmeister
#include "rslcpp_helper_nodes/orchestrator.hpp"

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_components/register_node_macro.hpp>
#include <rslcpp_dynamic_job/backend.hpp>

using PlaybackOrchestratorGeneric = rslcpp::helper_nodes::PlaybackOrchestrator;
namespace rslcpp_helper_nodes
{
struct PlaybackOrchestrator : public PlaybackOrchestratorGeneric
{
  explicit PlaybackOrchestrator(const rclcpp::NodeOptions & options)
  : PlaybackOrchestratorGeneric(
      [](std::uint8_t exit_code) { rslcpp::dynamic_job::set_exit_code(exit_code); }, options)
  {
  }
};
}  // namespace rslcpp_helper_nodes
RCLCPP_COMPONENTS_REGISTER_NODE(rslcpp_helper_nodes::PlaybackOrchestrator)
