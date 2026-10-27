#include "Display.h"

#include <esp_heap_caps.h>
#include <esp_log.h>

#include "BoardConfig.h"

#define ICON_WIFI         "\xEF\x87\xAB"   // 0xF1EB wifi
#define ICON_WIFI_SLASH   "\xEF\x9A\xAC"   // 0xF6AC wifi-slash
#define ICON_LINK         "\xEF\x83\x81"   // 0xF0C1 link
#define ICON_UNLINK       "\xEF\x84\xA7"   // 0xF127 unlink
#define ICON_FINGERPRINT  "\xEF\x95\xB7"   // 0xF577 fingerprint
#define ICON_FACE         "\xEE\x8B\xBF"   // 0xE2FF face
#define ICON_CHECK_CIRCLE "\xEF\x81\x98"   // 0xF058 check-circle
#define ICON_TIMES_CIRCLE "\xEF\x81\x97"   // 0xF057 xmark

LV_FONT_DECLARE(font_vietnamese_18);
LV_FONT_DECLARE(font_vietnamese_24);
LV_FONT_DECLARE(font_awesome_14);
LV_FONT_DECLARE(font_awesome_48);

namespace {

constexpr int32_t kTopBarHeight = 45;

// Bảng màu
constexpr uint32_t kColBg        = 0x11111B;
constexpr uint32_t kColBar       = 0x1E1E2E;
constexpr uint32_t kColCard      = 0x181825;
constexpr uint32_t kColText      = 0xCDD6F4;
constexpr uint32_t kColSubtext   = 0xA6ADC8;
constexpr uint32_t kColWhite     = 0xFFFFFF;
constexpr uint32_t kColBlue      = 0x89B4FA;
constexpr uint32_t kColGreen     = 0xA6E3A1;
constexpr uint32_t kColRed       = 0xF38BA8;
constexpr uint32_t kColOkBg      = 0x183028;
constexpr uint32_t kColFailBg    = 0x3A1D28;

constexpr const char* kTag = "Display";

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, const lv_font_t* font,
                    uint32_t color, int32_t wrapWidth = 0) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    if (wrapWidth > 0) {
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl, wrapWidth);
    }
    return lbl;
}

lv_obj_t* makeCard(lv_obj_t* parent, int32_t w, int32_t h, uint32_t bg) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(bg), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_pad_row(card, 10, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return card;
}

void pulse(lv_obj_t* obj) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, [](void* o, int32_t v) {
        lv_obj_set_style_text_opa(static_cast<lv_obj_t*>(o), static_cast<lv_opa_t>(v), 0);
    });
    lv_anim_set_values(&a, 90, 255);
    lv_anim_set_duration(&a, 700);
    lv_anim_set_reverse_duration(&a, 700);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

} // namespace

Display::Display() {
    _mutex = xSemaphoreCreateMutex();
}

void Display::my_disp_flush(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    Display* self = static_cast<Display*>(lv_display_get_user_data(disp));
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    if (self && self->_gfx) {
        self->_gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)px_map, w, h);
    }
    lv_display_flush_ready(disp);
}

void Display::begin() {
    _bus = new Arduino_ESP32SPI(
        BoardConfig::kDisplayDcPin,
        GFX_NOT_DEFINED,                 // CS nối GND
        BoardConfig::kDisplaySckPin,
        BoardConfig::kDisplayMosiPin,
        GFX_NOT_DEFINED,                 // MISO
        FSPI
    );
    _gfx = new Arduino_ILI9341(_bus, BoardConfig::kDisplayRstPin, 0, false);

    if (!_gfx->begin(20000000)) {
        ESP_LOGE(kTag, "Failed to initialize display");
        return;
    }
    _gfx->fillScreen(0x0000);

    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return millis(); });

    lv_display_t* disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_user_data(disp, this);

    const size_t bufSize = screenWidth * 40 * 2;   // RGB565 = 2 byte/pixel
    buf = (uint8_t*)heap_caps_malloc(bufSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buf) {
        ESP_LOGE(kTag, "Failed to allocate memory for display buffer");
        return;
    }
    lv_display_set_buffers(disp, buf, NULL, bufSize, LV_DISPLAY_RENDER_MODE_PARTIAL);

    _ready = true;
}

void Display::post(Command cmd) {
    if (!_mutex) return;

    xSemaphoreTake(_mutex, portMAX_DELAY);
    if (_pending.size() >= kMaxPending) {
        ESP_LOGW(kTag, "UI queue full, dropping oldest command");
        _pending.pop_front();
    }
    _pending.push_back(std::move(cmd));
    xSemaphoreGive(_mutex);
}

