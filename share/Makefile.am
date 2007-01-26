
## Makefile.am -- Process this file with automake to produce Makefile.in

dist_sysconf_DATA = etc/varalias.lst etc/screens.lst etc/offnominals.lst

nobase_dist_data_DATA = $(shared_scripts) $(shared_screens)

shared_scripts = \
	scripts/set_time.opl \
	scripts/fail_1.ofn_off.opl scripts/fail_1.ofn_on.opl \
	scripts/fail_2.ofn_off.opl scripts/fail_2.ofn_on.opl


shared_screens = \
	screens/curve.osx screens/dials.osx screens/manometers.osx \
	screens/offnom.osx screens/recycle.osx screens/root.osx \
	screens/sensors.osx screens/service.osx screens/solar.osx \
	screens/thermo.osx screens/time.osx screens/toggles.osx


EXTRA_DIST = $(shared_pics)

data_shared_dir = $(datadir)


install-data-local:
	@$(NORMAL_INSTALL)
	@echo "Installing shared data in $(DESTDIR)$(data_shared_dir) ..."
	@$(am__vpath_adj_setup) \
	list='$(shared_pics)'; \
	for p in $$list; do \
	  if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	  $(am__vpath_adj) \
	  echo " $(install_sh_DATA) '$$d$$p' '$(DESTDIR)$(data_shared_dir)/$$p'" > /dev/null; \
	  $(install_sh_DATA) "$$d$$p" "$(DESTDIR)$(data_shared_dir)/$$p"; \
	done

uninstall-local:
	@$(NORMAL_UNINSTALL)
	@echo "Uninstalling shared data from $(DESTDIR)$(data_shared_dir) ..."
	@$(am__vpath_adj_setup) \
	list='$(shared_pics)'; for p in $$list; do \
	  $(am__vpath_adj) \
	  echo " rm -f '$(DESTDIR)$(data_shared_dir)/$$p'" > /dev/null; \
	  rm -f "$(DESTDIR)$(data_shared_dir)/$$p"; \
	done


shared_pics = $(PIC_CONTROL) $(PIC_K) $(PIC_PNG) $(PIC_TILES) $(PIC_TOOLS)


PIC_CONTROL = \
	pic/controls/cntr_0.png pic/controls/cntr_1.png pic/controls/cntr_2.png \
	pic/controls/cntr_3.png pic/controls/cntr_4.png pic/controls/cntr_5.png \
	pic/controls/cntr_6.png pic/controls/cntr_7.png pic/controls/cntr_8.png \
	pic/controls/cntr_9.png pic/controls/cntr_apostrophe.png \
	pic/controls/cntr_colon.png pic/controls/cntr_comma.png \
	pic/controls/cntr_dash.png pic/controls/cntr_dblquote.png \
	pic/controls/cntr_dot.png pic/controls/cntr_space.png \
	pic/controls/sputnik6.png pic/controls/sw8_1.png pic/controls/sw8_2.png \
	pic/controls/sw9_off.png pic/controls/sw9_on.png

PIC_K = \
	pic/kpic/2downarrow.png pic/kpic/2uparrow.png pic/kpic/apply.png \
	pic/kpic/blend.png pic/kpic/button_cancel.png \
	pic/kpic/button_ok.png pic/kpic/cache.png pic/kpic/colorize.png \
	pic/kpic/configure.png pic/kpic/connect_creating.png \
	pic/kpic/connect_established.png pic/kpic/editclear.png \
	pic/kpic/editcopy.png pic/kpic/editcut.png pic/kpic/editdelete.png \
	pic/kpic/edit.png pic/kpic/exec.png \
	pic/kpic/exit.png pic/kpic/filenew.png pic/kpic/fileopen.png \
	pic/kpic/filesaveas.png pic/kpic/filesave.png pic/kpic/folder.png \
	pic/kpic/fontsizedown.png pic/kpic/fontsizeup.png pic/kpic/fork.png \
	pic/kpic/frame_chart.png pic/kpic/frame_formula.png pic/kpic/frame_image.png \
	pic/kpic/frame_spreadsheet.png pic/kpic/frame_text.png pic/kpic/help.png \
	pic/kpic/history.png pic/kpic/imagegallery.png pic/kpic/kalarm.png \
	pic/kpic/mail_send.png pic/kpic/math_brackets.png pic/kpic/move.png \
	pic/kpic/player_play.png pic/kpic/player_stop.png \
	pic/kpic/stop.png pic/kpic/toggle_log.png pic/kpic/viewmag.png \
	pic/kpic/view_remove.png pic/kpic/view_tree.png

