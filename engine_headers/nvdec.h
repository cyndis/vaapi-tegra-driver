/* kate: replace-tabs false; indent-width 8; tab-width 8
 *
 * Copyright © 2021 NVIDIA Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef ENGINE_HEADERS_NVDEC_H
#define ENGINE_HEADERS_NVDEC_H

#include <cstdint>

#define NVDEC_UCLASS_INCR_SYNCPT					0x00
#define NVDEC_UCLASS_METHOD_OFFSET				        0x10
#define NVDEC_UCLASS_METHOD_DATA					0x11

#define NVC5B0_SET_APPLICATION_ID (0x00000200)
#define NVC5B0_SET_APPLICATION_ID_ID_MPEG12 (0x00000001)
#define NVC5B0_SET_APPLICATION_ID_ID_VC1 (0x00000002)
#define NVC5B0_SET_APPLICATION_ID_ID_H264 (0x00000003)
#define NVC5B0_SET_APPLICATION_ID_ID_MPEG4 (0x00000004)
#define NVC5B0_SET_APPLICATION_ID_ID_VP8 (0x00000005)
#define NVC5B0_SET_APPLICATION_ID_ID_HEVC (0x00000007)
#define NVC5B0_SET_APPLICATION_ID_ID_VP9 (0x00000009)
#define NVC5B0_SET_APPLICATION_ID_ID_HEVC_PARSER (0x0000000C)

#define NVC5B0_EXECUTE (0x00000300)
#define NVC5B0_EXECUTE_NOTIFY_DISABLE (0x00000000 << 0)
#define NVC5B0_EXECUTE_NOTIFY_ENABLE (0x00000001 << 0)
#define NVC5B0_EXECUTE_NOTIFY_ON_END (0x00000000 << 1)
#define NVC5B0_EXECUTE_NOTIFY_ON_BEGIN (0x00000001 << 1)
#define NVC5B0_EXECUTE_AWAKEN_DISABLE (0x00000000 << 8)
#define NVC5B0_EXECUTE_AWAKEN_ENABLE (0x00000001 << 8)

#define NVC5B0_SET_CONTROL_PARAMS (0x00000400)
#define NVC5B0_SET_CONTROL_PARAMS_CODEC_TYPE_MPEG1 (0x00000000 << 0)
#define NVC5B0_SET_CONTROL_PARAMS_CODEC_TYPE_MPEG2 (0x00000001 << 0)
#define NVC5B0_SET_CONTROL_PARAMS_CODEC_TYPE_VC1 (0x00000002 << 0)
#define NVC5B0_SET_CONTROL_PARAMS_CODEC_TYPE_H264 (0x00000003 << 0)
#define NVC5B0_SET_CONTROL_PARAMS_CODEC_TYPE_MPEG4 (0x00000004 << 0)
#define NVC5B0_SET_CONTROL_PARAMS_CODEC_TYPE_DIVX3 (0x00000004 << 0)
#define NVC5B0_SET_CONTROL_PARAMS_CODEC_TYPE_VP8 (0x00000005 << 0)
#define NVC5B0_SET_CONTROL_PARAMS_CODEC_TYPE_HEVC (0x00000007 << 0)
#define NVC5B0_SET_CONTROL_PARAMS_CODEC_TYPE_VP9 (0x00000009 << 0)
#define NVC5B0_SET_CONTROL_PARAMS_GPTIMER_ON (1 << 4)
#define NVC5B0_SET_CONTROL_PARAMS_RET_ERROR (1 << 5)
#define NVC5B0_SET_CONTROL_PARAMS_ERR_CONCEAL_ON (1 << 6)
#define NVC5B0_SET_CONTROL_PARAMS_ERROR_FRM_IDX(v) ((v) << 7)
#define NVC5B0_SET_CONTROL_PARAMS_MBTIMER_ON (1 << 13)
#define NVC5B0_SET_CONTROL_PARAMS_EC_INTRA_FRAME_USING_PSLC (1 << 14)
#define NVC5B0_SET_CONTROL_PARAMS_ALL_INTRA_FRAME (1 << 17)

#define NVC5B0_SET_DRV_PIC_SETUP_OFFSET (0x00000404)
#define NVC5B0_SET_IN_BUF_BASE_OFFSET (0x00000408)
#define NVC5B0_SET_PICTURE_INDEX (0x0000040C)
#define NVC5B0_SET_SLICE_OFFSETS_BUF_OFFSET (0x00000410)
#define NVC5B0_SET_PICTURE_LUMA_OFFSET0 (0x00000430)
#define NVC5B0_SET_PICTURE_CHROMA_OFFSET0 (0x00000474)
#define NVC5B0_SET_PICTURE_LUMA_OFFSET1                                         (0x00000434)
#define NVC5B0_SET_PICTURE_LUMA_OFFSET2                                         (0x00000438)
#define NVC5B0_SET_PICTURE_CHROMA_OFFSET1                                       (0x00000478)
#define NVC5B0_SET_PICTURE_CHROMA_OFFSET2                                       (0x0000047C)
#define NVC5B0_SET_HISTOGRAM_OFFSET                                             (0x00000420)
#define NVC5B0_SET_NVDEC_STATUS_OFFSET                                          (0x00000424)

typedef struct _nvdec_display_param_s {
    uint32_t enableTFOutput : 1;
    uint32_t VC1MapYFlag : 1;
    uint32_t MapYValue : 3;
    uint32_t VC1MapUVFlag : 1;
    uint32_t MapUVValue : 3;
    uint32_t OutStride : 8;
    uint32_t TilingFormat : 3;
    uint32_t OutputStructure : 1;
    uint32_t reserved0 : 11;
    int32_t OutputTop[2];
    int32_t OutputBottom[2];
    uint32_t enableHistogram : 1;
    uint32_t HistogramStartX : 12;
    uint32_t HistogramStartY : 12;
    uint32_t reserved1 : 7;
    uint32_t HistogramEndX : 12;
    uint32_t HistogramEndY : 12;
    uint32_t reserved2 : 8;
} nvdec_display_param_s;

typedef struct _nvdec_mpeg2_pic_s {
    uint32_t reserved0[13];
    uint8_t eos[16];
    uint8_t explicitEOSPresentFlag;
    uint8_t reserved1[3];
    uint32_t stream_len;
    uint32_t slice_count;
    uint32_t gptimer_timeout_value;

    // Fields from vld_mpeg2_seq_pic_info_s
    uint16_t FrameWidth; // actual frame width
    uint16_t FrameHeight; // actual frame height
    uint8_t picture_structure; // 0 => Reserved, 1 => Top field, 2 => Bottom field, 3 => Frame picture. Table 6-14.
    uint8_t picture_coding_type; // 0 => Forbidden, 1 => I, 2 => P, 3 => B, 4 => D (for MPEG-2). Table 6-12.
    uint8_t intra_dc_precision; // 0 => 8 bits, 1=> 9 bits, 2 => 10 bits, 3 => 11 bits. Table 6-13.
    int8_t frame_pred_frame_dct; // as in section 6.3.10
    int8_t concealment_motion_vectors; // as in section 6.3.10
    int8_t intra_vlc_format; // as in section 6.3.10
    uint8_t tileFormat : 2; // 0: TBL; 1: KBL;
    uint8_t gob_height : 3; // Set GOB height, 0: GOB_2, 1: GOB_4, 2: GOB_8, 3: GOB_16, 4: GOB_32
    uint8_t reserverd_surface_format : 3;

    int8_t reserved2; // always 0
    int8_t f_code[4]; // as in section 6.3.10

    uint16_t PicWidthInMbs;
    uint16_t FrameHeightInMbs;
    uint32_t pitch_luma;
    uint32_t pitch_chroma;
    uint32_t luma_top_offset;
    uint32_t luma_bot_offset;
    uint32_t luma_frame_offset;
    uint32_t chroma_top_offset;
    uint32_t chroma_bot_offset;
    uint32_t chroma_frame_offset;
    uint32_t HistBufferSize;
    uint16_t output_memory_layout;
    uint16_t alternate_scan;
    uint16_t secondfield;
    uint16_t rounding_type;
    uint32_t MbInfoSizeInBytes;
    uint32_t q_scale_type;
    uint32_t top_field_first;
    uint32_t full_pel_fwd_vector;
    uint32_t full_pel_bwd_vector;
    uint8_t quant_mat_8x8intra[64];
    uint8_t quant_mat_8x8nonintra[64];
    uint32_t ref_memory_layout[2]; //0:for fwd; 1:for bwd

    nvdec_display_param_s displayPara;
    uint32_t reserved3[3];
} nvdec_mpeg2_pic_s;

typedef struct _nvdec_status_hevc_s
{
    uint32_t frame_status_intra_cnt;    //Intra block counter, in unit of 8x8 block, IPCM block included
    uint32_t frame_status_inter_cnt;    //Inter block counter, in unit of 8x8 block, SKIP block included
    uint32_t frame_status_skip_cnt;     //Skip block counter, in unit of 8x8 block, blocks having NO/ZERO texture/coeff data
    uint32_t frame_status_fwd_mvx_cnt;  //ABS sum of forward  MVx, one 14bit MVx(integer) per 8x8 block
    uint32_t frame_status_fwd_mvy_cnt;  //ABS sum of forward  MVy, one 14bit MVy(integer) per 8x8 block
    uint32_t frame_status_bwd_mvx_cnt;  //ABS sum of backward MVx, one 14bit MVx(integer) per 8x8 block
    uint32_t frame_status_bwd_mvy_cnt;  //ABS sum of backward MVy, one 14bit MVy(integer) per 8x8 block
    uint32_t error_ctb_pos;             //[15:0] error ctb   position in Y direction, [31:16] error ctb   position in X direction
    uint32_t error_slice_pos;           //[15:0] error slice position in Y direction, [31:16] error slice position in X direction
} nvdec_status_hevc_s;

typedef struct _nvdec_status_vp9_s
{
    uint32_t frame_status_intra_cnt;    //Intra block counter, in unit of 8x8 block, IPCM block included
    uint32_t frame_status_inter_cnt;    //Inter block counter, in unit of 8x8 block, SKIP block included
    uint32_t frame_status_skip_cnt;     //Skip block counter, in unit of 8x8 block, blocks having NO/ZERO texture/coeff data
    uint32_t frame_status_fwd_mvx_cnt;  //ABS sum of forward  MVx, one 14bit MVx(integer) per 8x8 block
    uint32_t frame_status_fwd_mvy_cnt;  //ABS sum of forward  MVy, one 14bit MVy(integer) per 8x8 block
    uint32_t frame_status_bwd_mvx_cnt;  //ABS sum of backward MVx, one 14bit MVx(integer) per 8x8 block
    uint32_t frame_status_bwd_mvy_cnt;  //ABS sum of backward MVy, one 14bit MVy(integer) per 8x8 block
    uint32_t error_ctb_pos;             //[15:0] error ctb   position in Y direction, [31:16] error ctb   position in X direction
    uint32_t error_slice_pos;           //[15:0] error slice position in Y direction, [31:16] error slice position in X direction
} nvdec_status_vp9_s;

typedef struct _nvdec_status_s
{
    uint32_t    mbs_correctly_decoded;          // total numers of correctly decoded macroblocks
    uint32_t    mbs_in_error;                   // number of error macroblocks.
    uint32_t    reserved;
    uint32_t    error_status;                   // report error if any
    union
    {
        nvdec_status_hevc_s hevc;
        nvdec_status_vp9_s vp9;
    };
    uint32_t    slice_header_error_code;        // report error in slice header

} nvdec_status_s;

#endif