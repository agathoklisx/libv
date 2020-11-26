// an example script (initial version)
var NULL = 0;

var v = v_get ();
var vwm = v_get_vwm (v);

v_set_sockname (v, "/tmp/sockname");
v_set_raw_mode (v);
v_set_size (v);

var rows = v_get_rows (v);
var cols = v_get_cols (v);

var num_frames = 0;
var max_frames = 0;
var log = 0;
var remove_log = 1;
var win = NULL;
var frame = NULL;

num_frames = 1;
max_frames = 1;
win = v_new_win (v, num_frames, max_frames);
frame = v_win_get_frame_at (v, win, 0);
log = 0;
remove_log = 1;
if (log) {
  v_set_frame_log (v, frame, "", remove_log);
}

v_set_frame_command (v, frame, "bash");

num_frames = 1;
max_frames = 1;
win = v_new_win (v, num_frames, max_frames);
frame = v_win_get_frame_at (v, win, 0);
log = 0;
remove_log = 1;
if (log) {
  v_set_frame_log (v, frame, "", remove_log);
}
v_set_frame_command (v, frame, "top");

num_frames = 1;
max_frames = 3;
win = v_new_win (v, num_frames, max_frames);
frame = v_win_get_frame_at (v, win, 0);
log = 1;
remove_log = 1;
if (log) {
  v_set_frame_log (v, frame, "", remove_log);
}
v_set_frame_command (v, frame, "zsh");

v_main (v);
