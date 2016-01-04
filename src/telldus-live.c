#include <pebble.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

// Persistent data
#define S_SENSOR_FLAG_PKEY 1
#define S_SENSOR_FORMAT_FLAG_PKEY 2
 
	// Device methods
#define TELLSTICK_TURNON	(1)
#define TELLSTICK_TURNOFF	(2)
#define TELLSTICK_BELL		(4)
#define TELLSTICK_TOGGLE	(8)
#define TELLSTICK_DIM			(16)
#define TELLSTICK_LEARN		(32)
#define TELLSTICK_EXECUTE	(64)
#define TELLSTICK_UP			(128)
#define TELLSTICK_DOWN	(256)
#define TELLSTICK_STOP		(512)

enum {
  AKEY_MODULE,
  AKEY_ACTION,
  AKEY_NAME,
  AKEY_ID,
  AKEY_STATE,
  AKEY_TEMP,
  AKEY_HUM,
  AKEY_METHODS,
  AKEY_DIMVALUE,
  AKEY_TYPE,
  AKEY_BATTERY,
};

static Window *window;
static TextLayer *textLayer;
static MenuLayer *menuLayer;
static GBitmap *TelldusOn, *TelldusOff, *Cloud, *Cloudd;
static GBitmap  *dim_icons[5];
//AppTimer *timer;

#define MAX_DEVICE_LIST_ITEMS (30)
#define MAX_SENSOR_LIST_ITEMS (30)
#define MAX_GROUP_LIST_ITEMS (20)
#define MAX_DEVICE_NAME_LENGTH (17)
#define MAX_SENSOR_NAME_LENGTH (17)
#define MAX_TEMP_LENGTH (5)
#define MAX_HUM_LENGTH (4)

// sensor battery status
// 0-100 - percentage left, if the sensor reports this
// 253 - battery ok
// 254 - battery status unknown (not reported by this sensor type, or not decoded)
// 255 - battery low
#define SENS_BATT_OK (253)
#define SENS_BATT_UNKNOWN (254)
#define SENS_BATT_LOW (255)
 
enum  {MENU_SECTION_CLIENT, MENU_SECTION_GROUP, MENU_SECTION_ENVIRONMENT, MENU_SECTION_DEVICE, MENU_SECTION_NUMBER};
char client[MAX_DEVICE_NAME_LENGTH+1];
char online[2];
	
typedef struct {
	int id;
	char name[MAX_DEVICE_NAME_LENGTH+1];
	int state;
	int methods;
	int dimvalue;
} Device;

Device Group;

typedef struct {
	int id;
	char name[MAX_DEVICE_NAME_LENGTH+1];
	char temp[MAX_TEMP_LENGTH+1];
	char hum[MAX_HUM_LENGTH+1];
	uint batt;
} Sensor;

static Device s_device_list_items[MAX_DEVICE_LIST_ITEMS];
static Device s_group_list_items[MAX_GROUP_LIST_ITEMS];
static Sensor s_sensor_list_items[MAX_SENSOR_LIST_ITEMS];
static int s_device_count = 0;
static int s_group_count = 0;
static int s_sensor_count = 0;
static bool s_sensor_flag = false;
static int s_sensor_show_count = 0;
static bool s_sensor_format_flag = false;
static bool done = false;
/*
void timer_callback(void *data) {
			client[0] = '\0';
			layer_mark_dirty(menu_layer_get_layer(menuLayer));
			timer = NULL;
   //Register next execution
}
*/

void out_sent_handler(DictionaryIterator *sent, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "outgoing message was delivered");
}

void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "outgoing message failed");
}

void setSensor(int id, char *name, char *temp, char *hum, uint batt) {
//	APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting sensor %i %s", id, name);
//		APP_LOG(APP_LOG_LEVEL_DEBUG, "battlevel %s %u", name, batt);
	if (s_sensor_count >= MAX_SENSOR_LIST_ITEMS) {
		return;
	}
	s_sensor_list_items[s_sensor_count].id = id;
 	strncpy(s_sensor_list_items[s_sensor_count].name, name, MAX_SENSOR_NAME_LENGTH);
 	strncpy(s_sensor_list_items[s_sensor_count].temp, temp, MAX_TEMP_LENGTH);
 	strncpy(s_sensor_list_items[s_sensor_count].hum, hum, MAX_HUM_LENGTH);
	s_sensor_list_items[s_sensor_count].batt = batt;
	s_sensor_count++;
	
	if (s_sensor_flag == true){
		s_sensor_show_count  = s_sensor_count;
	} else {
		s_sensor_show_count = 1;
	}	
}

