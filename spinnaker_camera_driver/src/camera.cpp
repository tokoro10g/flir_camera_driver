/**
Software License Agreement (BSD)

\file      camera.cpp
\authors   Michael Hosmar <mhosmar@clearpathrobotics.com>
\copyright Copyright (c) 2018, Clearpath Robotics, Inc., All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that
the following conditions are met:
 * Redistributions of source code must retain the above copyright notice, this list of conditions and the
   following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
   following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of Clearpath Robotics nor the names of its contributors may be used to endorse or promote
   products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WAR-
RANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, IN-
DIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "spinnaker_camera_driver/camera.h"

#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace spinnaker_camera_driver
{
void Camera::init()
{
  Spinnaker::GenApi::CIntegerPtr height_max_ptr = node_map_->GetNode("HeightMax");
  if (!IsAvailable(height_max_ptr) || !IsReadable(height_max_ptr))
  {
    throw std::runtime_error("[Camera::init] Unable to read HeightMax");
  }
  height_max_ = height_max_ptr->GetValue();
  roi_height_ = height_max_;
  roi_y_offset_ = 0;
  Spinnaker::GenApi::CIntegerPtr width_max_ptr = node_map_->GetNode("WidthMax");
  if (!IsAvailable(width_max_ptr) || !IsReadable(width_max_ptr))
  {
    throw std::runtime_error("[Camera::init] Unable to read WidthMax");
  }
  width_max_ = width_max_ptr->GetValue();
  roi_width_ = width_max_;
  roi_x_offset_ = 0;
  // Set Throughput to maximum
  //=====================================
  setMaxInt(node_map_, "DeviceLinkThroughputLimit");
}
void Camera::setFrameRate(const float frame_rate)
{
  // This enables the "AcquisitionFrameRateEnabled"
  //======================================
  setProperty(node_map_, "AcquisitionFrameRateEnable", true);

  // This sets the "AcquisitionFrameRate" to X FPS
  // ========================================

  Spinnaker::GenApi::CFloatPtr ptrAcquisitionFrameRate = node_map_->GetNode("AcquisitionFrameRate");
  ROS_DEBUG_STREAM("Minimum Frame Rate: \t " << ptrAcquisitionFrameRate->GetMin());
  ROS_DEBUG_STREAM("Maximum Frame rate: \t " << ptrAcquisitionFrameRate->GetMax());

  // Finally Set the Frame Rate
  setProperty(node_map_, "AcquisitionFrameRate", frame_rate);

  ROS_DEBUG_STREAM("Current Frame rate: \t " << ptrAcquisitionFrameRate->GetValue());
}

void Camera::setNewConfiguration(SpinnakerConfig& config, const uint32_t& level)
{
  try
  {
    if (level >= LEVEL_RECONFIGURE_STOP)
      setImageControlFormats(config);

    setFrameRate(static_cast<float>(config.acquisition_frame_rate));
    // Set enable after frame rate encase its false
    setProperty(node_map_, "AcquisitionFrameRateEnable", config.acquisition_frame_rate_enable);

    // Set Trigger and Strobe Settings
    // NOTE: The trigger must be disabled (i.e. TriggerMode = "Off") in order to configure whether the source is
    // software or hardware.
    setProperty(node_map_, "TriggerMode", std::string("Off"));
    setProperty(node_map_, "TriggerSource", config.trigger_source);
    setProperty(node_map_, "TriggerOverlap", config.trigger_overlap_mode);
    setProperty(node_map_, "TriggerSelector", config.trigger_selector);
    setProperty(node_map_, "TriggerActivation", config.trigger_activation_mode);
    setProperty(node_map_, "TriggerMode", config.enable_trigger);

    if (config.v3_3_selector != "Off")
    {
      setProperty(node_map_, "LineSelector", config.v3_3_selector);
      setProperty(node_map_, "V3_3Enable", true);
    }
    setProperty(node_map_, "LineSelector", config.line_selector);
    setProperty(node_map_, "LineMode", config.line_mode);
    setProperty(node_map_, "LineSource", config.line_source);


    // Set auto exposure
    setProperty(node_map_, "ExposureMode", config.exposure_mode);
    setProperty(node_map_, "ExposureAuto", config.exposure_auto);

    // Set sharpness
    if (IsAvailable(node_map_->GetNode("SharpeningEnable")))
    {
      setProperty(node_map_, "SharpeningEnable", config.sharpening_enable);
      if (config.sharpening_enable)
      {
        setProperty(node_map_, "SharpeningAuto", config.auto_sharpness);
        setProperty(node_map_, "Sharpening", static_cast<float>(config.sharpness));
        setProperty(node_map_, "SharpeningThreshold", static_cast<float>(config.sharpening_threshold));
        Spinnaker::GenApi::CFloatPtr ptrSharpening = node_map_->GetNode("Sharpening");
        config.sharpness = ptrSharpening->GetValue();
      }
    }

    // Set saturation
    if (IsAvailable(node_map_->GetNode("SaturationEnable")))
    {
      setProperty(node_map_, "SaturationEnable", config.saturation_enable);
      if (config.saturation_enable)
      {
        setProperty(node_map_, "Saturation", static_cast<float>(config.saturation));
      }
    }

    // Set shutter time/speed
    if (config.exposure_auto.compare(std::string("Off")) == 0)
    {
      setProperty(node_map_, "ExposureTime", static_cast<float>(config.exposure_time));
    }
    else
    {
      setProperty(node_map_, "AutoExposureExposureTimeUpperLimit",
                  static_cast<float>(config.auto_exposure_time_upper_limit));
    }
    Spinnaker::GenApi::CFloatPtr ptrExposureTime = node_map_->GetNode("ExposureTime");
    config.exposure_time = ptrExposureTime->GetValue();

    // Set gain
    setProperty(node_map_, "GainSelector", config.gain_selector);
    setProperty(node_map_, "GainAuto", config.auto_gain);
    if (config.auto_gain.compare(std::string("Off")) == 0)
    {
      setProperty(node_map_, "Gain", static_cast<float>(config.gain));
    }
    Spinnaker::GenApi::CFloatPtr ptrGain = node_map_->GetNode("Gain");
    config.gain = ptrGain->GetValue();

    // Set brightness
    setProperty(node_map_, "BlackLevel", static_cast<float>(config.brightness));

    // Set gamma
    if (config.gamma_enable)
    {
      setProperty(node_map_, "GammaEnable", config.gamma_enable);
      setProperty(node_map_, "Gamma", static_cast<float>(config.gamma));
    }

    // Set white balance
    if (IsAvailable(node_map_->GetNode("BalanceWhiteAuto")))
    {
      setProperty(node_map_, "BalanceWhiteAuto", config.auto_white_balance);
      if (config.auto_white_balance.compare(std::string("Off")) == 0)
      {
        setProperty(node_map_, "BalanceRatioSelector", std::string("Blue"));
        setProperty(node_map_, "BalanceRatio", static_cast<float>(config.white_balance_blue_ratio));
        Spinnaker::GenApi::CFloatPtr ptrBalanceRatioBlue = node_map_->GetNode("BalanceRatio");
        config.white_balance_blue_ratio = ptrBalanceRatioBlue->GetValue();
        setProperty(node_map_, "BalanceRatioSelector", std::string("Red"));
        setProperty(node_map_, "BalanceRatio", static_cast<float>(config.white_balance_red_ratio));
        Spinnaker::GenApi::CFloatPtr ptrBalanceRatioRed = node_map_->GetNode("BalanceRatio");
        config.white_balance_red_ratio = ptrBalanceRatioRed->GetValue();
      }
      else
      {
        setProperty(node_map_, "BalanceRatioSelector", std::string("Blue"));
        Spinnaker::GenApi::CFloatPtr ptrBalanceRatioBlue = node_map_->GetNode("BalanceRatio");
        config.white_balance_blue_ratio = ptrBalanceRatioBlue->GetValue();
        setProperty(node_map_, "BalanceRatioSelector", std::string("Red"));
        Spinnaker::GenApi::CFloatPtr ptrBalanceRatioRed = node_map_->GetNode("BalanceRatio");
        config.white_balance_red_ratio = ptrBalanceRatioRed->GetValue();
      }
    }

    // Color correction matrix
    if (IsAvailable(node_map_->GetNode("IspEnable")))
    {
      if (IsAvailable(node_map_->GetNode("ColorTransformationSelector")) &&
          IsAvailable(node_map_->GetNode("ColorTransformationEnable")) &&
          IsAvailable(node_map_->GetNode("RgbTransformLightSource")))
      {
        if (config.color_correction_enable)
        {
          setProperty(node_map_, "IspEnable", true);
          setProperty(node_map_, "ColorTransformationSelector", std::string("RGBtoRGB"));
          setProperty(node_map_, "ColorTransformationEnable", true);
          setProperty(node_map_, "RgbTransformLightSource", config.color_correction_light_source);
          std::vector<std::vector<float>> ccm = {
            { 1.0f, 0.0f, 0.0f, },
            { 0.0f, 1.0f, 0.0f, },
            { 0.0f, 0.0f, 1.0f, },
            { 0.0f, 0.0f, 0.0f, },
          };
          if (config.color_correction_light_source == "Custom")
          {
            try
            {
              ccm = YAML::Load(config.color_correction_matrix).as<std::vector<std::vector<float>>>();
              if (ccm.size() != 4) throw std::runtime_error("invalid");
              for (const auto &v : ccm)
                if (v.size() != 3) throw std::runtime_error("invalid");
            }
            catch (...)
            {
              ccm = {
                { 1.0f, 0.0f, 0.0f, },
                { 0.0f, 1.0f, 0.0f, },
                { 0.0f, 0.0f, 1.0f, },
                { 0.0f, 0.0f, 0.0f, },
              };
            }
            setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain00"));
            setProperty(node_map_, "ColorTransformationValue", ccm[0][0]);
            setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain01"));
            setProperty(node_map_, "ColorTransformationValue", ccm[0][1]);
            setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain02"));
            setProperty(node_map_, "ColorTransformationValue", ccm[0][2]);
            setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain10"));
            setProperty(node_map_, "ColorTransformationValue", ccm[1][0]);
            setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain11"));
            setProperty(node_map_, "ColorTransformationValue", ccm[1][1]);
            setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain12"));
            setProperty(node_map_, "ColorTransformationValue", ccm[1][2]);
            setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain20"));
            setProperty(node_map_, "ColorTransformationValue", ccm[2][0]);
            setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain21"));
            setProperty(node_map_, "ColorTransformationValue", ccm[2][1]);
            setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain22"));
            setProperty(node_map_, "ColorTransformationValue", ccm[2][2]);
            setProperty(node_map_, "ColorTransformationValueSelector", std::string("Offset0"));
            setProperty(node_map_, "ColorTransformationValue", ccm[3][0]);
            setProperty(node_map_, "ColorTransformationValueSelector", std::string("Offset1"));
            setProperty(node_map_, "ColorTransformationValue", ccm[3][1]);
            setProperty(node_map_, "ColorTransformationValueSelector", std::string("Offset2"));
            setProperty(node_map_, "ColorTransformationValue", ccm[3][2]);
          }

          setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain00"));
          ccm[0][0] = static_cast<Spinnaker::GenApi::CFloatPtr>(node_map_->GetNode("ColorTransformationValue"))->GetValue();
          setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain01"));
          ccm[0][1] = static_cast<Spinnaker::GenApi::CFloatPtr>(node_map_->GetNode("ColorTransformationValue"))->GetValue();
          setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain02"));
          ccm[0][2] = static_cast<Spinnaker::GenApi::CFloatPtr>(node_map_->GetNode("ColorTransformationValue"))->GetValue();
          setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain10"));
          ccm[1][0] = static_cast<Spinnaker::GenApi::CFloatPtr>(node_map_->GetNode("ColorTransformationValue"))->GetValue();
          setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain11"));
          ccm[1][1] = static_cast<Spinnaker::GenApi::CFloatPtr>(node_map_->GetNode("ColorTransformationValue"))->GetValue();
          setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain12"));
          ccm[1][2] = static_cast<Spinnaker::GenApi::CFloatPtr>(node_map_->GetNode("ColorTransformationValue"))->GetValue();
          setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain20"));
          ccm[2][0] = static_cast<Spinnaker::GenApi::CFloatPtr>(node_map_->GetNode("ColorTransformationValue"))->GetValue();
          setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain21"));
          ccm[2][1] = static_cast<Spinnaker::GenApi::CFloatPtr>(node_map_->GetNode("ColorTransformationValue"))->GetValue();
          setProperty(node_map_, "ColorTransformationValueSelector", std::string("Gain22"));
          ccm[2][2] = static_cast<Spinnaker::GenApi::CFloatPtr>(node_map_->GetNode("ColorTransformationValue"))->GetValue();
          setProperty(node_map_, "ColorTransformationValueSelector", std::string("Offset0"));
          ccm[3][0] = static_cast<Spinnaker::GenApi::CFloatPtr>(node_map_->GetNode("ColorTransformationValue"))->GetValue();
          setProperty(node_map_, "ColorTransformationValueSelector", std::string("Offset1"));
          ccm[3][1] = static_cast<Spinnaker::GenApi::CFloatPtr>(node_map_->GetNode("ColorTransformationValue"))->GetValue();
          setProperty(node_map_, "ColorTransformationValueSelector", std::string("Offset2"));
          ccm[3][2] = static_cast<Spinnaker::GenApi::CFloatPtr>(node_map_->GetNode("ColorTransformationValue"))->GetValue();
          std::ostringstream oss;
          oss << "[";
          for (size_t i = 0; i < ccm.size(); ++i)
          {
            oss << "[";
            for (size_t j = 0; j < ccm[i].size(); ++j)
            {
              oss << ccm[i][j];
              if (j < ccm[i].size() - 1) oss << ",";
            }
            oss << "]";
            if (i < ccm.size() - 1) oss << ",";
          }
          oss << "]";
          config.color_correction_matrix = oss.str();
        }
        else
        {
          setProperty(node_map_, "ColorTransformationEnable", false);
          setProperty(node_map_, "IspEnable", false);
        }
      }
    }

    setProperty(node_map_, "ReverseX", config.reverse_x);
    setProperty(node_map_, "ReverseY", config.reverse_y);

    if (config.gige_mode)
    {
      setProperty(node_map_, "GevSCPSPacketSize", config.gev_scps_packet_size);
      setProperty(node_map_, "DeviceLinkThroughputLimit", config.device_link_throughput_limit);
    }
    setProperty(node_map_, "DefectCorrectStaticEnable", config.defect_correct_static_enable);

    setProperty(node_map_, "ImageCompressionMode", config.image_compression_mode);
    setProperty(node_map_, "AdaptiveCompressionEnable", config.adaptive_compression_enable);
    setProperty(node_map_, "MaxDatarateThreshold", config.max_datarate_threshold);

    // Added for internal trigger generation using Counter
    setProperty(node_map_, "CounterSelector", config.counter_selector);
    setProperty(node_map_, "CounterEventSource", config.counter_event_source);
    setProperty(node_map_, "CounterDuration", config.counter_duration);
    setProperty(node_map_, "CounterDelay", config.counter_delay);
    setProperty(node_map_, "CounterTriggerSource", config.counter_trigger_source);
    setProperty(node_map_, "CounterTriggerActivation", config.counter_trigger_activation);
  }
  catch (const Spinnaker::Exception& e)
  {
    throw std::runtime_error("[Camera::setNewConfiguration] Failed to set configuration: " + std::string(e.what()));
  }
}

// Image Size and Pixel Format
void Camera::setImageControlFormats(const spinnaker_camera_driver::SpinnakerConfig& config)
{
  // Set Binning and Decimation
  setProperty(node_map_, "BinningHorizontal", config.image_format_x_binning);
  setProperty(node_map_, "BinningVertical", config.image_format_y_binning);
  setProperty(node_map_, "DecimationHorizontal", config.image_format_x_decimation);
  setProperty(node_map_, "DecimationVertical", config.image_format_y_decimation);

  // Grab the Max values after decimation
  Spinnaker::GenApi::CIntegerPtr height_max_ptr = node_map_->GetNode("HeightMax");
  if (!IsAvailable(height_max_ptr) || !IsReadable(height_max_ptr))
  {
    throw std::runtime_error("[Camera::setImageControlFormats] Unable to read HeightMax");
  }
  height_max_ = height_max_ptr->GetValue();
  Spinnaker::GenApi::CIntegerPtr width_max_ptr = node_map_->GetNode("WidthMax");
  if (!IsAvailable(width_max_ptr) || !IsReadable(width_max_ptr))
  {
    throw std::runtime_error("[Camera::setImageControlFormats] Unable to read WidthMax");
  }
  width_max_ = width_max_ptr->GetValue();

  // Offset first encase expanding ROI
  // Apply offset X
  setProperty(node_map_, "OffsetX", 0);
  // Apply offset Y
  setProperty(node_map_, "OffsetY", 0);

  setROI(config.image_format_x_offset, config.image_format_y_offset,
         config.image_format_roi_width, config.image_format_roi_height);

  // Set Pixel Format
  setProperty(node_map_, "PixelFormat", config.image_format_color_coding);
}

void Camera::setROI(const int x_offset, const int y_offset, const int roi_width, const int roi_height)
{
  // Set Width/Height
  if (roi_width != roi_width_)
  {
    if (roi_width <= 0 || roi_width > width_max_)
    {
      setProperty(node_map_, "Width", width_max_);
      roi_width_ = width_max_;
    }
    else
    {
      setProperty(node_map_, "Width", roi_width);
      roi_width_ = roi_width;
    }
  }
  if (roi_height != roi_height_)
  {
    if (roi_height <= 0 || roi_height > height_max_)
    {
      setProperty(node_map_, "Height", height_max_);
      roi_height_ = height_max_;
    }
    else
    {
      setProperty(node_map_, "Height", roi_height);
      roi_height_ = roi_height;
    }
  }

  // Apply offset X
  setProperty(node_map_, "OffsetX", x_offset);
  roi_x_offset_ = x_offset;
  // Apply offset Y
  setProperty(node_map_, "OffsetY", y_offset);
  roi_y_offset_ = y_offset;
}

void Camera::setGain(const float& gain)
{
  setProperty(node_map_, "GainAuto", "Off");
  setProperty(node_map_, "Gain", static_cast<float>(gain));
}

/*
void Camera::setGigEParameters(bool auto_packet_size, unsigned int packet_size, unsigned int packet_delay)
{
}

void Camera::setupGigEPacketSize(PGRGuid & guid)
{
}

void Camera::setupGigEPacketSize(PGRGuid & guid, unsigned int packet_size)
{

}

void Camera::setupGigEPacketDelay(PGRGuid & guid, unsigned int packet_delay)
{
}

*/

