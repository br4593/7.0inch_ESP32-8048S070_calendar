#include "fixtures.hpp"
#include "app/ui_controller.hpp"
#include "Arduino.h"
#include <lvgl.h>
extern "C" {
#include "ui_generated/screens.h"
#include "ui_generated/ui.h"
}
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr int kWidth=800,kHeight=480;
std::array<std::uint16_t,kWidth*kHeight> pixels{};
std::array<std::uint16_t,kWidth*40> draw_buffer{};
std::array<void*,4> supplementary_pools{};
std::ofstream geometry;

void flush(lv_display_t* display,const lv_area_t* area,std::uint8_t* data){
    auto* source=reinterpret_cast<std::uint16_t*>(data);
    for(int y=area->y1;y<=area->y2;++y)for(int x=area->x1;x<=area->x2;++x)pixels[y*kWidth+x]=*source++;
    lv_display_flush_ready(display);
}
void pump(int rounds=12){for(int i=0;i<rounds;++i){preview_millis+=20;lv_tick_inc(20);calendar::tick_calendar_ui();ui_tick();lv_timer_handler();}}
void capture(const std::string& path){
    lv_obj_update_layout(lv_screen_active());lv_obj_invalidate(lv_screen_active());lv_refr_now(nullptr);
    std::ofstream output(path,std::ios::binary);output<<"P6\n800 480\n255\n";
    for(auto color:pixels){const auto r=(color>>11)&31,g=(color>>5)&63,b=color&31;const char rgb[]{static_cast<char>((r<<3)|(r>>2)),static_cast<char>((g<<2)|(g>>4)),static_cast<char>((b<<3)|(b>>2))};output.write(rgb,3);}
}
void click(lv_obj_t* object){lv_obj_send_event(object,LV_EVENT_CLICKED,nullptr);pump(20);}
lv_obj_t* visible_child(lv_obj_t* parent,unsigned ordinal=0){if(!parent)return nullptr;for(unsigned i=0;i<lv_obj_get_child_count(parent);++i){auto* child=lv_obj_get_child(parent,static_cast<int32_t>(i));if(!lv_obj_has_flag(child,LV_OBJ_FLAG_HIDDEN)&&ordinal--==0)return child;}return nullptr;}
bool descendant_label_contains(lv_obj_t* parent,const char* needle){if(!parent)return false;if(lv_obj_check_type(parent,&lv_label_class)){const char* text=lv_label_get_text(parent);return text&&std::strstr(text,needle);}for(unsigned i=0;i<lv_obj_get_child_count(parent);++i)if(descendant_label_contains(lv_obj_get_child(parent,static_cast<int32_t>(i)),needle))return true;return false;}
lv_obj_t* visible_row_containing(lv_obj_t* parent,const char* needle){if(!parent)return nullptr;for(unsigned i=0;i<lv_obj_get_child_count(parent);++i){auto* child=lv_obj_get_child(parent,static_cast<int32_t>(i));if(!lv_obj_has_flag(child,LV_OBJ_FLAG_HIDDEN)&&descendant_label_contains(child,needle))return child;}return nullptr;}
lv_obj_t* first_label(lv_obj_t* parent){if(!parent)return nullptr;for(unsigned i=0;i<lv_obj_get_child_count(parent);++i){auto* child=lv_obj_get_child(parent,static_cast<int32_t>(i));if(lv_obj_check_type(child,&lv_label_class)&&!lv_obj_has_flag(child,LV_OBJ_FLAG_HIDDEN))return child;if(auto* nested=first_label(child))return nested;}return nullptr;}
lv_obj_t* find_type(lv_obj_t* parent,const lv_obj_class_t* type){if(!parent||lv_obj_has_flag(parent,LV_OBJ_FLAG_HIDDEN))return nullptr;if(lv_obj_check_type(parent,type))return parent;for(unsigned i=0;i<lv_obj_get_child_count(parent);++i)if(auto* match=find_type(lv_obj_get_child(parent,static_cast<int32_t>(i)),type))return match;return nullptr;}
std::string clean(const char* text){std::string value=text?text:"";for(char& c:value)if(c=='\n'||c=='\r'||c=='\t')c=' ';return value;}
std::string half(int doubled){std::ostringstream out;out<<(doubled/2);if(std::abs(doubled)%2)out<<(doubled<0?".5":".5");return out.str();}

