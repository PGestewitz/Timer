#include <pebble.h>
#include "list.h"
#include "persistent_stuff.h"

#define MODE_NULL 			0
#define MODE_SET_HOUR 			1
#define MODE_SET_MINUTE 		2
#define MODE_SET_SECOND 		3

#define POSITION_START 			0
#define POSITION_MINUTES	 	1
#define POSITION_HOURS 			2
#define POSITION_SECONDS	 	3
#define POSITION_MODE 			4

#define PERSIST_KEY_HRS			1
#define PERSIST_KEY_MINS		2
#define PERSIST_KEY_SECS		3

#define PERSIST_KEY_EXIT_TIME		4
#define PERSIST_KEY_TIMERS		5
#define PERSIST_KEY_TIMERS_COUNT	6

struct TimerItem {
	long time;
	long starttime;
	bool update;
	char name[20];
};	

static Window *window; //Window 1; the timer window.
static Window *TimerListWindow; //Window 2; the timer list.
static Window *TimerMenu; //Window 3; The menu to be used when dealing with timers.
static Window *StopwatchWindow; //Window 5; The stopwatch list.
static Window *StopwatchSplits; //Window 6; The menu encountered when dealing with timers.
static Window *NotifierWindow; //The window showing a "timer finished" notification
static TextLayer *Hrs;
static TextLayer *Hrs_label;
static TextLayer *Mins;
static TextLayer *Mins_label;
static TextLayer *Secs;
static TextLayer *Secs_label;
static TextLayer *Start;
static TextLayer *Mode;
static TextLayer *MessageText;
static BitmapLayer *BLayer;

static List *CountdownTimers = NULL;
static int Timer_Number = 1;

static BitmapLayer *MenuOption;
int TimerListSelectedIndex = 0;

static MenuLayer *TimerListMenu = NULL;
static long StopwatchTime = 0;
static AppTimer *StopwatchTimer;
static bool StopwatchPaused = true;
static ActionBarLayer *StopwatchActionBar;

static List *SplitList = NULL;
static MenuLayer *SplitMenu;

//RESOURCES//

static GBitmap *Menu_icon;
static GBitmap *Loopback_icon;
static GBitmap *Plus_Sign_icon;
static GBitmap *Trashcan_icon;
static GBitmap *Stopwatch_icon;
static GBitmap *Pause_icon;
static GBitmap *Play_icon;
static GBitmap *Action_next_icon;
static GBitmap *Trophy_icon;
static GBitmap *Caret_icon;
static GBitmap *half_black;

char CurrentTimerMode[10] = "Countdown";

TextLayer *LayerToFlash;
int flash_color = 1;
static AppTimer *FlashTimer;
int flash_off = 0;

static AppTimer *MessageTimer;

int Hours_cnt = 0;
char Hours_cnt_text[5] = "0";
int Minutes_cnt = 5;
char Minutes_cnt_text[5] = "0";
int Seconds_cnt = 0;
char Seconds_cnt_text[5] = "0";
int DisplayingMessage = 0;

char Message[50];

int CurrentMode = 0;

int Position = POSITION_START;
int max_position = 4;
const int MIN_POSITION = 0;

//Window Functions

static void select_long_click_handler(ClickRecognizerRef recognizer, void *context);
void half_black_me(GContext *ctx, GRect area);
void kill_animation(struct Animation *animation, bool finished, void *context);
void create_animation(struct Layer *layer, GRect *start, GRect *end, uint32_t delay, AnimationCurve curve, uint32_t duration);

static void TimerList_load(Window *window);
static void TimerList_unload(Window *window);

static void TimerMenu_load(Window *window);
static void TimerMenu_unload(Window *window);

static void StopwatchWindow_load(Window *window);
static void StopwatchWindow_click_config_provider(void *context);
static void StopwatchWindow_select_click_handler(ClickRecognizerRef recognizer, void *context);
static void StopwatchWindow_up_click_handler(ClickRecognizerRef recognizer, void *context);
static void StopwatchWindow_down_click_handler(ClickRecognizerRef recognizer, void *context);
static void StopwatchWindow_long_down_click_handler( ClickRecognizerRef recognizer, void *context );
static void StopwatchWindow_unload(Window *window);
static void StopwatchCallback(void *data);
static void PauseStopwatch(void);
static void ResetStopwatch(void);
static void UpdateProgress(void);
static void UpdateStopwatchDisplay(void);

static void StopwatchSplits_load(Window *window);
static void DrawSplitItem(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context);
static int16_t RetrieveSplitHeight(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context);
static uint16_t RetrieveSplitCount(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context);
static uint16_t RetrieveSplitSectionCount(struct MenuLayer *menu_layer, void *callback_context);
static void StopwatchSplits_unload(Window *window);

static void NotifierWindow_load(Window *window);
static void NotifierWindow_unload(Window *window);
static void NotifierWindow_click_config_provider(void *context);
static void Notify(char *Title, char *Message);
static void AcceptMessage(ClickRecognizerRef recognizer, void *context);

//Other Functions

void redraw_selection(void);
void click(void);
void update_numbers(void);
void flash_text_layer(TextLayer *text, int color);
void stop_flashing_text_layer(void);
void add_timer(void);
void timer_finished(item *t);
void cancel_countdown(item *CountdownItem);
void show_message(char *message, int time);
static void clear_message(void *data);

/* EVENTS */

static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
if ( item_count(CountdownTimers) && (Position == POSITION_START))
{
vibes_short_pulse();
window_stack_pop_all(0);
window_stack_push(window, 0);
window_stack_push(TimerListWindow, 1);
TimerListSelectedIndex = 0;
menu_layer_set_selected_index(TimerListMenu, (MenuIndex){.row = 0, .section = 0}, MenuRowAlignTop, 1);
}
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
click();
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
if(CurrentMode == MODE_NULL)
{
Position++;
  if (Position > max_position)
	Position = MIN_POSITION;
  redraw_selection();  
}
else
{
switch(CurrentMode)
{
case MODE_SET_HOUR:
	{
	Hours_cnt++;
	if(Hours_cnt < 0)
	Hours_cnt = 24;
	if(Hours_cnt > 24)
	Hours_cnt = 0;
	break;
	}
case MODE_SET_MINUTE:
	{
	Minutes_cnt++;
	//Adjust minutes
	if(Minutes_cnt < 0)
	{
	Minutes_cnt = 59;
	Hours_cnt--;
	}
	if(Minutes_cnt > 59)
	{
	Minutes_cnt = 0;
	Hours_cnt++;
	}
	//Then adjust hours
	if(Hours_cnt < 0)
	Hours_cnt = 24;
	if(Hours_cnt > 24)
	Hours_cnt = 0;
	break;
	}
case MODE_SET_SECOND:
	{
	Seconds_cnt++;
	if(Seconds_cnt < 0)
	{
	Seconds_cnt = 59;
	Minutes_cnt--;
	}
	if(Seconds_cnt > 59)
	{
	Seconds_cnt = 0;
	Minutes_cnt++;
	}
	//then adjust minutes
	if(Minutes_cnt < 0)
	{
	Minutes_cnt = 59;
	Hours_cnt--;
	}
	if(Minutes_cnt > 59)
	{
	Minutes_cnt = 0;
	Hours_cnt++;
	}
	//Then adjust hours
	if(Hours_cnt < 0)
	Hours_cnt = 24;
	if(Hours_cnt > 24)
	Hours_cnt = 0;
	break;
	}
}
update_numbers();
}
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
if(CurrentMode == MODE_NULL)
{
  Position--;
  if (Position < MIN_POSITION)
	Position = max_position;
  redraw_selection();
}
else
{
switch(CurrentMode)
{
case MODE_SET_HOUR:
	{
	Hours_cnt--;
	if(Hours_cnt < 0)
	Hours_cnt = 24;
	if(Hours_cnt > 24)
	Hours_cnt = 0;
	break;
	}
case MODE_SET_MINUTE:
	{
	Minutes_cnt--;
	//Adjust minutes
	if(Minutes_cnt < 0)
	{
	Minutes_cnt = 59;
	Hours_cnt--;
	}
	if(Minutes_cnt > 59)
	{
	Minutes_cnt = 0;
	Hours_cnt++;
	}
	//Then adjust hours
	if(Hours_cnt < 0)
	Hours_cnt = 24;
	if(Hours_cnt > 24)
	Hours_cnt = 0;
	break;
	}
case MODE_SET_SECOND:
	{
	Seconds_cnt--;
	if(Seconds_cnt < 0)
	{
	Seconds_cnt = 59;
	Minutes_cnt--;
	}
	if(Seconds_cnt > 59)
	{
	Seconds_cnt = 0;
	Minutes_cnt++;
	}
	//then adjust minutes
	if(Minutes_cnt < 0)
	{
	Minutes_cnt = 59;
	Hours_cnt--;
	}
	if(Minutes_cnt > 59)
	{
	Minutes_cnt = 0;
	Hours_cnt++;
	}
	//Then adjust hours
	if(Hours_cnt < 0)
	Hours_cnt = 24;
	if(Hours_cnt > 24)
	Hours_cnt = 0;
	break;
	}
}
update_numbers();
}
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, down_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 300, select_long_click_handler, NULL);
}

