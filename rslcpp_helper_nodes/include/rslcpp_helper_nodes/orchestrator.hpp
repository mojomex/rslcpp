// Copyright 2026 Simon Sagmeister
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/u_int8.hpp>
#include <std_srvs/srv/trigger.hpp>

namespace rslcpp::helper_nodes
{
class PlaybackOrchestrator : public rclcpp::Node
{
public:
  explicit PlaybackOrchestrator(
    std::function<void(std::uint8_t exit_code)> abort_callback, rclcpp::NodeOptions options)
  : Node("PlaybackOrchestratorHelperNode", options), abort_simulation_callback_(abort_callback)
  {
    declare_parameter("timeout_s", 86400);

    sub_error_ = this->create_subscription<std_msgs::msg::UInt8>(
      "/rslcpp/error_code", 1,
      std::bind(&PlaybackOrchestrator::error_code_callback, this, std::placeholders::_1));

    client_start_recorder_ = this->create_client<std_srvs::srv::Trigger>("/recorder/start_recording");
    client_discovery_complete_ =
      this->create_client<std_srvs::srv::Trigger>("/recorder/is_discovery_complete");
    client_start_player_ = this->create_client<std_srvs::srv::Trigger>("/player/start_playback");
    client_stop_recorder_ = this->create_client<std_srvs::srv::Trigger>("/recorder/stop_recording");

    timer_ = rclcpp::create_timer(
      this, this->get_clock(), std::chrono::seconds(1),
      std::bind(&PlaybackOrchestrator::timer_callback, this));
  }

private:
  enum class State : std::uint8_t {
    WAIT_FOR_SERVICES = 0,
    WAIT_DISCOVERY,
    START_RECORDER,
    START_PLAYER,
    WAIT_FINISH,
    STOP_RECORDER,
    DONE
  };

  void error_code_callback(const std_msgs::msg::UInt8::SharedPtr msg)
  {
    if (state_ == State::DONE) {
      return;
    }
    if (msg->data == 0 && state_ != State::WAIT_FINISH) {
      return;
    }
    terminal_exit_code_ = msg->data;
    state_ = State::STOP_RECORDER;
    stop_recorder_requested_ = false;
  }

  void timer_callback()
  {
    if (state_ == State::DONE) {
      return;
    }

    if (initial_cycle_) {
      start_time_ = std::chrono::steady_clock::now();
      initial_cycle_ = false;
    }

    const auto timeout = this->get_parameter("timeout_s").as_int();
    if (std::chrono::steady_clock::now() - start_time_ >= std::chrono::seconds(timeout)) {
      RCLCPP_ERROR(this->get_logger(), "Playback orchestrator timed out.");
      finish_with_exit_code(1);
      return;
    }

    using Trigger = std_srvs::srv::Trigger;

    switch (state_) {
      case State::WAIT_FOR_SERVICES:
        if (!client_start_recorder_->wait_for_service(std::chrono::seconds(0)) ||
          !client_discovery_complete_->wait_for_service(std::chrono::seconds(0)) ||
          !client_start_player_->wait_for_service(std::chrono::seconds(0)) ||
          !client_stop_recorder_->wait_for_service(std::chrono::seconds(0)))
        {
          return;
        }
        state_ = State::WAIT_DISCOVERY;
        break;

      case State::WAIT_DISCOVERY:
        if (!discovery_check_requested_) {
          discovery_complete_future_ =
            client_discovery_complete_->async_send_request(std::make_shared<Trigger::Request>());
          discovery_check_requested_ = true;
          return;
        }
        if (discovery_complete_future_.valid() &&
          discovery_complete_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
          auto response = discovery_complete_future_.get();
          discovery_check_requested_ = false;
          if (response->success) {
            state_ = State::START_RECORDER;
          }
        }
        break;

      case State::START_RECORDER:
        if (!start_recorder_requested_) {
          start_recorder_future_ =
            client_start_recorder_->async_send_request(std::make_shared<Trigger::Request>());
          start_recorder_requested_ = true;
          return;
        }
        if (start_recorder_future_.valid() &&
          start_recorder_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
          auto response = start_recorder_future_.get();
          if (!response->success) {
            RCLCPP_ERROR_STREAM(
              this->get_logger(),
              "Failed to start recorder: " << response->message);
            terminal_exit_code_ = 2;
            state_ = State::STOP_RECORDER;
            stop_recorder_requested_ = false;
            return;
          }
          state_ = State::START_PLAYER;
        }
        break;

      case State::START_PLAYER:
        if (!start_player_requested_) {
          start_player_future_ =
            client_start_player_->async_send_request(std::make_shared<Trigger::Request>());
          start_player_requested_ = true;
          return;
        }
        if (start_player_future_.valid() &&
          start_player_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
          auto response = start_player_future_.get();
          if (!response->success) {
            RCLCPP_ERROR_STREAM(
              this->get_logger(),
              "Failed to start player: " << response->message);
            terminal_exit_code_ = 3;
            state_ = State::STOP_RECORDER;
            stop_recorder_requested_ = false;
            return;
          }
          state_ = State::WAIT_FINISH;
        }
        break;

      case State::WAIT_FINISH:
        break;

      case State::STOP_RECORDER:
        if (!stop_recorder_requested_) {
          stop_recorder_future_ =
            client_stop_recorder_->async_send_request(std::make_shared<Trigger::Request>());
          stop_recorder_requested_ = true;
          return;
        }
        if (stop_recorder_future_.valid() &&
          stop_recorder_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
          auto response = stop_recorder_future_.get();
          if (!response->success) {
            RCLCPP_ERROR_STREAM(
              this->get_logger(),
              "Failed to stop recorder: " << response->message);
            finish_with_exit_code(4);
            return;
          }
          finish_with_exit_code(terminal_exit_code_);
        }
        break;

      case State::DONE:
        break;
    }
  }

  void finish_with_exit_code(std::uint8_t exit_code)
  {
    if (state_ == State::DONE) {
      return;
    }
    state_ = State::DONE;
    abort_simulation_callback_(exit_code);
  }

private:
  State state_{State::WAIT_FOR_SERVICES};
  bool initial_cycle_{true};
  std::uint8_t terminal_exit_code_{0};
  std::chrono::steady_clock::time_point start_time_;
  std::function<void(std::uint8_t exit_code)> abort_simulation_callback_;

  rclcpp::Subscription<std_msgs::msg::UInt8>::SharedPtr sub_error_{};

  rclcpp::Client<std_srvs::srv::Trigger>::SharedPtr client_start_recorder_{};
  rclcpp::Client<std_srvs::srv::Trigger>::SharedPtr client_discovery_complete_{};
  rclcpp::Client<std_srvs::srv::Trigger>::SharedPtr client_start_player_{};
  rclcpp::Client<std_srvs::srv::Trigger>::SharedPtr client_stop_recorder_{};

  bool discovery_check_requested_{false};
  bool start_recorder_requested_{false};
  bool start_player_requested_{false};
  bool stop_recorder_requested_{false};
  rclcpp::Client<std_srvs::srv::Trigger>::SharedFuture discovery_complete_future_{};
  rclcpp::Client<std_srvs::srv::Trigger>::SharedFuture start_recorder_future_{};
  rclcpp::Client<std_srvs::srv::Trigger>::SharedFuture start_player_future_{};
  rclcpp::Client<std_srvs::srv::Trigger>::SharedFuture stop_recorder_future_{};

  rclcpp::TimerBase::SharedPtr timer_{};
};
}  // namespace rslcpp::helper_nodes
