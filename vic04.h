/* kate: replace-tabs false; indent-width 8; tab-width 8
 *
 * Copyright © 2016-2017 NVIDIA Corporation
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

#ifndef VIC04_H
#define VIC04_H

#define VIC_UCLASS_INCR_SYNCPT					0x00
#define VIC_UCLASS_METHOD_OFFSET				0x10
#define VIC_UCLASS_METHOD_DATA					0x11

#define NVB0B6_VIDEO_COMPOSITOR_EXECUTE				0x00000300
#define NVB0B6_VIDEO_COMPOSITOR_SET_SURFACE0_SLOT0_LUMA_OFFSET	0x0000400
#define NVB0B6_VIDEO_COMPOSITOR_SET_SURFACE0_SLOT0_CHROMA_U_OFFSET \
								0x00000404
#define NVB0B6_VIDEO_COMPOSITOR_SET_SURFACE0_SLOT0_CHROMA_V_OFFSET \
								0x00000408
#define NVB1B6_VIDEO_COMPOSITOR_SET_SURFACE0_SLOT0_LUMA_OFFSET	0x00001200
#define NVB1B6_VIDEO_COMPOSITOR_SET_SURFACE0_SLOT0_CHROMA_U_OFFSET \
								0x00001204
#define NVB1B6_VIDEO_COMPOSITOR_SET_SURFACE0_SLOT0_CHROMA_V_OFFSET \
								0x00001208
#define NVB0B6_VIDEO_COMPOSITOR_SET_CONTROL_PARAMS		0x00000704
#define NVB0B6_VIDEO_COMPOSITOR_SET_CONFIG_STRUCT_OFFSET	0x00000708
#define NVB0B6_VIDEO_COMPOSITOR_SET_FILTER_STRUCT_OFFSET	0x0000070C
#define NVB0B6_VIDEO_COMPOSITOR_SET_HIST_OFFSET			0x00000714
#define NVB0B6_VIDEO_COMPOSITOR_SET_OUTPUT_SURFACE_LUMA_OFFSET	0x00000720

#define VIC_BLK_KIND_PITCH					0
#define VIC_BLK_KIND_GENERIC_16Bx2				1

#define VIC_PIXEL_FORMAT_L8					1
#define VIC_PIXEL_FORMAT_R8					4
#define VIC_PIXEL_FORMAT_A8R8G8B8				32
#define VIC_PIXEL_FORMAT_Y8_U8V8_N420				67
#define VIC_PIXEL_FORMAT_Y8_V8U8_N420				68

#define VIC_CACHE_WIDTH_32Bx8					1
#define VIC_CACHE_WIDTH_64Bx4					2
#define VIC_CACHE_WIDTH_128Bx2					3

#define __int64 long long

typedef struct _SlotConfig {
    unsigned __int64 SlotEnable          : 1;    // 0
    unsigned __int64 DeNoise             : 1;    // 1
    unsigned __int64 AdvancedDenoise     : 1;    // 2
    unsigned __int64 CadenceDetect       : 1;    // 3
    unsigned __int64 MotionMap           : 1;    // 4
    unsigned __int64 MMapCombine         : 1;    // 5
    unsigned __int64 IsEven              : 1;    // 6
    unsigned __int64 ChromaEven          : 1;    // 7
    unsigned __int64 CurrentFieldEnable  : 1;    // 8
    unsigned __int64 PrevFieldEnable     : 1;    // 9
    unsigned __int64 NextFieldEnable     : 1;    // 10
    unsigned __int64 NextNrFieldEnable   : 1;    // 11
    unsigned __int64 CurMotionFieldEnable : 1;   // 12
    unsigned __int64 PrevMotionFieldEnable : 1;  // 13
    unsigned __int64 PpMotionFieldEnable : 1;    // 14
    unsigned __int64 CombMotionFieldEnable : 1;  // 15
    unsigned __int64 FrameFormat         : 4;    // 19..16
    unsigned __int64 FilterLengthY       : 2;    // 21..20
    unsigned __int64 FilterLengthX       : 2;    // 23..22
    unsigned __int64 Panoramic           : 12;   // 35..24
    unsigned __int64 reserved1           : 22;   // 57..36
    unsigned __int64 DetailFltClamp      : 6;    // 63..58
    unsigned __int64 FilterNoise         : 10;   // 73..64
    unsigned __int64 FilterDetail        : 10;   // 83..74
    unsigned __int64 ChromaNoise         : 10;   // 93..84
    unsigned __int64 ChromaDetail        : 10;   // 103..94
    unsigned __int64 DeinterlaceMode     : 4;    // 107..104
    unsigned __int64 MotionAccumWeight   : 3;    // 110..108
    unsigned __int64 NoiseIir            : 11;   // 121..111
    unsigned __int64 LightLevel          : 4;    // 125..122
    unsigned __int64 reserved4           : 2;    // 127..126
//----------------------------------------------------
    unsigned __int64 SoftClampLow        : 10;   // 9..0
    unsigned __int64 SoftClampHigh       : 10;   // 19..10
    unsigned __int64 reserved5           : 3;    // 22..20
    unsigned __int64 reserved6           : 9;    // 31..23
    unsigned __int64 PlanarAlpha         : 10;   // 41..32
    unsigned __int64 ConstantAlpha       : 1;    // 42
    unsigned __int64 StereoInterleave    : 3;    // 45..43
    unsigned __int64 ClipEnabled         : 1;    // 46
    unsigned __int64 ClearRectMask       : 8;    // 54..47
    unsigned __int64 DegammaMode         : 2;    // 56..55
    unsigned __int64 reserved7           : 1;    // 57
    unsigned __int64 DecompressEnable    : 1;    // 58
    unsigned __int64 reserved9           : 5;    // 63..59
    unsigned __int64 DecompressCtbCount  : 8;    // 71..64
    unsigned __int64 DecompressZbcColor  : 32;   // 103..72
    unsigned __int64 reserved12          : 24;   // 127..104
//----------------------------------------------------
    unsigned __int64 SourceRectLeft      : 30;   // 29..0
    unsigned __int64 reserved14          : 2;    // 31..30
    unsigned __int64 SourceRectRight     : 30;   // 61..32
    unsigned __int64 reserved15          : 2;    // 63..62
    unsigned __int64 SourceRectTop       : 30;   // 93..64
    unsigned __int64 reserved16          : 2;    // 95..94
    unsigned __int64 SourceRectBottom    : 30;   // 125..96
    unsigned __int64 reserved17          : 2;    // 127..126
//----------------------------------------------------
    unsigned __int64 DestRectLeft        : 14;   // 13..0
    unsigned __int64 reserved18          : 2;    // 15..14
    unsigned __int64 DestRectRight       : 14;   // 29..16
    unsigned __int64 reserved19          : 2;    // 31..30
    unsigned __int64 DestRectTop         : 14;   // 45..32
    unsigned __int64 reserved20          : 2;    // 47..46
    unsigned __int64 DestRectBottom      : 14;   // 61..48
    unsigned __int64 reserved21          : 2;    // 63..62
    unsigned __int64 reserved22          : 32;   // 95..64
    unsigned __int64 reserved23          : 32;   // 127..96
} SlotConfig;


typedef struct _SlotSurfaceConfig {
    unsigned __int64 SlotPixelFormat     : 7;    // 6..0
    unsigned __int64 SlotChromaLocHoriz  : 2;    // 8..7
    unsigned __int64 SlotChromaLocVert   : 2;    // 10..9
    unsigned __int64 SlotBlkKind         : 4;    // 14..11
    unsigned __int64 SlotBlkHeight       : 4;    // 18..15
    unsigned __int64 SlotCacheWidth      : 3;    // 21..19
    unsigned __int64 reserved0           : 10;   // 31..22
    unsigned __int64 SlotSurfaceWidth    : 14;   // 45..32
    unsigned __int64 SlotSurfaceHeight   : 14;   // 59..46
    unsigned __int64 reserved1           : 4;    // 63..60
    unsigned __int64 SlotLumaWidth       : 14;   // 77..64
    unsigned __int64 SlotLumaHeight      : 14;   // 91..78
    unsigned __int64 reserved2           : 4;    // 95..92
    unsigned __int64 SlotChromaWidth     : 14;   // 109..96
    unsigned __int64 SlotChromaHeight    : 14;   // 123..110
    unsigned __int64 reserved3           : 4;    // 127..124
} SlotSurfaceConfig;


typedef struct _LumaKeyStruct {
    unsigned __int64 luma_coeff0         : 20;   // 19..0
    unsigned __int64 luma_coeff1         : 20;   // 39..20
    unsigned __int64 luma_coeff2         : 20;   // 59..40
    unsigned __int64 luma_r_shift        : 4;    // 63..60
    unsigned __int64 luma_coeff3         : 20;   // 83..64
    unsigned __int64 LumaKeyLower        : 10;   // 93..84
    unsigned __int64 LumaKeyUpper        : 10;   // 103..94
    unsigned __int64 LumaKeyEnabled      : 1;    // 104
    unsigned __int64 reserved0           : 2;    // 106..105
    unsigned __int64 reserved1           : 21;   // 127..107
} LumaKeyStruct;


typedef struct _MatrixStruct {
    unsigned __int64 matrix_coeff00      : 20;   // 19..0
    unsigned __int64 matrix_coeff10      : 20;   // 39..20
    unsigned __int64 matrix_coeff20      : 20;   // 59..40
    unsigned __int64 matrix_r_shift      : 4;    // 63..60
    unsigned __int64 matrix_coeff01      : 20;   // 83..64
    unsigned __int64 matrix_coeff11      : 20;   // 103..84
    unsigned __int64 matrix_coeff21      : 20;   // 123..104
    unsigned __int64 reserved0           : 3;    // 126..124
    unsigned __int64 matrix_enable       : 1;    // 127
//----------------------------------------------------
    unsigned __int64 matrix_coeff02      : 20;   // 19..0
    unsigned __int64 matrix_coeff12      : 20;   // 39..20
    unsigned __int64 matrix_coeff22      : 20;   // 59..40
    unsigned __int64 reserved1           : 4;    // 63..60
    unsigned __int64 matrix_coeff03      : 20;   // 83..64
    unsigned __int64 matrix_coeff13      : 20;   // 103..84
    unsigned __int64 matrix_coeff23      : 20;   // 123..104
    unsigned __int64 reserved2           : 4;    // 127..124
} MatrixStruct;


typedef struct _ClearRectStruct {
    unsigned __int64 ClearRect0Left      : 14;   // 13..0
    unsigned __int64 reserved0           : 2;    // 15..14
    unsigned __int64 ClearRect0Right     : 14;   // 29..16
    unsigned __int64 reserved1           : 2;    // 31..30
    unsigned __int64 ClearRect0Top       : 14;   // 45..32
    unsigned __int64 reserved2           : 2;    // 47..46
    unsigned __int64 ClearRect0Bottom    : 14;   // 61..48
    unsigned __int64 reserved3           : 2;    // 63..62
    unsigned __int64 ClearRect1Left      : 14;   // 77..64
    unsigned __int64 reserved4           : 2;    // 79..78
    unsigned __int64 ClearRect1Right     : 14;   // 93..80
    unsigned __int64 reserved5           : 2;    // 95..94
    unsigned __int64 ClearRect1Top       : 14;   // 109..96
    unsigned __int64 reserved6           : 2;    // 111..110
    unsigned __int64 ClearRect1Bottom    : 14;   // 125..112
    unsigned __int64 reserved7           : 2;    // 127..126
} ClearRectStruct;


typedef struct _BlendingSlotStruct {
    unsigned __int64 AlphaK1             : 10;   // 9..0
    unsigned __int64 reserved0           : 6;    // 15..10
    unsigned __int64 AlphaK2             : 10;   // 25..16
    unsigned __int64 reserved1           : 6;    // 31..26
    unsigned __int64 SrcFactCMatchSelect : 3;    // 34..32
    unsigned __int64 reserved2           : 1;    // 35
    unsigned __int64 DstFactCMatchSelect : 3;    // 38..36
    unsigned __int64 reserved3           : 1;    // 39
    unsigned __int64 SrcFactAMatchSelect : 3;    // 42..40
    unsigned __int64 reserved4           : 1;    // 43
    unsigned __int64 DstFactAMatchSelect : 3;    // 46..44
    unsigned __int64 reserved5           : 1;    // 47
    unsigned __int64 reserved6           : 4;    // 51..48
    unsigned __int64 reserved7           : 4;    // 55..52
    unsigned __int64 reserved8           : 4;    // 59..56
    unsigned __int64 reserved9           : 4;    // 63..60
    unsigned __int64 reserved10          : 2;    // 65..64
    unsigned __int64 OverrideR           : 10;   // 75..66
    unsigned __int64 OverrideG           : 10;   // 85..76
    unsigned __int64 OverrideB           : 10;   // 95..86
    unsigned __int64 OverrideA           : 10;   // 105..96
    unsigned __int64 reserved11          : 2;    // 107..106
    unsigned __int64 UseOverrideR        : 1;    // 108
    unsigned __int64 UseOverrideG        : 1;    // 109
    unsigned __int64 UseOverrideB        : 1;    // 110
    unsigned __int64 UseOverrideA        : 1;    // 111
    unsigned __int64 MaskR               : 1;    // 112
    unsigned __int64 MaskG               : 1;    // 113
    unsigned __int64 MaskB               : 1;    // 114
    unsigned __int64 MaskA               : 1;    // 115
    unsigned __int64 reserved12          : 12;   // 127..116
} BlendingSlotStruct;


typedef struct _OutputConfig {
    unsigned __int64 AlphaFillMode       : 3;    // 2..0
    unsigned __int64 AlphaFillSlot       : 3;    // 5..3
    unsigned __int64 BackgroundAlpha     : 10;   // 15..6
    unsigned __int64 BackgroundR         : 10;   // 25..16
    unsigned __int64 BackgroundG         : 10;   // 35..26
    unsigned __int64 BackgroundB         : 10;   // 45..36
    unsigned __int64 RegammaMode         : 2;    // 47..46
    unsigned __int64 OutputFlipX         : 1;    // 48
    unsigned __int64 OutputFlipY         : 1;    // 49
    unsigned __int64 OutputTranspose     : 1;    // 50
    unsigned __int64 reserved1           : 1;    // 51
    unsigned __int64 reserved2           : 12;   // 63..52
    unsigned __int64 TargetRectLeft      : 14;   // 77..64
    unsigned __int64 reserved3           : 2;    // 79..78
    unsigned __int64 TargetRectRight     : 14;   // 93..80
    unsigned __int64 reserved4           : 2;    // 95..94
    unsigned __int64 TargetRectTop       : 14;   // 109..96
    unsigned __int64 reserved5           : 2;    // 111..110
    unsigned __int64 TargetRectBottom    : 14;   // 125..112
    unsigned __int64 reserved6           : 2;    // 127..126
} OutputConfig;


typedef struct _OutputSurfaceConfig {
    unsigned __int64 OutPixelFormat      : 7;    // 6..0
    unsigned __int64 OutChromaLocHoriz   : 2;    // 8..7
    unsigned __int64 OutChromaLocVert    : 2;    // 10..9
    unsigned __int64 OutBlkKind          : 4;    // 14..11
    unsigned __int64 OutBlkHeight        : 4;    // 18..15
    unsigned __int64 reserved0           : 3;    // 21..19
    unsigned __int64 reserved1           : 10;   // 31..22
    unsigned __int64 OutSurfaceWidth     : 14;   // 45..32
    unsigned __int64 OutSurfaceHeight    : 14;   // 59..46
    unsigned __int64 reserved2           : 4;    // 63..60
    unsigned __int64 OutLumaWidth        : 14;   // 77..64
    unsigned __int64 OutLumaHeight       : 14;   // 91..78
    unsigned __int64 reserved3           : 4;    // 95..92
    unsigned __int64 OutChromaWidth      : 14;   // 109..96
    unsigned __int64 OutChromaHeight     : 14;   // 123..110
    unsigned __int64 reserved4           : 4;    // 127..124
} OutputSurfaceConfig;


typedef struct _PipeConfig {
    unsigned __int64 DownsampleHoriz     : 11;   // 10..0
    unsigned __int64 reserved0           : 5;    // 15..11
    unsigned __int64 DownsampleVert      : 11;   // 26..16
    unsigned __int64 reserved1           : 5;    // 31..27
    unsigned __int64 reserved2           : 32;   // 63..32
    unsigned __int64 reserved3           : 32;   // 95..64
    unsigned __int64 reserved4           : 32;   // 127..96
} PipeConfig;


typedef struct _SlotStruct {
    SlotConfig       slotConfig;
    SlotSurfaceConfig                    slotSurfaceConfig;
    LumaKeyStruct                        lumaKeyStruct;
    MatrixStruct                         colorMatrixStruct;
    MatrixStruct                         gamutMatrixStruct;
    BlendingSlotStruct                   blendingSlotStruct;
} SlotStruct;


typedef struct _ConfigStruct_VIC40 {
    PipeConfig       pipeConfig;
    OutputConfig     outputConfig;
    OutputSurfaceConfig                  outputSurfaceConfig;
    MatrixStruct                         outColorMatrixStruct;
    ClearRectStruct                      clearRectStruct[4];
    SlotStruct                           slotStruct[8];
} ConfigStruct_VIC40;

typedef struct _ConfigStruct_VIC41 {
    PipeConfig       pipeConfig;
    OutputConfig     outputConfig;
    OutputSurfaceConfig                  outputSurfaceConfig;
    MatrixStruct                         outColorMatrixStruct;
    ClearRectStruct                      clearRectStruct[4];
    SlotStruct                           slotStruct[16];
} ConfigStruct_VIC41;

#endif
