// Copyright (c) 2023 Franka Robotics GmbH
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <Eigen/Dense>
#include <string>

#include <controller_interface/controller_interface.hpp>
#include <franka_example_controllers/robot_utils.hpp>
#include <moveit_msgs/srv/get_position_ik.hpp>
#include <rclcpp/rclcpp.hpp>
#include "franka_semantic_components/franka_cartesian_pose_interface.hpp"
#include "franka_semantic_components/franka_robot_model.hpp"
#include "std_msgs/msg/string.hpp"
#include "geometry_msgs/msg/point.hpp"
#include "rclcpp/rclcpp.hpp"
using std::placeholders::_1;


using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;


std::array<double, 7> seven_dim_pos_goal = {0.3,0,0.487,1,0,0,0};
std::mutex position_array_mutex;

class MinimalSubscriber : public rclcpp::Node
{
  public:
    MinimalSubscriber()
    : Node("minimal_subscriber")
    {
      subscription_ = this->create_subscription<geometry_msgs::msg::Pose>( 
      "/unity_to_ros", 10, std::bind(&MinimalSubscriber::topic_callback, this, _1));
      RCLCPP_INFO(this->get_logger(), "sbuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribebuscribed :");
    }

  private:
  //franka_example_controllers::JointVelocityExampleController& controller;
    void topic_callback(const geometry_msgs::msg::Pose& msg) const
    {
        RCLCPP_INFO(this->get_logger(), "Received Point: x=%.2f, y=%.2f, z=%.2f",
        msg.position.x, msg.position.y, msg.position.z);

      std::lock_guard<std::mutex> lock(position_array_mutex);
      seven_dim_pos_goal[0] = msg.position.x;
      seven_dim_pos_goal[1] = msg.position.y;
      seven_dim_pos_goal[2] = msg.position.z;

      seven_dim_pos_goal[3] = msg.orientation.x;
      seven_dim_pos_goal[4] = msg.orientation.y;
      seven_dim_pos_goal[5] = msg.orientation.z;
      seven_dim_pos_goal[6] = msg.orientation.w;

      if (seven_dim_pos_goal[2] < 0.05) {
        seven_dim_pos_goal[2] = 0.05;
      }

      //controller.topic_callback(msg.data);
    }
    rclcpp::Subscription<geometry_msgs::msg::Pose>::SharedPtr subscription_;
};

namespace franka_example_controllers {

/**
 * joint impedance example controller get desired pose and use inverse kinematics LMA
 * (Levenberg-Marquardt) from Orocos KDL. IK returns the desired joint positions from the desired
 * pose. Desired joint positions are fed to the impedance control law together with the current
 * joint velocities to calculate the desired joint torques.
 */
class JointImpedanceWithIKExampleController : public controller_interface::ControllerInterface {
 public:
  using Vector7d = Eigen::Matrix<double, 7, 1>;
  [[nodiscard]] controller_interface::InterfaceConfiguration command_interface_configuration()
      const override;
  [[nodiscard]] controller_interface::InterfaceConfiguration state_interface_configuration()
      const override;
  controller_interface::return_type update(const rclcpp::Time& time,
                                           const rclcpp::Duration& period) override;
  CallbackReturn on_init() override;
  CallbackReturn on_configure(const rclcpp_lifecycle::State& previous_state) override;
  CallbackReturn on_activate(const rclcpp_lifecycle::State& previous_state) override;
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State& previous_state) override;

 private:
  void update_joint_states();

  /**
   * @brief Calculates the new pose based on the initial pose.
   *
   * @return  Eigen::Vector3d calculated sinosuidal period for the x,z position of the pose.
   */
  Eigen::Vector3d compute_new_position();
  Eigen::Quaterniond compute_new_orientation();

  /**
   * @brief creates the ik service request for ik service from moveit. Assigns the move-group,
   * desired pose of the desired link.
   *
   * @return std::shared_ptr<moveit_msgs::srv::GetPositionIK::Request> request service message
   */
  std::shared_ptr<moveit_msgs::srv::GetPositionIK::Request> create_ik_service_request(
      const Eigen::Vector3d& new_position,
      const Eigen::Quaterniond& new_orientation,
      const std::vector<double>& joint_positions_desired,
      const std::vector<double>& joint_positions_current,
      const std::vector<double>& joint_efforts_current);

  /**
   * @brief computes the torque commands based on impedance control law with compensated coriolis
   * terms
   *
   * @return Eigen::Vector7d torque for each joint of the robot
   */
  Vector7d compute_torque_command(const Vector7d& joint_positions_desired,
                                  const Vector7d& joint_positions_current,
                                  const Vector7d& joint_velocities_current);

  /**
   * @brief assigns the Kp, Kd and arm_id parameters
   *
   * @return true when parameters are present, false when parameters are not available
   */
  bool assign_parameters();

  std::unique_ptr<franka_semantic_components::FrankaCartesianPoseInterface> franka_cartesian_pose_;

  Eigen::Quaterniond orientation_;
  Eigen::Vector3d position_;
  rclcpp::Client<moveit_msgs::srv::GetPositionIK>::SharedPtr compute_ik_client_;

  const bool k_elbow_activated_{false};
  bool initialization_flag_{true};

  std::string arm_id_;
  bool is_gripper_loaded_ = true;
  std::string robot_description_;

  double elapsed_time_{0.0};
  double initial_robot_time_{0.0};
  double robot_time_{0.0};
  std::unique_ptr<franka_semantic_components::FrankaRobotModel> franka_robot_model_;

  const std::string k_robot_state_interface_name{"robot_state"};
  const std::string k_robot_model_interface_name{"robot_model"};

  Vector7d dq_filtered_;
  Vector7d k_gains_;
  Vector7d d_gains_;
  int num_joints_{7};

  std::vector<double> joint_positions_desired_;
  std::vector<double> joint_positions_current_{0, 0, 0, 0, 0, 0, 0};
  std::vector<double> joint_velocities_current_{0, 0, 0, 0, 0, 0, 0};
  std::vector<double> joint_efforts_current_{0, 0, 0, 0, 0, 0, 0};
};
}  // namespace franka_example_controllers