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
static GBitmap *TelldusOnI, *TelldusOffI, *CloudI, *ClouddI;
static GBitmap  *dim_icons[5];
static GBitmap  *dim_icons_i[5];
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
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "outgoing message was delivered");
}

void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
//	APP_LOG(APP_LOG_LEVEL_DEBUG, "outgoing message failed");
}

void setSensor(int id, char *name, char *temp, char *hum, uint batt) {
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting sensor %i %s", id, name);
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
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting group %i %s", id, name);
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
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "incoming message received");
	// Check for fields you expect to receive
	Tuple *moduleTuple = dict_find(received, AKEY_MODULE);
	Tuple *actionTuple = dict_find(received, AKEY_ACTION);
	if (!moduleTuple || !actionTuple) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "Malformed message");
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

#if defined(PBL_ROUND)
	if( !menu_layer_is_index_selected(menuLayer, cell_index))
		return 44;
	else
#endif
	return PBL_IF_ROUND_ELSE(60, 44);
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
      return s_group_count;
		
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
	int startx = 128;

	graphics_context_set_compositing_mode(ctx, GCompOpSet);

	if (cell_index->section == MENU_SECTION_ENVIRONMENT) {
		
    Sensor* sensor = &s_sensor_list_items[cell_index->row];
		
		uint battlevel = sensor->batt;
		char buff1[30] = "";
    char p1 = ' ';
    if (sensor->hum[0] != '\0') {
      p1 = '%';
    }  
//		snprintf (buff1,30,"        %s%c%cC   %s%c",sensor->temp,0xc2,0xb0,sensor->hum,p1);
//    menu_cell_basic_draw(ctx, cell_layer, buff1, sensor->name,  NULL);
		
		
		if (!s_sensor_format_flag) {
			if (menu_cell_layer_is_highlighted( cell_layer)) {
				graphics_context_set_text_color(ctx, GColorWhite);
				graphics_context_set_stroke_color(ctx, GColorWhite);
			//	graphics_context_set_fill_color(ctx, GColorBlack);
			}
			else {
				graphics_context_set_text_color(ctx, GColorBlack);				
				graphics_context_set_stroke_color(ctx, GColorBlack);
		//		graphics_context_set_fill_color(ctx, GColorWhite);
			}

			#if defined(PBL_ROUND)
			if ( menu_layer_is_index_selected(menuLayer, cell_index) ){ 
			#endif
				snprintf (buff1,2," ");
				menu_cell_basic_draw(ctx, cell_layer, sensor->name,  " " , NULL);
				snprintf (buff1,30,"%s%c%cC   %s%c",sensor->temp,0xc2,0xb0,sensor->hum,p1);
				GRect bounds = layer_get_bounds(cell_layer);
				graphics_draw_text(ctx, buff1, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GRect( bounds.origin.x, bounds.origin.y + bounds.size.h/2-5, bounds.size.w, bounds.size.h/2), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
				startx = 126;
#if defined(PBL_ROUND)
			} else {
				menu_cell_basic_draw(ctx, cell_layer, sensor->name, NULL,  NULL);			
			}
#endif			
		}
		else {
			startx = 5;
			snprintf (buff1,30,"        %s%c%cC   %s%c",sensor->temp,0xc2,0xb0,sensor->hum,p1);
#if defined(PBL_ROUND)
			if ( !menu_layer_is_index_selected(menuLayer, cell_index) )
				menu_cell_basic_draw(ctx, cell_layer, sensor->name, NULL,  NULL);
			else
#endif
			 menu_cell_basic_draw(ctx, cell_layer, buff1, sensor->name,  NULL);
		}
#if defined(PBL_ROUND)
		if ( menu_layer_is_index_selected(menuLayer, cell_index) ) { 
			startx = 160;
#endif
			if (battlevel != SENS_BATT_UNKNOWN) {
				if (battlevel < SENS_BATT_OK) 
					battlevel = battlevel / 10;
				else if (battlevel == SENS_BATT_OK)
					battlevel = 10;
					else battlevel = 1;

				graphics_context_set_stroke_color(ctx, GColorYellow);				
				graphics_draw_rect(ctx, GRect(startx,7,14,8));
				graphics_fill_rect(ctx, GRect(startx + 2,9,battlevel,4),0,GCornerNone);
				graphics_draw_line(ctx, GPoint(startx + 14,9), GPoint(startx + 14,12));
				
			}
#if defined(PBL_ROUND)
		}
#endif	
	} else 	if (cell_index->section == MENU_SECTION_CLIENT) { 
	
		if (client[0] != '\0') {
			if (online[0] == '1') {
				if (menu_cell_layer_is_highlighted( cell_layer)) {
					Img_status = CloudI;
				} else {
					Img_status = Cloud;
				}
			} else {
				if (menu_cell_layer_is_highlighted( cell_layer)) {
					Img_status = ClouddI;
				} else {
					Img_status = Cloudd;
				}
			}
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
				if (menu_cell_layer_is_highlighted( cell_layer)) {
					Img_status = dim_icons_i[0]; 
				} else {
					Img_status = dim_icons[0]; 				
				}
      } 
      else if (device->state == TELLSTICK_TURNON) {
				if (menu_cell_layer_is_highlighted( cell_layer)) {
        	Img_status = dim_icons_i[4];
				} else {
        	Img_status = dim_icons[4];					
				}
      } else {
				if (menu_cell_layer_is_highlighted( cell_layer)) {
        	Img_status = dim_icons_i[(device->dimvalue + 63)/64];
				} else {
        	Img_status = dim_icons[(device->dimvalue + 63)/64];    
				}
      }			
    }
    else {			
			if (menu_cell_layer_is_highlighted( cell_layer)) {
	      Img_status = TelldusOnI; 
			} else {
				Img_status = TelldusOn; 
			}
      if (device->state == TELLSTICK_TURNOFF) {
				if (menu_cell_layer_is_highlighted( cell_layer)) {
					Img_status = TelldusOffI; 
				} else {
					Img_status = TelldusOff; 
				}
      }			
    }  
#if defined(PBL_COLOR)
	 if (device->state != TELLSTICK_TURNOFF) graphics_context_set_text_color(ctx, GColorYellow);				
#endif
#if defined(PBL_ROUND)
		if ( !menu_layer_is_index_selected(menuLayer, cell_index) ) Img_status = NULL;
#endif					
    menu_cell_basic_draw(ctx, cell_layer, device->name, NULL, Img_status);
	}
}