void dump_object(const char* screen,const char* name,lv_obj_t* object){
    if(!object)return;lv_obj_update_layout(object);lv_area_t a{},content{};lv_obj_get_coords(object,&a);lv_obj_get_content_coords(object,&content);
    const bool label=lv_obj_check_type(object,&lv_label_class);const char* text=label?lv_label_get_text(object):"";
    geometry<<"OBJECT\t"<<screen<<'\t'<<name<<'\t'<<a.x1<<'\t'<<a.y1<<'\t'<<(a.x2-a.x1+1)<<'\t'<<(a.y2-a.y1+1)
            <<'\t'<<content.x1<<'\t'<<content.y1<<'\t'<<(content.x2-content.x1+1)<<'\t'<<(content.y2-content.y1+1)
            <<'\t'<<lv_obj_get_style_border_width(object,LV_PART_MAIN)
            <<'\t'<<lv_obj_get_style_pad_left(object,LV_PART_MAIN)<<'\t'<<lv_obj_get_style_pad_right(object,LV_PART_MAIN)
            <<'\t'<<lv_obj_get_style_pad_top(object,LV_PART_MAIN)<<'\t'<<lv_obj_get_style_pad_bottom(object,LV_PART_MAIN)
            <<'\t'<<clean(text)<<'\n';
}
void dump_button(const char* screen,const char* name,lv_obj_t* button){
    if(!button||lv_obj_has_flag(button,LV_OBJ_FLAG_HIDDEN))return;lv_obj_update_layout(button);lv_area_t b{};lv_obj_get_coords(button,&b);auto* label=first_label(button);if(!label){geometry<<"BUTTON\t"<<screen<<'\t'<<name<<"\tNO_LABEL\n";return;}lv_area_t l{};lv_obj_get_coords(label,&l);
    const auto* font=lv_obj_get_style_text_font(label,LV_PART_MAIN);lv_point_t size{};const char* text=lv_label_get_text(label);lv_text_get_size(&size,text?text:"",font,lv_obj_get_style_text_letter_space(label,LV_PART_MAIN),lv_obj_get_style_text_line_space(label,LV_PART_MAIN),lv_obj_get_content_width(label),LV_TEXT_FLAG_NONE);
    const int dx2=(l.x1+l.x2)-(b.x1+b.x2),dy2=(l.y1+l.y2)-(b.y1+b.y2);
    const bool contained=l.x1>=b.x1&&l.x2<=b.x2&&l.y1>=b.y1&&l.y2<=b.y2;
    geometry<<"BUTTON\t"<<screen<<'\t'<<name<<'\t'<<b.x1<<'\t'<<b.y1<<'\t'<<(b.x2-b.x1+1)<<'\t'<<(b.y2-b.y1+1)
            <<'\t'<<l.x1<<'\t'<<l.y1<<'\t'<<(l.x2-l.x1+1)<<'\t'<<(l.y2-l.y1+1)
            <<'\t'<<half(dx2)<<'\t'<<half(dy2)<<'\t'<<(contained?"yes":"no")
            <<'\t'<<size.x<<'\t'<<size.y<<'\t'<<font->line_height<<'\t'<<font->base_line
            <<'\t'<<lv_obj_get_style_border_width(button,LV_PART_MAIN)
            <<'\t'<<lv_obj_get_style_pad_left(button,LV_PART_MAIN)<<'\t'<<lv_obj_get_style_pad_right(button,LV_PART_MAIN)
            <<'\t'<<lv_obj_get_style_pad_top(button,LV_PART_MAIN)<<'\t'<<lv_obj_get_style_pad_bottom(button,LV_PART_MAIN)
            <<'\t'<<std::hex<<lv_color_to_u32(lv_obj_get_style_text_color_filtered(label,LV_PART_MAIN))
            <<'\t'<<lv_color_to_u32(lv_obj_get_style_text_color_filtered(button,LV_PART_MAIN))<<std::dec
            <<'\t'<<static_cast<unsigned>(lv_obj_get_style_text_opa(label,LV_PART_MAIN))
            <<'\t'<<clean(text)<<'\n';
}
void dump_group(const char* screen,const char* name,std::initializer_list<lv_obj_t*> items){
    geometry<<"GROUP\t"<<screen<<'\t'<<name;int previous=-1;for(auto* item:items){lv_area_t a{};lv_obj_get_coords(item,&a);geometry<<'\t'<<a.x1<<','<<a.y1<<','<<(a.x2-a.x1+1)<<','<<(a.y2-a.y1+1);if(previous>=0)geometry<<",gap="<<(a.x1-previous-1);previous=a.x2;}geometry<<'\n';
}
const char* class_name(lv_obj_t* object){
    if(lv_obj_check_type(object,&lv_label_class))return "label";
    if(lv_obj_check_type(object,&lv_button_class))return "button";
    if(lv_obj_check_type(object,&lv_textarea_class))return "textarea";
    if(lv_obj_check_type(object,&lv_dropdown_class))return "dropdown";
    if(lv_obj_check_type(object,&lv_keyboard_class))return "keyboard";
    if(lv_obj_check_type(object,&lv_table_class))return "table";
    if(lv_obj_check_type(object,&lv_switch_class))return "switch";
    if(lv_obj_check_type(object,&lv_slider_class))return "slider";
    if(lv_obj_check_type(object,&lv_bar_class))return "bar";
    if(lv_obj_check_type(object,&lv_image_class))return "image";
    return "object";
}
void dump_tree(const char* screen,lv_obj_t* object,int depth=0){
    if(!object||lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN))return;lv_area_t a{};lv_obj_get_coords(object,&a);const char* klass=class_name(object);const char* text=lv_obj_check_type(object,&lv_label_class)?lv_label_get_text(object):"";
    geometry<<"TREE\t"<<screen<<'\t'<<depth<<'\t'<<(klass?klass:"")<<'\t'<<a.x1<<'\t'<<a.y1<<'\t'<<(a.x2-a.x1+1)<<'\t'<<(a.y2-a.y1+1)<<'\t'<<clean(text)<<'\n';
    if(lv_obj_check_type(object,&lv_button_class))dump_button(screen,"dynamic/button",object);
    for(unsigned i=0;i<lv_obj_get_child_count(object);++i)dump_tree(screen,lv_obj_get_child(object,static_cast<int32_t>(i)),depth+1);
}
void standard_header(const char* screen,lv_obj_t* wifi,lv_obj_t* settings){dump_button(screen,"header/wifi",wifi);dump_button(screen,"header/settings",settings);dump_group(screen,"header/actions",{wifi,settings});}
void primary_nav(const char* screen,lv_obj_t* a,lv_obj_t* b,lv_obj_t* c){dump_button(screen,"nav/today",a);dump_button(screen,"nav/calendar",b);dump_button(screen,"nav/forecast",c);dump_group(screen,"nav/buttons",{a,b,c});}
void dump_dropdown_measure(const char* screen,lv_obj_t* dropdown){
    if(!dropdown)return;dump_object(screen,"units-dropdown",dropdown);const auto* font=lv_obj_get_style_text_font(dropdown,LV_PART_MAIN);const char* symbol=lv_dropdown_get_symbol(dropdown);lv_point_t metric{},imperial{},symbol_size{};lv_text_get_size(&metric,"Metric (C, km/h)",font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);lv_text_get_size(&imperial,"Imperial (F, mph)",font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);lv_text_get_size(&symbol_size,symbol?symbol:"",font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);lv_area_t content{};lv_obj_get_content_coords(dropdown,&content);geometry<<"MEASURE\t"<<screen<<"\tunits-dropdown\tcontent="<<(content.x2-content.x1+1)<<"\tmetric="<<metric.x<<"\timperial="<<imperial.x<<"\tsymbol="<<symbol_size.x<<"\ttext+symbol(metric)="<<(metric.x+symbol_size.x)<<"\ttext+symbol(imperial)="<<(imperial.x+symbol_size.x)<<'\n';
}

