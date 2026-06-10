/*
 * Copyright (c) 2026, HIS Team.
 * 文件名称：inpatient.cpp
 * 修复说明：【升级6.0 修正链接规范】
 * 1. 住院时记录入院日期到患者结构体的 admit_date 字段。
 * 2. 出院时记录出院日期到 discharge_date 字段。
 * 3. 所有时间戳调用改为使用模拟时间函数。
 * 4. 所有函数定义放入 extern "C" 块，确保与头文件声明一致。
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inpatient.h"
#include "utils.h"

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RED     "\033[31m"

 /* 内部辅助函数：添加病历记录（静态函数无需 extern "C"） */
static void local_add_record_to_patient(Patient* p, const char* type, const char* doc, const char* content) {
    MedicalRecord* r = (MedicalRecord*)malloc(sizeof(MedicalRecord));
    if (r == NULL) return;
    memset(r, 0, sizeof(MedicalRecord));
    strcpy(r->type, type);
    strcpy(r->doctor_name, doc);
    strcpy(r->content, content);
    get_current_datetime_str(r->date);
    r->next = p->record_head;
    p->record_head = r;
}

/* 将需要跨模块调用的函数定义放入 extern "C" 块 */
#ifdef __cplusplus
extern "C" {
#endif

    void init_wards_data() {
        if (g_ward_head != NULL) return;
        int room_idx = 1000;

        /* 1. 急诊 ICU: 1间房，10个床位 */
        Ward* icu = (Ward*)malloc(sizeof(Ward));
        if (icu != NULL) {
            icu->room_no = ++room_idx; icu->type = 3; icu->dept_id = 1;
            icu->total_beds = 10; icu->occupied_beds = 0; icu->price_per_day = 2000.0f;
            icu->next = g_ward_head; g_ward_head = icu;
        }

        /* 2. 普通病房: 20间，每间2床位 */
        for (int i = 0; i < 20; i++) {
            Ward* w = (Ward*)malloc(sizeof(Ward));
            if (w != NULL) {
                w->room_no = ++room_idx; w->type = 1;
                if (i < 16) w->dept_id = (i / 4) + 2;
                else w->dept_id = 0;
                w->total_beds = 2; w->occupied_beds = 0; w->price_per_day = 100.0f;
                w->next = g_ward_head; g_ward_head = w;
            }
        }

        /* 3. VIP病房: 10间，每间1床位 */
        for (int i = 0; i < 10; i++) {
            Ward* w = (Ward*)malloc(sizeof(Ward));
            if (w != NULL) {
                w->room_no = ++room_idx; w->type = 2;
                if (i < 8) w->dept_id = (i / 2) + 2;
                else w->dept_id = 0;
                w->total_beds = 1; w->occupied_beds = 0; w->price_per_day = 800.0f;
                w->next = g_ward_head; g_ward_head = w;
            }
        }
    }

    static const char* local_get_dept_name(int dept_id) {
        switch (dept_id) {
        case 0: return "公共";
        case 1: return "急诊科";
        case 2: return "内科";
        case 3: return "外科";
        case 4: return "儿科";
        case 5: return "妇科";
        default: return "未知";
        }
    }

    static int assign_bed(Patient* p, int target_type, int target_dept) {
        Ward* w = g_ward_head;

        while (w) {
            if (w->type == target_type && w->dept_id == target_dept) {
                if (w->occupied_beds < w->total_beds) {
                    w->occupied_beds++;
                    p->ward_type = target_type;
                    p->room_no = w->room_no;
                    p->fee_ward += w->price_per_day;
                    p->is_paid = 0;

                    printf(COLOR_GREEN "\n成功入住%s%s病房 | 房间号: %d | 床号: %02d\n" COLOR_RESET,
                        local_get_dept_name(target_dept),
                        (target_type == 1 ? "普通" : (target_type == 2 ? "VIP" : "ICU")),
                        w->room_no, w->occupied_beds);

                    local_add_record_to_patient(p, "办理住院", "住院部", "分配床位成功");
                    return 1;
                }
            }
            w = w->next;
        }

        w = g_ward_head;
        while (w) {
            if (w->type == target_type && w->dept_id == 0) {
                if (w->occupied_beds < w->total_beds) {
                    w->occupied_beds++;
                    p->ward_type = target_type;
                    p->room_no = w->room_no;
                    p->fee_ward += w->price_per_day;
                    p->is_paid = 0;

                    printf(COLOR_CYAN "\n[注意] 本科室专属已满，已为您分配公共%s病房 | 房间号: %d | 床号: %02d\n" COLOR_RESET,
                        (target_type == 1 ? "普通" : "VIP"),
                        w->room_no, w->occupied_beds);

                    local_add_record_to_patient(p, "办理住院", "住院部", "分配公共床位成功");
                    return 1;
                }
            }
            w = w->next;
        }
        return 0;
    }

    int doctor_admit_to_icu(Patient* p) {
        if (p->dept_id != 1) return 0;
        int success = assign_bed(p, 3, 1);
        if (!success) {
            printf(COLOR_RED "\n[危急] 急诊ICU已全面满员！请立即安排患者转院！\n" COLOR_RESET);
            p->status = 4;
        }
        return success;
    }

    void patient_admission_flow(Patient* p) {
        clear_screen();
        if (p->need_inpatient != 1 || p->room_no != 0 || p->is_paid == 0) {
            printf(COLOR_YELLOW "您当前不符合办理住院的条件。\n" COLOR_RESET);
            printf("(提示: 请确认是否已由医生开具住院单、是否已结清门诊欠费、或是否已办理过入住)\n");
            press_enter_to_continue(); return;
        }

        printf(COLOR_CYAN "--- 自助办理住院 ---\n" COLOR_RESET);
        printf(" 1. 普通病房 (100元/天)\n");
        printf(" 2. VIP病房 (800元/天)\n");
        printf("请选择期望的病房类型: ");
        int choice;
        (void)scanf("%d", &choice);

        if (choice == 1 || choice == 2) {
            // 二次确认患者信息
            printf("\n=====================================================\n");
            printf("                 住院信息确认\n");
            printf("=====================================================\n");
            printf(" 患者ID：%d  姓名：%s\n", p->id, p->name);
            printf(" 病房类型：%s\n", choice == 1 ? "普通病房 (100元/天)" : "VIP病房 (800元/天)");
            printf("=====================================================\n");

            if (!ask_yes_no("是否确认办理住院？(Y/N): ")) {
                printf("\n[提示] 住院办理已取消。\n");
                press_enter_to_continue();
                return;
            }
            if (assign_bed(p, choice, p->dept_id)) {
                p->status = 3;
                get_current_date_str(p->admit_date);
                printf(COLOR_YELLOW ">> 已产生住院预缴费 %.2f 元，请再次前往收费处缴费。\n" COLOR_RESET, choice == 1 ? 100.0f : 800.0f);
            }
            else {
                printf(COLOR_RED "\n抱歉，全院此类病房（含公共池）均已满员，办理失败。请在候诊区等待空床位。\n" COLOR_RESET);
            }
        }
        else {
            printf(COLOR_RED "无效选择。\n" COLOR_RESET);
        }
        press_enter_to_continue();
    }

    void patient_discharge_flow(Patient* p) {
        clear_screen();
        if (p->room_no == 0) {
            printf(COLOR_YELLOW "您尚未办理入住分配床位。\n" COLOR_RESET);
            press_enter_to_continue(); return;
        }

        // 【新增】计算实际住院天数
        char today[12];
        get_current_date_str(today);
        int y1, m1, d1, y2, m2, d2;
        sscanf(p->admit_date, "%d-%d-%d", &y1, &m1, &d1);
        sscanf(today, "%d-%d-%d", &y2, &m2, &d2);
        int total1 = y1 * 365 + m1 * 30 + d1;
        int total2 = y2 * 365 + m2 * 30 + d2;
        int stay_days = total2 - total1;
        if (stay_days <= 0) stay_days = 1;

        // 【新增】补收多日住院费
        Ward* w = g_ward_head;
        while (w) {
            if (w->room_no == p->room_no) {
                float additional_fee = w->price_per_day * (stay_days - 1);
                if (additional_fee > 0) {
                    p->fee_ward += additional_fee;
                    printf(COLOR_YELLOW "住院 %d 天，补收床位费：%.2f 元（已含首日费用）\n" COLOR_RESET,
                        stay_days, additional_fee);
                    p->is_paid = 0;  // 标记为未缴费
                }
                break;
            }
            w = w->next;
        }

        if (p->is_paid == 0) {
            printf(COLOR_RED "您还有未结清的账单(如床位费等)，请先前往收费处缴费再办理出院。\n" COLOR_RESET);
            press_enter_to_continue(); return;
        }

        // 原有出院逻辑保持不变...
        w = g_ward_head;
        while (w) {
            if (w->room_no == p->room_no) {
                if (w->occupied_beds > 0) w->occupied_beds--;
                break;
            }
            w = w->next;
        }
        p->status = 4;
        get_current_date_str(p->discharge_date);
        p->room_no = 0;
        printf(COLOR_GREEN "\n出院办理成功，祝您早日康复！\n" COLOR_RESET);
        local_add_record_to_patient(p, "办理出院", "住院部", "结账出院完毕");
        press_enter_to_continue();
    }

#ifdef __cplusplus
}
#endif