void setDevice(int id, char *name, int state, int methods, int dimvalue) {
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting device %i %s", id, name);
	if (s_device_count >= MAX_DEVICE_LIST_ITEMS) {
		return;
	}
	s_device_list_items[s_device_count].id = id;
	s_device_list_items[s_device_count].state = state;
	s_device_list_items[s_device_count].methods = methods;
	s_device_list_items[s_device_count].dimvalue = dimvalue;
 	strncpy(s_device_list_items[s_device_count].name, name, MAX_DEVICE_NAME_LENGTH);
	s_device_count++;
}

void setGroup(int id, char *name, int state, int methods, int dimvalue) {
//	APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting group %i %s", id, name);
	if (s_group_count >= MAX_DEVICE_LIST_ITEMS) {
		return;
	}
	s_group_list_items[s_group_count].id = id;
	s_group_list_items[s_group_count].state = state;
	s_group_list_items[s_group_count].methods = methods;
	s_group_list_items[s_group_count].dimvalue = dimvalue;
 	strncpy(s_group_list_items[s_group_count].name, name, MAX_DEVICE_NAME_LENGTH);
	s_group_count++;
}

void replaceDevice(int id, char *name, int state) {
//	APP_LOG(APP_LOG_LEVEL_DEBUG, "Replacing device %i state %i", id, state);
	s_device_list_items[id].state = state;
//	strncpy(s_device_list_items[id].name, name, MAX_DEVICE_NAME_LENGTH);
}

