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

namespace oi { namespace core {

    typedef struct {
        uint8_t packageFamily;
        uint8_t packageType;
    } PackageType;
    
    typedef struct {
        PackageType packageType;
        uint8_t channel;
    } ChannelId;
    
	typedef std::pair<uint8_t, uint8_t> MsgType;
    
    const int MAX_UDP_PACKET_SIZE = 65500;
    
    const unsigned char RGBD_DATA  = 1 << 0;
    const unsigned char AUDIO_DATA = 1 << 1;
    const unsigned char LIVE_DATA  = 1 << 2;
    const unsigned char BODY_DATA  = 1 << 3;
    const unsigned char HD_DATA    = 1 << 4;
    const unsigned char BIDX_DATA  = 1 << 5;


    enum OI_LEGACY_MSG_FAMILY {
        OI_LEGACY_MSG_FAMILY_MM=0x64,
        OI_LEGACY_MSG_FAMILY_DATA=0x14,
        
        OI_LEGACY_MSG_FAMILY_RGBD=0x02,
        OI_LEGACY_MSG_FAMILY_RGBD_CMD=0x12,
        
        OI_LEGACY_MSG_FAMILY_MOCAP=0x03,
        OI_LEGACY_MSG_FAMILY_AUDIO=0x04,
        OI_LEGACY_MSG_FAMILY_XR=0x10
    };
    
    enum OI_MSG_TYPE_RGBD {
        OI_MSG_TYPE_RGBD_CONFIG=0x01,
        OI_MSG_TYPE_RGBD_DEPTH=0x11,
        OI_MSG_TYPE_RGBD_DEPTH_BLOCK=0x12,
        OI_MSG_TYPE_RGBD_COLOR=0x21,
        OI_MSG_TYPE_RGBD_COLOR_BLOCK=0x22,
        OI_MSG_TYPE_RGBD_CTRL_REQUEST=0x31,
        OI_MSG_TYPE_RGBD_CTRL_REQUEST_JSON=0x32, // TODO: replace with content format header...
        OI_MSG_TYPE_RGBD_CTRL_RESPONSE=0x41,
        OI_MSG_TYPE_RGBD_CTRL_RESPONSE_JSON=0x42, // TODO: replace with content format header...
        OI_MSG_TYPE_RGBD_BODY_ID_TEXTURE=0x51,
        OI_MSG_TYPE_RGBD_BODY_ID_TEXTURE_BLOCK=0x52
    };
    
    enum OI_MSG_TYPE_MOCAP {
        OI_MSG_TYPE_MOCAP_CONFIG=0x01,
        OI_MSG_TYPE_MOCAP_BODY_FRAME_KINECTV1=0x12,
        OI_MSG_TYPE_MOCAP_BODY_FRAME_KINECTV2=0x13,
        
        // XSens, Vicon, OptiTrack, ...
        OI_MSG_TYPE_HUMAN_BODY_FRAME=0x21,
        OI_MSG_TYPE_RIGIDBODY_FRAME=0x22,
        
        OI_MSG_TYPE_MOCAP_LEAP_MOTION_CONFIG=0x41,
        OI_MSG_TYPE_MOCAP_LEAP_MOTION_FRAME=0x42
    };
    
    enum OI_MSG_TYPE_AUDIO {
        OI_MSG_TYPE_AUDIO_CONFIG=0x01,
        OI_MSG_TYPE_AUDIO_DEFAULT_FRAME=0x11
    };
    
    enum OI_MSG_TYPE_XR {
        OI_MSG_TYPE_XR_TRANSFORM=0x11,
        OI_MSG_TYPE_XR_LINE_DRAWING=0x21,
        OI_MSG_TYPE_XR_MESH=0x51,
        OI_MSG_TYPE_XR_SPATIAL_MESH=0x55
    };
    
    // MM header is just a single byte (0x64) in front of json...
    
    typedef struct {
        uint8_t packageFamily=OI_LEGACY_MSG_FAMILY_DATA;
        uint8_t packageType;
        uint16_t unused2;
        uint32_t sequence;
        uint32_t partsTotal;
        uint32_t currentPart;
        uint64_t timestamp;
    } OI_LEGACY_HEADER;
    
    // TODO : implement this as actual flags...
    const uint8_t MSG_FLAG_DEFAULT = 0; // 0x1
    const uint8_t MSG_FLAG_HAS_MULTIPART = 1; // 0x1
    // 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80

    
    
