#ifndef OPUS_H
#define OPUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct OpusDecoder OpusDecoder;
typedef struct OpusEncoder OpusEncoder;

// Opus error codes
#define OPUS_OK 0
#define OPUS_BAD_ARG -1
#define OPUS_BUFFER_TOO_SMALL -2
#define OPUS_INTERNAL_ERROR -3
#define OPUS_INVALID_PACKET -4
#define OPUS_UNIMPLEMENTED -5
#define OPUS_INVALID_STATE -6
#define OPUS_ALLOC_FAIL -7

// Opus application types
#define OPUS_APPLICATION_VOIP 2048
#define OPUS_APPLICATION_AUDIO 2049
#define OPUS_APPLICATION_RESTRICTED_LOWDELAY 2051

// Opus signal types
#define OPUS_SIGNAL_AUTO -1000
#define OPUS_SIGNAL_VOICE -3001
#define OPUS_SIGNAL_MUSIC -3002

// Opus bandwidth types
#define OPUS_BANDWIDTH_NARROWBAND 1101
#define OPUS_BANDWIDTH_MEDIUMBAND 1102
#define OPUS_BANDWIDTH_WIDEBAND 1103
#define OPUS_BANDWIDTH_SUPERWIDEBAND 1104
#define OPUS_BANDWIDTH_FULLBAND 1105

// Opus frame sizes
#define OPUS_FRAMESIZE_2_5_MS 101
#define OPUS_FRAMESIZE_5_MS 102
#define OPUS_FRAMESIZE_10_MS 103
#define OPUS_FRAMESIZE_20_MS 104
#define OPUS_FRAMESIZE_40_MS 105
#define OPUS_FRAMESIZE_60_MS 106
#define OPUS_FRAMESIZE_80_MS 107
#define OPUS_FRAMESIZE_100_MS 108
#define OPUS_FRAMESIZE_120_MS 109

// Opus decoder functions
OpusDecoder* opus_decoder_create(int Fs, int channels, int *error);
int opus_decoder_init(OpusDecoder *st, int Fs, int channels, int *error);
int opus_decode(OpusDecoder *st, const unsigned char *data, int len, float *pcm, int frame_size, int decode_fec);
int opus_decode_float(OpusDecoder *st, const unsigned char *data, int len, float *pcm, int frame_size, int decode_fec);
void opus_decoder_destroy(OpusDecoder *st);

// Opus encoder functions
OpusEncoder* opus_encoder_create(int Fs, int channels, int application, int *error);
int opus_encoder_init(OpusEncoder *st, int Fs, int channels, int application, int *error);
int opus_encode(OpusEncoder *st, const float *pcm, int frame_size, unsigned char *data, int max_data_bytes);
int opus_encode_float(OpusEncoder *st, const float *pcm, int frame_size, unsigned char *data, int max_data_bytes);
void opus_encoder_destroy(OpusEncoder *st);

// Opus control functions
int opus_decoder_ctl(OpusDecoder *st, int request, ...);
int opus_encoder_ctl(OpusEncoder *st, int request, ...);

// Opus control requests
#define OPUS_SET_BITRATE_REQUEST 4002
#define OPUS_GET_BITRATE_REQUEST 4003
#define OPUS_SET_BANDWIDTH_REQUEST 4008
#define OPUS_GET_BANDWIDTH_REQUEST 4009
#define OPUS_SET_SIGNAL_REQUEST 4020
#define OPUS_GET_SIGNAL_REQUEST 4021
#define OPUS_SET_APPLICATION_REQUEST 4000
#define OPUS_GET_APPLICATION_REQUEST 4001
#define OPUS_SET_COMPLEXITY_REQUEST 4010
#define OPUS_GET_COMPLEXITY_REQUEST 4011
#define OPUS_SET_INBAND_FEC_REQUEST 4012
#define OPUS_GET_INBAND_FEC_REQUEST 4013
#define OPUS_SET_PACKET_LOSS_PERC_REQUEST 4014
#define OPUS_GET_PACKET_LOSS_PERC_REQUEST 4015
#define OPUS_SET_DTX_REQUEST 4016
#define OPUS_GET_DTX_REQUEST 4017
#define OPUS_SET_VBR_REQUEST 4006
#define OPUS_GET_VBR_REQUEST 4007
#define OPUS_SET_VBR_CONSTRAINT_REQUEST 4022
#define OPUS_GET_VBR_CONSTRAINT_REQUEST 4023
#define OPUS_SET_MAX_BANDWIDTH_REQUEST 4018
#define OPUS_GET_MAX_BANDWIDTH_REQUEST 4019
#define OPUS_SET_LSB_DEPTH_REQUEST 4036
#define OPUS_GET_LSB_DEPTH_REQUEST 4037
#define OPUS_SET_EXPERT_FRAME_DURATION_REQUEST 4024
#define OPUS_GET_EXPERT_FRAME_DURATION_REQUEST 4025
#define OPUS_SET_PREDICTION_DISABLED_REQUEST 4026
#define OPUS_GET_PREDICTION_DISABLED_REQUEST 4027
#define OPUS_SET_PHASE_INVERSION_DISABLED_REQUEST 4028
#define OPUS_GET_PHASE_INVERSION_DISABLED_REQUEST 4029

// Additional Opus definitions
#define OPUS_AUTO -1000
#define OPUS_SET_BITRATE OPUS_SET_BITRATE_REQUEST
#define OPUS_SET_VBR OPUS_SET_VBR_REQUEST
#define OPUS_SET_VBR_CONSTRAINT OPUS_SET_VBR_CONSTRAINT_REQUEST
#define OPUS_SET_COMPLEXITY OPUS_SET_COMPLEXITY_REQUEST
#define OPUS_SET_INBAND_FEC OPUS_SET_INBAND_FEC_REQUEST
#define OPUS_SET_DTX OPUS_SET_DTX_REQUEST
#define OPUS_SET_SIGNAL OPUS_SET_SIGNAL_REQUEST
#define OPUS_SET_PACKET_LOSS_PERC OPUS_SET_PACKET_LOSS_PERC_REQUEST
#define OPUS_SET_MAX_BANDWIDTH OPUS_SET_MAX_BANDWIDTH_REQUEST

// Opus types
typedef int16_t opus_int16;
typedef int32_t opus_int32;

// Opus informational functions
const char* opus_strerror(int error);
const char* opus_get_version_string(void);

#ifdef __cplusplus
}
#endif

#endif // OPUS_H