void in_received_handler(DictionaryIterator *received, void *context) {
//	APP_LOG(APP_LOG_LEVEL_DEBUG, "incoming message received");
	// Check for fields you expect to receive
	Tuple *moduleTuple = dict_find(received, AKEY_MODULE);
	Tuple *actionTuple = dict_find(received, AKEY_ACTION);
	if (!moduleTuple || !actionTuple) {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Malformed message");
		return;
	}
//	APP_LOG(APP_LOG_LEVEL_DEBUG, "Got module: %s, action: %s", moduleTuple->value->cstring, actionTuple->value->cstring);
	if (strcmp(moduleTuple->value->cstring, "auth") == 0) {
		if (strcmp(actionTuple->value->cstring, "request") == 0) {
			text_layer_set_text(textLayer, "Please log in on your phone");
		} else if (strcmp(actionTuple->value->cstring, "authenticating") == 0) {
			text_layer_set_text(textLayer, "Authenticating...");
//		} else if (strcmp(actionTuple->value->cstring, "log") == 0) {
//			text_layer_set_text(textLayer, "LARSSONS   ONLINE");
		} else if (strcmp(actionTuple->value->cstring, "done") == 0) {
			if (done == false) {
				done = true;
			} else {
//				timer = app_timer_register(5000, timer_callback, NULL);
				layer_set_hidden(text_layer_get_layer(textLayer), true);
				layer_set_hidden(menu_layer_get_layer(menuLayer), false);
				menu_layer_reload_data(menuLayer);
				MenuIndex index;
				if (s_group_count > 0 ) 
					index.section = 0;
				else if (s_sensor_count > 0 ) 
					index.section = 1;
				else
					index.section = 2;
				index.row = 0;
	//    APP_LOG(APP_LOG_LEVEL_DEBUG,"Set row: %i section: %i ",index.row, index.section);
				menu_layer_set_selected_index( menuLayer, index, MenuRowAlignNone, false );  	
			}
		} else if (strcmp(actionTuple->value->cstring, "clear") == 0) {
			s_device_count = 0;
			s_sensor_count = 0;
			layer_set_hidden(text_layer_get_layer(textLayer), false);
			layer_set_hidden(menu_layer_get_layer(menuLayer), true);
		}
	} else if (strcmp(moduleTuple->value->cstring, "device") == 0) {
		Tuple *idTuple = dict_find(received, AKEY_ID);
		Tuple *nameTuple = dict_find(received, AKEY_NAME);
		Tuple *stateTuple = dict_find(received, AKEY_STATE);
		Tuple *methodsTuple = dict_find(received, AKEY_METHODS);
		Tuple *dimvalueTuple = dict_find(received, AKEY_DIMVALUE);
		Tuple *typeTuple = dict_find(received, AKEY_TYPE);
   // APP_LOG(APP_LOG_LEVEL_DEBUG,"Letar upp Name: %s Id: %i Stste: %i",nameTuple->value->cstring,idTuple->value->int8, stateTuple->value->int8);
		if (!idTuple || !nameTuple || !stateTuple || !methodsTuple || !dimvalueTuple || !typeTuple) {
			return;
		} 
   // APP_LOG(APP_LOG_LEVEL_DEBUG,"Letar upp Name: %s Id: %i Stste: %i",nameTuple->value->cstring,idTuple->value->int8, stateTuple->value->int8);
    for(int i=0; i<s_device_count; i++) { 
      if(s_device_list_items[i].id == idTuple->value->int8) {  
				s_device_list_items[i].state = stateTuple->value->int8;
        layer_mark_dirty(menu_layer_get_layer(menuLayer));
        return;
        }
    }
    for(int i=0; i<s_group_count; i++) { 
      if(s_group_list_items[i].id == idTuple->value->int8) {  
				s_group_list_items[i].state = stateTuple->value->int8;
        layer_mark_dirty(menu_layer_get_layer(menuLayer));
        return;
        }
    }
		if(typeTuple->value->int8 == 1) {
			setDevice(idTuple->value->int8, 
				nameTuple->value->cstring, 
				stateTuple->value->int8, 
				methodsTuple->value->int16,
				dimvalueTuple->value->int16);
		} else {
				setGroup(idTuple->value->int8, 
					nameTuple->value->cstring, 
					stateTuple->value->int8, 
					methodsTuple->value->int16,
					dimvalueTuple->value->int16);
		}
		
	} else if (strcmp(moduleTuple->value->cstring, "sensor") == 0) {
		Tuple *idTuple = dict_find(received, AKEY_ID);
		Tuple *nameTuple = dict_find(received, AKEY_NAME);
		Tuple *tempTuple = dict_find(received, AKEY_TEMP);
		Tuple *humTuple = dict_find(received, AKEY_HUM);
		Tuple *battTuple = dict_find(received, AKEY_BATTERY);
		if (!idTuple || !nameTuple || !tempTuple || !humTuple || !battTuple) {
			return;
		} 
    for(int i=0; i<s_sensor_count; i++) { 
      if(s_sensor_list_items[i].id == idTuple->value->int8) {  
        return;
      }
    }
	  setSensor(idTuple->value->int8, nameTuple->value->cstring, tempTuple->value->cstring, humTuple->value->cstring, battTuple->value->uint8);

	} else if (strcmp(moduleTuple->value->cstring, "client") == 0) {
		Tuple *nameTuple = dict_find(received, AKEY_NAME);
		Tuple *tempTuple = dict_find(received, AKEY_TEMP);
		if (!nameTuple || !tempTuple) {
			return;
		} 
	 	strncpy(client , nameTuple->value->cstring , MAX_SENSOR_NAME_LENGTH);
	 	strncpy(online , tempTuple->value->cstring , 2);
	}
}

void in_dropped_handler(AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "incoming message dropped");
}

static int16_t get_cell_height_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  return 44;
	/*
	switch (cell_index->section) {
		case 0:
		return 44;;
		case 1:
		return 44;;
		case 2:
		return 44;
		default:
		return 44;
  }
	*/
}

static uint16_t menu_get_num_sections_callback(MenuLayer *menu_layer, void *data) {
  return MENU_SECTION_NUMBER;
}

// Each section has a number of items;  we use a callback to specify this
// You can also dynamically add and remove items using this
static uint16_t get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  switch (section_index) {
		
		case MENU_SECTION_CLIENT:
			if (client[0] !='\0')
					return 1;
		case MENU_SECTION_GROUP:
      return s_group_count;;
    case MENU_SECTION_ENVIRONMENT:
      return s_sensor_show_count;;
    case MENU_SECTION_DEVICE:
      return s_device_count;
    default:
      return 0;
  }
}

