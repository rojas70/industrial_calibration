/*
 * Software License Agreement (Apache License)
 *
 * Copyright (c) 2014, Southwest Research Institute
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <industrial_extrinsic_cal/calibration_job_definition2.h>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>
#include <ros/package.h>
#include <actionlib/client/simple_action_client.h>
#include <industrial_extrinsic_cal/manual_triggerAction.h>
#include <industrial_extrinsic_cal/ros_scene_triggers.h>

using std::string;
using boost::shared_ptr;
using boost::make_shared;
using ceres::CostFunction;

namespace industrial_extrinsic_cal
{

  bool CalibrationJob::load()
  {
    if(CalibrationJob::loadCamera())
      {
	ROS_INFO_STREAM("Successfully read in cameras ");
      }
    else
      {
	ROS_ERROR_STREAM("Camera file parsing failed");
	return false;
      }
    if(CalibrationJob::loadTarget())
      {
	ROS_INFO_STREAM("Successfully read in targets");
      }
    else
      {
	ROS_ERROR_STREAM("Target file parsing failed");
	return false;
      }
    if(CalibrationJob::loadCalJob())
      {
	ROS_INFO_STREAM("Successfully read in CalJob");
      }
    else
      {
	ROS_ERROR_STREAM("Calibration Job file parsing failed");
	return false;
      }

    return (true);
  } // end load()

  bool CalibrationJob::loadCamera()
  {
    std::ifstream camera_input_file(camera_def_file_name_.c_str());
    if (camera_input_file.fail())
      {
	ROS_ERROR_STREAM(
			 "ERROR CalibrationJob::load(), couldn't open camera_input_file:    "
			 << camera_def_file_name_.c_str());
	return (false);
      }

    string temp_name, temp_topic, camera_optical_frame, camera_intermediate_frame;
    CameraParameters temp_parameters;
    P_BLOCK extrinsics;

    unsigned int scene_id;
    try
      {
	YAML::Parser camera_parser(camera_input_file);
	YAML::Node camera_doc;
	camera_parser.GetNextDocument(camera_doc);

	// read in all static cameras
	if (const YAML::Node *camera_parameters = camera_doc.FindValue("static_cameras"))
	  {
	    ROS_DEBUG_STREAM("Found "<<camera_parameters->size()<<" static cameras ");
	    for (unsigned int i = 0; i < camera_parameters->size(); i++)
	      {
		(*camera_parameters)[i]["camera_name"] >> temp_name;
		(*camera_parameters)[i]["image_topic"] >> temp_topic;
		(*camera_parameters)[i]["camera_optical_frame"] >> camera_optical_frame;
		(*camera_parameters)[i]["camera_intermediate_frame"] >> camera_intermediate_frame;
		(*camera_parameters)[i]["angle_axis_ax"] >> temp_parameters.angle_axis[0];
		(*camera_parameters)[i]["angle_axis_ay"] >> temp_parameters.angle_axis[1];
		(*camera_parameters)[i]["angle_axis_az"] >> temp_parameters.angle_axis[2];
		(*camera_parameters)[i]["position_x"] >> temp_parameters.position[0];
		(*camera_parameters)[i]["position_y"] >> temp_parameters.position[1];
		(*camera_parameters)[i]["position_z"] >> temp_parameters.position[2];
		(*camera_parameters)[i]["focal_length_x"] >> temp_parameters.focal_length_x;
		(*camera_parameters)[i]["focal_length_y"] >> temp_parameters.focal_length_y;
		(*camera_parameters)[i]["center_x"] >> temp_parameters.center_x;
		(*camera_parameters)[i]["center_y"] >> temp_parameters.center_y;
		(*camera_parameters)[i]["distortion_k1"] >> temp_parameters.distortion_k1;
		(*camera_parameters)[i]["distortion_k2"] >> temp_parameters.distortion_k2;
		(*camera_parameters)[i]["distortion_k3"] >> temp_parameters.distortion_k3;
		(*camera_parameters)[i]["distortion_p1"] >> temp_parameters.distortion_p1;
		(*camera_parameters)[i]["distortion_p2"] >> temp_parameters.distortion_p2;
		// create a static camera
		shared_ptr<Camera> temp_camera = make_shared<Camera>(temp_name, temp_parameters, false);
		temp_camera->camera_observer_ = make_shared<ROSCameraObserver>(temp_topic);
		ceres_blocks_.addStaticCamera(temp_camera);
		camera_optical_frames_.push_back(camera_optical_frame);
		camera_intermediate_frames_.push_back(camera_intermediate_frame);
		extrinsics = ceres_blocks_.getStaticCameraParameterBlockExtrinsics(temp_name);
		original_extrinsics_.push_back(extrinsics);
	      }
	  }

	// read in all moving cameras
	if (const YAML::Node *camera_parameters = camera_doc.FindValue("moving_cameras"))
	  {
	    ROS_DEBUG_STREAM("Found "<<camera_parameters->size() << " moving cameras ");
	    for (unsigned int i = 0; i < camera_parameters->size(); i++)
	      {
		(*camera_parameters)[i]["camera_name"] >> temp_name;
		(*camera_parameters)[i]["image_topic"] >> temp_topic;
		(*camera_parameters)[i]["camera_optical_frame"] >> camera_optical_frame;
		(*camera_parameters)[i]["camera_intermediate_frame"] >> camera_intermediate_frame;
		(*camera_parameters)[i]["angle_axis_ax"] >> temp_parameters.angle_axis[0];
		(*camera_parameters)[i]["angle_axis_ay"] >> temp_parameters.angle_axis[1];
		(*camera_parameters)[i]["angle_axis_az"] >> temp_parameters.angle_axis[2];
		(*camera_parameters)[i]["position_x"] >> temp_parameters.position[0];
		(*camera_parameters)[i]["position_y"] >> temp_parameters.position[1];
		(*camera_parameters)[i]["position_z"] >> temp_parameters.position[2];
		(*camera_parameters)[i]["focal_length_x"] >> temp_parameters.focal_length_x;
		(*camera_parameters)[i]["focal_length_y"] >> temp_parameters.focal_length_y;
		(*camera_parameters)[i]["center_x"] >> temp_parameters.center_x;
		(*camera_parameters)[i]["center_y"] >> temp_parameters.center_y;
		(*camera_parameters)[i]["distortion_k1"] >> temp_parameters.distortion_k1;
		(*camera_parameters)[i]["distortion_k2"] >> temp_parameters.distortion_k2;
		(*camera_parameters)[i]["distortion_k3"] >> temp_parameters.distortion_k3;
		(*camera_parameters)[i]["distortion_p1"] >> temp_parameters.distortion_p1;
		(*camera_parameters)[i]["distortion_p2"] >> temp_parameters.distortion_p2;
		(*camera_parameters)[i]["scene_id"] >> scene_id;
		shared_ptr<Camera> temp_camera = make_shared<Camera>(temp_name, temp_parameters, true);
		temp_camera->camera_observer_ = make_shared<ROSCameraObserver>(temp_topic);
		ceres_blocks_.addMovingCamera(temp_camera, scene_id);
		camera_optical_frames_.push_back(camera_optical_frame);
		camera_intermediate_frames_.push_back(camera_intermediate_frame);
		extrinsics = ceres_blocks_.getStaticCameraParameterBlockExtrinsics(temp_name);
		original_extrinsics_.push_back(extrinsics);
	      }
	  }
      } // end try
    catch (YAML::ParserException& e)
      {
	ROS_INFO_STREAM("load() Failed to read in moving cameras from  yaml file ");
	/*ROS_INFO_STREAM("camera name =     "<<temp_name.c_str());
	  ROS_INFO_STREAM("angle_axis_ax =  "<<temp_parameters.angle_axis[0]);
	  ROS_INFO_STREAM("angle_axis_ay = "<<temp_parameters.angle_axis[1]);
	  ROS_INFO_STREAM("angle_axis_az =  "<<temp_parameters.angle_axis[2]);
	  ROS_INFO_STREAM("position_x =  "<<temp_parameters.position[0]);
	  ROS_INFO_STREAM("position_y =  "<<temp_parameters.position[1]);
	  ROS_INFO_STREAM("position_z =  "<<temp_parameters.position[2]);
	  ROS_INFO_STREAM("focal_length_x =  "<<temp_parameters.focal_length_x);
	  ROS_INFO_STREAM("focal_length_y =  "<<temp_parameters.focal_length_y);
	  ROS_INFO_STREAM("center_x = "<<temp_parameters.center_x);
	  ROS_INFO_STREAM("center_y =  "<<temp_parameters.center_y);
	  ROS_INFO_STREAM("distortion_k1 =  "<<temp_parameters.distortion_k1);
	  ROS_INFO_STREAM("distortion_k2 =  "<<temp_parameters.distortion_k2);
	  ROS_INFO_STREAM("distortion_k3 =  "<<temp_parameters.distortion_k3);
	  ROS_INFO_STREAM("distortion_p1 =  "<<temp_parameters.distortion_p1);
	  ROS_INFO_STREAM("distortion_p2 =  "<<temp_parameters.distortion_p2);
	  ROS_INFO_STREAM("scene_id = "<<scene_id);*/
	ROS_ERROR("load() Failed to read in cameras yaml file");
	ROS_ERROR_STREAM("Failed with exception "<< e.what());
	return (false);
      }
    return true;
  }

  bool CalibrationJob::loadTarget()
  {
    std::ifstream target_input_file(target_def_file_name_.c_str());
    if (target_input_file.fail())
      {
	ROS_ERROR_STREAM(
			 "ERROR CalibrationJob::load(), couldn't open target_input_file: "
			 << target_def_file_name_.c_str());
	return (false);
      }
    Target temp_target;
    std::string temp_frame;
    try
      {
	YAML::Parser target_parser(target_input_file);
	YAML::Node target_doc;
	target_parser.GetNextDocument(target_doc);
	// read in all static targets
	if (const YAML::Node *target_parameters = target_doc.FindValue("static_targets"))
	  {
	    ROS_DEBUG_STREAM("Found "<<target_parameters->size() <<" targets ");
	    shared_ptr<Target> temp_target = make_shared<Target>();
	    temp_target->is_moving = false;
	    for (unsigned int i = 0; i < target_parameters->size(); i++)
	      {
		shared_ptr<Target> temp_target = make_shared<Target>();
		(*target_parameters)[i]["target_name"] >> temp_target->target_name;
		(*target_parameters)[i]["target_frame"] >> temp_frame;
		(*target_parameters)[i]["target_type"] >> temp_target->target_type;
		//ROS_DEBUG_STREAM("TargetFrame: "<<temp_frame);
		switch (temp_target->target_type)
		  {
		  case pattern_options::Chessboard:
		    (*target_parameters)[i]["target_rows"] >> temp_target->checker_board_parameters.pattern_rows;
		    (*target_parameters)[i]["target_cols"] >> temp_target->checker_board_parameters.pattern_cols;
		    ROS_DEBUG_STREAM("TargetRows: "<<temp_target->checker_board_parameters.pattern_rows);
		    break;
		  case pattern_options::CircleGrid:
		    (*target_parameters)[i]["target_rows"] >> temp_target->circle_grid_parameters.pattern_rows;
		    (*target_parameters)[i]["target_cols"] >> temp_target->circle_grid_parameters.pattern_cols;
		    (*target_parameters)[i]["circle_dia"]  >> temp_target->circle_grid_parameters.circle_diameter;
		    temp_target->circle_grid_parameters.is_symmetric=true;
		    ROS_DEBUG_STREAM("TargetRows: "<<temp_target->circle_grid_parameters.pattern_rows);
		    break;
		  default:
		    ROS_ERROR_STREAM("target_type does not correlate to a known pattern option (Chessboard or CircleGrid)");
		    return false;
		    break;
		  }
		(*target_parameters)[i]["angle_axis_ax"] >> temp_target->pose.ax;
		(*target_parameters)[i]["angle_axis_ay"] >> temp_target->pose.ay;
		(*target_parameters)[i]["angle_axis_az"] >> temp_target->pose.az;
		(*target_parameters)[i]["position_x"] >> temp_target->pose.x;
		(*target_parameters)[i]["position_y"] >> temp_target->pose.y;
		(*target_parameters)[i]["position_z"] >> temp_target->pose.z;
		(*target_parameters)[i]["num_points"] >> temp_target->num_points;
		const YAML::Node *points_node = (*target_parameters)[i].FindValue("points");
		ROS_DEBUG_STREAM("FoundPoints: "<<points_node->size());
		for (int j = 0; j < points_node->size(); j++)
		  {
		    const YAML::Node *pnt_node = (*points_node)[j].FindValue("pnt");
		    std::vector<float> temp_pnt;
		    (*pnt_node) >> temp_pnt;
		    Point3d temp_pnt3d;
		    //ROS_DEBUG_STREAM("pntx: "<<temp_pnt3d.x);
		    temp_pnt3d.x = temp_pnt[0];
		    temp_pnt3d.y = temp_pnt[1];
		    temp_pnt3d.z = temp_pnt[2];
		    temp_target->pts.push_back(temp_pnt3d);
		  }
		ceres_blocks_.addStaticTarget(temp_target);
		target_frames_.push_back(temp_frame);
	      }
	  }

	// read in all moving targets
	if (const YAML::Node *target_parameters = target_doc.FindValue("moving_targets"))
	  {
	    ROS_DEBUG_STREAM("Found "<<target_parameters->size() <<"  moving targets ");
	    shared_ptr<Target> temp_target = make_shared<Target>();
	    unsigned int scene_id;
	    temp_target->is_moving = true;
	    for (unsigned int i = 0; i < target_parameters->size(); i++)
	      {
		(*target_parameters)[i]["target_name"] >> temp_target->target_name;
		(*target_parameters)[i]["target_frame"] >> temp_frame;
		(*target_parameters)[i]["target_type"] >> temp_target->target_type;
		//ROS_DEBUG_STREAM("TargetFrame: "<<temp_frame);
		switch (temp_target->target_type)
		  {
		  case pattern_options::Chessboard:
		    (*target_parameters)[i]["target_rows"] >> temp_target->checker_board_parameters.pattern_rows;
		    (*target_parameters)[i]["target_cols"] >> temp_target->checker_board_parameters.pattern_cols;
		    ROS_INFO_STREAM("TargetRows: "<<temp_target->checker_board_parameters.pattern_rows);
		    break;
		  case pattern_options::CircleGrid:
		    (*target_parameters)[i]["target_rows"] >> temp_target->circle_grid_parameters.pattern_rows;
		    (*target_parameters)[i]["target_cols"] >> temp_target->circle_grid_parameters.pattern_cols;
		    (*target_parameters)[i]["circle_dia"]  >> temp_target->circle_grid_parameters.circle_diameter;
		    temp_target->circle_grid_parameters.is_symmetric=true;
		    break;
		  default:
		    ROS_ERROR_STREAM("target_type does not correlate to a known pattern option (Chessboard or CircleGrid)");
		    return false;
		    break;
		  }
		(*target_parameters)[i]["angle_axis_ax"] >> temp_target->pose.ax;
		(*target_parameters)[i]["angle_axis_ax"] >> temp_target->pose.ay;
		(*target_parameters)[i]["angle_axis_ay"] >> temp_target->pose.az;
		(*target_parameters)[i]["position_x"] >> temp_target->pose.x;
		(*target_parameters)[i]["position_y"] >> temp_target->pose.y;
		(*target_parameters)[i]["position_z"] >> temp_target->pose.z;
		(*target_parameters)[i]["scene_id"] >> scene_id;
		(*target_parameters)[i]["num_points"] >> temp_target->num_points;
		const YAML::Node *points_node = (*target_parameters)[i].FindValue("points");
		for (int j = 0; j < points_node->size(); j++)
		  {
		    const YAML::Node *pnt_node = (*points_node)[j].FindValue("pnt");
		    std::vector<float> temp_pnt;
		    (*pnt_node) >> temp_pnt;
		    Point3d temp_pnt3d;
		    temp_pnt3d.x = temp_pnt[0];
		    temp_pnt3d.y = temp_pnt[1];
		    temp_pnt3d.z = temp_pnt[2];
		    temp_target->pts.push_back(temp_pnt3d);
		  }
		ceres_blocks_.addMovingTarget(temp_target, scene_id);
		target_frames_.push_back(temp_frame);
	      }
	  }
      } // end try
    catch (YAML::ParserException& e)
      {
	ROS_ERROR("load() Failed to read in target yaml file");
	ROS_ERROR_STREAM("Failed with exception "<< e.what());
	return (false);
      }
    return true;

  }

  bool CalibrationJob::loadCalJob()
  {
    std::ifstream caljob_input_file(caljob_def_file_name_.c_str());
    if (caljob_input_file.fail())
      {
	ROS_ERROR_STREAM(
			 "ERROR CalibrationJob::load(), couldn't open caljob_input_file: "
			 << caljob_def_file_name_.c_str());
	return (false);
      }

    std::string opt_params;
    int scene_id_num;
    int trig_type;
    SceneTrigger *scene_trig;
    std::string camera_name;
    std::string target_name;
    shared_ptr<Camera> temp_cam = make_shared<Camera>();
    shared_ptr<Target> temp_targ = make_shared<Target>();
    Roi temp_roi;

    try
      {
	YAML::Parser caljob_parser(caljob_input_file);
	YAML::Node caljob_doc;
	caljob_parser.GetNextDocument(caljob_doc);

	caljob_doc["reference_frame"] >> ceres_blocks_.reference_frame_;
	caljob_doc["optimization_parameters"] >> opt_params;
	// read in all scenes
	if (const YAML::Node *caljob_scenes = caljob_doc.FindValue("scenes"))
	  {
	    ROS_DEBUG_STREAM("Found "<<caljob_scenes->size() <<" scenes");
	    scene_list_.resize(caljob_scenes->size() );
	    for (unsigned int i = 0; i < caljob_scenes->size(); i++)
	      {
		(*caljob_scenes)[i]["scene_id"] >> scene_id_num;
		(*caljob_scenes)[i]["trigger_type"] >> trig_type;
		string ros_bool_param;
		string message;
		string server_name;
		switch(trig_type)
		  {
		  case 1: // GRAB next image, no message, don't wait, just do it
		    scene_trig = new ImmediateSceneTrigger();
		    break;
		  case 2: // 
		    (*caljob_scenes)[i]["ros_bool_param"] >> ros_bool_param;
		    scene_trig = new ROSParamSceneTrigger(ros_bool_param);
		    break;
		  case 3: // send a message to an action server and wait till finished
		    (*caljob_scenes)[i]["action_message"] >> message;
		    (*caljob_scenes)[i]["action_server"]    >> server_name;
		    scene_trig = new ROSActionServerSceneTrigger(server_name,message);
		    break;
		  default:
		    ROS_ERROR("TRIGGER TYPE NOT IMPLEMENTED");
		    break;
		  }
		scene_list_.at(i).setTrig(scene_trig);
		scene_list_.at(i).setSceneId(scene_id_num);
		const YAML::Node *obs_node = (*caljob_scenes)[i].FindValue("observations");
		ROS_DEBUG_STREAM("Found "<<obs_node->size() <<" observations within scene "<<i);
		for (unsigned int j = 0; j < obs_node->size(); j++)
		  {
		    (*obs_node)[j]["camera"] >> camera_name;
		    (*obs_node)[j]["roi_x_min"] >> temp_roi.x_min;
		    (*obs_node)[j]["roi_x_max"] >> temp_roi.x_max;
		    (*obs_node)[j]["roi_y_min"] >> temp_roi.y_min;
		    (*obs_node)[j]["roi_y_max"] >> temp_roi.y_max;
		    (*obs_node)[j]["target"] >> target_name;

		    // TODO CHECK FOR valid return
		    temp_cam = ceres_blocks_.getCameraByName(camera_name);
		    temp_targ = ceres_blocks_.getTargetByName(target_name);
		    scene_list_.at(i).addCameraToScene(temp_cam);
		    scene_list_.at(i).populateObsCmdList(temp_cam, temp_targ, temp_roi);
		  }
	      }
	  }
      } // end try
    catch (YAML::ParserException& e)
      {
	ROS_ERROR("load() Failed to read in caljob yaml file");
	ROS_ERROR_STREAM("Failed with exception "<< e.what());
	return (false);
      }
    return true;
  }

  bool CalibrationJob::run()
  {
    ROS_INFO("RUNNING_OBSERVATION");
    runObservations();
    ROS_INFO("RUNNING_OPTIMIZATIONS");
    return runOptimization();
  }

  bool CalibrationJob::runObservations()
  {
    ROS_DEBUG_STREAM("Running observations...");

    // the result of this function are twofold
    // First, it fills up observation_data_point_list_ with lists of observationspercamera
    // Second it adds parameter blocks to the ceres_blocks
    // extrinsics and intrinsics for each static camera
    // The whole target for once every static target (parameter blocks are in  Pose6d and an array of points)
    // The whole target once a scene for each moving target
    observation_data_point_list_.clear(); // clear previously recorded observations

    // For each scene
    BOOST_FOREACH(ObservationScene current_scene, scene_list_)
      {
	int scene_id = current_scene.get_id();
	ROS_DEBUG_STREAM("Processing Scene " << scene_id+1<<" of "<< scene_list_.size());
	ROS_INFO("Processing Scene  %d of %d",scene_id,scene_list_.size());

	BOOST_FOREACH(shared_ptr<Camera> current_camera, current_scene.cameras_in_scene_)
	  {			// clear camera of existing observations
	    current_camera->camera_observer_->clearObservations(); // clear any recorded data
	    current_camera->camera_observer_->clearTargets(); // clear all targets
	  }

	BOOST_FOREACH(ObservationCmd o_command, current_scene.observation_command_list_)
	  {	// add each target and roi each camera's list of observations
	    o_command.camera->camera_observer_->addTarget(o_command.target, o_command.roi);
	  }
	
	current_scene.get_trigger()->waitForTrigger(); // this indicates scene is ready to capture

	BOOST_FOREACH( shared_ptr<Camera> current_camera, current_scene.cameras_in_scene_)
	  {// trigger the cameras
	    current_camera->camera_observer_->triggerCamera();
	  }

	// collect results
	P_BLOCK intrinsics;
	P_BLOCK extrinsics;
	P_BLOCK target_pose;
	P_BLOCK pnt_pos;
	std::string camera_name;
	std::string target_name;
	int target_type;

	// for each camera in scene get a list of observations, and add camera parameters to ceres_blocks
	ObservationDataPointList listpercamera;
	BOOST_FOREACH( shared_ptr<Camera> camera, current_scene.cameras_in_scene_)
	  {
	    // wait until observation is done
	    while (!camera->camera_observer_->observationsDone()) ;

	    camera_name = camera->camera_name_;
	    if (camera->isMoving())
	      {
		// next line does nothing if camera already exist in blocks
		ceres_blocks_.addMovingCamera(camera, scene_id);
		intrinsics = ceres_blocks_.getMovingCameraParameterBlockIntrinsics(camera_name);
		extrinsics = ceres_blocks_.getMovingCameraParameterBlockExtrinsics(camera_name, scene_id);
	      }
	    else
	      {
		// next line does nothing if camera already exist in blocks
		ceres_blocks_.addStaticCamera(camera);
		intrinsics = ceres_blocks_.getStaticCameraParameterBlockIntrinsics(camera_name);
		extrinsics = ceres_blocks_.getStaticCameraParameterBlockExtrinsics(camera_name);
	      }

	    // Get the observations from this camera whose P_BLOCKs are intrinsics and extrinsics
	    CameraObservations camera_observations;
	    int number_returned;
	    number_returned = camera->camera_observer_->getObservations(camera_observations);

	    ROS_DEBUG_STREAM("Processing " << camera_observations.observations.size() << " Observations");
	    ROS_INFO("Processing %d Observations ",camera_observations.observations.size());
	    BOOST_FOREACH(Observation observation, camera_observations.observations)
	      {
		target_name = observation.target->target_name;
		target_type = observation.target->target_type;
		double circle_dia=0.0;
		if(target_type == pattern_options::CircleGrid){
		  circle_dia = observation.target->circle_grid_parameters.circle_diameter;
		}
		int pnt_id = observation.point_id;
		double observation_x = observation.image_loc_x;
		double observation_y = observation.image_loc_y;
		if (observation.target->is_moving)
		  {
		    ceres_blocks_.addMovingTarget(observation.target, scene_id);
		    target_pose = ceres_blocks_.getMovingTargetPoseParameterBlock(target_name, scene_id);
		    pnt_pos = ceres_blocks_.getMovingTargetPointParameterBlock(target_name, pnt_id);
		  }
		else
		  {
		    ceres_blocks_.addStaticTarget(observation.target); // if exist, does nothing
		    target_pose = ceres_blocks_.getStaticTargetPoseParameterBlock(target_name);
		    pnt_pos = ceres_blocks_.getStaticTargetPointParameterBlock(target_name, pnt_id);
		  }
		ObservationDataPoint temp_ODP(camera_name, target_name, target_type,
					      scene_id, intrinsics, extrinsics, pnt_id, target_pose,
					      pnt_pos, observation_x, observation_y, circle_dia);
		listpercamera.addObservationPoint(temp_ODP);
	      }//end for each observed point
	  }//end for each camera
	observation_data_point_list_.push_back(listpercamera);
      } //end for each scene
    return true;
  }

  bool CalibrationJob::runOptimization()
  {
    int total_observations =0;
    for(int i=0;i<observation_data_point_list_.size();i++){
      total_observations += observation_data_point_list_[i].items.size();
    }
    if(total_observations == 0){ // TODO really need more than number of parameters being computed
      ROS_ERROR("TOO FEW OBSERVATIONS: %d",total_observations);
      return(false);
    }

    // take all the data collected and create a Ceres optimization problem and run it
    ROS_INFO("Running Optimization with %d scenes",(int)scene_list_.size());
    ROS_DEBUG_STREAM("Optimizing "<<scene_list_.size()<<" scenes");
    BOOST_FOREACH(ObservationScene current_scene, scene_list_)
      {

	int scene_id = current_scene.get_id();
	BOOST_FOREACH(shared_ptr<Camera> camera, current_scene.cameras_in_scene_)
	  {
	    ROS_DEBUG_STREAM("Current observation data point list size: "<<observation_data_point_list_.at(scene_id).items.size());
	    // take all the data collected and create a Ceres optimization problem and run it
	    P_BLOCK extrinsics;
	    P_BLOCK target_pose;
	    P_BLOCK point_position;
	    BOOST_FOREACH(ObservationDataPoint ODP, observation_data_point_list_.at(scene_id).items)
	      {
		// create cost function
		// there are several options
		// 1. the complete reprojection error cost function "Create(obs_x,obs_y)"
		//    this cost function has the following parameters:
		//      a. camera intrinsics
		//      b. camera extrinsics
		//      c. target pose
		//      d. point location in target frame
		// 2. the same as 1, but without d  "Create(obs_x,obs_y,t_pnt_x, t_pnt_y, t_pnt_z)
		// 3. the same as 1, but without a  "Create(obs_x,obs_y,fx,fy,cx,cy,cz)"
		//    Note that this one assumes we are using rectified images to compute the observations
		// 4. the same as 3, point location fixed too "Create(obs_x,obs_y,fx,fy,cx,cy,cz,t_x,t_y,t_z)"
		//        implemented in TargetCameraReprjErrorNoDistortion
		// 5. the same as 4, but with target in known location
		//    "Create(obs_x,obs_y,fx,fy,cx,cy,cz,t_x,t_y,t_z,p_tx,p_ty,p_tz,p_ax,p_ay,p_az)"
		// pull out the constants from the observation point data

		double focal_length_x = ODP.camera_intrinsics_[0]; // TODO, make this not so ugly
		double focal_length_y = ODP.camera_intrinsics_[1];
		double center_pnt_x   = ODP.camera_intrinsics_[2];
		double center_pnt_y   = ODP.camera_intrinsics_[3];
		double image_x        = ODP.image_x_;
		double image_y        = ODP.image_y_;
		double point_x        = ODP.point_position_[0];// location of point within target frame
		double point_y        = ODP.point_position_[1];
		double point_z        = ODP.point_position_[2];
		unsigned int target_type    = ODP.target_type_;
	      
		// pull out pointers to the parameter blocks in the observation point data
		extrinsics        = ODP.camera_extrinsics_;
		target_pose     = ODP.target_pose_;
		point_position = ODP.point_position_;
	      
		// create the cost function
		if(target_type == pattern_options::CircleGrid){
		  double circle_dia = ODP.circle_dia_;

		  CostFunction* cost_function = CircleTargetCameraReprjErrorNoDFixedPoint::Create(image_x, image_y,
												  circle_dia,
												  focal_length_x,
												  focal_length_y,
												  center_pnt_x,
												  center_pnt_y,
												  point_x,
												  point_y,
												  point_z);
		  // add it as a residual using parameter blocks
		  problem_.AddResidualBlock(cost_function, NULL , extrinsics, target_pose);
		}
		else{
		  CostFunction* cost_function = TargetCameraReprjErrorNoDistortion::Create(image_x, image_y,
											   focal_length_x,
											   focal_length_y,
											   center_pnt_x,
											   center_pnt_y,
											   point_x,
											   point_y,
											   point_z);
		  // add it as a residual using parameter blocks
		  problem_.AddResidualBlock(cost_function, NULL , extrinsics, target_pose);
		}
	      }//for each observation
	    problem_.SetParameterBlockConstant(target_pose);
	  
	  }//for each camera
      }//for each scene
    ROS_INFO("total observations: %d ",total_observations);

    // Make Ceres automatically detect the bundle structure. Note that the
    // standard solver, SPARSE_NORMAL_CHOLESKY, also works fine but it is slower
    // for standard bundle adjustment problems.
    ceres::Solver::Options options;
    ceres::Solver::Summary summary;
    options.linear_solver_type = ceres::DENSE_SCHUR;
    options.minimizer_progress_to_stdout = true;
    options.max_num_iterations = 1000;
    ceres::Solve(options, &problem_, &summary);

    return true;
  }//end runOptimization

  bool CalibrationJob::store()
  {
    std::string path = ros::package::getPath("industrial_extrinsic_cal");
    std::string file_path = "/launch/target_to_camera_optical_transform_publisher.launch";
    std::string filepath = path+file_path;
    return ceres_blocks_.write_all_static_transforms(filepath);
  }

  void CalibrationJob::show()
  {
    ceres_blocks_.display_all_cameras_and_targets();
  }

}//end namespace industrial_extrinsic_cal