//static void menu_cell_track_draw (GContext* ctx, const Layer *cell_layer, const char *text) {
//    graphics_context_set_text_color(ctx, GColorBlack);
//    graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18), layer_get_bounds(cell_layer), GTextOverflowWrap, GTextAlignmentLeft ,NULL 
//}


static void draw_separator_callback(GContext* ctx, Layer *cell_layer, MenuIndex *cell_index, void *data) {
	graphics_draw_line( ctx, GPoint (0, 0), GPoint (143,0));
}

void set_sec_text(GContext* ctx, char * text, const Layer *cell_layer) {
	GRect bounds = layer_get_bounds(cell_layer);
	graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(  bounds.origin.x, bounds.origin.y, bounds.size.w, bounds.size.h), GTextOverflowModeTrailingEllipsis, PBL_IF_ROUND_ELSE(GTextAlignmentCenter,GTextAlignmentLeft), NULL);
}

static void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
  // Determine which section we're working with
  switch (section_index) {
		
    case MENU_SECTION_CLIENT:
      // Draw title text in the section header
				set_sec_text(ctx, "Client", cell_layer);
      break;
		
    case MENU_SECTION_GROUP:
      // Draw title text in the section header
				set_sec_text(ctx, "Groups", cell_layer);
      break;
		
		case MENU_SECTION_ENVIRONMENT:	
			if (s_sensor_flag == false)
				set_sec_text(ctx, "Environment >>>>", cell_layer);
			else
				set_sec_text(ctx, "Environment", cell_layer);
      break;	
		
    case MENU_SECTION_DEVICE:
			set_sec_text(ctx, "Devices", cell_layer);
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
		menu_layer_set_selected_index( menuLayer, index, MenuRowAlignCenter , false );  	
		menu_layer_reload_data(menuLayer);
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
	TelldusOnI = gbitmap_create_with_resource(RESOURCE_ID_IMG_ONI);
	TelldusOffI = gbitmap_create_with_resource(RESOURCE_ID_IMG_OFF2I);
	Cloud = gbitmap_create_with_resource(RESOURCE_ID_IMG_CLOUD);
	Cloudd = gbitmap_create_with_resource(RESOURCE_ID_IMG_CLOUDD);
	CloudI = gbitmap_create_with_resource(RESOURCE_ID_IMG_CLOUDI);
	ClouddI = gbitmap_create_with_resource(RESOURCE_ID_IMG_CLOUDDI);
    
	int num_menu_icons = 0;
	dim_icons_i[num_menu_icons] = gbitmap_create_with_resource(RESOURCE_ID_IMG_DIM0II);
	dim_icons[num_menu_icons++] = gbitmap_create_with_resource(RESOURCE_ID_IMG_DIM0I);
  dim_icons_i[num_menu_icons] = gbitmap_create_with_resource(RESOURCE_ID_IMG_DIM25II);
  dim_icons[num_menu_icons++] = gbitmap_create_with_resource(RESOURCE_ID_IMG_DIM25I);
  dim_icons_i[num_menu_icons] = gbitmap_create_with_resource(RESOURCE_ID_IMG_DIM50II);
  dim_icons[num_menu_icons++] = gbitmap_create_with_resource(RESOURCE_ID_IMG_DIM50I);
  dim_icons_i[num_menu_icons] = gbitmap_create_with_resource(RESOURCE_ID_IMG_DIM75II);
  dim_icons[num_menu_icons++] = gbitmap_create_with_resource(RESOURCE_ID_IMG_DIM75I);
  dim_icons[num_menu_icons] = gbitmap_create_with_resource(RESOURCE_ID_IMG_DIM100I);
  dim_icons_i[num_menu_icons] = gbitmap_create_with_resource(RESOURCE_ID_IMG_DIM100II);
 
  
	textLayer = text_layer_create((GRect) { .origin = { 0, 72 }, .size = { bounds.size.w, 40 } });
	text_layer_set_background_color(textLayer, PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorWhite)) ;
	text_layer_set_text_color(textLayer, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack)) ;
	text_layer_set_text(textLayer, "Loading...");
	text_layer_set_text_alignment(textLayer, GTextAlignmentCenter);
	text_layer_set_overflow_mode(textLayer, GTextOverflowModeWordWrap);
	layer_add_child(windowLayer, text_layer_get_layer(textLayer));
  

	menuLayer = menu_layer_create(frame);
	menu_layer_set_highlight_colors(menuLayer, GColorDukeBlue, GColorWhite);
	menu_layer_set_callbacks(menuLayer, NULL, (MenuLayerCallbacks) {
		.get_cell_height = (MenuLayerGetCellHeightCallback) get_cell_height_callback,
		.draw_row = (MenuLayerDrawRowCallback) draw_row_callback,
		.get_num_rows = (MenuLayerGetNumberOfRowsInSectionsCallback) get_num_rows_callback,
		.select_click = (MenuLayerSelectCallback) select_callback,
		.select_long_click = (MenuLayerSelectCallback) select_long_callback,
    .draw_header = (MenuLayerDrawHeaderCallback) menu_draw_header_callback,
    .get_header_height = (MenuLayerGetHeaderHeightCallback) menu_get_header_height_callback,
    .get_num_sections = (MenuLayerGetNumberOfSectionsCallback) menu_get_num_sections_callback
//    .draw_separator = (MenuLayerDrawSeparatorCallback) draw_separator_callback
	});
  scroll_layer_set_shadow_hidden(menu_layer_get_scroll_layer(menuLayer), true);
	menu_layer_set_click_config_onto_window(menuLayer, window);
	layer_set_hidden(menu_layer_get_layer(menuLayer), true);
	layer_add_child(windowLayer, menu_layer_get_layer(menuLayer));
	menu_layer_set_normal_colors(menuLayer,PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorWhite), GColorBlack) ;
}