static void timer_callback(struct tm *tick_time, TimeUnits units_changed)
{
int x, cnt = item_count(CountdownTimers);
//Decrease Timers
for ( x = 0; x < cnt; x++ )
{
item *i = get_item_by_index(x, CountdownTimers);
struct TimerItem *ti = ((struct TimerItem *)i->data);
if(ti->update)
ti->time--;
if ( ti->time <= 0 )
timer_finished(i);
}
if (TimerListMenu != NULL)
menu_layer_reload_data(TimerListMenu);
}

static void flash_callback(void *data)
{
if(!flash_off) //If the box is on, flash off
{
GColor C;
if ( flash_color == 1 )
C = GColorWhite;
else
C = GColorBlack;
text_layer_set_text_color(LayerToFlash, C);
flash_off = !flash_off;
FlashTimer = app_timer_register(500, flash_callback, NULL);
}
else //If the box is off, turn it back on
{
GColor C;
if ( flash_color == 1 )
C = GColorBlack;
else
C = GColorWhite;
text_layer_set_text_color(LayerToFlash, C);
flash_off = !flash_off;
FlashTimer = app_timer_register(500, flash_callback, NULL);
}
}

static void BLayer_Update_Proc(struct Layer *layer, GContext *ctx)
{
GRect b = layer_get_bounds(bitmap_layer_get_layer(BLayer));
graphics_context_set_fill_color(ctx, GColorWhite);
graphics_fill_rect(ctx, (GRect){.origin = {5, ((int)(b.size.h/2))-20}, .size = {b.size.w-10, 40}}, 3, GCornersAll);
graphics_draw_round_rect(ctx, (GRect){.origin = {5, ((int)(b.size.h/2))-20}, .size = {b.size.w-10, 40}}, 3);
}

/* CODE */
void redraw_selection(void)
{

text_layer_set_background_color(Hrs, GColorBlack);
text_layer_set_background_color(Mins, GColorBlack);
text_layer_set_background_color(Secs, GColorBlack);
text_layer_set_background_color(Start, GColorBlack);
text_layer_set_background_color(Mode, GColorBlack);

text_layer_set_text_color(Hrs, GColorWhite);
layer_set_hidden(text_layer_get_layer(Hrs_label), 0);
text_layer_set_text_color(Mins, GColorWhite);
layer_set_hidden(text_layer_get_layer(Mins_label), 0);
text_layer_set_text_color(Secs, GColorWhite);
layer_set_hidden(text_layer_get_layer(Secs_label), 0);
text_layer_set_text_color(Start, GColorWhite);
text_layer_set_text_color(Mode, GColorWhite);

switch(Position)
{
case POSITION_HOURS: //Hours
	{
	text_layer_set_background_color(Hrs, GColorWhite);
	text_layer_set_text_color(Hrs, GColorBlack);
	layer_set_hidden(text_layer_get_layer(Hrs_label), 1);
	break;
	}
case POSITION_MINUTES: //Minutes
	{
	text_layer_set_background_color(Mins, GColorWhite);
	text_layer_set_text_color(Mins, GColorBlack);
	layer_set_hidden(text_layer_get_layer(Mins_label), 1);
	break;
	}
case POSITION_SECONDS: //Seconds
	{
	text_layer_set_background_color(Secs, GColorWhite);
	text_layer_set_text_color(Secs, GColorBlack);
	layer_set_hidden(text_layer_get_layer(Secs_label), 1);
	break;
	}
case POSITION_START: //Start
	{
	text_layer_set_background_color(Start, GColorWhite);
	text_layer_set_text_color(Start, GColorBlack);
	break;
	}
case POSITION_MODE: //Mode
	{
	text_layer_set_background_color(Mode, GColorWhite);
	text_layer_set_text_color(Mode, GColorBlack);
	break;
	}
};

if ( item_count(CountdownTimers) && (Position == POSITION_START) )
{
layer_set_hidden(bitmap_layer_get_layer(MenuOption), 0);
text_layer_set_text(Mode, "Long select for timers.");
}
else
{
layer_set_hidden(bitmap_layer_get_layer(MenuOption), 1);
text_layer_set_text(Mode, "Switch to Stopwatch");
}

}
void click(void)
{
if(CurrentMode != MODE_NULL)
{
stop_flashing_text_layer();
text_layer_set_text(Mode, CurrentTimerMode);
CurrentMode = MODE_NULL;
Position = POSITION_START;
redraw_selection();
}
else
switch(Position)
{
case POSITION_HOURS: //Hours Clicked
	{
	CurrentMode = MODE_SET_HOUR;
	text_layer_set_text(Mode, "Choose hours.");
	flash_text_layer(Hrs, 0);
	break;
	}
case POSITION_MINUTES: //Minutes Clicked
	{
	CurrentMode = MODE_SET_MINUTE;
	text_layer_set_text(Mode, "Choose minutes.");
	flash_text_layer(Mins, 0);
	break;
	}
case POSITION_SECONDS: //Seconds Clicked
	{
	CurrentMode = MODE_SET_SECOND;
	text_layer_set_text(Mode, "Choose seconds.");
	flash_text_layer(Secs, 0);
	break;
	}
case POSITION_START: //Start Clicked
	{
	while(window_stack_remove(TimerListWindow, 0));
	add_timer();
	break;
	}
case POSITION_MODE: //Mode Clicked
	{
	window_stack_push(StopwatchWindow, 0);
	break;
	}
}

}

void update_numbers(void)
{
snprintf(Hours_cnt_text, 5, "%d", Hours_cnt);
snprintf(Minutes_cnt_text, 5, "%d", Minutes_cnt);
snprintf(Seconds_cnt_text, 5, "%d", Seconds_cnt);
text_layer_set_text(Hrs, Hours_cnt_text);
text_layer_set_text(Mins, Minutes_cnt_text);
text_layer_set_text(Secs, Seconds_cnt_text);
}

void flash_text_layer(TextLayer *text, int color)
{
flash_color = !color;
LayerToFlash = text;
FlashTimer = app_timer_register(500, flash_callback, NULL);
}

void stop_flashing_text_layer(void)
{
app_timer_cancel(FlashTimer);
GColor C;
if ( flash_color == 1 )
C = GColorBlack;
else
C = GColorWhite;
text_layer_set_text_color(LayerToFlash, C);
}

void add_timer(void)
{
long Seconds = Seconds_cnt + (Minutes_cnt * 60) + (Hours_cnt * 3600);
struct TimerItem *t = malloc(sizeof(struct TimerItem));
if ( t != NULL )
{
t->time = Seconds;
t->starttime = Seconds;
t->update = 1;
snprintf(t->name, 20, "Timer %d", Timer_Number++);
add_item(CountdownTimers, t, -1);
while(window_stack_remove(window, 0));
window_stack_push(window, 0);
window_stack_push(TimerListWindow, 1);
menu_layer_reload_data(TimerListMenu);
layer_mark_dirty(menu_layer_get_layer(TimerListMenu));
menu_layer_set_selected_index(TimerListMenu, (MenuIndex){.row = item_count(CountdownTimers)-1, .section = 0}, MenuRowAlignTop, 1);
}
else
show_message("Could Not Add Timer.", 2000);
}

static void clear_message(void *data)
{
light_enable(0);
DisplayingMessage = 0;
layer_remove_from_parent(bitmap_layer_get_layer(BLayer));
layer_set_hidden(text_layer_get_layer(MessageText), 1);
layer_set_hidden(bitmap_layer_get_layer(BLayer), 1);
layer_mark_dirty(window_get_root_layer(window_stack_get_top_window()));
}

void show_message(char *message, int time)
{
DisplayingMessage = 1;
layer_remove_from_parent(bitmap_layer_get_layer(BLayer));
layer_add_child(window_get_root_layer(window_stack_get_top_window()), bitmap_layer_get_layer(BLayer));
text_layer_set_text(MessageText, message);
layer_set_hidden(text_layer_get_layer(MessageText), 0);
layer_set_hidden(bitmap_layer_get_layer(BLayer), 0);
layer_mark_dirty(bitmap_layer_get_layer(BLayer));
MessageTimer = app_timer_register(time, clear_message, NULL);
light_enable(1);
}

void half_black_me(GContext *ctx, GRect area)
{
graphics_context_set_compositing_mode(ctx, GCompOpSet);
graphics_draw_bitmap_in_rect(ctx, half_black, area);
}

void kill_animation(struct Animation *animation, bool finished, void *context)
{
property_animation_destroy((PropertyAnimation*)animation);
}

