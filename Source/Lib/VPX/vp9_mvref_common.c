
/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vp9_mvref_common.h"

 // This function searches the neighborhood of a given MB/SB
// to try and find candidate reference vectors.
#if 1
// 0: do not use reference MV(s) (only ZERO_MV as INTER candidate)
// 1: nearest only
// 2: both nearest and near
static int find_mv_refs_idx(EncDecContext   *context_ptr, const VP9_COMMON *cm, const MACROBLOCKD *xd,
#else
static void find_mv_refs_idx(EncDecContext   *context_ptr, const VP9_COMMON *cm, const MACROBLOCKD *xd,
#endif
                             ModeInfo *mi, MV_REFERENCE_FRAME ref_frame,
                             int_mv *mv_ref_list, int block, int mi_row,
                             int mi_col, uint8_t *mode_context) {
    (void)mi;

  const int *ref_sign_bias = cm->ref_frame_sign_bias;
  int i, refmv_count = 0;
#if 1
  const POSITION *const mv_ref_search = mv_ref_blocks[context_ptr->ep_block_stats_ptr->bsize];
#else
  const POSITION *const mv_ref_search = mv_ref_blocks[mi->sb_type];
#endif
  int different_ref_found = 0;
  int context_counter = 0;
#if 0
  const MV_REF *const prev_frame_mvs =
      cm->use_prev_frame_mvs
          ? cm->prev_frame->mvs + mi_row * cm->mi_cols + mi_col
          : NULL;
#endif
  const TileInfo *const tile = &xd->tile;

  // Blank the reference vector list
  memset(mv_ref_list, 0, sizeof(*mv_ref_list) * MAX_MV_REF_CANDIDATES);

  // The nearest 2 blocks are treated differently
  // if the size < 8x8 we get the mv from the bmi substructure,
  // and we also need to keep a mode count.
  for (i = 0; i < 2; ++i) {
    const POSITION *const mv_ref = &mv_ref_search[i];
    if (is_inside(tile, mi_col, mi_row, cm->mi_rows, mv_ref)) {

#if 1
        ModeInfo *candidate_mi = context_ptr->mode_info_array[(context_ptr->mi_col + mv_ref->col)+ (context_ptr->mi_row + mv_ref->row) * context_ptr->mi_stride];
#else
        const ModeInfo *const candidate_mi =
          xd->mi[mv_ref->col + mv_ref->row * xd->mi_stride];
#endif
      // Keep counts for entropy encoding.
      context_counter += mode_2_counter[candidate_mi->mode];
      different_ref_found = 1;

      if (candidate_mi->ref_frame[0] == ref_frame)
        ADD_MV_REF_LIST(get_sub_block_mv(candidate_mi, 0, mv_ref->col, block),
                        refmv_count, mv_ref_list, Done);
      else if (candidate_mi->ref_frame[1] == ref_frame)
        ADD_MV_REF_LIST(get_sub_block_mv(candidate_mi, 1, mv_ref->col, block),
                        refmv_count, mv_ref_list, Done);
    }
  }

  // Check the rest of the neighbors in much the same way
  // as before except we don't need to keep track of sub blocks or
  // mode counts.
  for (; i < MVREF_NEIGHBOURS; ++i) {
    const POSITION *const mv_ref = &mv_ref_search[i];

    if (is_inside(tile, mi_col, mi_row, cm->mi_rows, mv_ref)) {
#if 1
        ModeInfo *candidate_mi = context_ptr->mode_info_array[(context_ptr->mi_col + mv_ref->col) + (context_ptr->mi_row + mv_ref->row) * context_ptr->mi_stride];
#else
        const ModeInfo *const candidate_mi =
          xd->mi[mv_ref->col + mv_ref->row * xd->mi_stride];
#endif
      different_ref_found = 1;

      if (candidate_mi->ref_frame[0] == ref_frame)
        ADD_MV_REF_LIST(candidate_mi->mv[0], refmv_count, mv_ref_list, Done);
      else if (candidate_mi->ref_frame[1] == ref_frame)
        ADD_MV_REF_LIST(candidate_mi->mv[1], refmv_count, mv_ref_list, Done);
    }
  }

#if 1
  if (cm->use_prev_frame_mvs == EB_TRUE) {
      // Hsan : SVT-VP9 does not support temporal MV(s) as reference MV(s)
      // If here then reference MV(s) will not be used (i.e. only ZERO_MV as INTER canidate)
      mode_context[ref_frame] = (uint8_t)counter_to_context[context_counter];

      // Clamp vectors
      for (i = 0; i < refmv_count; ++i)
          clamp_mv_ref(&mv_ref_list[i].as_mv, xd);

      if (refmv_count == 0)
          return 0;
      else if (refmv_count == 1)
          return 1;
      else
          assert(0);
  }
#else
  // Check the last frame's mode and mv info.
  if (cm->use_prev_frame_mvs) {
      if (prev_frame_mvs->ref_frame[0] == ref_frame) {
          ADD_MV_REF_LIST(prev_frame_mvs->mv[0], refmv_count, mv_ref_list, Done);
      }
      else if (prev_frame_mvs->ref_frame[1] == ref_frame) {
          ADD_MV_REF_LIST(prev_frame_mvs->mv[1], refmv_count, mv_ref_list, Done);
      }
  }
#endif
  // Since we couldn't find 2 mvs from the same reference frame
  // go back through the neighbors and find motion vectors from
  // different reference frames.
  if (different_ref_found) {
    for (i = 0; i < MVREF_NEIGHBOURS; ++i) {
      const POSITION *mv_ref = &mv_ref_search[i];
      if (is_inside(tile, mi_col, mi_row, cm->mi_rows, mv_ref)) {

#if 1 
          ModeInfo *candidate_mi = context_ptr->mode_info_array[(context_ptr->mi_col + mv_ref->col) + (context_ptr->mi_row + mv_ref->row) * context_ptr->mi_stride];
#else
          const ModeInfo *const candidate_mi =
            xd->mi[mv_ref->col + mv_ref->row * xd->mi_stride];
#endif
        // If the candidate is INTRA we don't want to consider its mv.
        IF_DIFF_REF_FRAME_ADD_MV(candidate_mi, ref_frame, ref_sign_bias,
                                 refmv_count, mv_ref_list, Done);
      }
    }
  }

#if 1
  //if (cm->use_prev_frame_mvs == EB_TRUE) 
  {  
      // Hsan : SVT-VP9 does not support temporal MV(s) as reference MV(s)
      // If here then reference MV(s) will not be used (i.e. only ZERO_MV as INTER canidate)   
      mode_context[ref_frame] = (uint8_t)counter_to_context[context_counter];

      // Clamp vectors
      for (i = 0; i < refmv_count; ++i)
          clamp_mv_ref(&mv_ref_list[i].as_mv, xd);

      if (refmv_count == 0)
          return 0;
      else if (refmv_count == 1)
          return 1;
      else
          assert(0);
  }
#endif
#if 0
  // Since we still don't have a candidate we'll try the last frame.
  if (cm->use_prev_frame_mvs) {
    if (prev_frame_mvs->ref_frame[0] != ref_frame &&
        prev_frame_mvs->ref_frame[0] > INTRA_FRAME) {
      int_mv mv = prev_frame_mvs->mv[0];
      if (ref_sign_bias[prev_frame_mvs->ref_frame[0]] !=
          ref_sign_bias[ref_frame]) {
        mv.as_mv.row *= -1;
        mv.as_mv.col *= -1;
      }
      ADD_MV_REF_LIST(mv, refmv_count, mv_ref_list, Done);
    }

    if (prev_frame_mvs->ref_frame[1] > INTRA_FRAME &&
        prev_frame_mvs->ref_frame[1] != ref_frame &&
        prev_frame_mvs->mv[1].as_int != prev_frame_mvs->mv[0].as_int) {
      int_mv mv = prev_frame_mvs->mv[1];
      if (ref_sign_bias[prev_frame_mvs->ref_frame[1]] !=
          ref_sign_bias[ref_frame]) {
        mv.as_mv.row *= -1;
        mv.as_mv.col *= -1;
      }
      ADD_MV_REF_LIST(mv, refmv_count, mv_ref_list, Done);
    }
  }
#endif
Done:

  mode_context[ref_frame] = (uint8_t)counter_to_context[context_counter];

  // Clamp vectors
  for (i = 0; i < MAX_MV_REF_CANDIDATES; ++i)
      clamp_mv_ref(&mv_ref_list[i].as_mv, xd);

#if 1
  // If here then refernce MV(s) will be used
  return 2;
#endif
}
#if 1
int vp9_find_mv_refs(EncDecContext   *context_ptr, const VP9_COMMON *cm, const MACROBLOCKD *xd,
#else
void vp9_find_mv_refs(EncDecContext   *context_ptr, const VP9_COMMON *cm, const MACROBLOCKD *xd,
#endif
                      ModeInfo *mi, MV_REFERENCE_FRAME ref_frame,
                      int_mv *mv_ref_list, int mi_row, int mi_col,
                      uint8_t *mode_context) {
#if 1
    return find_mv_refs_idx(context_ptr, cm, xd, mi, ref_frame, mv_ref_list, -1, mi_row, mi_col, mode_context);
#else
  find_mv_refs_idx(context_ptr, cm, xd, mi, ref_frame, mv_ref_list, -1, mi_row, mi_col,
                   mode_context);
#endif
}

void vp9_find_best_ref_mvs(MACROBLOCKD *xd, int allow_hp, int_mv *mvlist,
                           int_mv *nearest_mv, int_mv *near_mv) {
  int i;
  // Make sure all the candidates are properly clamped etc
  for (i = 0; i < MAX_MV_REF_CANDIDATES; ++i) {
    lower_mv_precision(&mvlist[i].as_mv, allow_hp);
    clamp_mv2(&mvlist[i].as_mv, xd);
  }
  *nearest_mv = mvlist[0];
  *near_mv = mvlist[1];
}
#if 0
void vp9_append_sub8x8_mvs_for_idx(VP9_COMMON *cm, MACROBLOCKD *xd, int block,
                                   int ref, int mi_row, int mi_col,
                                   int_mv *nearest_mv, int_mv *near_mv,
                                   uint8_t *mode_context) {
  int_mv mv_list[MAX_MV_REF_CANDIDATES];
  ModeInfo *const mi = xd->mi[0];
  b_mode_info *bmi = mi->bmi;
  int n;

  assert(MAX_MV_REF_CANDIDATES == 2);

  find_mv_refs_idx(cm, xd, mi, mi->ref_frame[ref], mv_list, block, mi_row,
                   mi_col, mode_context);

  near_mv->as_int = 0;
  switch (block) {
    case 0:
      nearest_mv->as_int = mv_list[0].as_int;
      near_mv->as_int = mv_list[1].as_int;
      break;
    case 1:
    case 2:
      nearest_mv->as_int = bmi[0].as_mv[ref].as_int;
      for (n = 0; n < MAX_MV_REF_CANDIDATES; ++n)
        if (nearest_mv->as_int != mv_list[n].as_int) {
          near_mv->as_int = mv_list[n].as_int;
          break;
        }
      break;
    case 3: {
      int_mv candidates[2 + MAX_MV_REF_CANDIDATES];
      candidates[0] = bmi[1].as_mv[ref];
      candidates[1] = bmi[0].as_mv[ref];
      candidates[2] = mv_list[0];
      candidates[3] = mv_list[1];

      nearest_mv->as_int = bmi[2].as_mv[ref].as_int;
      for (n = 0; n < 2 + MAX_MV_REF_CANDIDATES; ++n)
        if (nearest_mv->as_int != candidates[n].as_int) {
          near_mv->as_int = candidates[n].as_int;
          break;
        }
      break;
    }
    default: assert(0 && "Invalid block index.");
  }
}
#endif