void Display::drainPending() {
    std::deque<Command> local;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    local.swap(_pending);
    xSemaphoreGive(_mutex);

    for (auto& cmd : local) {
        cmd();
    }
}

void Display::taskHandler() {
    if (!_ready) return;
    drainPending();
    lv_timer_handler();
}

void Display::showBootMode(BootType mode) {
    post([this, mode] { doShowBootMode(mode); });
}

void Display::showConfigMode(const String& wifiName, const String& ip) {
    post([this, wifiName, ip] { doShowConfigMode(wifiName, ip); });
}

void Display::showIdle(const Status& status) {
    post([this, status] { doShowIdle(status); });
}

void Display::showProcessing(Method method) {
    post([this, method] { doShowProcessing(method); });
}

void Display::showCollecting(Method method, const String& hint) {
    post([this, method, hint] { doShowCollecting(method, hint); });
}

void Display::showResult(ResultType result, const String& message, int durationMs, const String& fullName) {
    post([this, result, message, durationMs, fullName] { doShowResult(result, message, durationMs, fullName); });
}

void Display::updateStatus(const Status& status) {
    post([this, status] { doUpdateStatus(status); });
}

lv_obj_t* Display::beginScreen(bool withTopBar) {
    if (_resultTimer) {
        lv_timer_delete(_resultTimer);
        _resultTimer = nullptr;
    }

    lv_obj_t* scr = lv_screen_active();
    lv_obj_clean(scr);

    lbl_wifi_icon = nullptr;
    lbl_server_icon = nullptr;
    lbl_time = nullptr;
    lbl_date = nullptr;

    lv_obj_set_style_bg_color(scr, lv_color_hex(kColBg), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    if (!withTopBar) {
        return scr;
    }

    buildTopBar(scr);

    lv_obj_t* content = lv_obj_create(scr);
    lv_obj_set_size(content, (int32_t)screenWidth, (int32_t)screenHeight - kTopBarHeight);
    lv_obj_set_pos(content, 0, kTopBarHeight);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    return content;
}

void Display::buildTopBar(lv_obj_t* parent) {
    lv_obj_t* top_bar = lv_obj_create(parent);
    lv_obj_set_size(top_bar, (int32_t)screenWidth, kTopBarHeight);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(kColBar), 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    lv_obj_set_style_pad_all(top_bar, 8, 0);
    lv_obj_remove_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

    lbl_wifi_icon = lv_label_create(top_bar);
    lv_obj_set_style_text_font(lbl_wifi_icon, &font_awesome_14, 0);
    lv_obj_align(lbl_wifi_icon, LV_ALIGN_LEFT_MID, 5, 0);

    lbl_server_icon = lv_label_create(top_bar);
    lv_obj_set_style_text_font(lbl_server_icon, &font_awesome_14, 0);
    lv_obj_align(lbl_server_icon, LV_ALIGN_LEFT_MID, 35, 0);

    lbl_time = lv_label_create(top_bar);
    lv_obj_set_style_text_color(lbl_time, lv_color_hex(kColWhite), 0);
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_time, LV_ALIGN_RIGHT_MID, -5, -8);

    lbl_date = lv_label_create(top_bar);
    lv_obj_set_style_text_color(lbl_date, lv_color_hex(kColSubtext), 0);
    lv_obj_set_style_text_font(lbl_date, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_date, LV_ALIGN_RIGHT_MID, -5, 10);

    applyStatus();
}

void Display::storeStatus(const Status& status) {
    if (&status == &_status) return;
    _status.wifiConnected = status.wifiConnected;
    _status.serverConnected = status.serverConnected;

    if (!status.time.isEmpty()) _status.time = status.time;
    if (!status.date.isEmpty()) _status.date = status.date;
}

void Display::applyStatus() {
    if (lbl_wifi_icon) {
        lv_label_set_text(lbl_wifi_icon, _status.wifiConnected ? ICON_WIFI : ICON_WIFI_SLASH);
        lv_obj_set_style_text_color(lbl_wifi_icon,
            lv_color_hex(_status.wifiConnected ? kColGreen : kColRed), 0);
    }
    if (lbl_server_icon) {
        lv_label_set_text(lbl_server_icon, _status.serverConnected ? ICON_LINK : ICON_UNLINK);
        lv_obj_set_style_text_color(lbl_server_icon,
            lv_color_hex(_status.serverConnected ? kColGreen : kColRed), 0);
    }
    if (lbl_time && !_status.time.isEmpty()) {
        lv_label_set_text(lbl_time, _status.time.c_str());
    }
    if (lbl_date && !_status.date.isEmpty()) {
        lv_label_set_text(lbl_date, _status.date.c_str());
    }
}