void create_animation(struct Layer *layer, GRect *start, GRect *end, uint32_t delay, AnimationCurve curve, uint32_t duration)
{
  struct PropertyAnimation *animation = property_animation_create_layer_frame(layer, start, end);
  animation_set_delay((Animation*)animation, delay);
  animation_set_curve((Animation*)animation, curve);
  animation_set_duration((Animation*)animation,duration);
  animation_set_handlers((Animation*)animation, (AnimationHandlers){.started = NULL, .stopped = kill_animation}, NULL);
  animation_schedule((Animation *)animation);
}


/* SETUP */

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  GRect Hrs_rect = GRect(0, 0, (int)(bounds.size.w/2), 60);
  GRect Mins_rect = GRect((int)(bounds.size.w/2), 0, (int)(bounds.size.w/2), 60);
  GRect Secs_rect = GRect(0, 75, (int)(bounds.size.w/2), 40);
  GRect Secs_label_rect = GRect(0, 103, (int)(bounds.size.w/2), 20);
  GRect Mins_label_rect = GRect((int)(bounds.size.w/2), 50, (int)(bounds.size.w/2), 20);
  GRect Hrs_label_rect = GRect(0, 50, (int)(bounds.size.w/2), 20);
  GRect Start_rect = GRect((int)(bounds.size.w/2), 75, (int)(bounds.size.w/2), 40);
  GRect Mode_rect = GRect(((int)(bounds.size.w/2))-(120/2), 130, 120, 20);

  Hrs = text_layer_create( GRectZero );
  text_layer_set_text(Hrs, "24");
  text_layer_set_text_alignment(Hrs, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(Hrs));
  text_layer_set_font(Hrs, fonts_get_system_font("RESOURCE_ID_ROBOTO_BOLD_SUBSET_49"));
  create_animation(
		text_layer_get_layer(Hrs),
		&GRect(
		Hrs_rect.origin.x,
		Hrs_rect.origin.y + bounds.size.h,
		Hrs_rect.size.w,
		Hrs_rect.size.h),
		&Hrs_rect,
		100,
		AnimationCurveEaseInOut,
		1100);

  Mins = text_layer_create(GRectZero);
  text_layer_set_text(Mins, "59");
  text_layer_set_text_alignment(Mins, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(Mins));
  text_layer_set_font(Mins, fonts_get_system_font("RESOURCE_ID_ROBOTO_BOLD_SUBSET_49"));
  create_animation(
		text_layer_get_layer(Mins),
		&GRect(
		Mins_rect.origin.x,
		Mins_rect.origin.y + bounds.size.h,
		Mins_rect.size.w,
		Mins_rect.size.h),
		&Mins_rect,
		100,
		AnimationCurveEaseInOut,
		1000);

  Secs = text_layer_create(Secs_rect);
  text_layer_set_text(Secs, "59");
  text_layer_set_text_alignment(Secs, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(Secs));
  text_layer_set_font(Secs, fonts_get_system_font("RESOURCE_ID_DROID_SERIF_28_BOLD"));

  create_animation(
		text_layer_get_layer(Secs),
		&GRect(
		Secs_rect.origin.x,
		Secs_rect.origin.y + bounds.size.h,
		Secs_rect.size.w,
		Secs_rect.size.h),
		&Secs_rect,
		100,
		AnimationCurveEaseInOut,
		1200);

  Secs_label = text_layer_create(GRectZero);
  text_layer_set_text(Secs_label, "Seconds");
  text_layer_set_text_alignment(Secs_label, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(Secs_label));
  text_layer_set_background_color(Secs_label, GColorClear);
  text_layer_set_text_color(Secs_label, GColorWhite);
  text_layer_set_font(Secs_label, fonts_get_system_font("RESOURCE_ID_GOTHIC_14"));

  create_animation(
		text_layer_get_layer(Secs_label),
		&GRect(
		Secs_label_rect.origin.x - bounds.size.w,
		Secs_label_rect.origin.y,
		Secs_label_rect.size.w,
		Secs_label_rect.size.h),
		&Secs_label_rect,
		700,
		AnimationCurveEaseInOut,
		800);

  Mins_label = text_layer_create(GRectZero);
  text_layer_set_text(Mins_label, "Minutes");
  text_layer_set_text_alignment(Mins_label, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(Mins_label));
  text_layer_set_background_color(Mins_label, GColorClear);
  text_layer_set_text_color(Mins_label, GColorWhite);
  text_layer_set_font(Mins_label, fonts_get_system_font("RESOURCE_ID_GOTHIC_14"));

  create_animation(
		text_layer_get_layer(Mins_label),
		&GRect(
		Mins_label_rect.origin.x - bounds.size.w,
		Mins_label_rect.origin.y,
		Mins_label_rect.size.w,
		Mins_label_rect.size.h),
		&Mins_label_rect,
		700,
		AnimationCurveEaseInOut,
		700);

  Hrs_label = text_layer_create(GRectZero);
  text_layer_set_text(Hrs_label, "Hours");
  text_layer_set_text_alignment(Hrs_label, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(Hrs_label));
  text_layer_set_background_color(Hrs_label, GColorClear);
  text_layer_set_text_color(Hrs_label, GColorWhite);
  text_layer_set_font(Hrs_label, fonts_get_system_font("RESOURCE_ID_GOTHIC_14"));

  create_animation(
		text_layer_get_layer(Hrs_label),
		&GRect(
		Hrs_label_rect.origin.x - bounds.size.w,
		Hrs_label_rect.origin.y,
		Hrs_label_rect.size.w,
		Hrs_label_rect.size.h),
		&Hrs_label_rect,
		700,
		AnimationCurveEaseInOut,
		750);

  Start = text_layer_create(GRectZero);
  text_layer_set_text(Start, "Go");
  text_layer_set_text_alignment(Start, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(Start));
  text_layer_set_font(Start, fonts_get_system_font("RESOURCE_ID_GOTHIC_28_BOLD"));

  create_animation(
		text_layer_get_layer(Start),
		&GRect(
		Start_rect.size.w + bounds.size.w,
		Start_rect.origin.y,
		Start_rect.size.w,
		Start_rect.size.h),
		&Start_rect,
		0,
		AnimationCurveEaseInOut,
		600);

  Mode = text_layer_create(GRectZero);
  text_layer_set_text(Mode, "Switch to Stopwatch");
  text_layer_set_text_alignment(Mode, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(Mode));
  text_layer_set_font(Mode, fonts_get_system_font("RESOURCE_ID_GOTHIC_14"));

  create_animation(
		text_layer_get_layer(Mode),
		&GRect(
		Mode_rect.origin.x,
		bounds.size.h,
		Mode_rect.size.w,
		Mode_rect.size.h),
		&Mode_rect,
		800,
		AnimationCurveEaseInOut,
		500);

  BLayer = bitmap_layer_create(bounds);
  layer_add_child(window_layer, bitmap_layer_get_layer(BLayer));
  layer_set_hidden(bitmap_layer_get_layer(BLayer), 1);

  GRect MenuOptionBox = GRect(117, 86, 18, 18);
  
  MenuOption = bitmap_layer_create(MenuOptionBox);
  bitmap_layer_set_compositing_mode(MenuOption, GCompOpAnd);
  bitmap_layer_set_bitmap(MenuOption,Menu_icon);
  layer_set_hidden(bitmap_layer_get_layer(MenuOption), 1);
  layer_add_child(window_layer, bitmap_layer_get_layer(MenuOption));

  MessageText = text_layer_create((GRect){.origin = {5, ((int)(bounds.size.h/2))-18}, .size = {bounds.size.w-10, 40} });
  text_layer_set_text(MessageText, "");
  text_layer_set_text_alignment(MessageText, GTextAlignmentCenter);
  layer_add_child(bitmap_layer_get_layer(BLayer), text_layer_get_layer(MessageText));
  text_layer_set_font(MessageText, fonts_get_system_font("RESOURCE_ID_GOTHIC_24"));
  text_layer_set_background_color(MessageText, GColorClear);

  layer_set_update_proc(bitmap_layer_get_layer(BLayer), BLayer_Update_Proc);
  update_numbers();
  redraw_selection();
}

static void window_unload(Window *window) {
  text_layer_destroy(Hrs);
  text_layer_destroy(Hrs_label);
  text_layer_destroy(Mins);
  text_layer_destroy(Mins_label);
  text_layer_destroy(Secs);
  text_layer_destroy(Secs_label);
  text_layer_destroy(Start);
  text_layer_destroy(Mode);

  bitmap_layer_destroy(MenuOption);

}

