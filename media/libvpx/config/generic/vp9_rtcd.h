/*
 *  Copyright (c) 2025 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file is generated. Do not edit.
#ifndef VP9_RTCD_H_
#define VP9_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

/*
 * VP9
 */

#include "vpx/vpx_integer.h"
#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_enums.h"
#include "vp9/common/vp9_filter.h"
#if !CONFIG_REALTIME_ONLY && CONFIG_VP9_ENCODER
#include "vp9/encoder/vp9_temporal_filter.h"
#endif

struct macroblockd;

/* Encoder forward decls */
struct macroblock;
struct macroblock_plane;
struct vp9_sad_table;
struct ScanOrder;
struct search_site_config;
struct mv;
union int_mv;
struct yv12_buffer_config;

#ifdef __cplusplus
extern "C" {
#endif

void vp9_apply_temporal_filter_c(const uint8_t *y_src, int y_src_stride, const uint8_t *y_pre, int y_pre_stride, const uint8_t *u_src, const uint8_t *v_src, int uv_src_stride, const uint8_t *u_pre, const uint8_t *v_pre, int uv_pre_stride, unsigned int block_width, unsigned int block_height, int ss_x, int ss_y, int strength, const int *const blk_fw, int use_32x32, uint32_t *y_accumulator, uint16_t *y_count, uint32_t *u_accumulator, uint16_t *u_count, uint32_t *v_accumulator, uint16_t *v_count);
#define vp9_apply_temporal_filter vp9_apply_temporal_filter_c

int64_t vp9_block_error_c(const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz);
#define vp9_block_error vp9_block_error_c

int64_t vp9_block_error_fp_c(const tran_low_t *coeff, const tran_low_t *dqcoeff, int block_size);
#define vp9_block_error_fp vp9_block_error_fp_c

int vp9_diamond_search_sad_c(const struct macroblock *x, const struct search_site_config *cfg,  struct mv *ref_mv, uint32_t start_mv_sad, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct vp9_sad_table *sad_fn_ptr, const struct mv *center_mv);
#define vp9_diamond_search_sad vp9_diamond_search_sad_c

void vp9_fht16x16_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define vp9_fht16x16 vp9_fht16x16_c

void vp9_fht4x4_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define vp9_fht4x4 vp9_fht4x4_c

void vp9_fht8x8_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define vp9_fht8x8 vp9_fht8x8_c

void vp9_fwht4x4_c(const int16_t *input, tran_low_t *output, int stride);
#define vp9_fwht4x4 vp9_fwht4x4_c

void vp9_iht16x16_256_add_c(const tran_low_t *input, uint8_t *dest, int stride, int tx_type);
#define vp9_iht16x16_256_add vp9_iht16x16_256_add_c

void vp9_iht4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int stride, int tx_type);
#define vp9_iht4x4_16_add vp9_iht4x4_16_add_c

void vp9_iht8x8_64_add_c(const tran_low_t *input, uint8_t *dest, int stride, int tx_type);
#define vp9_iht8x8_64_add vp9_iht8x8_64_add_c

void vp9_quantize_fp_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const struct macroblock_plane *const mb_plane, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const struct ScanOrder *const scan_order);
#define vp9_quantize_fp vp9_quantize_fp_c

void vp9_quantize_fp_32x32_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const struct macroblock_plane *const mb_plane, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const struct ScanOrder *const scan_order);
#define vp9_quantize_fp_32x32 vp9_quantize_fp_32x32_c

void vp9_scale_and_extend_frame_c(const struct yv12_buffer_config *src, struct yv12_buffer_config *dst, INTERP_FILTER filter_type, int phase_scaler);
#define vp9_scale_and_extend_frame vp9_scale_and_extend_frame_c

void vpx_convolve12_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const InterpKernel12 *filter, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h);
#define vpx_convolve12 vpx_convolve12_c

void vpx_convolve12_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const InterpKernel12 *filter, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h);
#define vpx_convolve12_horiz vpx_convolve12_horiz_c

void vpx_convolve12_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const InterpKernel12 *filter, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h);
#define vpx_convolve12_vert vpx_convolve12_vert_c

void vp9_rtcd(void);

#include "vpx_config.h"

#ifdef RTCD_C
static void setup_rtcd_internal(void)
{
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_RTCD_H_