    typedef struct {
        uint8_t  msg_family;   //
        uint8_t  msg_type;     //
        uint8_t  msg_format;   // in body, is it binary, json, raw string, protobuf, ...
        uint8_t  msg_flags;    // ~ has multipart header, has source data, ...
        uint32_t msg_sequence; // ...
        uint32_t body_start;   // where does content start
        uint32_t body_length;  // how long is the content
        uint32_t source_id;    // ...
        uint64_t send_time;    // When was this packet sent
    } OI_MSG_HEADER; // 28 bytes
    
    typedef struct {
        uint32_t total_size;      // Total size of body in all parts
        uint16_t part;            // ...this is part X...
        uint16_t n_parts;         // ...of Y.
        uint16_t multipart_flags; // ...
        uint16_t unused_1;        // ...for alignment...
        uint32_t memory_location; // ...
    } OI_MULTIPART_HEADER; // 16 bytes
    
    
     typedef struct {
         OI_MSG_HEADER header;   // msg_family=0x02, msg_type=0x11; ..
         uint16_t      startRow; //
         uint16_t      endRow;   //
         uint8_t*      data;     //
         // TODO: this should contain more information on how the data is formatted...
         // cound be blocks instead of lines, could contain stride, ...
     } OI_RGBD_DEPTH; // ...
    
    typedef struct {
        OI_MSG_HEADER header;   // msg_family=0x02, msg_type=0x21; ...
        
        // encoding etc.
        // ...
    } OI_RGBD_COLOR; // ...
    
    
    enum OI_MSG_FAMILY {
        OI_MSG_FAMILY_MM = 0x01,
        OI_MSG_FAMILY_RGBD = 0x02,
        OI_MSG_FAMILY_MOCAP = 0x03,
        OI_MSG_FAMILY_AUDIO = 0x04,
        OI_MSG_FAMILY_XR = 0x10,
        OI_MSG_FAMILY_SAIBA = 0x60
    };
    
    enum OI_CLIENT_ROLE {
        OI_CLIENT_ROLE_PRODUCE = 1,
        OI_CLIENT_ROLE_CONSUME = 2,
        OI_CLIENT_ROLE_PRODUCE_CONSUME = 3
    };
    
    enum OI_MESSAGE_FORMAT {
        OI_MESSAGE_FORMAT_BINARY = 1,
        OI_MESSAGE_FORMAT_PROTOBUF = 2,
        OI_MESSAGE_FORMAT_STRING = 3,
        OI_MESSAGE_FORMAT_JSON = 4
    };
    
    
    typedef struct { // DO NOT REARRANGE THESE!
        OI_LEGACY_HEADER header;
        uint16_t   unused1 = 0x00;    // 01
        uint16_t  frequency;        // 02-03
        uint16_t  channels;         // 04-05
        uint16_t  samples;          // 06-07
        //uint64_t  timestamp;        // 08-15
    } AUDIO_HEADER_STRUCT;
    
    
    typedef struct { // DO NOT REARRANGE THESE!
        //uint8_t  msgType = 0x03;   // 00 // 0x03 = Depth; 0x04 = Color; // 0x33 = BIDX
        //uint8_t  deviceID = 0x00;  // 01
        OI_LEGACY_HEADER header;
        uint16_t unused1 = 0x00;
        uint16_t delta_t = 0;      // 02-03 Delta milliseconds
        uint16_t startRow = 0;     // 04-05
        uint16_t endRow = 0;       // 06-07
        //uint64_t timestamp;        // 08-15
    } RGBD_HEADER_STRUCT;
    
    typedef struct {
        OI_LEGACY_HEADER header;   // ...
        uint32_t size;             // ...
        uint8_t * data;
    } IMG_STRUCT;
    
    typedef struct {
        OI_LEGACY_HEADER header;
        uint16_t  unused1 = 0x00;
		uint16_t  n_bodies;
        uint32_t  unused2 = 0x00;
        //uint64_t timestamp;
        // followed by BODY_STRUCT * body_data
    } BODY_HEADER_STRUCT;
    
    typedef struct {
        uint32_t tracking_id;        // 000-003
        uint8_t left_hand_state;     // 004
        uint8_t right_hand_state;    // 005
        uint8_t unused1;             // 006
        uint8_t lean_tracking_state; // 007
        float leanX;                       // 008-011
        float leanY;                       // 012-015
        float joints_position[3 * 25];     // 016-315 (25*3*4)
        uint8_t unused2;             // 316
        uint8_t unused3;             // 317
        uint8_t unused4;             // 318
        uint8_t joints_tracked[25];  // 319-343
    } BODY_STRUCT;
    