static void init(void) {

  half_black = gbitmap_create_with_resource(RESOURCE_ID_HALF_BLACKER);
  Menu_icon = gbitmap_create_with_resource(RESOURCE_ID_MENU_ICON);
  Action_next_icon = gbitmap_create_with_resource(RESOURCE_ID_ACTION_NEXT);
  Caret_icon = gbitmap_create_with_resource(RESOURCE_ID_CARET_ICON);

  SplitList = create_list();

  CountdownTimers = create_list();

  if (CountdownTimers == NULL)
  window_stack_pop_all(0);

  //Initialize stuff from memory
  Hours_cnt = persist_exists(PERSIST_KEY_HRS) ? persist_read_int(PERSIST_KEY_HRS) : 0;
  Minutes_cnt = persist_exists(PERSIST_KEY_MINS) ? persist_read_int(PERSIST_KEY_MINS) : 5;
  Seconds_cnt = persist_exists(PERSIST_KEY_SECS) ? persist_read_int(PERSIST_KEY_SECS) : 0;
   // Timers
  int timers_cnt = persist_read_int(PERSIST_KEY_TIMERS_COUNT); // Get the number of timers stored in persistent memory
  int bytes = (timers_cnt * sizeof(struct TimerItem)); // Find the number of bytes saved in memory
  char buffer[bytes]; // Create a buffer to hold the data in
  persist_read_data(PERSIST_KEY_TIMERS, buffer, bytes); // Read the data into the buffer
  read_from_data_to_list(buffer, sizeof(struct TimerItem), timers_cnt, CountdownTimers); // Rip the data into a buffer
   // Subtract elapsed time from saved timers
  time_t exit_time, current_time = time(0);
  persist_read_data(PERSIST_KEY_EXIT_TIME, &exit_time, sizeof(time_t));
  int x;
  for (x = 0; x < item_count(CountdownTimers); x++)
  {
   struct TimerItem *t = get_item_by_index(x, CountdownTimers)->data;
   if (t->update)
   t->time -= (current_time - exit_time);
  }

  Timer_Number = x+1;

  tick_timer_service_subscribe(SECOND_UNIT, timer_callback);

  //Initialize Window 1

  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_set_background_color(window, GColorBlue);
  // Push the main window
  window_stack_push(window, 1);

  //Initialize Window 2

  TimerListWindow = window_create();
  window_set_window_handlers(TimerListWindow, (WindowHandlers) {
    .load = TimerList_load,
    .unload = TimerList_unload,
  });

  if (x)
  window_stack_push(TimerListWindow, 1);

  //Initialize Window 3

 TimerMenu = window_create();
  window_set_window_handlers(TimerMenu, (WindowHandlers) {
    .load = TimerMenu_load,
    .unload = TimerMenu_unload,
  });


  //Initialize Window 5

  StopwatchWindow = window_create();
  window_set_window_handlers(StopwatchWindow, (WindowHandlers) {
    .load = StopwatchWindow_load,
    .unload = StopwatchWindow_unload,
  });
  window_set_background_color(StopwatchWindow, GColorBlack);

  //Initialize Window 5

  StopwatchSplits = window_create();
  window_set_window_handlers(StopwatchSplits, (WindowHandlers) {
    .load = StopwatchSplits_load,
    .unload = StopwatchSplits_unload,
  });

  //Initialize The Notifier Window

  NotifierWindow = window_create();
  window_set_window_handlers(NotifierWindow, (WindowHandlers) {
    .load = NotifierWindow_load,
    .unload = NotifierWindow_unload,
  });
    window_set_click_config_provider(NotifierWindow, NotifierWindow_click_config_provider);

}

static void deinit(void) {

  // Write CountdownTimers to a persistent memory
  char *buffer = serialize_to_new_byte_array(CountdownTimers, sizeof(struct TimerItem));
  if (buffer != NULL)
  {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Write: %d", persist_write_data(PERSIST_KEY_TIMERS, buffer, sizeof(struct TimerItem) * item_count(CountdownTimers)));
  free(buffer);
  }
  persist_write_int(PERSIST_KEY_TIMERS_COUNT, item_count(CountdownTimers));
  persist_write_int(PERSIST_KEY_HRS, Hours_cnt);
  persist_write_int(PERSIST_KEY_MINS, Minutes_cnt);
  persist_write_int(PERSIST_KEY_SECS, Seconds_cnt);

  destroy_list(CountdownTimers);
  destroy_list(SplitList);
  text_layer_destroy(MessageText);
  bitmap_layer_destroy(BLayer);

  gbitmap_destroy(half_black);
  gbitmap_destroy(Menu_icon);
  gbitmap_destroy(Loopback_icon);
  gbitmap_destroy(Plus_Sign_icon);
  gbitmap_destroy(Trashcan_icon);
  gbitmap_destroy(Stopwatch_icon);
  gbitmap_destroy(Pause_icon);
  gbitmap_destroy(Play_icon);
  gbitmap_destroy(Action_next_icon);
  gbitmap_destroy(Trophy_icon);
  gbitmap_destroy(Caret_icon);

  window_destroy(window);
  window_destroy(TimerListWindow);
  window_destroy(TimerMenu);
  window_destroy(StopwatchWindow);
  window_destroy(StopwatchSplits);

  //Remember the exact exit time of the app
  time_t t = time(0);
  persist_write_data(PERSIST_KEY_EXIT_TIME, &t, sizeof(time_t));
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();

  deinit();
}

/* ----------------------TIMER LIST--------------------*/
struct TimerListItem {
	struct TimerItem *AssociatedTimer;
	Layer *ItemLayer;
};

static GBitmap *slash;

static void /*MenuLayerDrawRowCallback*/ DrawTimerListItem(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context);
static int16_t /*MenuLayerGetCellHeightCallback*/ GetTimerListCellHeight(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context);
static uint16_t /*MenuLayerGetNumberOfRowsInSectionsCallback*/ GetTimerListNumberOfRowsInSections(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context);
static void /*MenuLayerSelectCallback*/ TimerListMenuSelectCallback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context);
static void /*MenuLayerSelectionChangedCallback*/ TimerListMenuSelectionChangedCallback(struct MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context);

static MenuLayerCallbacks TimerListMenuLayerCallbacks = { 
.draw_header = NULL,
.draw_row = DrawTimerListItem,
.get_cell_height = GetTimerListCellHeight,
.get_header_height = NULL,
.get_num_rows = GetTimerListNumberOfRowsInSections,
.get_num_sections = NULL,
.select_click = TimerListMenuSelectCallback,
.select_long_click = NULL,
.selection_changed = TimerListMenuSelectionChangedCallback, };

static void TimerList_load(Window *window) {
Layer *window_layer = window_get_root_layer(TimerListWindow);
GRect bounds = layer_get_bounds(window_layer);

TimerListMenu = menu_layer_create(bounds);
menu_layer_set_callbacks(TimerListMenu, NULL, TimerListMenuLayerCallbacks);
menu_layer_set_click_config_onto_window(TimerListMenu, TimerListWindow);
layer_add_child(window_layer, menu_layer_get_layer(TimerListMenu));

slash = gbitmap_create_with_resource(RESOURCE_ID_SLASH);

if (item_count(CountdownTimers) == 0)
{
snprintf(Message, 50, "No timers left.");
show_message(Message, 2000);
}

}