static void draw_row_callback(GContext* ctx, Layer *cell_layer, MenuIndex *cell_index, void *data) {
  GBitmap *Img_status;

	if (cell_index->section == MENU_SECTION_ENVIRONMENT){
		
    Sensor* sensor = &s_sensor_list_items[cell_index->row];
		
		uint battlevel = sensor->batt;
		int startx;
		char buff1[30] = "";
    char p1 = ' ';
    if (sensor->hum[0] != '\0') {
      p1 = '%';
    }  
//		snprintf (buff1,30,"        %s%c%cC   %s%c",sensor->temp,0xc2,0xb0,sensor->hum,p1);
//    menu_cell_basic_draw(ctx, cell_layer, buff1, sensor->name,  NULL);
		
		
		if (!s_sensor_format_flag) {
			graphics_context_set_text_color(ctx, GColorBlack);
			snprintf (buff1,30,"%s%c%cC   %s%c",sensor->temp,0xc2,0xb0,sensor->hum,p1);
			graphics_draw_text(ctx, buff1, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GRect(30, 15, 110, 30), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
			snprintf (buff1,19,"%s",sensor->name);
			graphics_draw_text(ctx, buff1, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GRect(2, 0, 125, 14), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
			startx = 126;
		}
		else {
			snprintf (buff1,30,"        %s%c%cC   %s%c",sensor->temp,0xc2,0xb0,sensor->hum,p1);
   		menu_cell_basic_draw(ctx, cell_layer, buff1, sensor->name,  NULL);
			startx = 5;
		}
		if (battlevel != SENS_BATT_UNKNOWN) {
			if (battlevel < SENS_BATT_OK) 
				battlevel = battlevel / 10;
			else if (battlevel == SENS_BATT_OK)
				battlevel = 10;
			else battlevel = 1;
			graphics_draw_rect(ctx, GRect(startx,7,14,8));
			graphics_fill_rect(ctx, GRect(startx + 2,9,battlevel,4),0,GCornerNone);
			graphics_draw_line(ctx, GPoint(startx + 14,9), GPoint(startx + 14,12));
		}
	
	} else 	if (cell_index->section == MENU_SECTION_CLIENT) { 
	
		if (client[0] != '\0') {
			if (online[0] == '1')
				Img_status = Cloud;
			else
				Img_status = Cloudd;
			menu_cell_basic_draw(ctx, cell_layer, client, NULL, Img_status);
		}
	} else {	
		
    Device* device;		
		if (cell_index->section == MENU_SECTION_DEVICE)
    	device = &s_device_list_items[cell_index->row];
		else
			device = &s_group_list_items[cell_index->row];

    if (device->methods & TELLSTICK_DIM) {
      if (device->state == TELLSTICK_TURNOFF) {
        Img_status = dim_icons[0]; 
      } 
      else if (device->state == TELLSTICK_TURNON) {
        Img_status = dim_icons[4];
      } else {
        Img_status = dim_icons[(device->dimvalue + 63)/64];    
      }			
    }
    else {			
      Img_status = TelldusOn; 
      if (device->state == TELLSTICK_TURNOFF) {
        Img_status = TelldusOff; 
      }			
    }  
    menu_cell_basic_draw(ctx, cell_layer, device->name, NULL, Img_status);
	}
}

//static void menu_cell_track_draw (GContext* ctx, const Layer *cell_layer, const char *text) {
//    graphics_context_set_text_color(ctx, GColorBlack);
//    graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18), layer_get_bounds(cell_layer), GTextOverflowWrap, GTextAlignmentLeft ,NULL 
//}


//static void draw_separator_callback(GContext* ctx, Layer *cell_layer, MenuIndex *cell_index, void *data) {
//}
static void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
  // Determine which section we're working with
  switch (section_index) {
		
    case MENU_SECTION_CLIENT:
      // Draw title text in the section header
      menu_cell_basic_header_draw(ctx, cell_layer, "Client");//client);
      break;
		
    case MENU_SECTION_GROUP:
      // Draw title text in the section header
      menu_cell_basic_header_draw(ctx, cell_layer, "Groups");
      break;
		
		case MENU_SECTION_ENVIRONMENT:	
			if (s_sensor_flag == false)
  	    menu_cell_basic_header_draw(ctx, cell_layer, "Environment        >>>>");
			else
	      menu_cell_basic_header_draw(ctx, cell_layer, "Environment");
      break;	
		
    case MENU_SECTION_DEVICE:
      menu_cell_basic_header_draw(ctx, cell_layer, "Devices");
      break;
  }
}

// A callback is used to specify the height of the section header
static int16_t menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  // This is a define provided in pebble.h that you may use for the default height
 // return MENU_CELL_BASIC_HEADER_HEIGHT;
  switch (section_index) {

		case MENU_SECTION_CLIENT:
			break; 
		
		case MENU_SECTION_GROUP:
			if (s_group_count == 0) 
				return 0;
			break; 
		
    case MENU_SECTION_ENVIRONMENT:
			if (s_sensor_show_count == 0) 
				return 0;
			break; 
		
    case MENU_SECTION_DEVICE:
			if (s_device_count == 0) 
				return 0;
			break; 
		
    default:
      return 0;	
  }
	return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
	
	if (cell_index->section == MENU_SECTION_CLIENT) return;
	
	if(cell_index->section == MENU_SECTION_ENVIRONMENT) {
		if (s_sensor_flag == false){
			s_sensor_flag = true;
			s_sensor_show_count  = s_sensor_count;
		} else {
			s_sensor_flag = false;
			s_sensor_show_count = 1;
		}
		MenuIndex index;
		index.section = MENU_SECTION_ENVIRONMENT;
		index.row =  0;
		menu_layer_set_selected_index( menuLayer, index, MenuRowAlignCenter , true );  	
		menu_layer_reload_data(menuLayer);
    return;

	} else 	{
	
		Device* device;
		if (cell_index->section == MENU_SECTION_DEVICE)
				device = &s_device_list_items[cell_index->row];
			else
				device = &s_group_list_items[cell_index->row];
		DictionaryIterator *iter;
		app_message_outbox_begin(&iter);
		Tuplet module = TupletCString(AKEY_MODULE, "device");
		dict_write_tuplet(iter, &module);
		Tuplet action = TupletCString(AKEY_ACTION, "select");
		dict_write_tuplet(iter, &action);
		Tuplet id = TupletInteger(AKEY_ID, device->id);
		dict_write_tuplet(iter, &id);
		Tuplet methods = TupletInteger(AKEY_METHODS, 3);
		dict_write_tuplet(iter, &methods);
		app_message_outbox_send();
  device->dimvalue = 0;
	}
}

