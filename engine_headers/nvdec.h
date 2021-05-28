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
#define NVC5B0_SET_COLOC_DATA_OFFSET                                            (0x00000414)
#define NVC5B0_SET_HISTORY_OFFSET                                               (0x00000418)
#define NVC5B0_H264_SET_MBHIST_BUF_OFFSET                                       (0x00000500)

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

// H.264
typedef struct _nvdec_dpb_entry_s  // 16 bytes
{
    uint32_t index          : 7;    // uncompressed frame buffer index
    uint32_t col_idx        : 5;    // index of associated co-located motion data buffer
    uint32_t state          : 2;    // bit1(state)=1: top field used for reference, bit1(state)=1: bottom field used for reference
    uint32_t is_long_term   : 1;    // 0=short-term, 1=long-term
    uint32_t not_existing   : 1;    // 1=marked as non-existing
    uint32_t is_field       : 1;    // set if unpaired field or complementary field pair
    uint32_t top_field_marking : 4;
    uint32_t bottom_field_marking : 4;
    uint32_t output_memory_layout : 1;  // Set according to picture level output NV12/NV24 setting.
    uint32_t reserved       : 6;
    uint32_t FieldOrderCnt[2];      // : 2*32 [top/bottom]
    uint32_t FrameIdx;                       // : 16   short-term: FrameNum (16 bits), long-term: LongTermFrameIdx (4 bits)
} nvdec_dpb_entry_s;

typedef struct _nvdec_h264_pic_s
{
    uint32_t reserved0[13];
    uint8_t  eos[16];
    uint8_t  explicitEOSPresentFlag;
    uint8_t  hint_dump_en; //enable COLOMV surface dump for all frames, which includes hints of "MV/REFIDX/QP/CBP/MBPART/MBTYPE", nvbug: 200212874
    uint8_t  reserved1[2];
    uint32_t stream_len;
    uint32_t slice_count;
    uint32_t mbhist_buffer_size;     // to pass buffer size of MBHIST_BUFFER

    // Driver may or may not use based upon need.
    // If 0 then default value of 1<<27 = 298ms @ 450MHz will be used in ucode.
    // Driver can send this value based upon resolution using the formula:
    // gptimer_timeout_value = 3 * (cycles required for one frame)
    uint32_t gptimer_timeout_value;

    // Fields from msvld_h264_seq_s
    int32_t log2_max_pic_order_cnt_lsb_minus4;
    int32_t delta_pic_order_always_zero_flag;
    int32_t frame_mbs_only_flag;
    uint32_t PicWidthInMbs;
    uint32_t FrameHeightInMbs;

    uint32_t tileFormat                 : 2 ;   // 0: TBL; 1: KBL;
    uint32_t gob_height                 : 3 ;   // Set GOB height, 0: GOB_2, 1: GOB_4, 2: GOB_8, 3: GOB_16, 4: GOB_32 (NVDEC3 onwards)
    uint32_t reserverd_surface_format   : 27;

    // Fields from msvld_h264_pic_s
    uint32_t entropy_coding_mode_flag;
    int32_t pic_order_present_flag;
    int32_t num_ref_idx_l0_active_minus1;
    int32_t num_ref_idx_l1_active_minus1;
    int32_t deblocking_filter_control_present_flag;
    int32_t redundant_pic_cnt_present_flag;
    uint32_t transform_8x8_mode_flag;

    // Fields from mspdec_h264_picture_setup_s
    uint32_t pitch_luma;                    // Luma pitch
    uint32_t pitch_chroma;                  // chroma pitch

    uint32_t luma_top_offset;               // offset of luma top field in units of 256
    uint32_t luma_bot_offset;               // offset of luma bottom field in units of 256
    uint32_t luma_frame_offset;             // offset of luma frame in units of 256
    uint32_t chroma_top_offset;             // offset of chroma top field in units of 256
    uint32_t chroma_bot_offset;             // offset of chroma bottom field in units of 256
    uint32_t chroma_frame_offset;           // offset of chroma frame in units of 256
    uint32_t HistBufferSize;                // in units of 256

    uint32_t MbaffFrameFlag           : 1;  //
    uint32_t direct_8x8_inference_flag: 1;  //
    uint32_t weighted_pred_flag       : 1;  //
    uint32_t constrained_intra_pred_flag:1; //
    uint32_t ref_pic_flag             : 1;  // reference picture (nal_ref_idc != 0)
    uint32_t field_pic_flag           : 1;  //
    uint32_t bottom_field_flag        : 1;  //
    uint32_t second_field             : 1;  // second field of complementary reference field
    uint32_t log2_max_frame_num_minus4: 4;  //  (0..12)
    uint32_t chroma_format_idc        : 2;  //
    uint32_t pic_order_cnt_type       : 2;  //  (0..2)
    int32_t pic_init_qp_minus26               : 6;  // : 6 (-26..+25)
    int32_t chroma_qp_index_offset            : 5;  // : 5 (-12..+12)
    int32_t second_chroma_qp_index_offset     : 5;  // : 5 (-12..+12)

    uint32_t weighted_bipred_idc      : 2;  // : 2 (0..2)
    uint32_t CurrPicIdx               : 7;  // : 7  uncompressed frame buffer index
    uint32_t CurrColIdx               : 5;  // : 5  index of associated co-located motion data buffer
    uint32_t frame_num                : 16; //
    uint32_t frame_surfaces           : 1;  // frame surfaces flag
    uint32_t output_memory_layout     : 1;  // 0: NV12; 1:NV24. Field pair must use the same setting.

    int32_t CurrFieldOrderCnt[2];                   // : 32 [Top_Bottom], [0]=TopFieldOrderCnt, [1]=BottomFieldOrderCnt
    nvdec_dpb_entry_s dpb[16];
    uint8_t  WeightScale[6][4][4];         // : 6*4*4*8 in raster scan order (not zig-zag order)
    uint8_t  WeightScale8x8[2][8][8];      // : 2*8*8*8 in raster scan order (not zig-zag order)

    // mvc setup info, must be zero if not mvc
    uint8_t num_inter_view_refs_lX[2];         // number of inter-view references
    int8_t reserved2[14];                               // reserved for alignment
    int8_t inter_view_refidx_lX[2][16];         // DPB indices (must also be marked as long-term)

    // lossless decode (At the time of writing this manual, x264 and JM encoders, differ in Intra_8x8 reference sample filtering)
    uint32_t lossless_ipred8x8_filter_enable        : 1;       // = 0, skips Intra_8x8 reference sample filtering, for vertical and horizontal predictions (x264 encoded streams); = 1, filter Intra_8x8 reference samples (JM encoded streams)
    uint32_t qpprime_y_zero_transform_bypass_flag   : 1;       // determines the transform bypass mode
    uint32_t reserved3                              : 30;      // kept for alignment; may be used for other parameters

    nvdec_display_param_s displayPara;
    uint32_t reserved4[3];

} nvdec_h264_pic_s;

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