static void /*MenuLayerDrawRowCallback*/ DrawTimerListItem(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context)
{
if (cell_index->row < item_count(CountdownTimers))
{
GRect bounds = GRectZero;
bounds.size = layer_get_frame((Layer *)cell_layer).size;
struct TimerItem *ti = (struct TimerItem *)(get_item_by_index(cell_index->row, CountdownTimers)->data);

char Current_Timer_Text[50];
graphics_context_set_text_color(ctx, GColorBlack);

snprintf(Current_Timer_Text, 20, "%s", ti->name);
GRect title_bounds = bounds;
title_bounds.size.h = 30;
title_bounds.origin.y = 2;
graphics_draw_text(ctx, (const char *)Current_Timer_Text, fonts_get_system_font("RESOURCE_ID_GOTHIC_24_BOLD"), title_bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

half_black_me(ctx, title_bounds);

//Time Variables
int hour = ti->time/3600;
int min = ((ti->time-(hour*3600))/60);
int sec = ti->time-(hour*3600)-(min*60);
if (hour > 0 && min > 0 && sec >= 0)
snprintf(Current_Timer_Text, 20, "%dh %dm %ds", hour, min, sec);
else if (min > 0 && sec >= 0)
snprintf(Current_Timer_Text, 20, "%dm %ds", min, sec);
else
snprintf(Current_Timer_Text, 20, "%ds",sec);
MenuIndex current_selection = menu_layer_get_selected_index(TimerListMenu);
GRect progress_bounds = bounds;
progress_bounds.origin.y = bounds.size.h - 23;
progress_bounds.size.h = 6;
double percent_completed = (double)ti->time / (double)ti->starttime;
progress_bounds.size.w = (int)((bounds.size.w-15) * percent_completed );
progress_bounds.origin.x = (bounds.size.w/2)-(progress_bounds.size.w/2);
graphics_fill_rect(ctx, progress_bounds, 2, GCornersAll);

graphics_draw_round_rect(ctx, bounds, 3);
GRect timer_bounds = bounds;
timer_bounds.size.h = 50;
timer_bounds.origin.y = ((bounds.size.h/2) - (timer_bounds.size.h/2))+7;
graphics_draw_text(ctx, (const char *)Current_Timer_Text, fonts_get_system_font("RESOURCE_ID_GOTHIC_28_BOLD"), timer_bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

if ( !ti->update )
{
GRect paused_bounds = bounds;
paused_bounds.origin.x = 18;
paused_bounds.origin.y = 10;
paused_bounds.size.w = 18;
paused_bounds.size.h = 18;
graphics_context_set_compositing_mode(ctx, GCompOpAssign);
graphics_draw_bitmap_in_rect(ctx, Pause_icon, paused_bounds);
half_black_me(ctx, paused_bounds);
}

if ( menu_index_compare(&current_selection, cell_index) != 0 )
{//If NOT drawing the current selection...
graphics_draw_bitmap_in_rect(ctx, slash, progress_bounds);
}
else
{//If drawing the current selection...
GRect caret_bounds = bounds;
caret_bounds.origin.x = (bounds.size.w - (18 + 5));
caret_bounds.origin.y = ((bounds.size.h/2) - (18/2));
caret_bounds.size.w = 18;
caret_bounds.size.h = 18;
graphics_context_set_compositing_mode(ctx, GCompOpAssign);
graphics_draw_bitmap_in_rect(ctx, Caret_icon, caret_bounds);
}

}
else
menu_cell_basic_draw(ctx, cell_layer, "Create another", "Return to the timer creation menu", NULL);
}

static int16_t /*MenuLayerGetCellHeightCallback*/ GetTimerListCellHeight(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context)
{
if (!(cell_index->row >= item_count(CountdownTimers)))
return (layer_get_frame(window_get_root_layer(TimerListWindow))).size.h*.56;
else
return 55;
}

static uint16_t /*MenuLayerGetNumberOfRowsInSectionsCallback*/ GetTimerListNumberOfRowsInSections(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context)
{
return item_count(CountdownTimers) + 1;
}

static void /*MenuLayerSelectCallback*/ TimerListMenuSelectCallback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context)
{
if ( TimerListSelectedIndex != item_count(CountdownTimers) )
{
window_stack_push(TimerMenu, 1);
}
else
{
window_stack_push(window, 1);
}
}

static void /*MenuLayerSelectionChangedCallback*/ TimerListMenuSelectionChangedCallback(struct MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context)
{
TimerListSelectedIndex = new_index.row;
}

void cancel_countdown(item *CountdownItem)
{
struct TimerItem *t = (struct TimerItem *)(CountdownItem->data);
snprintf(Message, 50, "%s cancelled.", t->name);
show_message(Message, 2000);
}

void timer_finished(item *t)
{
uint32_t segments[] = {400, 50, 300, 50, 500};
VibePattern pattern = {.durations = segments, .num_segments = ARRAY_LENGTH(segments)};
vibes_enqueue_custom_pattern(pattern);
menu_layer_reload_data(TimerListMenu);
layer_mark_dirty(menu_layer_get_layer(TimerListMenu));
struct TimerItem *ti  = (struct TimerItem *)(t->data);
snprintf(Message, 50, "%s", ti->name);
char Time[20];
//Time Variables
int hour = ti->starttime/3600;
int min = ((ti->starttime-(hour*3600))/60);
int sec = ti->starttime-(hour*3600)-(min*60);
if (hour > 0 && min > 0 && sec >= 0)
snprintf(Time, 20, "%dh %dm %ds", hour, min, sec);
else if (min > 0 && sec >= 0)
snprintf(Time, 20, "%dm %ds", min, sec);
else
snprintf(Time, 20, "%ds",sec);
//show_message(Message, 2000);
remove_item(CountdownTimers, t);
if ( window_stack_contains_window(TimerMenu) )
window_stack_remove(TimerMenu, 0);
if (item_count(CountdownTimers) == 0)
{
Notify(Message, "The last timer has just gone off.");
window_stack_remove(TimerListWindow, 1);
if ( window_stack_contains_window(window) )
redraw_selection();
}
else
Notify(Message, "A Timer has finished!");
}

static void TimerList_unload(Window *window) {
menu_layer_destroy(TimerListMenu);
gbitmap_destroy(slash);
}

/* ----------------------TIMER MENU--------------------*/

static MenuLayer *TheTimerMenu;

static void /*MenuLayerDrawHeaderCallback*/ DrawTimerMenuHeader(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *callback_context);
static void /*MenuLayerDrawRowCallback*/ DrawTimerMenuItem(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context);
static int16_t /*MenuLayerGetHeaderHeightCallback */ GetTimerMenuHeaderHeight( struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context);
static uint16_t /*MenuLayerGetNumberOfRowsInSectionsCallback*/ GetTimerMenuNumberOfRowsInSections(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context);
static uint16_t /*MenuLayerGetNumberOfSectionsCallback*/ TimerMenuGetNumberOfSections(struct MenuLayer *menu_layer, void *callback_context);
static void /*MenuLayerSelectCallback*/ TimerMenuSelectCallback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context);
static void /*MenuLayerSelectionChangedCallback*/ TimerMenuSelectionChangedCallback(struct MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context);

static MenuLayerCallbacks TimerMenuCallbacks = { 
.draw_header = DrawTimerMenuHeader,
.draw_row = DrawTimerMenuItem,
.get_cell_height = NULL,
.get_header_height = GetTimerMenuHeaderHeight,
.get_num_rows = GetTimerMenuNumberOfRowsInSections,
.get_num_sections = TimerMenuGetNumberOfSections,
.select_click = TimerMenuSelectCallback,
.select_long_click = NULL,
.selection_changed = TimerMenuSelectionChangedCallback, };

static void TimerMenu_load(Window *window) {
  Loopback_icon = gbitmap_create_with_resource(RESOURCE_ID_LOOPBACK); //
  Plus_Sign_icon = gbitmap_create_with_resource(RESOURCE_ID_PLUS_SIGN); //
  Trashcan_icon = gbitmap_create_with_resource(RESOURCE_ID_TRASHCAN_ICON); //
  Stopwatch_icon = gbitmap_create_with_resource(RESOURCE_ID_STOPWATCH_ICON); //
  Pause_icon = gbitmap_create_with_resource(RESOURCE_ID_PAUSE_ICON); //
  Play_icon = gbitmap_create_with_resource(RESOURCE_ID_PLAY_ICON); //
Layer *window_layer = window_get_root_layer(TimerMenu);
GRect bounds = layer_get_bounds(window_layer);
TheTimerMenu = menu_layer_create(bounds);
menu_layer_set_callbacks(TheTimerMenu, NULL, TimerMenuCallbacks);
menu_layer_set_click_config_onto_window(TheTimerMenu, TimerMenu);
layer_add_child(window_layer, menu_layer_get_layer(TheTimerMenu));
}

static void /*MenuLayerDrawHeaderCallback*/ DrawTimerMenuHeader(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *callback_context)
{
switch (section_index)
{
case 0:
{
menu_cell_basic_header_draw(ctx, cell_layer, "Timer");
break;
};
case 1:
{
menu_cell_basic_header_draw(ctx, cell_layer, "Navigation");
};
};
}

static void /*MenuLayerDrawRowCallback*/ DrawTimerMenuItem(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context)
{
switch (cell_index->section)
{
case 0: //Section 1
{

switch (cell_index->row)
{
case 0: //Delete timer
{
menu_cell_basic_draw(ctx, cell_layer, "Delete Timer", NULL, Trashcan_icon);
break;
};
case 1: //Pause timer
{
struct TimerItem *ti = (struct TimerItem *)(get_item_by_index(TimerListSelectedIndex, CountdownTimers)->data);
if ( ti->update )
menu_cell_basic_draw(ctx, cell_layer, "Pause Timer", NULL, Pause_icon);
else
menu_cell_basic_draw(ctx, cell_layer, "Un-Pause", "Continue timer", Play_icon);
break;
};
case 2: //Reset timer
{
menu_cell_basic_draw(ctx, cell_layer, "Reset Timer", NULL, Loopback_icon);
break;
};
case 3: //New timer
{
menu_cell_basic_draw(ctx, cell_layer, "New Timer", NULL, Plus_Sign_icon);
break;
};
};
break;
};
case 1: //Section 2
{
switch (cell_index->row)
{
case 0: //Stopwatch link
{
menu_cell_basic_draw(ctx, cell_layer, "Stopwatches", "Go to stopwatches", Stopwatch_icon);
};
};
};
}
}

static int16_t /*MenuLayerGetHeaderHeightCallback */ GetTimerMenuHeaderHeight( struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context)
{
return 18;
}

static uint16_t /*MenuLayerGetNumberOfRowsInSectionsCallback*/ GetTimerMenuNumberOfRowsInSections(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context)
{

switch (section_index)
{

case 0:
{
return 4;
break;
};

case 1:
{
return 1;
};

};

return 0;

}

static uint16_t /*MenuLayerGetNumberOfSectionsCallback*/ TimerMenuGetNumberOfSections(struct MenuLayer *menu_layer, void *callback_context)
{
return 2;
}