static void select_long_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  if(cell_index->section == MENU_SECTION_GROUP) return;
  if(cell_index->section == MENU_SECTION_CLIENT) return;
//	APP_LOG(APP_LOG_LEVEL_DEBUG, "Select long callback, index: %i", cell_index->row);
	if(cell_index->section == MENU_SECTION_ENVIRONMENT) {
		s_sensor_format_flag = !s_sensor_format_flag;
		layer_mark_dirty(menu_layer_get_layer(menuLayer));
		return;
	}
	Device* device = &s_device_list_items[cell_index->row];
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "Select callback, index: %i", cell_index->row);
	if (device->methods & TELLSTICK_DIM) {
		DictionaryIterator *iter;
		app_message_outbox_begin(&iter);
		Tuplet module = TupletCString(AKEY_MODULE, "device");
		dict_write_tuplet(iter, &module);
		Tuplet action = TupletCString(AKEY_ACTION, "select");
		dict_write_tuplet(iter, &action);
		Tuplet id = TupletInteger(AKEY_ID, device->id);
		dict_write_tuplet(iter, &id);

		if (device->dimvalue == 255) {
			device->dimvalue = 0;
		} else {
			device->dimvalue +=1;
			device->dimvalue &= 0xff;
			device->dimvalue += 64;
			device->dimvalue /=64;
			device->dimvalue *= 64;
			device->dimvalue -=1;
			device->dimvalue &= 0xff;
		}

		layer_mark_dirty(menu_layer_get_layer(menuLayer));
		Tuplet dimvalue = TupletInteger(AKEY_DIMVALUE, device->dimvalue);
		dict_write_tuplet(iter, &dimvalue);
		Tuplet methods = TupletInteger(AKEY_METHODS, TELLSTICK_DIM);
		dict_write_tuplet(iter, &methods);
		app_message_outbox_send();
	}
}