    typedef struct {
        OI_LEGACY_HEADER header;
        uint8_t  deviceID = 0x00;   // 00
        uint8_t  deviceType = 0x02; // 01
        uint8_t  dataFlags = 0x00;  // 02
        uint8_t  unused1 = 0x00;    // 03 => for consistent alignment
        uint16_t frameWidth = 0;   // 04-05
        uint16_t frameHeight = 0;  // 06-07
        uint16_t maxLines = 0;     // 08-09
        uint16_t unused2 = 0;      // 10-11 => for consistent alignment
        float Cx = 0.0f;           // 12-15
        float Cy = 0.0f;           // 16-19
        float Fx = 0.0f;           // 20-23
        float Fy = 0.0f;           // 24-27
        float DepthScale = 0.0f;   // 28-31
        float Px = 0.0f;           // 32-35
        float Py = 0.0f;           // 36-39
        float Pz = 0.0f;           // 40-43
        float Qx = 0.0f;           // 44-47
        float Qy = 0.0f;           // 48-51
        float Qz = 0.0f;           // 52-55
        float Qw = 1.0f;           // 56-59
        uint8_t unused3 = 0x00;    // 60
        uint8_t unused4 = 0x00;    // 61
        uint8_t unused5 = 0x00;    // 62
        char guid[33]     = "00000000000000000000000000000000"; // 63-95
        uint8_t unused6 = 0x00;    // 96
        uint8_t unused7 = 0x00;    // 97
        uint8_t unused8 = 0x00;    // 98
        char filename[33] = "00000000000000000000000000000000"; // 99-131
    } CONFIG_STRUCT;
    
    
    class OIHeaderHelper {
    public:
        OI_MSG_HEADER * oi_msg_header;
        OI_MULTIPART_HEADER * oi_multipart_header;
        uint8_t * data;
        static OIHeaderHelper InitMsgHeader(uint8_t * buffer, bool is_multipart) {
            OIHeaderHelper result;
            result.oi_msg_header = (OI_MSG_HEADER *) &buffer[0];
            result.oi_msg_header->msg_family = 0;   //
            result.oi_msg_header->msg_type = 0;     //
            result.oi_msg_header->msg_flags = (uint8_t) 0;
            result.oi_msg_header->msg_format = (uint8_t) OI_MESSAGE_FORMAT_BINARY;
            result.oi_msg_header->msg_sequence = 0; // Total size of body in all parts
            result.oi_msg_header->body_start = sizeof(OI_MSG_HEADER);   // where does content start
            result.oi_msg_header->body_length = 0;  // how long is the content
            result.oi_msg_header->source_id = 0;    // ...
            result.oi_msg_header->send_time = 0;
            
            if (is_multipart) {
                result.oi_msg_header->msg_flags = MSG_FLAG_HAS_MULTIPART;    // ~ has multipart header, has source data, ...
                result.oi_msg_header->body_start = result.oi_msg_header->body_start + sizeof(OI_MULTIPART_HEADER);
                result.oi_multipart_header = (OI_MULTIPART_HEADER *) &buffer[sizeof(OI_MSG_HEADER)];
                result.oi_multipart_header->total_size = 0;
                result.oi_multipart_header->total_size = 0;
                result.oi_multipart_header->part = 1;
                result.oi_multipart_header->n_parts = 1;
                result.oi_multipart_header->multipart_flags = 0x00;
                result.oi_multipart_header->unused_1 = 0;
                result.oi_multipart_header->memory_location = 0;
            } else {
                result.oi_multipart_header = nullptr;
            }
            result.data = &buffer[result.oi_msg_header->body_start];
            return result;
        }
    };

	// TODO: add friendly name?
	struct OI_META_CHANNEL_HEADER {
		uint32_t channelIdx;
		uint8_t packageFamily;
		uint8_t packageType;
		uint8_t unused1;
		uint8_t unused2;
	};

	struct OI_META_ENTRY {
		uint32_t channelIdx;
		int64_t timeOffset;
		uint64_t data_start;
		uint64_t data_length;
	};

	struct OI_META_FILE_HEADER {
		uint64_t sessionTimestamp;
		uint32_t channelCount;
		uint32_t unused1; // version?
		OI_META_CHANNEL_HEADER * channelHeader;
		//OI_META_ENTRY * metaEntries;
	};
    
    /*
    // todo
    typedef struct {
        uint32_t source_device_name_length = 0;
        uint32_t source_socket_name_length = 0;
        uint32_t source_flags              = 0;
        char source_device_name[1024];
        char source_socket_name[1024];
        //
    } OI_SOURCE_SPECIFICATION; // ...
    */
} }