static void /*MenuLayerSelectCallback*/ TimerMenuSelectCallback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context)
{
switch ( cell_index->section  )
{

case 0: //Section 1
{

switch (cell_index->row)
{
case 0: //Delete button
{
remove_at(CountdownTimers, TimerListSelectedIndex);
window_stack_remove(TimerMenu, 1);
if ( item_count(CountdownTimers) > 0 )
{
menu_layer_set_selected_index(TimerListMenu, (MenuIndex){.row = 0, .section = 0}, MenuRowAlignCenter, 1);
show_message("Item deleted.", 1000);
}
else
{
window_stack_pop(1);
show_message("No more timers.", 2000);
redraw_selection();
}
break;
};

case 1: //Pause button
{
item *i = get_item_by_index(TimerListSelectedIndex, CountdownTimers);
struct TimerItem *ti = (struct TimerItem *)(i->data);
ti->update = !ti->update;
window_stack_remove(TimerMenu, 1);
if (ti->update)
show_message("Item un-paused.", 1000);
else
show_message("Item paused.", 1000);
break;
};

case 2: //Reset button
{
item *i = get_item_by_index(TimerListSelectedIndex, CountdownTimers);
struct TimerItem *ti = (struct TimerItem *)(i->data);
ti->time = ti->starttime;
window_stack_remove(TimerMenu, 1);
show_message("Item reset.", 1000);
break;
};

case 3: //New Timer Button
{
window_stack_pop_all(1);
window_stack_push(window, 1);
};

};
break;
};

case 1: //Section 2
{
switch(cell_index->row)
{
case 0: //Stopwatch link
{
window_stack_pop_all(0);
window_stack_push(window, 0);
window_stack_push(StopwatchWindow, 1);
};
};

break;
};

};

}

static void /*MenuLayerSelectionChangedCallback*/ TimerMenuSelectionChangedCallback(struct MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context)
{

}

static void TimerMenu_unload(Window *window) {
  gbitmap_destroy(Loopback_icon);
  gbitmap_destroy(Plus_Sign_icon);
  gbitmap_destroy(Trashcan_icon);
  gbitmap_destroy(Stopwatch_icon);
  gbitmap_destroy(Pause_icon);
  gbitmap_destroy(Play_icon);
  menu_layer_destroy(TheTimerMenu);
}

/* ----------------------STOPWATCH WINDOW--------------------*/

//static MenuLayer *StopwatchList;

struct StopwatchDot{
	int position;
	int size;
};

static AppTimer *StopwatchTimer;
bool StopwatchTimerRunning = 0;
static TextLayer *StopwatchSecondsLabel;
static TextLayer *StopwatchMinutesLabel;
static TextLayer *StopwatchHoursLabel;
static TextLayer *StopwatchSecondsLabellabel;
static TextLayer *StopwatchMinutesLabellabel;
static TextLayer *StopwatchHoursLabellabel;
static TextLayer *SplitLabelHeader;
static InverterLayer *StopwatchProgress;
static BitmapLayer *TIconBLayer;
static char Current_Hours_Text[10];
static char Current_Minutes_Text[10];
static char Current_Seconds_Text[10];
static char Current_Best_Time_Text[30];
static long Dots_out_of = 100;
static int swipes = 0;
static bool swipe_direction_down = 1;
static long lowest_split = 0;
static long last_split_at = 0;

