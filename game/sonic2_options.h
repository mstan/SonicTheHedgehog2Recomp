#pragma once
#include <stdint.h>
struct GVDP;
int s2_options_hook(uint32_t pc);
void s2_options_overlay(const struct GVDP *vdp, int line, uint32_t *out, int width);
void s2_options_load(const char *settings_path);
int s2_options_netplay_allowed(void);

#include <stddef.h>
/* Netplay session config seal (GameSpec netplay_config_image / _adopt). */
void s2_netplay_config_image(char *out, size_t cap);
int  s2_netplay_config_adopt(const char *line);
