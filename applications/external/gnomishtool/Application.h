#pragma once

#define TOOL_DEFAULT    0
#define TOOL_LIGHTER    1
#define TOOL_WIRETESTER 2
#define TOOL_VOLT       3
#define TOOL_WORKSHOP   4

typedef struct {
    int selectedTool;
    int mode; //0-tool, 1-tool selected
    bool led;
    bool ringing;
    int lighter_mode;
    bool alarming;
    bool running;
    int counter;
    bool ExtLedOn;
    int IntLedMode;
    float pc3Voltage;
    FuriHalAdcHandle* adc_handle;
} App_Global_Data;

int AppInit();
int AppDeinit();
int AppWork();
void Draw(Canvas* c, void* ctx);

int KeyProc(int type, int key);
int ToolDefaultKey(int type, int key);
int ToolLighterKey(int type, int key);

void start_feedback();
void stop_feedback();
void ExtLedToggle(int action);
void IntLedToggle(int action);
