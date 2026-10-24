#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

#include <deque>
#include <functional>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Quy tắc luồng:
//  - begin() và taskHandler() chỉ được gọi từ task main (loop()).
//  - Các hàm showXxx() / updateStatus() an toàn khi gọi từ BẤT KỲ task nào (mạng, vân tay, camera...).
//    Chúng chỉ đẩy lệnh vào hàng đợi và trả về ngay; lệnh được thực thi trong taskHandler()
//    nên mọi lời gọi LVGL / SPI luôn nằm trong task main.
class Display {
public:
    enum class BootType { Configuration, Normal };
    enum class Method { Face, Fingerprint };
    enum class ResultType { Success, Failed };

    struct Status {
        bool wifiConnected = false;
        bool serverConnected = false;
        String date = "";
        String time = "";
    };

    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    static Display& getInstance() {
        static Display instance;
        return instance;
    }

    void begin();
    void taskHandler();

    void showBootMode(BootType mode);
    void showConfigMode(const String& wifiName, const String& ip);

    void showIdle(const Status& status);
    void showProcessing(Method method);
    void showCollecting(Method method, const String& hint = "");
    void showResult(ResultType result, const String& message, int durationMs = 2000, const String& fullName = "");

    void updateStatus(const Status& status);

private:
    Display();
    ~Display() = default;

    Arduino_DataBus* _bus = nullptr;
    Arduino_GFX* _gfx = nullptr;

    static constexpr uint32_t screenWidth = 240;
    static constexpr uint32_t screenHeight = 320;

    uint8_t* buf = nullptr;

    Status _status;                    // chỉ đọc/ghi trong task main
    lv_timer_t* _resultTimer = nullptr;

    lv_obj_t* lbl_wifi_icon = nullptr;
    lv_obj_t* lbl_server_icon = nullptr;
    lv_obj_t* lbl_time = nullptr;
    lv_obj_t* lbl_date = nullptr;

    using Command = std::function<void()>;
    static constexpr size_t kMaxPending = 64;

    SemaphoreHandle_t _mutex = nullptr;
    std::deque<Command> _pending;
    bool _ready = false; 

    void post(Command cmd);
    void drainPending();

    void doShowBootMode(BootType mode);
    void doShowConfigMode(const String& wifiName, const String& ip);
    void doShowIdle(const Status& status);
    void doShowProcessing(Method method);
    void doShowCollecting(Method method, const String& hint);
    void doShowResult(ResultType result, const String& message, int durationMs, const String& fullName);
    void doUpdateStatus(const Status& status);

    static void my_disp_flush(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map);

    lv_obj_t* beginScreen(bool withTopBar);
    void buildTopBar(lv_obj_t* parent);
    void storeStatus(const Status& status);
    void applyStatus();
};

#endif // DISPLAY_H