/* Independent music instance of the shared chip wrapper. */
#define psg_init s2_vgm_psg_init
#define psg_advance s2_vgm_psg_advance
#define psg_write s2_vgm_psg_write
#define psg_render s2_vgm_psg_render
#define psg_render_pair s2_vgm_psg_render_pair
#define psg_set_output_mask s2_vgm_psg_set_output_mask
#define psg_samples_available s2_vgm_psg_samples_available
#define psg_sample_rate s2_vgm_psg_sample_rate
#define psg_reset_leftover s2_vgm_psg_reset_leftover
#define psg_save_state s2_vgm_psg_save_state
#define psg_load_state s2_vgm_psg_load_state
#define psg_rb_save s2_vgm_psg_rb_save
#define psg_rb_load s2_vgm_psg_rb_load
#define psg_cosim_dump s2_vgm_psg_cosim_dump
#define psg_cosim_hash s2_vgm_psg_cosim_hash
#include "sn76489.c"