void audit_today(const std::string& out){calendar::show_calendar_main();pump(20);standard_header("today",objects.main_wifi_button,objects.main_settings_button);dump_button("today","weather/card",objects.main_weather_card);primary_nav("today",objects.main_nav_main_button,objects.main_nav_calendar_button,objects.main_nav_forecast_button);dump_group("today","cards",{objects.main_events_container,objects.main_weather_card});dump_tree("today",objects.main_screen);capture(out+"/today.ppm");}
void audit_week(const std::string& out){calendar::show_calendar_primary_page();pump(20);calendar::show_calendar_week_view();pump(20);standard_header("week",objects.agenda_wifi_button,objects.settings_button);for(auto p:std::array<std::pair<const char*,lv_obj_t*>,5>{{{"period/prev",objects.agenda_previous_button},{"period/today",objects.agenda_today_button},{"period/next",objects.agenda_next_button},{"period/week",objects.agenda_week_button},{"period/month",objects.agenda_month_button}}})dump_button("week",p.first,p.second);dump_group("week","period/left",{objects.agenda_previous_button,objects.agenda_today_button,objects.agenda_next_button});dump_group("week","period/right",{objects.agenda_week_button,objects.agenda_month_button});const std::array<lv_obj_t*,7> days{objects.day_button_0,objects.day_button_1,objects.day_button_2,objects.day_button_3,objects.day_button_4,objects.day_button_5,objects.day_button_6};for(int i=0;i<7;++i)dump_button("week",("day/"+std::to_string(i)).c_str(),days[i]);dump_group("week","day/buttons",{objects.day_button_0,objects.day_button_1,objects.day_button_2,objects.day_button_3,objects.day_button_4,objects.day_button_5,objects.day_button_6});primary_nav("week",objects.agenda_nav_main_button,objects.agenda_nav_calendar_button,objects.agenda_nav_forecast_button);dump_tree("week",objects.agenda_screen);capture(out+"/week.ppm");}
void audit_month(const std::string& out){calendar::show_calendar_primary_page();pump();calendar::show_calendar_month_view();pump(20);standard_header("month",objects.month_wifi_button,objects.month_settings_button);for(auto p:std::array<std::pair<const char*,lv_obj_t*>,5>{{{"period/prev",objects.month_previous_button},{"period/today",objects.month_today_button},{"period/next",objects.month_next_button},{"period/week",objects.month_week_button},{"period/month",objects.month_month_button}}})dump_button("month",p.first,p.second);dump_group("month","period/left",{objects.month_previous_button,objects.month_today_button,objects.month_next_button});dump_group("month","period/right",{objects.month_week_button,objects.month_month_button});dump_group("month","weekday/labels",{objects.month_weekday_label_0,objects.month_weekday_label_1,objects.month_weekday_label_2,objects.month_weekday_label_3,objects.month_weekday_label_4,objects.month_weekday_label_5,objects.month_weekday_label_6});dump_object("month","grid",objects.month_grid_container);primary_nav("month",objects.month_nav_main_button,objects.month_nav_calendar_button,objects.month_nav_forecast_button);dump_tree("month",objects.month_screen);capture(out+"/month.ppm");}
void audit_forecast(const std::string& out){calendar::show_calendar_forecast();pump(20);dump_button("forecast","header/back",objects.weather_back_button);dump_button("forecast","header/wifi",objects.forecast_wifi_button);dump_button("forecast","action/location",objects.weather_set_location_button);dump_button("forecast","action/refresh",objects.weather_refresh_button);dump_group("forecast","header/actions",{objects.weather_back_button,objects.forecast_wifi_button,objects.weather_set_location_button,objects.weather_refresh_button});primary_nav("forecast",objects.forecast_nav_main_button,objects.forecast_nav_calendar_button,objects.forecast_nav_forecast_button);dump_tree("forecast",objects.weather_screen);capture(out+"/forecast.ppm");calendar::open_calendar_weather_location_editor();pump(20);dump_tree("weather-form",objects.weather_screen);dump_dropdown_measure("weather-form",find_type(objects.weather_screen,&lv_dropdown_class));capture(out+"/weather-form.ppm");}
void audit_details(const std::string& out){
    calendar::show_calendar_week_view();pump(20);if(auto* row=visible_row_containing(objects.agenda_rows_container,"deliberately long"))click(row);dump_button("details","header/back",objects.details_back_button);dump_object("details","heading",objects.details_heading_label);dump_object("details","content",objects.details_content);dump_object("details","title",objects.details_title_label);dump_object("details","time",objects.details_time_label);dump_object("details","location",objects.details_location_label);dump_object("details","calendar",objects.details_calendar_label);dump_tree("details",objects.event_details_screen);capture(out+"/details.ppm");
    std::string title(160,'W');std::string location(160,'W');
    const std::string document="X-ESP32-CALENDAR-ID:audit\r\nX-ESP32-CALENDAR-NAME:Audit\r\nX-ESP32-CALENDAR-COLOR:2D7FF9\r\nBEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nUID:long-scroll\r\nDTSTART:20260928T070000Z\r\nDTEND:20260928T080000Z\r\nSUMMARY:"+title+"\r\nLOCATION:"+location+"\r\nEND:VEVENT\r\nBEGIN:VEVENT\r\nUID:short-after-scroll\r\nDTSTART:20260928T090000Z\r\nDTEND:20260928T100000Z\r\nSUMMARY:Short event after scroll\r\nLOCATION:Room 1\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n";preview::queue_calendar(document);pump(120);calendar::show_calendar_week_view();pump(40);
    if(auto* long_row=visible_row_containing(objects.agenda_rows_container,"WWWW")){click(long_row);lv_obj_update_layout(objects.details_content);const int32_t bottom=lv_obj_get_scroll_bottom(objects.details_content);lv_obj_scroll_to_y(objects.details_content,10000,LV_ANIM_OFF);pump(4);const int32_t scrolled=lv_obj_get_scroll_y(objects.details_content);capture(out+"/details-long-scrolled.ppm");click(objects.details_back_button);if(auto* short_row=visible_row_containing(objects.agenda_rows_container,"Short event")){click(short_row);lv_obj_update_layout(objects.details_content);const int32_t reopened=lv_obj_get_scroll_y(objects.details_content);geometry<<"CHECK\tdetails\tscroll-reset\tlong-bottom="<<bottom<<"\tlong-scrolled="<<scrolled<<"\tshort-reopened="<<reopened<<'\n';capture(out+"/details-short-after-long.ppm");}}
}
void audit_settings(const std::string& out){calendar::show_calendar_settings();pump(20);dump_button("settings","header/back",objects.settings_back_button);for(auto p:std::array<std::pair<const char*,lv_obj_t*>,6>{{{"action/wifi",objects.settings_enter_wifi_button},{"action/setup",objects.settings_start_setup_button},{"action/sync",objects.settings_sync_now_button},{"action/appearance",objects.settings_brightness_button},{"action/weather",objects.settings_weather_button},{"action/firmware",objects.settings_firmware_button}}})dump_button("settings",p.first,p.second);dump_group("settings","action/row1",{objects.settings_enter_wifi_button,objects.settings_start_setup_button,objects.settings_sync_now_button});dump_group("settings","action/row2",{objects.settings_brightness_button,objects.settings_weather_button,objects.settings_firmware_button});dump_tree("settings",objects.settings_screen);capture(out+"/settings.ppm");calendar::open_calendar_display_wifi_setup();pump(20);dump_tree("wifi-form",objects.settings_screen);capture(out+"/wifi-form.ppm");}
void audit_appearance(const std::string& out){calendar::show_calendar_brightness_settings();pump(20);dump_button("appearance","header/back",objects.brightness_back_button);dump_object("appearance","heading",objects.appearance_heading_label);dump_object("appearance","card",objects.brightness_card);dump_object("appearance","preview",objects.theme_preview_canvas);dump_object("appearance","theme-dropdown",objects.theme_style_dropdown);dump_object("appearance","dark-switch",objects.dark_theme_switch);dump_object("appearance","brightness-slider",objects.brightness_slider);dump_tree("appearance",objects.brightness_settings_screen);capture(out+"/appearance.ppm");}
void audit_firmware(const std::string& out){calendar::show_calendar_firmware_update();pump(20);dump_button("firmware","header/back",objects.firmware_back_button);dump_button("firmware","action/enable",objects.firmware_enable_button);dump_button("firmware","action/cancel",objects.firmware_cancel_button);dump_button("firmware","action/reboot",objects.firmware_reboot_button);dump_group("firmware","action/buttons",{objects.firmware_enable_button,objects.firmware_cancel_button,objects.firmware_reboot_button});dump_object("firmware","heading",objects.firmware_heading_label);dump_object("firmware","version",objects.firmware_version_label);dump_object("firmware","state",objects.firmware_state_label);dump_object("firmware","instructions",objects.firmware_instructions_label);dump_object("firmware","progress",objects.firmware_progress_bar);dump_tree("firmware",objects.firmware_update_screen);capture(out+"/firmware.ppm");}
}