static void StopwatchWindow_load(Window *window) {
Layer *window_layer = window_get_root_layer(StopwatchWindow);

  Loopback_icon = gbitmap_create_with_resource(RESOURCE_ID_LOOPBACK); //
  Plus_Sign_icon = gbitmap_create_with_resource(RESOURCE_ID_PLUS_SIGN); //
  Pause_icon = gbitmap_create_with_resource(RESOURCE_ID_PAUSE_ICON); //
  Play_icon = gbitmap_create_with_resource(RESOURCE_ID_PLAY_ICON); //

//Seconds
GRect StopwatchSecondsBox = GRectZero;
StopwatchSecondsBox.size.w = 130;
StopwatchSecondsBox.size.h = 100;
StopwatchSecondsBox.origin.x = 8;
StopwatchSecondsBox.origin.y = 0;

StopwatchSecondsLabel = text_layer_create(StopwatchSecondsBox);
text_layer_set_font(StopwatchSecondsLabel, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
text_layer_set_text_alignment(StopwatchSecondsLabel, GTextAlignmentLeft);
text_layer_set_background_color(StopwatchSecondsLabel, GColorClear);
text_layer_set_text_color(StopwatchSecondsLabel, GColorWhite);
text_layer_set_text(StopwatchSecondsLabel, "0.0");
layer_add_child(window_layer, text_layer_get_layer(StopwatchSecondsLabel));

//Minutes
GRect StopwatchMinutesBox = GRectZero;
StopwatchMinutesBox.size.w = 130;
StopwatchMinutesBox.size.h = 100;
StopwatchMinutesBox.origin.x = 8;
StopwatchMinutesBox.origin.y = 45;

StopwatchMinutesLabel = text_layer_create(StopwatchMinutesBox);
text_layer_set_font(StopwatchMinutesLabel, fonts_get_system_font(FONT_KEY_BITHAM_42_MEDIUM_NUMBERS));
text_layer_set_text_alignment(StopwatchMinutesLabel, GTextAlignmentLeft);
text_layer_set_background_color(StopwatchMinutesLabel, GColorClear);
text_layer_set_text_color(StopwatchMinutesLabel, GColorWhite);
layer_set_hidden(text_layer_get_layer(StopwatchMinutesLabel), 1);
text_layer_set_text(StopwatchMinutesLabel, "60");
layer_add_child(window_layer, text_layer_get_layer(StopwatchMinutesLabel));

//Hours
GRect StopwatchHoursBox = GRectZero;
StopwatchHoursBox.size.w = 130;
StopwatchHoursBox.size.h = 100;
StopwatchHoursBox.origin.x = 8;
StopwatchHoursBox.origin.y = 90;

StopwatchHoursLabel = text_layer_create(StopwatchHoursBox);
text_layer_set_font(StopwatchHoursLabel, fonts_get_system_font(FONT_KEY_BITHAM_42_MEDIUM_NUMBERS));
text_layer_set_text_alignment(StopwatchHoursLabel, GTextAlignmentLeft);
text_layer_set_background_color(StopwatchHoursLabel, GColorClear);
text_layer_set_text_color(StopwatchHoursLabel, GColorWhite);
layer_set_hidden(text_layer_get_layer(StopwatchHoursLabel), 1);
text_layer_set_text(StopwatchHoursLabel, "24");
layer_add_child(window_layer, text_layer_get_layer(StopwatchHoursLabel));

/* LABELS */


const int labels_down_by = 40;
const int label_out_by = 12;

//Seconds label
GRect StopwatchSecondslabelBox = GRectZero;
StopwatchSecondslabelBox.size.w = 50;
StopwatchSecondslabelBox.size.h = 18;
StopwatchSecondslabelBox.origin.x = label_out_by;
StopwatchSecondslabelBox.origin.y = StopwatchSecondsBox.origin.y + labels_down_by;

StopwatchSecondsLabellabel = text_layer_create(StopwatchSecondslabelBox);
text_layer_set_font(StopwatchSecondsLabellabel, fonts_get_system_font(FONT_KEY_GOTHIC_14));
text_layer_set_text_alignment(StopwatchSecondsLabellabel, GTextAlignmentLeft);
text_layer_set_background_color(StopwatchSecondsLabellabel, GColorClear);
text_layer_set_text_color(StopwatchSecondsLabellabel, GColorWhite);
text_layer_set_text(StopwatchSecondsLabellabel, "seconds");
layer_add_child(window_layer, text_layer_get_layer(StopwatchSecondsLabellabel));

//Minutes label
GRect StopwatchMinuteslabelBox = GRectZero;
StopwatchMinuteslabelBox.size.w = 50;
StopwatchMinuteslabelBox.size.h = 18;
StopwatchMinuteslabelBox.origin.x = label_out_by;
StopwatchMinuteslabelBox.origin.y = StopwatchMinutesBox.origin.y + labels_down_by;

StopwatchMinutesLabellabel = text_layer_create(StopwatchMinuteslabelBox);
text_layer_set_font(StopwatchMinutesLabellabel, fonts_get_system_font(FONT_KEY_GOTHIC_14));
text_layer_set_text_alignment(StopwatchMinutesLabellabel, GTextAlignmentLeft);
text_layer_set_background_color(StopwatchMinutesLabellabel, GColorClear);
text_layer_set_text_color(StopwatchMinutesLabellabel, GColorWhite);
layer_set_hidden(text_layer_get_layer(StopwatchMinutesLabellabel), 1);
text_layer_set_text(StopwatchMinutesLabellabel, "minutes");
layer_add_child(window_layer, text_layer_get_layer(StopwatchMinutesLabellabel));

//Hours label
GRect StopwatchHourslabelBox = GRectZero;
StopwatchHourslabelBox.size.w = 50;
StopwatchHourslabelBox.size.h = 18;
StopwatchHourslabelBox.origin.x = label_out_by;
StopwatchHourslabelBox.origin.y = StopwatchHoursBox.origin.y + labels_down_by;

StopwatchHoursLabellabel = text_layer_create(StopwatchHourslabelBox);
text_layer_set_font(StopwatchHoursLabellabel, fonts_get_system_font(FONT_KEY_GOTHIC_14));
text_layer_set_text_alignment(StopwatchHoursLabellabel, GTextAlignmentLeft);
text_layer_set_background_color(StopwatchHoursLabellabel, GColorClear);
text_layer_set_text_color(StopwatchHoursLabellabel, GColorWhite);
layer_set_hidden(text_layer_get_layer(StopwatchHoursLabellabel), 1);
text_layer_set_text(StopwatchHoursLabellabel, "hours");
layer_add_child(window_layer, text_layer_get_layer(StopwatchHoursLabellabel));

StopwatchActionBar = action_bar_layer_create();
action_bar_layer_set_background_color(StopwatchActionBar, GColorWhite);
if ( StopwatchPaused )
action_bar_layer_set_icon(StopwatchActionBar, BUTTON_ID_UP, Play_icon);
else
action_bar_layer_set_icon(StopwatchActionBar, BUTTON_ID_UP, Pause_icon);
//action_bar_layer_set_icon(StopwatchActionBar, BUTTON_ID_SELECT, Loopback_icon);
action_bar_layer_set_icon(StopwatchActionBar, BUTTON_ID_DOWN, Plus_Sign_icon);
action_bar_layer_add_to_window(StopwatchActionBar, StopwatchWindow);
action_bar_layer_set_click_config_provider(StopwatchActionBar, StopwatchWindow_click_config_provider);

//Trophy icon
GRect icBox = GRect(101, 112, 18, 18);

Trophy_icon = gbitmap_create_with_resource(RESOURCE_ID_TROPHY_ICON);
TIconBLayer = bitmap_layer_create(icBox);
bitmap_layer_set_compositing_mode(TIconBLayer, GCompOpSet);
bitmap_layer_set_bitmap(TIconBLayer, Trophy_icon);
layer_add_child(window_layer, bitmap_layer_get_layer(TIconBLayer));
layer_set_hidden(bitmap_layer_get_layer(TIconBLayer), 1);

//Split hint
GRect SplitLabelHeaderBox = GRect(20, 128, 100, 18);

SplitLabelHeader = text_layer_create(SplitLabelHeaderBox);
text_layer_set_font(SplitLabelHeader, fonts_get_system_font(FONT_KEY_GOTHIC_14));
text_layer_set_text_alignment(SplitLabelHeader, GTextAlignmentRight);
text_layer_set_background_color(SplitLabelHeader, GColorClear);
text_layer_set_text_color(SplitLabelHeader, GColorWhite);
text_layer_set_text(SplitLabelHeader, "24h:60m:60.9s");
layer_add_child(window_layer, text_layer_get_layer(SplitLabelHeader));
layer_set_hidden(text_layer_get_layer(SplitLabelHeader), 1);

StopwatchProgress = inverter_layer_create(GRectZero);
layer_add_child(window_layer, inverter_layer_get_layer(StopwatchProgress));
//UpdateStopwatchDisplay();

}

static void UpdateProgress(void)
{
if (last_split_at)
{
float percent = (((float)StopwatchTime - ((float)swipes * (float)Dots_out_of)) - (float)last_split_at) / (float)Dots_out_of;
if ( percent <= 1 )
{
GRect box = GRectZero;
box.size.w = 80;

if ( swipe_direction_down)
{
box.origin.y = 0;
box.size.h = (int)((float)layer_get_frame(window_get_root_layer(StopwatchWindow)).size.h * percent);
}
else
{
box.size.h = layer_get_frame(window_get_root_layer(StopwatchWindow)).size.h - ((float)layer_get_frame(window_get_root_layer(StopwatchWindow)).size.h * percent);
box.origin.y = layer_get_frame(window_get_root_layer(StopwatchWindow)).size.h - box.size.h;
}

layer_set_frame(inverter_layer_get_layer(StopwatchProgress), box);
}
else
{
swipes++;
swipe_direction_down = !swipe_direction_down;
}
}
}

static void PauseStopwatch(void)
{
StopwatchPaused = !StopwatchPaused;
if (StopwatchPaused)
{//Pause
app_timer_cancel(StopwatchTimer);
action_bar_layer_set_icon(StopwatchActionBar, BUTTON_ID_UP, Play_icon);
action_bar_layer_set_icon(StopwatchActionBar, BUTTON_ID_SELECT, Loopback_icon);
layer_set_hidden(inverter_layer_get_layer(StopwatchProgress), 1);
}
else
{//Play
StopwatchTimer = app_timer_register(100, StopwatchCallback, NULL);
action_bar_layer_set_icon(StopwatchActionBar, BUTTON_ID_UP, Pause_icon);
action_bar_layer_clear_icon(StopwatchActionBar, BUTTON_ID_SELECT);
layer_set_hidden(inverter_layer_get_layer(StopwatchProgress), 0);
}
}

static void ResetStopwatch(void)
{

if (StopwatchPaused && (StopwatchTime > 0))
{

vibes_double_pulse();

app_timer_cancel(StopwatchTimer);
StopwatchPaused = 1;
StopwatchTime = 0;
action_bar_layer_set_icon(StopwatchActionBar, BUTTON_ID_UP, Play_icon);
action_bar_layer_clear_icon(StopwatchActionBar, BUTTON_ID_SELECT);
layer_set_hidden(inverter_layer_get_layer(StopwatchProgress), 1);

text_layer_set_text(StopwatchSecondsLabel, "0.0");
text_layer_set_text(StopwatchSecondsLabellabel, "seconds");

layer_set_hidden(text_layer_get_layer(StopwatchHoursLabel), 1);
layer_set_hidden(text_layer_get_layer(StopwatchHoursLabellabel), 1);
layer_set_hidden(text_layer_get_layer(StopwatchMinutesLabel), 1);
layer_set_hidden(text_layer_get_layer(StopwatchMinutesLabellabel), 1);
layer_set_hidden(bitmap_layer_get_layer(TIconBLayer), 1);
layer_set_hidden(text_layer_get_layer(SplitLabelHeader), 1);

swipes = 0;
swipe_direction_down = 1;
lowest_split = 0;
last_split_at = 0;

destroy_list(SplitList);
SplitList = create_list();

layer_set_frame(inverter_layer_get_layer(StopwatchProgress), GRectZero);
}
}


static void StopwatchWindow_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, StopwatchWindow_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, StopwatchWindow_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, StopwatchWindow_down_click_handler);
  window_long_click_subscribe(BUTTON_ID_DOWN, 0, StopwatchWindow_long_down_click_handler, NULL);
}

static void StopwatchWindow_select_click_handler(ClickRecognizerRef recognizer, void *context) {
ResetStopwatch();
}

static void StopwatchWindow_up_click_handler(ClickRecognizerRef recognizer, void *context) {
vibes_short_pulse();
PauseStopwatch();
}

static void StopwatchWindow_down_click_handler(ClickRecognizerRef recognizer, void *context) {
if(StopwatchTime - last_split_at)
{
long *i = malloc(sizeof(long));
if ( i != NULL )
{
*i = StopwatchTime - last_split_at;
add_item(SplitList, i, -1);
vibes_short_pulse();
if ((lowest_split == 0) && (*i > 0))
{
lowest_split = *i;
last_split_at = StopwatchTime;
Dots_out_of = lowest_split;
swipes = 0;
swipe_direction_down = 1;

layer_set_hidden(bitmap_layer_get_layer(TIconBLayer), 0);
layer_set_hidden(text_layer_get_layer(SplitLabelHeader), 0);
}
else if (lowest_split > *i)
{
lowest_split = *i;
}

last_split_at = StopwatchTime;
Dots_out_of = lowest_split;
swipes = 0;
swipe_direction_down = 1;

int hour = lowest_split/36000;
long min = ((lowest_split-(hour*36000))/600);
long sec = (lowest_split-(hour*36000)-(min*600))/10;
long tenth = (lowest_split-(hour*3600)-(min*600)-(sec*10));
if (hour > 0 && min > 0 && sec >= 0)
{
snprintf(Current_Best_Time_Text, 30, "%dh:%dm:%d.%ds", (int)hour, (int)min ,(int)sec , (int)tenth);
}
else if (min > 0 && sec >= 0)
{
snprintf(Current_Best_Time_Text, 30, "%dm:%d.%ds", (int)min, (int)sec , (int)tenth);
}
else
{
snprintf(Current_Best_Time_Text, 30, "%d.%ds",(int)sec, (int)tenth);
}

text_layer_set_text(SplitLabelHeader, Current_Best_Time_Text);
}
}
else
show_message("Can't split at zero.", 1000);
}

static void StopwatchWindow_long_down_click_handler( ClickRecognizerRef recognizer, void *context )
{
if (item_count(SplitList))
{
vibes_short_pulse();
window_stack_push(StopwatchSplits, 1);
}
else
show_message("No splits to show.", 1000);
}