static void window_unload(Window *window) {
//	if (timer != NULL) app_timer_cancel(timer);
  menu_layer_destroy(menuLayer);
	text_layer_destroy(textLayer);
  gbitmap_destroy(TelldusOn);
  gbitmap_destroy(TelldusOff);
  gbitmap_destroy(TelldusOnI);
  gbitmap_destroy(TelldusOffI);
  gbitmap_destroy(Cloud);
  gbitmap_destroy(Cloudd);
   gbitmap_destroy(CloudI);
  gbitmap_destroy(ClouddI);
  
  for (int i = 0; i < 5; i++) {
    gbitmap_destroy(dim_icons[i]);
    gbitmap_destroy(dim_icons_i[i]);
  }
 }


static void init(void) {
	window = window_create();

	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});
	window_set_background_color(window,PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorWhite));
	const bool animated = true;
	s_sensor_flag = persist_exists(S_SENSOR_FLAG_PKEY) ? persist_read_bool(S_SENSOR_FLAG_PKEY) : false;
	s_sensor_format_flag = persist_exists(S_SENSOR_FORMAT_FLAG_PKEY) ? persist_read_bool(S_SENSOR_FORMAT_FLAG_PKEY) : false;
	window_stack_push(window, animated);

	app_message_register_inbox_received(in_received_handler);
	app_message_register_inbox_dropped(in_dropped_handler);
	app_message_register_outbox_sent(out_sent_handler);
	app_message_register_outbox_failed(out_failed_handler);

	const uint32_t inbound_size = 1024;
	const uint32_t outbound_size = 1024;
	app_message_open(inbound_size, outbound_size);
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