int main(int argc,char** argv){
    const std::string out=argc>1?argv[1]:"/tmp/esp32-alignment-audit-qa/output";std::filesystem::create_directories(out);geometry.open(out+"/geometry.tsv");
    geometry<<"TYPE\tSCREEN\tNAME/DEPTH\tX\tY\tW\tH\t...\n";
    setenv("TZ","IST-2IDT,M3.4.4/26,M10.5.0",1);tzset();std::tm local{};local.tm_year=126;local.tm_mon=8;local.tm_mday=28;local.tm_hour=9;local.tm_min=41;local.tm_isdst=-1;preview::now=std::mktime(&local);preview::appearance.theme_id=calendar::ThemeId::SilverBlue;preview::appearance.dark_theme=false;preview::set_fixture(true,false);
    lv_init();for(auto& pool:supplementary_pools){pool=std::malloc(64*1024);if(!pool||!lv_mem_add_pool(pool,64*1024))return 2;}auto* display=lv_display_create(kWidth,kHeight);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(display,draw_buffer.data(),nullptr,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,flush);ui_init();calendar::initialize_calendar_ui();pump(40);
    audit_today(out);audit_week(out);audit_month(out);audit_forecast(out);audit_details(out);audit_settings(out);audit_appearance(out);audit_firmware(out);geometry.flush();std::cout<<out<<"/geometry.tsv\n";return 0;
}