static void window_load(Window *window) {
	Layer *windowLayer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(windowLayer);
	GRect frame = layer_get_frame(windowLayer);
	TelldusOn = gbitmap_create_with_resource(RESOURCE_ID_IMG_ON);
	TelldusOff = gbitmap_create_with_resource(RESOURCE_ID_IMG_OFF2);
	Cloud = gbitmap_create_with_resource(RESOURCE_ID_IMG_CLOUD);
	Cloudd = gbitmap_create_with_resource(RESOURCE_ID_IMG_CLOUDD);
    
	int num_menu_icons = 0;
	dim_icons[num_menu_icons++] = gbitmap_create_with_resource(RESOURCE_ID_IMG_DIM0I);
  dim_icons[num_menu_icons++] = gbitmap_create_with_resource(RESOURCE_ID_IMG_DIM25I);
  dim_icons[num_menu_icons++] = gbitmap_create_with_resource(RESOURCE_ID_IMG_DIM50I);
  dim_icons[num_menu_icons++] = gbitmap_create_with_resource(RESOURCE_ID_IMG_DIM75I);
  dim_icons[num_menu_icons] = gbitmap_create_with_resource(RESOURCE_ID_IMG_DIM100I);
 
  
	textLayer = text_layer_create((GRect) { .origin = { 0, 72 }, .size = { bounds.size.w, 40 } });
	text_layer_set_text(textLayer, "Loading...");
	text_layer_set_text_alignment(textLayer, GTextAlignmentCenter);
	text_layer_set_overflow_mode(textLayer, GTextOverflowModeWordWrap);
	layer_add_child(windowLayer, text_layer_get_layer(textLayer));
  

	menuLayer = menu_layer_create(frame);
	menu_layer_set_callbacks(menuLayer, NULL, (MenuLayerCallbacks) {
		.get_cell_height = (MenuLayerGetCellHeightCallback) get_cell_height_callback,
		.draw_row = (MenuLayerDrawRowCallback) draw_row_callback,
		.get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback) get_num_rows_callback,
		.select_click = (MenuLayerSelectCallback) select_callback,
		.select_long_click = (MenuLayerSelectCallback) select_long_callback,
    .draw_header = (MenuLayerDrawHeaderCallback) menu_draw_header_callback,
    .get_header_height = menu_get_header_height_callback,
    .get_num_sections = menu_get_num_sections_callback
  //  .draw_separator = (MenuLayerDrawSeparatorCallback) draw_separator_callback
	});
  scroll_layer_set_shadow_hidden(menu_layer_get_scroll_layer(menuLayer), true);
	menu_layer_set_click_config_onto_window(menuLayer, window);
	layer_set_hidden(menu_layer_get_layer(menuLayer), false);
	layer_add_child(windowLayer, menu_layer_get_layer(menuLayer));
}

static void window_unload(Window *window) {
//	if (timer != NULL) app_timer_cancel(timer);
  menu_layer_destroy(menuLayer);
	text_layer_destroy(textLayer);
  gbitmap_destroy(TelldusOn);
  gbitmap_destroy(TelldusOff);
  gbitmap_destroy(Cloud);
  gbitmap_destroy(Cloudd);
  
  for (int i = 0; i < 5; i++) {
    gbitmap_destroy(dim_icons[i]);
  }
 }


static void init(void) {
	window = window_create();

	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});
	const bool animated = true;
	s_sensor_flag = persist_exists(S_SENSOR_FLAG_PKEY) ? persist_read_bool(S_SENSOR_FLAG_PKEY) : false;
	s_sensor_format_flag = persist_exists(S_SENSOR_FORMAT_FLAG_PKEY) ? persist_read_bool(S_SENSOR_FORMAT_FLAG_PKEY) : false;
	window_stack_push(window, animated);

	app_message_register_inbox_received(in_received_handler);
	app_message_register_inbox_dropped(in_dropped_handler);
	app_message_register_outbox_sent(out_sent_handler);
	app_message_register_outbox_failed(out_failed_handler);

//	const uint32_t inbound_size = 128;
//	const uint32_t outbound_size = 128;
//	app_message_open(inbound_size, outbound_size);
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void deinit(void) {
	persist_write_bool(S_SENSOR_FLAG_PKEY, s_sensor_flag);
	persist_write_bool(S_SENSOR_FORMAT_FLAG_PKEY, s_sensor_format_flag);
	window_destroy(window);
}

int main(void) {
	init();

	APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

	app_event_loop();
	deinit();
} 