/* Independent music instance of the shared chip wrapper. */
#define ym2612_init s2_vgm_fm_init
#define ym2612_advance s2_vgm_fm_advance
#define ym2612_write s2_vgm_fm_write
#define ym2612_render s2_vgm_fm_render
#define ym2612_render_pair s2_vgm_fm_render_pair
#define ym2612_set_output_mask s2_vgm_fm_set_output_mask
#define ym2612_samples_available s2_vgm_fm_samples_available
#define ym2612_sample_rate s2_vgm_fm_sample_rate
#define ym2612_save_state s2_vgm_fm_save_state
#define ym2612_load_state s2_vgm_fm_load_state
#define ym2612_rb_save s2_vgm_fm_rb_save
#define ym2612_rb_load s2_vgm_fm_rb_load
#define ym2612_cosim_hash s2_vgm_fm_cosim_hash
#include "ym2612_ymfm.cpp"