int Camera::getHeightMax() const
{
  return height_max_;
}

int Camera::getWidthMax() const
{
  return width_max_;
}

int Camera::getROIXOffset() const
{
  return roi_x_offset_;
}

int Camera::getROIYOffset() const
{
  return roi_y_offset_;
}

int Camera::getROIWidth() const
{
  return roi_width_;
}

int Camera::getROIHeight() const
{
  return roi_height_;
}

// uint SpinnakerCamera::getGain()
// {
//   return metadata_.embeddedGain >> 20;
// }

// uint Camera::getShutter()
// {
//   return metadata_.embeddedShutter >> 20;
// }

// uint Camera::getBrightness()
// {
//   return metadata_.embeddedTimeStamp >> 20;
// }

// uint Camera::getExposure()
// {
//   return metadata_.embeddedBrightness >> 20;
// }

// uint Camera::getWhiteBalance()
// {
//   return metadata_.embeddedExposure >> 8;
// }

// uint Camera::getROIPosition()
// {
//   return metadata_.embeddedROIPosition >> 24;
// }

// float Camera::getCameraTemperature()
//{
//}

// float Camera::getCameraFrameRate()
//{
//}
Spinnaker::GenApi::CNodePtr Camera::readProperty(const Spinnaker::GenICam::gcstring property_name)
{
  Spinnaker::GenApi::CNodePtr ptr = node_map_->GetNode(property_name);
  if (!Spinnaker::GenApi::IsAvailable(ptr) || !Spinnaker::GenApi::IsReadable(ptr))
  {
    throw std::runtime_error("Unable to get parmeter " + property_name);
  }
  return ptr;
}

Camera::Camera(Spinnaker::GenApi::INodeMap* node_map)
{
  node_map_ = node_map;
  init();
}
}  // namespace spinnaker_camera_driver