PIC_PNG = \
	pic/png/blue.png pic/png/green.png pic/png/green1.png pic/png/yellow.png \
	pic/png/red1.png pic/png/red.png pic/png/empty.png \
	pic/png/meter.png pic/png/smallmeter.png pic/png/dial_bg.png \
	pic/png/mnmtr_medium.png pic/png/mnmtr_midi.png \
	pic/png/mnmtr_mini.png pic/png/mnmtr_normal.png \
	pic/png/thermometer_empty.png pic/png/thermometer_red.png \
	pic/png/bigtank_blue.png pic/png/bigtank_empty.png \
	pic/png/bigtank_green.png pic/png/bigtank_red.png \
	pic/png/roundtank_blue.png pic/png/roundtank_empty.png \
	pic/png/roundtank_green.png pic/png/roundtank_red.png \
	pic/png/smalltank_blue.png pic/png/smalltank_empty.png \
	pic/png/smalltank_green.png pic/png/smalltank_red.png \
	pic/png/volume_blue.png pic/png/volume_empty.png \
	pic/png/volume_green.png pic/png/volume_red.png \
	pic/png/b_0.png pic/png/b_1.png pic/png/b_2.png pic/png/b_3.png pic/png/b_4.png \
	pic/png/b_5.png pic/png/b_6.png pic/png/b_7.png pic/png/b_8.png pic/png/b_9.png \
	pic/png/b_colon.png pic/png/b_comma.png pic/png/b_dash.png pic/png/b_dot.png \
	pic/png/b_left.png pic/png/b_plate.png pic/png/b_right.png \
	pic/png/b_a.png pic/png/b_p.png \
	pic/png/blamp_blue.png pic/png/blamp_green.png pic/png/blamp_off.png \
	pic/png/blamp_red.png pic/png/blamp_white.png pic/png/blamp_yellow.png \
	pic/png/button2_off.png pic/png/button2_on.png pic/png/button2_press.png \
	pic/png/button_pressed.png pic/png/button_released.png \
	pic/png/cbutton_close.png pic/png/cbutton_open.png pic/png/cbutton_press.png \
	pic/png/cntr_0.png pic/png/cntr_1.png pic/png/cntr_2.png pic/png/cntr_3.png \
	pic/png/cntr_4.png pic/png/cntr_5.png pic/png/cntr_6.png pic/png/cntr_7.png \
	pic/png/cntr_8.png pic/png/cntr_9.png \
	pic/png/cntr_comma.png pic/png/cntr_dash.png pic/png/cntr_dot.png \
	pic/png/d_0.png pic/png/d_1.png pic/png/d_2.png pic/png/d_3.png pic/png/d_4.png \
	pic/png/d_5.png pic/png/d_6.png pic/png/d_7.png pic/png/d_8.png pic/png/d_9.png \
	pic/png/d_a.png pic/png/d_p.png pic/png/d_plate.png \
	pic/png/d_colon.png pic/png/d_comma.png pic/png/d_dash.png pic/png/d_dot.png \
	pic/png/fbutton_off.png pic/png/fbutton_on.png \
	pic/png/fuse_in.png pic/png/fuse_out.png \
	pic/png/G_0.png pic/png/G_1.png pic/png/G_2.png pic/png/G_3.png pic/png/G_4.png \
	pic/png/G_5.png pic/png/G_6.png pic/png/G_7.png pic/png/G_8.png pic/png/G_9.png \
	pic/png/G_colon.png pic/png/G_comma.png pic/png/G_dash.png pic/png/G_dot.png \
	pic/png/G_left.png pic/png/G_plate.png pic/png/G_right.png \
	pic/png/G_a.png pic/png/G_b.png pic/png/G_c.png pic/png/G_d.png \
	pic/png/G_e.png pic/png/G_f.png pic/png/G_h.png pic/png/G_j.png \
	pic/png/G_k.png pic/png/G_l.png pic/png/G_m.png pic/png/G_n.png \
	pic/png/G_p.png pic/png/G_q.png pic/png/G_r.png pic/png/G_t.png \
	pic/png/G_u.png pic/png/G_v.png pic/png/G_w.png pic/png/G_x.png \
	pic/png/G_y.png pic/png/G_z.png \
	pic/png/g4button_off.png pic/png/g4button_on.png pic/png/g4button_released.png \
	pic/png/gbutton_off.png pic/png/gbutton_on.png pic/png/gbutton_released.png \
	pic/png/gklapan_off.png pic/png/gklapan_on.png \
	pic/png/greenbtn2_off.png pic/png/greenbtn2_on.png \
	pic/png/greenbtn_off.png pic/png/greenbtn_on.png \
	pic/png/klapan2_close.png pic/png/klapan2_open.png \
	pic/png/klapan_close.png pic/png/klapan_open.png \
	pic/png/lbuttong_on.png pic/png/lbuttong_released.png pic/png/lbutton_off.png \
	pic/png/lbutton_on.png pic/png/lbutton_released.png \
	pic/png/miniempty.png pic/png/minigreen.png \
	pic/png/mlamp_blue.png pic/png/mlamp_green.png pic/png/mlamp_off.png \
	pic/png/mlamp_red.png pic/png/mlamp_white.png pic/png/mlamp_yellow.png \
	pic/png/offnom_idle.png pic/png/offnom_run.png \
	pic/png/pred_in.png pic/png/pred_out.png \
	pic/png/rbutton_off.png pic/png/rbutton_on.png \
	pic/png/redbtn2_off.png pic/png/redbtn2_on.png \
	pic/png/redbtn_off.png pic/png/redbtn_on.png \
	pic/png/sb_2.png pic/png/sb_4.png pic/png/sb_sun.png \
	pic/png/sklapan_off.png pic/png/sklapan_on.png \
	pic/png/slamp_blue.png pic/png/slamp_green.png pic/png/slamp_off.png \
	pic/png/slamp_red.png pic/png/slamp_white.png pic/png/slamp_yellow.png \
	pic/png/snd_off.png pic/png/snd_on.png \
	pic/png/spinner-00.png pic/png/spinner-01.png pic/png/spinner-02.png \
	pic/png/spinner-03.png pic/png/spinner-04.png pic/png/spinner-05.png \
	pic/png/spinner-06.png pic/png/spinner-07.png pic/png/spinner-08.png \
	pic/png/spinner-09.png \
	pic/png/sw1_off.png pic/png/sw1_on.png pic/png/sw2_off.png pic/png/sw2_on.png \
	pic/png/switch2_off.png pic/png/switch2_on.png \
	pic/png/switch_off.png pic/png/switch_on.png \
	pic/png/transmini_blue.png pic/png/transmini_green.png pic/png/transmini_off.png \
	pic/png/transmini_red.png pic/png/transmini_yellow.png \
	pic/png/transprnt_blue.png pic/png/transprnt_green.png pic/png/transprnt_off.png \
	pic/png/transprnt_red.png pic/png/transprnt_yellow.png \
	pic/png/tumbler_off.png pic/png/tumbler_on.png \
	pic/png/yelbtn2_off.png pic/png/yelbtn2_on.png

PIC_TILES = \
	pic/tiles/All-Good-People-1.jpg pic/tiles/blue_ridge.png \
	pic/tiles/brushed_metal.png pic/tiles/Chicken-Songs-2.jpg \
	pic/tiles/countertop.png pic/tiles/fossil.png pic/tiles/snow_ridge.png \
	pic/tiles/stonewall2.png pic/tiles/photo_800.jpg

PIC_TOOLS = \
	pic/tools/angel.png pic/tools/ar_r.png pic/tools/bell_off.png \
	pic/tools/bell_on.png pic/tools/bio.png pic/tools/book.png \
	pic/tools/bottle.png pic/tools/bulb.png pic/tools/card.png \
	pic/tools/clock.png pic/tools/conn_on.png pic/tools/earth.png \
	pic/tools/fuel.png pic/tools/g_check.png pic/tools/help.png \
	pic/tools/nav.png pic/tools/red_ex.png pic/tools/s_excl.png \
	pic/tools/smoke.png pic/tools/tools.png pic/tools/tree.png

