/* This file is part of the Pangolin Project.
 * http://github.com/stevenlovegrove/Pangolin
 *
 * Copyright (c) 2011 Steven Lovegrove
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "firewire.h"
#ifdef HAVE_DC1394

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>

using namespace std;

namespace pangolin
{

void FirewireVideo::init_camera(
    uint64_t guid, int dma_frames,
    dc1394speed_t iso_speed,
    dc1394video_mode_t video_mode,
    dc1394framerate_t framerate
    ) {
    camera = dc1394_camera_new (d, guid);
    if (!camera)
        throw VideoException("Failed to initialize camera");

    // Attempt to stop camera if it is already running
    dc1394switch_t is_iso_on = DC1394_OFF;
    dc1394_video_get_transmission(camera, &is_iso_on);
    if (is_iso_on==DC1394_ON) {
        dc1394_video_set_transmission(camera, DC1394_OFF);
    }


    cout << "Using camera with GUID " << camera->guid << endl;

    //-----------------------------------------------------------------------
    //  setup capture
    //-----------------------------------------------------------------------

    if( iso_speed >= DC1394_ISO_SPEED_800)
    {
        err=dc1394_video_set_operation_mode(camera, DC1394_OPERATION_MODE_1394B);
        if( err != DC1394_SUCCESS )
            throw VideoException("Could not set DC1394_OPERATION_MODE_1394B");
    }

    err=dc1394_video_set_iso_speed(camera, iso_speed);
    if( err != DC1394_SUCCESS )
        throw VideoException("Could not set iso speed");

    err=dc1394_video_set_mode(camera, video_mode);
    if( err != DC1394_SUCCESS )
        throw VideoException("Could not set video mode");

    err=dc1394_video_set_framerate(camera, framerate);
    if( err != DC1394_SUCCESS )
        throw VideoException("Could not set framerate");

    err=dc1394_capture_setup(camera,dma_frames, DC1394_CAPTURE_FLAGS_DEFAULT);
    if( err != DC1394_SUCCESS )
        throw VideoException("Could not setup camera - check settings");

    //-----------------------------------------------------------------------
    //  initialise width and height from mode
    //-----------------------------------------------------------------------
    dc1394_get_image_size_from_video_mode(camera, video_mode, &width, &height);

    Start();
}


// Note:
// the following was tested on a IIDC camera over USB therefore might not work as
// well on a camera over proper firewire transport
void FirewireVideo::init_format7_camera(
    uint64_t guid, int dma_frames,
    dc1394speed_t iso_speed,
    dc1394video_mode_t video_mode,
    uint32_t framerate,
    uint32_t width, uint32_t height,
    uint32_t left, uint32_t top
    ) {

    if(video_mode< DC1394_VIDEO_MODE_FORMAT7_0)
        throw VideoException("roi can be specified only for format7 modes");

    camera = dc1394_camera_new (d, guid);
    if (!camera)
        throw VideoException("Failed to initialize camera");

    // Attempt to stop camera if it is already running
    dc1394switch_t is_iso_on = DC1394_OFF;
    dc1394_video_get_transmission(camera, &is_iso_on);
    if (is_iso_on==DC1394_ON) {
        dc1394_video_set_transmission(camera, DC1394_OFF);
    }

    cout << "Using camera with GUID " << camera->guid << endl;

    //-----------------------------------------------------------------------
    //  setup capture
    //-----------------------------------------------------------------------

    dc1394format7mode_t format7_info;

    err = dc1394_format7_get_mode_info(camera, video_mode, &format7_info);
    if( err != DC1394_SUCCESS )
      throw VideoException("Could not get format7 mode info");

    if(format7_info.present==DC1394_FALSE)
      throw VideoException("Mode not supported by the camera");

    err = dc1394_format7_set_image_position(camera,video_mode,0,0);
    if( err != DC1394_SUCCESS )
      throw VideoException("Could not reset format7 position");

    width = nearest_value(width, format7_info.unit_pos_x, 0, format7_info.max_size_x - format7_info.unit_pos_x);
    height = nearest_value(height, format7_info.unit_pos_y, 0, format7_info.max_size_y - format7_info.unit_pos_y);

    err = dc1394_format7_set_image_size(camera,video_mode,width,height);
    if( err != DC1394_SUCCESS )
      throw VideoException("Could not set format7 size");
//
//    cout<<" sx:"<<format7_info.size_x
//        <<" sy:"<<format7_info.size_y
//        <<" maxsx:"<<format7_info.max_size_x
//        <<" maxsy:"<<format7_info.max_size_y
//        <<" px:"<<format7_info.pos_x
//        <<" py:"<<format7_info.pos_y
//        <<" unitsx:"<<format7_info.unit_size_x
//        <<" uintsy:"<<format7_info.unit_size_y
//        <<" unitpx:"<<format7_info.unit_pos_x
//        <<" unitpy:"<<format7_info.unit_pos_y
//        <<" pixnum:"<<format7_info.pixnum
//        <<" pktsize:"<<format7_info.packet_size // bpp is byte_per_packet, not bit per pixel.
//        <<" unitpktsize:"<<format7_info.unit_packet_size
//        <<" maxpktsize:"<<format7_info.max_packet_size<<endl;
//

    if(iso_speed >= DC1394_ISO_SPEED_800)
    {
        err=dc1394_video_set_operation_mode(camera, DC1394_OPERATION_MODE_1394B);
        if( err != DC1394_SUCCESS )
           throw VideoException("Could not set DC1394_OPERATION_MODE_1394B");
    }

    err=dc1394_video_set_iso_speed(camera, iso_speed);
    if( err != DC1394_SUCCESS )
        throw VideoException("Could not set iso speed");

    err=dc1394_video_set_mode(camera, video_mode);
    if( err != DC1394_SUCCESS )
        throw VideoException("Could not set format7 video mode");

    uint32_t depth;
    err=dc1394_format7_get_data_depth(camera,video_mode,&depth);
    if( err != DC1394_SUCCESS )
          throw VideoException("Could not get format7 data depth");

    if(framerate != MAX_FR){

      double bus_period;

      switch(iso_speed){
        case DC1394_ISO_SPEED_3200:
          bus_period = 15.625e-6;
          break;
        case DC1394_ISO_SPEED_1600:
          bus_period = 31.25e-6;
          break;
        case DC1394_ISO_SPEED_800:
          bus_period = 62.5e-6;
          break;
        case DC1394_ISO_SPEED_400:
           bus_period = 125e-6;
           break;
        case DC1394_ISO_SPEED_200:
           bus_period = 250e-6;
           break;
        case DC1394_ISO_SPEED_100:
           bus_period = 500e-6;
           break;
        default:
          throw VideoException("iso speed not valid");
      }

      // work out the max number of packets that the bus can deliver
      int num_packets = (int) (1.0/(bus_period*framerate) + 0.5);

      // work out what the packet size should be for the requested size and framerate
      uint32_t packet_size = (width*964*depth + (num_packets*8) - 1)/(num_packets*8);
      packet_size = nearest_value(packet_size,format7_info.unit_packet_size,format7_info.unit_packet_size,format7_info.max_packet_size);


      if((num_packets > 4095)||(num_packets < 0))
        throw VideoException("number of format7 packets out of range");

      if(packet_size > format7_info.max_packet_size){
        throw VideoException("format7 requested frame rate and size exceed bus bandwidth");
      }

      err=dc1394_format7_set_packet_size(camera,video_mode, packet_size);
      if( err != DC1394_SUCCESS ){
        throw VideoException("Could not set format7 packet size");
      }
    } else {
      err=dc1394_format7_set_packet_size(camera,video_mode, format7_info.max_packet_size);
      if( err != DC1394_SUCCESS ){
        throw VideoException("Could not set format7 packet size");
      }
    }

    left = nearest_value(left, format7_info.unit_size_x, format7_info.unit_size_x, format7_info.max_size_x - width);
    top = nearest_value(top, format7_info.unit_size_y, format7_info.unit_size_y, format7_info.max_size_y - height);

    err=dc1394_format7_set_roi(camera,video_mode, format7_info.color_coding,DC1394_QUERY_FROM_CAMERA,left,top,width,height);
    if( err != DC1394_SUCCESS ){
      throw VideoException("Could not set format7 image ROI");
    } else {
      cout<<"ROI: "<<left<<" "<<top<<" "<<width<<" "<<height<<endl;
    }

    this->width = width;
    this->height = height;
    this->top = top;
    this->left = left;

    err = dc1394_format7_get_mode_info(camera, video_mode, &format7_info);
    if( err != DC1394_SUCCESS )
      throw VideoException("Could not get format7 mode info");

    err = dc1394_feature_set_power(camera,DC1394_FEATURE_FRAME_RATE,DC1394_OFF);
    if( err != DC1394_SUCCESS )
        throw VideoException("Could not turn off frame rate");

    float value;
    err=dc1394_feature_get_absolute_value(camera,DC1394_FEATURE_FRAME_RATE,&value);
    if( err != DC1394_SUCCESS )
        throw VideoException("Could not get framerate");

    cout<<" framerate:"<<value<<endl;

    err=dc1394_capture_setup(camera,dma_frames, DC1394_CAPTURE_FLAGS_DEFAULT);
    if( err != DC1394_SUCCESS )
        throw VideoException("Could not setup camera - check settings");

    Start();
}


std::string Dc1394ColorCodingToString(dc1394color_coding_t coding)
{
    switch(coding)
    {
    case DC1394_COLOR_CODING_MONO8 : return "MONO8";
    case DC1394_COLOR_CODING_YUV411 : return "YUV411";
    case DC1394_COLOR_CODING_YUV422 : return "YUV422";
    case DC1394_COLOR_CODING_YUV444 : return "YUV444";
    case DC1394_COLOR_CODING_RGB8 : return "RGB8";
    case DC1394_COLOR_CODING_MONO16 : return "MONO16";
    case DC1394_COLOR_CODING_RGB16 : return "RGB16";
    case DC1394_COLOR_CODING_MONO16S : return "MONO16S";
    case DC1394_COLOR_CODING_RGB16S : return "RGB16S";
    case DC1394_COLOR_CODING_RAW8 : return "RAW8";
    case DC1394_COLOR_CODING_RAW16 : return "RAW16";
    default: return "";
    }
}

dc1394color_coding_t Dc1394ColorCodingFromString(std::string coding)
{
    if( !coding.compare("MONO8")) return DC1394_COLOR_CODING_MONO8;
    else if(!coding.compare("YUV411")) return DC1394_COLOR_CODING_YUV411;
    else if(!coding.compare("YUV422")) return DC1394_COLOR_CODING_YUV422;
    else if(!coding.compare("YUV444")) return DC1394_COLOR_CODING_YUV444;
    else if(!coding.compare("RGB8")) return DC1394_COLOR_CODING_RGB8;
    else if(!coding.compare("MONO16")) return DC1394_COLOR_CODING_MONO16;
    else if(!coding.compare("RGB16")) return DC1394_COLOR_CODING_RGB16;
    else if(!coding.compare("MONO16S")) return DC1394_COLOR_CODING_MONO16S;
    else if(!coding.compare("RGB16S")) return DC1394_COLOR_CODING_RGB16S;
    else if(!coding.compare("RAW8")) return DC1394_COLOR_CODING_RAW8;
    else if(!coding.compare("RAW16")) return DC1394_COLOR_CODING_RAW16;
    throw VideoException("Unknown Colour coding");
}

std::string FirewireVideo::PixFormat() const
{
    dc1394video_mode_t video_mode;
    dc1394color_coding_t color_coding;
    dc1394_video_get_mode(camera,&video_mode);
    dc1394_get_color_coding_from_video_mode(camera,video_mode,&color_coding);
    return Dc1394ColorCodingToString(color_coding);
}

void FirewireVideo::Start()
{
    if( !running )
    {
        err=dc1394_video_set_transmission(camera, DC1394_ON);
        if( err != DC1394_SUCCESS )
            throw VideoException("Could not start camera iso transmission");
        running = true;
    }
}

void FirewireVideo::Stop()
{
    if( running )
    {
        // Stop transmission
        err=dc1394_video_set_transmission(camera,DC1394_OFF);
        if( err != DC1394_SUCCESS )
            throw VideoException("Could not stop the camera");
        running = false;
    }
}

FirewireVideo::FirewireVideo(
    Guid guid,
    dc1394video_mode_t video_mode,
    dc1394framerate_t framerate,
    dc1394speed_t iso_speed,
    int dma_buffers
) :running(false),top(0),left(0)
{
    d = dc1394_new ();
    if (!d)
        throw VideoException("Failed to get 1394 bus");

    init_camera(guid.guid,dma_buffers,iso_speed,video_mode,framerate);
}

FirewireVideo::FirewireVideo(
    Guid guid,
    dc1394video_mode_t video_mode,
    uint32_t framerate,
    uint32_t width, uint32_t height,
    uint32_t left, uint32_t top,
    dc1394speed_t iso_speed,
    int dma_buffers
) :running(false)
{
    d = dc1394_new ();
    if (!d)
        throw VideoException("Failed to get 1394 bus");

    init_format7_camera(guid.guid,dma_buffers,iso_speed,video_mode,framerate,width,height,left,top);
}

FirewireVideo::FirewireVideo(
    unsigned deviceid,
    dc1394video_mode_t video_mode,
    dc1394framerate_t framerate,
    dc1394speed_t iso_speed,
    int dma_buffers
) :running(false),top(0),left(0)
{
    d = dc1394_new ();
    if (!d)
        throw VideoException("Failed to get 1394 bus");

    err=dc1394_camera_enumerate (d, &list);
    if( err != DC1394_SUCCESS )
        throw VideoException("Failed to enumerate cameras");

    if (list->num == 0)
        throw VideoException("No cameras found");

    if( deviceid >= list->num )
        throw VideoException("Invalid camera index");

    const uint64_t guid = list->ids[deviceid].guid;

    dc1394_camera_free_list (list);

    init_camera(guid,dma_buffers,iso_speed,video_mode,framerate);

}

bool FirewireVideo::GrabNext( unsigned char* image, bool wait )
{
    const dc1394capture_policy_t policy =
            wait ? DC1394_CAPTURE_POLICY_WAIT : DC1394_CAPTURE_POLICY_POLL;

    dc1394video_frame_t *frame;
    dc1394_capture_dequeue(camera, policy, &frame);
    if( frame )
    {
        memcpy(image,frame->image,frame->image_bytes);
        dc1394_capture_enqueue(camera,frame);
        return true;
    }
    return false;
}

bool FirewireVideo::GrabNewest( unsigned char* image, bool wait )
{
    dc1394video_frame_t *f;
    dc1394_capture_dequeue(camera, DC1394_CAPTURE_POLICY_POLL, &f);

    if( f ) {
        while( true )
        {
            dc1394video_frame_t *nf;
            dc1394_capture_dequeue(camera, DC1394_CAPTURE_POLICY_POLL, &nf);
            if( nf )
            {
                err=dc1394_capture_enqueue(camera,f);
                f = nf;
            }else{
                break;
            }
        }
        memcpy(image,f->image,f->image_bytes);
        err=dc1394_capture_enqueue(camera,f);
        return true;
    }else if(wait){
        return GrabNext(image,true);
    }
    return false;
}

FirewireFrame FirewireVideo::GetNext(bool wait)
{
    const dc1394capture_policy_t policy =
            wait ? DC1394_CAPTURE_POLICY_WAIT : DC1394_CAPTURE_POLICY_POLL;

    dc1394video_frame_t *frame;
    dc1394_capture_dequeue(camera, policy, &frame);
    return FirewireFrame(frame);
}

FirewireFrame FirewireVideo::GetNewest(bool wait)
{
    dc1394video_frame_t *f;
    dc1394_capture_dequeue(camera, DC1394_CAPTURE_POLICY_POLL, &f);

    if( f ) {
        while( true )
        {
            dc1394video_frame_t *nf;
            dc1394_capture_dequeue(camera, DC1394_CAPTURE_POLICY_POLL, &nf);
            if( nf )
            {
                err=dc1394_capture_enqueue(camera,f);
                f = nf;
            }else{
                break;
            }
        }
        return FirewireFrame(f);
    }else if(wait){
        return GetNext(true);
    }
    return FirewireFrame(0);
}

void FirewireVideo::PutFrame(FirewireFrame& f)
{
    if( f.frame )
    {
        dc1394_capture_enqueue(camera,f.frame);
        f.frame = 0;
    }
}

float FirewireVideo::GetShutterTime() const
{
    float shutter;
    err = dc1394_feature_get_absolute_value(camera,DC1394_FEATURE_SHUTTER,&shutter);
    if( err != DC1394_SUCCESS )
        throw VideoException("Failed to read shutter");

    return shutter;

}

void FirewireVideo::SetAutoShutterTime(){

	dc1394error_t err = dc1394_feature_set_mode(camera, DC1394_FEATURE_SHUTTER, DC1394_FEATURE_MODE_AUTO);
	if (err < 0) {
		throw VideoException("Could not set auto shutter mode");
	}
}

void FirewireVideo::SetShutterTime(int val){

	val = std::max(std::min(1077, val), 0);

	dc1394error_t err = dc1394_feature_set_mode(camera, DC1394_FEATURE_SHUTTER, DC1394_FEATURE_MODE_MANUAL);
	if (err < 0) {
		throw VideoException("Could not set manual shutter mode");
	}

	err = dc1394_feature_set_value(camera, DC1394_FEATURE_SHUTTER, val);
	if (err < 0) {
		throw VideoException("Could not set manual shutter value");
	}
}

float FirewireVideo::GetGamma() const
{
    float gamma;
    err = dc1394_feature_get_absolute_value(camera,DC1394_FEATURE_GAMMA,&gamma);
    if( err != DC1394_SUCCESS )
        throw VideoException("Failed to read gamma");
    return gamma;
}

void FirewireVideo::SetInternalTrigger() 
{
    dc1394error_t err = dc1394_external_trigger_set_power(camera, DC1394_OFF);
    if (err < 0) {
        throw VideoException("Could not set internal trigger mode");
    }
}

void FirewireVideo::SetExternalTrigger() 
{
    dc1394error_t err = dc1394_external_trigger_set_polarity(camera, DC1394_TRIGGER_ACTIVE_HIGH);
    if (err < 0) {
        throw VideoException("Could not set external trigger polarity");
    }

    err = dc1394_external_trigger_set_mode(camera, DC1394_TRIGGER_MODE_0);
    if (err < 0) {
        throw VideoException("Could not set external trigger mode");
    }

    err = dc1394_external_trigger_set_power(camera, DC1394_ON);
    if (err < 0) {
        throw VideoException("Could not set external trigger power");
    }
}


FirewireVideo::~FirewireVideo()
{
    Stop();

    // Close camera
    dc1394_video_set_transmission(camera, DC1394_OFF);
    dc1394_capture_stop(camera);
    //dc1394_camera_set_power(camera, DC1394_OFF);
    dc1394_camera_free(camera);
    dc1394_free (d);
}


int FirewireVideo::nearest_value(int value, int step, int min, int max) {

  int low, high;

  low=value-(value%step);
  high=value-(value%step)+step;
  if (low<min)
    low=min;
  if (high>max)
    high=max;

  if (abs(low-value)<abs(high-value))
    return low;
  else
    return high;
}

}

#endif
