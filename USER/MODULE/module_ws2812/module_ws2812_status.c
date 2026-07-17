#include "module_ws2812_status.h"
#include "module_ws2812_effect.h"




/*
红色警戒灯，用于开门/关门时    不全量显示，仅几个定位角
200ms 亮 200ms 灭
*/
static strip_flash_state_t door_flash_state = {0};
static strip_flash_state_t heliped_flash_state = {0};
static strip_run_state_t door_run_state = {0};
static strip_run_state_t heliped_run_state = {0};
static void dev_door_actioning(uint16_t cyctime)
{
    //color_time=4 cyctime=50   ->  200
    for(int i = 0; i<4; i++)
    {
        ws2812_flash_with(MODULE_WS2812_STRIP_0,(i<<2),(i<<2)+1,WS2812_RGB(252,3,3),WS2812_RGB(0,0,0),-1,90,&door_flash_state);
    }
    ws2812_flash_update(4,4,-1,&door_flash_state);
}

static void dev_heliped_actioning(uint16_t cyctime)
{
    for(int i = 0; i<4; i++)
    {
        ws2812_flash_with(MODULE_WS2812_STRIP_1,(i<<2),(i<<2)+1,WS2812_RGB(252,3,3),WS2812_RGB(0,0,0),-1,90,&heliped_flash_state);
    }
    ws2812_flash_update(4,4,-1,&heliped_flash_state);
}


/*
导航灯：四角白色定位，绿色作为边界指示，黄色跑马灯是正向指示灯
定位：白光：100ms亮，100ms灭，循环2次，1s内
边界：绿色常亮
跑马灯：
*/
static void dev_heliped_navigate(uint16_t cyctime)
{
    int i = 0;
    for(; i<4; i++)
    {
        ws2812_flash_with(MODULE_WS2812_STRIP_1,(i<<2),(i<<2)+1,WS2812_RGB(255,255,255),WS2812_RGB(0,0,0),2,90,&heliped_flash_state);
    }
    ws2812_flash_update(2,2,2,&heliped_flash_state);

    for(i = 0; i<4; i++)
    {
        if(i == 3)
        {
            ws2812_run_with(MODULE_WS2812_STRIP_1,WS2812_RGB(255,255,255),2,&heliped_run_state);
            continue;
        }
        module_ws2812_effect_at(MODULE_WS2812_STRIP_1,2+(i<<4),3+(i<<4),WS2812_RGB(0,252,93),90);
    }
    
}

static door_running_in_e door_running;
static heliped_running_in_e heliped_running;

typedef void (*ws2812_strip_update_cb_t)(uint16_t cyctime);

static void update_door_strip(uint16_t cyctime)
{
    switch(door_running)
    {
        case E_DOOR_RUN_IN_ACTION:
            dev_door_actioning(cyctime);
            break;

        default:
            break;
    }
}

static void update_heliped_strip(uint16_t cyctime)
{
    static uint16_t run_cnt;

    switch(heliped_running)
    {
        case E_HELIPED_RUN_IN_ACTION:
            dev_heliped_actioning(cyctime);
            break;

        case E_HELIPED_NAVIGATE:
            run_cnt++;
            if(run_cnt >= 20)
            {
                run_cnt = 0;
                heliped_flash_state.stage = 0;
                heliped_flash_state.elapsed_ticks = 0;
                heliped_flash_state.completed_cycles = 0;
            }

            dev_heliped_navigate(cyctime);
            break;

        default:
            break;
    }
}

static const ws2812_strip_update_cb_t strip_update_cbs[MODULE_WS2812_ID_COUNT] = {
    [MODULE_WS2812_STRIP_0] = update_door_strip,
    [MODULE_WS2812_STRIP_1] = update_heliped_strip,
};


void set_door_running_in(door_running_in_e in)
{
    door_running = in;
}

void set_heliped_running_in(heliped_running_in_e in)
{
    heliped_running = in;
}




void module_ws2812_status_update(uint16_t cyctime)
{
    uint32_t strip_index;

    for (strip_index = 0U; strip_index < MODULE_WS2812_ID_COUNT; strip_index++)
    {
        module_ws2812_id_e strip_id = (module_ws2812_id_e)strip_index;

        module_ws2812_clear_work_frame();

        if (strip_update_cbs[strip_id] != 0)
        {
            strip_update_cbs[strip_id](cyctime);
        }

        module_ws2812_copy_work_frame_to_dma(strip_id);
    }
}
