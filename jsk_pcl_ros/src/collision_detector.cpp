// -*- mode: c++ -*-
/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2015, JSK Lab
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/o2r other materials provided
 *     with the distribution.
 *   * Neither the name of the JSK Lab nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

#define BOOST_PARAMETER_MAX_ARITY 7
#include "jsk_pcl_ros/collision_detector.h"
#include <cmath>

namespace jsk_pcl_ros
{
  CollisionDetector::CollisionDetector()
  {
    ros::NodeHandle pn;
    ros::NodeHandle pnh("~");
    initSelfMask(pn, pnh);
    pnh.param<std::string>("world_frame_id", world_frame_id_, "map");

    sub_pointcloud_.subscribe(pnh, "input", 1);
    sub_jointstate_.subscribe(pnh, "input_joint", 1);
    async_ = boost::make_shared<message_filters::Synchronizer<ApproximateSyncPolicy> >(100);
    async_->connectInput(sub_pointcloud_, sub_jointstate_);
    async_->registerCallback(boost::bind(&CollisionDetector::checkCollision, this, _1, _2));

    pub_ = pnh.advertise<std_msgs::Bool>("collision_status", 1);
  }

  void CollisionDetector::initSelfMask(const ros::NodeHandle& pn, const ros::NodeHandle& pnh)
  {
    // genearte urdf model
    std::string content;
    urdf::Model urdf_model;
    if (pn.getParam("robot_description", content)) {
      if(!urdf_model.initString(content)) {
        ROS_ERROR("Unable to parse URDF description!");
      }
    }

    std::string root_link_id;
    std::string world_frame_id;
    pnh.param<std::string>("root_link_id", root_link_id, "BODY");
    pnh.param<std::string>("world_frame_id", world_frame_id, "map");

    pnh.param("min_sensor_dist", min_sensor_dist_, 0.01);
    double default_padding, default_scale;
    pnh.param("self_see_default_padding", default_padding, 0.01);
    pnh.param("self_see_default_scale", default_scale, 1.0);
    std::vector<robot_self_filter::LinkInfo> links;
    std::string link_names;

    if(!pnh.hasParam("self_see_links")) {
      ROS_WARN("No links specified for self filtering.");
    } else {
      XmlRpc::XmlRpcValue ssl_vals;;
      pnh.getParam("self_see_links", ssl_vals);
      if(ssl_vals.getType() != XmlRpc::XmlRpcValue::TypeArray) {
        ROS_WARN("Self see links need to be an array");
      } else {
        if(ssl_vals.size() == 0) {
          ROS_WARN("No values in self see links array");
        } else {
          for(int i = 0; i < ssl_vals.size(); i++) {
            robot_self_filter::LinkInfo li;
            if(ssl_vals[i].getType() != XmlRpc::XmlRpcValue::TypeStruct) {
              ROS_WARN("Self see links entry %d is not a structure.  Stopping processing of self see links",i);
              break;
            }
            if(!ssl_vals[i].hasMember("name")) {
              ROS_WARN("Self see links entry %d has no name.  Stopping processing of self see links",i);
              break;
            }
            li.name = std::string(ssl_vals[i]["name"]);
            if(!ssl_vals[i].hasMember("padding")) {
              ROS_DEBUG("Self see links entry %d has no padding.  Assuming default padding of %g",i,default_padding);
              li.padding = default_padding;
            } else {
              li.padding = ssl_vals[i]["padding"];
            }
            if(!ssl_vals[i].hasMember("scale")) {
              ROS_DEBUG("Self see links entry %d has no scale.  Assuming default scale of %g",i,default_scale);
              li.scale = default_scale;
            } else {
              li.scale = ssl_vals[i]["scale"];
            }
            links.push_back(li);
          }
        }
      }
    }
    self_mask_ = new robot_self_filter::SelfMaskUrdfRobot(tf_listener_, tf_broadcaster_, links, urdf_model, root_link_id, world_frame_id);
  }

  void CollisionDetector::checkCollision(const sensor_msgs::PointCloud2::ConstPtr& cloud_msg,
                                         const jsk_recognition_msgs::JointStateWithPose::ConstPtr& joint_msg)
  {
    ROS_DEBUG("checkCollision is called.");

    // convert point cloud
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::fromROSMsg(*cloud_msg, *cloud);

    // calculate the sensor transformation
    tf::StampedTransform sensor_to_world_tf;
    try {
      tf_listener_.lookupTransform(world_frame_id_, cloud_msg->header.frame_id, cloud_msg->header.stamp, sensor_to_world_tf);
    } catch(tf::TransformException& ex){
      ROS_ERROR_STREAM( "Transform error of sensor data: " << ex.what() << ", quitting callback");
      return;
    }

    // transform point cloud
    Eigen::Matrix4f sensor_to_world;
    pcl_ros::transformAsMatrix(sensor_to_world_tf, sensor_to_world);
    pcl::transformPointCloud(*cloud, *cloud, sensor_to_world);

    self_mask_->assumeFrameFromJointAngle(joint_msg->joint, joint_msg->pose);

    // check containment for all point cloud
    bool contain_flag = false;
    pcl::PointXYZ p;
    for(size_t i = 0; i < cloud->size(); i++) {
      p = cloud->at(i);
      if(finite(p.x) && finite(p.y) && finite(p.z) &&
         self_mask_->getMaskContainment(p.x, p.y, p.z) == robot_self_filter::INSIDE) {
        contain_flag = true;
        break;
      }
    }

    if(contain_flag) {
      ROS_INFO("collision!");
    } else {
      ROS_INFO("no collision!");
    }

    std_msgs::Bool contact_msg;
    contact_msg.data = contain_flag;
    pub_.publish(contact_msg);
  }
}

using namespace jsk_pcl_ros;
int main(int argc, char** argv)
{
  ros::init(argc, argv, "collision_detector");
  CollisionDetector collisionDetector;
  ros::spin();
}
