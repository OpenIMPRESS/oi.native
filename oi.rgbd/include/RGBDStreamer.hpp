/*
This file is part of the OpenIMPRESS project.

OpenIMPRESS is free software: you can redistribute it and/or modify
it under the terms of the Lesser GNU Lesser General Public License as published
by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

OpenIMPRESS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with OpenIMPRESS. If not, see <https://www.gnu.org/licenses/>.
*/
#pragma once

#include "OICore.hpp"
#include "UDPConnector.hpp"
#include "OIHeaders.hpp"
#include <turbojpeg.h>

namespace oi { namespace core { namespace rgbd {
    class RGBDStreamer;
    
    class RGBDDeviceInterface {
    public:
        //virtual int HandleData(oi::core::network::DataContainer * dc) = 0;
        
        //virtual int HandleData(oi::core::... * dc) = 0;
        virtual int Cycle(RGBDStreamer * streamer) = 0;
        virtual int OpenDevice() = 0; // ...
        virtual int CloseDevice() = 0;
        
        virtual int frame_width() = 0;
        virtual int frame_height() = 0;
        virtual int raw_depth_stride() = 0;
        virtual int raw_color_stride() = 0;
        
        virtual float device_cx() = 0;
        virtual float device_cy() = 0;
        virtual float device_fx() = 0;
        virtual float device_fy() = 0;
        virtual float device_depth_scale() = 0;
        virtual std::string device_guid() = 0;
        virtual TJPF color_pixel_format() = 0;
        virtual bool supports_audio() = 0;
        virtual bool supports_body() = 0;
        virtual bool supports_bidx() = 0;
        virtual bool supports_hd() = 0;
    };
    
    class RGBDStreamerConfig {
    public:
        RGBDStreamerConfig(int argc, char *argv[]);
        RGBDStreamerConfig();
        void Parse(int argc, char *argv[]);
        bool useMatchMaking = false;
        std::string socketID = "kinect1";
        std::string mmHost = "";
        int mmPort = 6312;
        int listenPort = 5066;
        std::vector<std::pair<std::string, std::string>> default_endpoints;
        std::string deviceSerial = ""; //
        std::string pipeline = "opengl";
        float maxDepth = 8.0f;
    };

    class RGBDStreamer {
    public:
        RGBDStreamer(RGBDDeviceInterface& device, RGBDStreamerConfig streamer_cfg, asio::io_service& io_service);
        int UpdateStreamConfig(CONFIG_STRUCT cfg);
        
        int QueueAudioFrame(uint32_t sequence, float * samples, size_t n_samples, uint16_t freq, uint16_t channels, std::chrono::milliseconds timestamp);
        int QueueBodyFrame(BODY_STRUCT * bodies, uint16_t n_bodies, std::chrono::milliseconds timestamp);
        int QueueRGBDFrame(uint64_t sequence, uint8_t * rgbdata, uint8_t * depthdata, std::chrono::milliseconds timestamp);
        int QueueRGBDFrame(uint64_t sequence, uint8_t * rgbdata, uint16_t * depthdata, std::chrono::milliseconds timestamp);
        int QueueRGBDFrame(uint64_t sequence, uint8_t * rgbdata, uint8_t * depth_any, uint16_t * depth_ushort, std::chrono::milliseconds timestamp);
        
        int QueueHDFrame(unsigned char * rgbdata, int width, int height, TJPF pix_fmt, std::chrono::milliseconds timestamp);
        int QueueBodyIndexFrame(unsigned char * bidata, int width, int height, TJPF pix_fmt, std::chrono::milliseconds timestamp);

        int SendConfig();
        
        RGBDStreamerConfig _rgbdstreamer_config;
        const int JPEG_QUALITY = 40;
        const int MAX_LINES_PER_MESSAGE = 5;
        
        uint32_t _audio_samples_counter;
    private:
        int HandleStream();
        std::thread * handleStreamThread;
        
        oi::core::network::UDPConnector * _udpc;
        oi::core::worker::ObjectPool<oi::core::network::UDPMessageObject> * _bufferPool;
        RGBDDeviceInterface * _device;
        std::chrono::milliseconds _prev_frame;
        uint32_t fps_counter;
        CONFIG_STRUCT _stream_config;
        
        std::chrono::milliseconds _config_send_interval = (std::chrono::milliseconds) 5000;
        std::chrono::milliseconds _last_config_sent;
    };

} } }