void Display::doUpdateStatus(const Status& status) {
    storeStatus(status);
    applyStatus();
}

void Display::doShowBootMode(BootType mode) {
    lv_obj_t* scr = beginScreen(false);

    lv_obj_t* lbl = makeLabel(scr,
        mode == BootType::Normal ? "ATTENDX\nĐang khởi động..." : "CHẾ ĐỘ CẤU HÌNH",
        &font_vietnamese_18, kColBlue);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
}

void Display::doShowConfigMode(const String& wifiName, const String& ip) {
    lv_obj_t* scr = beginScreen(false);

    String content = "CẤU HÌNH THIẾT BỊ\n\nKết nối WiFi:\n" + wifiName + "\n\nTruy cập Web:\nhttp://" + ip;
    lv_obj_t* lbl = makeLabel(scr, content.c_str(), &font_vietnamese_18, kColGreen, 220);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
}

void Display::doShowIdle(const Status& status) {
    storeStatus(status);
    lv_obj_t* content = beginScreen(true);

    lv_obj_t* card = makeCard(content, 210, 230, kColCard);
    makeLabel(card, "SẴN SÀNG CHẤM CÔNG\n\nĐặt vân tay hoặc\nđứng trước Camera",
              &font_vietnamese_18, kColText, 180);
}

void Display::doShowProcessing(Method method) {
    lv_obj_t* content = beginScreen(true);

    lv_obj_t* spinner = lv_spinner_create(content);
    lv_obj_set_size(spinner, 80, 80);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -30);
    lv_spinner_set_anim_params(spinner, 1000, 60);

    lv_obj_t* lbl = makeLabel(content,
        method == Method::Face ? "Đang nhận diện khuôn mặt..." : "Đang kiểm tra vân tay...",
        &font_vietnamese_18, kColWhite, 220);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 40);
}

void Display::doShowCollecting(Method method, const String& hint) {
    lv_obj_t* content = beginScreen(true);
    const bool isFace = (method == Method::Face);

    lv_obj_t* card = makeCard(content, 210, 240, kColCard);
    lv_obj_t* icon = makeLabel(card, isFace ? ICON_FACE : ICON_FINGERPRINT, &font_awesome_48, kColBlue);
    pulse(icon);

    makeLabel(card, isFace ? "ĐANG THU THẬP\nKHUÔN MẶT" : "ĐANG THU THẬP\nVÂN TAY",
              &font_vietnamese_18, kColText, 180);

    const char* defaultHint = isFace ? "Nhìn thẳng\nvào Camera" : "Đặt ngón tay\nlên cảm biến";
    makeLabel(card, hint.isEmpty() ? defaultHint : hint.c_str(),
              &font_vietnamese_18, kColSubtext, 180);
}

void Display::doShowResult(ResultType result, const String& message, int durationMs, const String& fullName) {
    lv_obj_t* content = beginScreen(true);
    const bool success = (result == ResultType::Success);
    const uint32_t accent = success ? kColGreen : kColRed;

    lv_obj_t* card = makeCard(content, 220, 255, success ? kColOkBg : kColFailBg);

    makeLabel(card, success ? ICON_CHECK_CIRCLE : ICON_TIMES_CIRCLE, &font_awesome_48, accent);
    makeLabel(card, success ? "THÀNH CÔNG" : "THẤT BẠI", &font_vietnamese_18, accent);

    if (success && !fullName.isEmpty()) {
        makeLabel(card, fullName.c_str(), &font_vietnamese_24, kColWhite, 190);
    }
    if (!message.isEmpty()) {
        makeLabel(card, message.c_str(), &font_vietnamese_18, kColText, 190);
    }

    if (durationMs > 0) {
        _resultTimer = lv_timer_create([](lv_timer_t* t) {
            Display* self = static_cast<Display*>(lv_timer_get_user_data(t));
            self->_resultTimer = nullptr; 
            self->doShowIdle(self->_status);
        }, (uint32_t)durationMs, this);
        lv_timer_set_repeat_count(_resultTimer, 1);
    }
}