static void UpdateStopwatchDisplay(void)
{
if (StopwatchTime > 36000)
{
layer_set_hidden(text_layer_get_layer(StopwatchHoursLabel), 0);
layer_set_hidden(text_layer_get_layer(StopwatchHoursLabellabel), 0);
}
else
{
layer_set_hidden(text_layer_get_layer(StopwatchHoursLabel), 1);
layer_set_hidden(text_layer_get_layer(StopwatchHoursLabellabel), 1);
}
if (StopwatchTime > 600)
{
layer_set_hidden(text_layer_get_layer(StopwatchMinutesLabel), 0);
layer_set_hidden(text_layer_get_layer(StopwatchMinutesLabellabel), 0);
}
else
{
layer_set_hidden(text_layer_get_layer(StopwatchMinutesLabel), 1);
layer_set_hidden(text_layer_get_layer(StopwatchMinutesLabellabel), 1);
}


int hour = StopwatchTime/36000;
long min = ((StopwatchTime-(hour*36000))/600);
long sec = (StopwatchTime-(hour*36000)-(min*600))/10;
long tenth = (StopwatchTime-(hour*3600)-(min*600)-(sec*10));
if (hour > 0 && min > 0 && sec >= 0)
{
snprintf(Current_Seconds_Text, 10, "%d.%d", (int)sec , (int)tenth);
snprintf(Current_Minutes_Text, 10, "%d", (int)min);
snprintf(Current_Hours_Text, 10, "%d", (int)hour);
}
else if (min > 0 && sec >= 0)
{
snprintf(Current_Seconds_Text, 10, "%d.%d", (int)sec , (int)tenth);
snprintf(Current_Minutes_Text, 10, "%d", (int)min);
}
else
{
snprintf(Current_Seconds_Text, 10, "%d.%d",(int)sec, (int)tenth);
}

UpdateProgress();

if (((float)sec + ((float)tenth/10)) == 1)
text_layer_set_text(StopwatchSecondsLabellabel, "second");
else
text_layer_set_text(StopwatchSecondsLabellabel, "seconds");

if (min == 1)
text_layer_set_text(StopwatchMinutesLabellabel, "minute");
else
text_layer_set_text(StopwatchMinutesLabellabel, "minutes");

if (hour == 1)
text_layer_set_text(StopwatchHoursLabellabel, "hour");
else
text_layer_set_text(StopwatchHoursLabellabel, "hours");

text_layer_set_text(StopwatchSecondsLabel, Current_Seconds_Text);
text_layer_set_text(StopwatchMinutesLabel, Current_Minutes_Text);
text_layer_set_text(StopwatchHoursLabel, Current_Hours_Text);
}

static void StopwatchCallback(void *data)
{
if (!StopwatchPaused)
{
StopwatchTimer = app_timer_register(100, StopwatchCallback, NULL);
StopwatchTime++;

UpdateStopwatchDisplay();
}
}

static void StopwatchWindow_unload(Window *window) {
action_bar_layer_destroy(StopwatchActionBar);
if (!StopwatchPaused)
{
StopwatchPaused = 1;
app_timer_cancel(StopwatchTimer);
layer_set_hidden(inverter_layer_get_layer(StopwatchProgress), 1);
show_message("Stopwatch paused.", 1000);
}
text_layer_destroy(StopwatchSecondsLabel);
text_layer_destroy(StopwatchMinutesLabel);
text_layer_destroy(StopwatchHoursLabel);
text_layer_destroy(StopwatchSecondsLabellabel);
text_layer_destroy(StopwatchMinutesLabellabel);
text_layer_destroy(StopwatchHoursLabellabel);
text_layer_destroy(SplitLabelHeader);
inverter_layer_destroy(StopwatchProgress);
bitmap_layer_destroy(TIconBLayer);
gbitmap_destroy(Trophy_icon);
gbitmap_destroy(Loopback_icon);
gbitmap_destroy(Plus_Sign_icon);
gbitmap_destroy(Pause_icon);
gbitmap_destroy(Play_icon);
}

/* ----------------------STOPWATCH SPLITS--------------------*/

static bool fastest_already_defined = 0;

static MenuLayerCallbacks StopwatchSplitMenuCallbacks = { 
.draw_header = NULL,
.draw_row = DrawSplitItem,
.get_cell_height = RetrieveSplitHeight,
.get_header_height = NULL,
.get_num_rows = RetrieveSplitCount,
.get_num_sections = RetrieveSplitSectionCount,
.select_click = NULL,
.select_long_click = NULL,
.selection_changed = NULL, };

static void StopwatchSplits_load(Window *window) {
Layer *window_layer = window_get_root_layer(StopwatchSplits);
GRect bounds = layer_get_bounds(window_layer);

SplitMenu = menu_layer_create(bounds);
menu_layer_set_callbacks(SplitMenu, NULL, StopwatchSplitMenuCallbacks);
layer_add_child(window_layer, menu_layer_get_layer(SplitMenu));
menu_layer_set_click_config_onto_window(SplitMenu, StopwatchSplits);

}

static void DrawSplitItem(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context)
{
int index = cell_index->row;

if (index == 0)
fastest_already_defined = 0;

long time = *(long *)(get_item_by_index(cell_index->row, SplitList)->data);
char time_string[20];
char index_string[5];

int hour = time/36000;
long min = ((time-(hour*36000))/600);
long sec = (time-(hour*36000)-(min*600))/10;
long tenth = (time-(hour*3600)-(min*600)-(sec*10));
if (hour > 0 && min > 0 && sec >= 0)
snprintf(time_string, 20, "%dh, %dm, %d.%ds", (int)hour, (int)min, (int)sec, (int)tenth);
else if (min > 0 && sec >= 0)
snprintf(time_string, 20, "%dm, %d.%ds", (int)min, (int)sec, (int)tenth);
else
snprintf(time_string, 20, "%d.%ds", (int)sec, (int)tenth);

snprintf(index_string, 5, "#%d", index + 1);

if ( (time == lowest_split) && !fastest_already_defined)
menu_cell_basic_draw(ctx, cell_layer, (const char *)time_string, "Fastest time", Trophy_icon);
else
menu_cell_basic_draw(ctx, cell_layer, (const char *)time_string, index_string, NULL);
}

static int16_t RetrieveSplitHeight(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context)
{
return 50;
}

static uint16_t RetrieveSplitCount(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context)
{
return item_count(SplitList);
}

static uint16_t RetrieveSplitSectionCount(struct MenuLayer *menu_layer, void *callback_context)
{
return 1;
}
static void StopwatchSplits_unload(Window *window) {
menu_layer_destroy(SplitMenu);
}

/* ----------------------NOTIFIER WINDOW--------------------*/

static TextLayer *NotifyTitle;
static TextLayer *NotifyBody;
static TextLayer *NotifyHint;

static void NotifierWindow_load(Window *window) {
Layer *window_layer = window_get_root_layer(NotifierWindow);
GRect bounds = layer_get_bounds(window_layer);

//Title
GRect TitleBox = GRect(0, 20, bounds.size.w, 50);

NotifyTitle = text_layer_create(TitleBox);
text_layer_set_text_alignment(NotifyTitle, GTextAlignmentCenter);
text_layer_set_font(NotifyTitle, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
layer_add_child(window_layer, text_layer_get_layer(NotifyTitle));

//Body
GRect BodyBox = GRect(0, 70, bounds.size.w, 100);

NotifyBody = text_layer_create(BodyBox);
text_layer_set_text_alignment(NotifyBody, GTextAlignmentCenter);
text_layer_set_font(NotifyBody, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
layer_add_child(window_layer, text_layer_get_layer(NotifyBody));

//Hint
GRect HintBox = GRect(0, bounds.size.h - 30, bounds.size.w, 30);

NotifyHint = text_layer_create(HintBox);
text_layer_set_text_alignment(NotifyHint, GTextAlignmentCenter);
text_layer_set_font(NotifyHint, fonts_get_system_font(FONT_KEY_GOTHIC_18));
text_layer_set_text(NotifyHint, "Press any button");
layer_add_child(window_layer, text_layer_get_layer(NotifyHint));
}

static void Notify(char *Title, char *Message)
{
window_stack_remove(NotifierWindow, 0);
window_stack_push(NotifierWindow, 1);
text_layer_set_text(NotifyTitle, Title);
text_layer_set_text(NotifyBody, Message);
}

static void NotifierWindow_click_config_provider(void *context)
{
window_single_click_subscribe(BUTTON_ID_UP, AcceptMessage);
window_single_click_subscribe(BUTTON_ID_SELECT, AcceptMessage);
window_single_click_subscribe(BUTTON_ID_DOWN, AcceptMessage);
}

static void AcceptMessage(ClickRecognizerRef recognizer, void *context)
{
window_stack_remove(NotifierWindow, 1);
}

static void NotifierWindow_unload(Window *window) {
text_layer_destroy(NotifyTitle);
text_layer_destroy(NotifyBody);
text_layer_destroy(NotifyHint);
}
