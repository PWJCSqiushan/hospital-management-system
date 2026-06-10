/*
 * Copyright (c) 2026, HIS Team.
 * 文件名称：main.cpp
 * 修改摘要：【升级6.0 最终修正】
 * 1. 新增时间推进时自动清理过期待诊患者和预约记录。
 * 2. 主菜单显示当前日期，支持 ANSI 颜色。
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "utils.h"
#include "hospital_system.h"
#include "demo_data.h"
#include "file_io.h"

Doctor* g_doctor_head = NULL;
Patient* g_patient_head = NULL;
Medicine* g_medicine_head = NULL;
Ward* g_ward_head = NULL;
Appointment* g_appointment_head = NULL;
SignInQueue* g_sign_in_head = NULL;

char g_current_date[12] = "2026-03-09";

static void advance_time_flow(void);
static void reset_system_data(void);
static void manual_save_data(void);

static void enable_ansi_support(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

static void draw_interactive_menu(int current_select) {
    clear_screen();
    printf("========================================\n");
    printf("        欢迎来到医院管理系统\n");
    printf("             版本 6.0\n");
    printf("========================================\n");
    printf("        当前系统日期: %s\n", g_current_date);
    printf("----------------------------------------\n\n");

    const char* options[] = {
        " 0. 退出系统",
        " 1. 患者",
        " 2. 医生",
        " 3. 药房/收费管理人员",
        " 4. 医院管理人员 (数据分析)",
        " 5. 推进时间一天",
        " 6. 重置/初始化系统数据",
        " 7. 手动保存数据"
    };

    int option_codes[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    int num_options = 8;

    for (int i = 0; i < num_options; i++) {
        if (i == current_select) {
            switch (option_codes[i]) {
            case 1: printf("\033[42;37m%s\033[0m\n", options[i]); break;
            case 2: printf("\033[46;37m%s\033[0m\n", options[i]); break;
            case 3: printf("\033[43;30m%s\033[0m\n", options[i]); break;
            case 4: printf("\033[45;37m%s\033[0m\n", options[i]); break;
            case 5: printf("\033[44;37m%s\033[0m\n", options[i]); break;
            case 6: printf("\033[41;37m%s\033[0m\n", options[i]); break;
            case 7: printf("\033[46;30m%s\033[0m\n", options[i]); break;
            default: printf("\033[47;30m%s\033[0m\n", options[i]); break;
            }
        }
        else {
            switch (option_codes[i]) {
            case 1: printf("\033[32m%s\033[0m\n", options[i]); break;
            case 2: printf("\033[36m%s\033[0m\n", options[i]); break;
            case 3: printf("\033[33m%s\033[0m\n", options[i]); break;
            case 4: printf("\033[35m%s\033[0m\n", options[i]); break;
            case 5: printf("\033[34m%s\033[0m\n", options[i]); break;
            case 6: printf("\033[31m%s\033[0m\n", options[i]); break;
            case 7: printf("\033[36m%s\033[0m\n", options[i]); break;
            default: printf("%s\n", options[i]); break;
            }
        }
    }
    printf("\n----------------------------------------\n");
    printf("请使用键盘【↑】【↓】选择，按【Enter】确认，或直接输入数字键。\n");
}

static void advance_time_flow(void) {
    int year, month, day;
    if (sscanf(g_current_date, "%d-%d-%d", &year, &month, &day) != 3) {
        printf("日期格式错误，无法推进。\n");
        press_enter_to_continue();
        return;
    }

    int days_in_month[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)))
        days_in_month[1] = 29;

    day++;
    if (day > days_in_month[month - 1]) {
        day = 1;
        month++;
        if (month > 12) {
            month = 1;
            year++;
        }
    }

    sprintf(g_current_date, "%04d-%02d-%02d", year, month, day);

    // 清理过期待诊患者（状态为0且挂号日期小于当前日期）
    Patient* p = g_patient_head;
    Patient* prev = NULL;
    while (p) {
        if (p->status == 0) {
            // 获取该患者的挂号日期
            char reg_date[12] = "";
            MedicalRecord* mr = p->record_head;
            while (mr) {
                if (strcmp(mr->type, "挂号") == 0) {
                    strncpy(reg_date, mr->date, 10);
                    reg_date[10] = '\0';
                    break;
                }
                mr = mr->next;
            }
            // 如果挂号日期小于当前日期，标记为过期
            if (strlen(reg_date) > 0 && strcmp(reg_date, g_current_date) < 0) {
                // 添加过期记录
                MedicalRecord* nr = (MedicalRecord*)malloc(sizeof(MedicalRecord));
                if (nr) {
                    strcpy(nr->type, "系统处理");
                    strcpy(nr->doctor_name, "系统");
                    sprintf(nr->content, "挂号过期未就诊，挂号日期：%s", reg_date);
                    strcpy(nr->date, g_current_date);
                    nr->next = p->record_head;
                    p->record_head = nr;
                }
                p->status = 0;      // 保持待诊状态
                p->checked_in = 0;  // 签到作废
                // 释放床位（如果有）
                if (p->room_no > 0) {
                    Ward* w = g_ward_head;
                    while (w) {
                        if (w->room_no == p->room_no && w->occupied_beds > 0) {
                            w->occupied_beds--;
                            break;
                        }
                        w = w->next;
                    }
                    p->room_no = 0;
                }
            }
        }
        p = p->next;
    }

    // 清理过期预约记录（预约日期小于当前日期）
    Appointment* app = g_appointment_head;
    Appointment* app_prev = NULL;
    while (app) {
        if (strcmp(app->date, g_current_date) < 0) {
            Appointment* to_delete = app;
            if (app_prev) {
                app_prev->next = app->next;
                app = app->next;
            }
            else {
                g_appointment_head = app->next;
                app = g_appointment_head;
            }
            free(to_delete);
        }
        else {
            app_prev = app;
            app = app->next;
        }
    }

    printf("\n系统时间已推进至: %s\n", g_current_date);
    printf("过期待诊患者已自动清理，过期预约已删除。\n");
    press_enter_to_continue();
}

static void reset_system_data(void) {
    printf("\n[警告] 重置将清空所有患者、预约、病历记录，并恢复医生/药品/病房初始状态。\n");
    if (!ask_yes_no("是否继续？(Y/N): ")) {
        printf("操作已取消。\n");
        press_enter_to_continue();
        return;
    }

    Patient* p = g_patient_head;
    while (p) {
        Patient* next = p->next;
        MedicalRecord* mr = p->record_head;
        while (mr) {
            MedicalRecord* mr_next = mr->next;
            free(mr);
            mr = mr_next;
        }
        free(p);
        p = next;
    }
    g_patient_head = NULL;

    Appointment* app = g_appointment_head;
    while (app) {
        Appointment* next = app->next;
        free(app);
        app = next;
    }
    g_appointment_head = NULL;

    Doctor* d = g_doctor_head;
    while (d) {
        d->is_active = 1;
        d = d->next;
    }

    Ward* w = g_ward_head;
    while (w) {
        w->occupied_beds = 0;
        w = w->next;
    }

    Medicine* m = g_medicine_head;
    while (m) {
        m->stock = 1000;
        m->total_sold = 0;
        m = m->next;
    }

    strcpy(g_current_date, "2026-03-09");

    generate_demo_data();

    save_all_data();
    printf("\n系统已重置，演示数据已注入，并已保存至文件。\n");
    press_enter_to_continue();
}

static void manual_save_data(void) {
    if (save_all_data()) {
        printf("\n数据保存成功！\n");
    }
    else {
        printf("\n数据保存失败，请检查磁盘空间或权限。\n");
    }
    press_enter_to_continue();
}

int main() {
    enable_ansi_support();

    if (!load_all_data()) {
        init_hospital_data();
        generate_demo_data();
        printf("首次运行，已初始化基础数据和演示患者。\n");
    }

    int current_select = 1;
    int option_codes[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    int max_options = 8;
    int run = 1;

    while (run) {
        draw_interactive_menu(current_select);
        int key = get_single_key();

        if (key == 224) {
            key = get_single_key();
            if (key == 72) {
                current_select--;
                if (current_select < 0) current_select = max_options - 1;
            }
            else if (key == 80) {
                current_select++;
                if (current_select >= max_options) current_select = 0;
            }
        }
        else if (key == 13) {
            int choice = option_codes[current_select];
            switch (choice) {
            case 0: run = 0; break;
            case 1: patient_function(); break;
            case 2: doctor_function(); break;
            case 3: pharmacist_function(); break;
            case 4: admin_function(); break;
            case 5: advance_time_flow(); break;
            case 6: reset_system_data(); break;
            case 7: manual_save_data(); break;
            }
        }
        else if (key >= '0' && key <= '7') {
            int direct_choice = key - '0';
            for (int i = 0; i < max_options; i++) {
                if (option_codes[i] == direct_choice) {
                    current_select = i;
                    switch (direct_choice) {
                    case 0: run = 0; break;
                    case 1: patient_function(); break;
                    case 2: doctor_function(); break;
                    case 3: pharmacist_function(); break;
                    case 4: admin_function(); break;
                    case 5: advance_time_flow(); break;
                    case 6: reset_system_data(); break;
                    case 7: manual_save_data(); break;
                    }
                    break;
                }
            }
        }
    }

    save_all_data();

    clear_screen();
    printf("========================================\n");
    printf("        感谢使用医院管理系统\n");
    printf("             期待再次使用\n");
    printf("========================================\n");
    return 0;
}