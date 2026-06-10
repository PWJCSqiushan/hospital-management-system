/*
 * Copyright (c) 2026, HIS Team.
 * 文件名称：demo_data.cpp
 * 修改摘要：【升级6.0 - 仿真数据适配】
 * 1. 移除真实系统时间依赖，所有时间戳改为使用模拟时间函数。
 * 2. 修复发票生成器：将真实检查费与床位费写入发票字符串。
 * 3. 状态分布随机化，并自动为已结束患者打上[已发]标签。
 * 4. 严格遵循科室关联规则进行药品和病房的随机抽取。
 * 5. 支持作为重置功能的一部分被调用。
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "demo_data.h"
#include "hospital_system.h"
#include "utils.h"

 /* -------------------------------------------------------------------------
  * 仿真资源池
  * ------------------------------------------------------------------------- */
static const char* SURNAMES[] = { "张","王","李","赵","陈","刘","杨","黄","吴","周","徐","孙" };
static const char* GIVEN_NAMES_M[] = { "伟","强","磊","军","洋","勇","杰","涛","明","超" };
static const char* GIVEN_NAMES_F[] = { "芳","娜","敏","静","艳","娟","秀英","霞","平","雪" };

static const char* FOREIGN_NAMES[] = {
    "John Smith", "Alice Johnson", "Michael Brown", "Emma Davis", "Chris Evans"
};

static const char* LONG_MINZU_NAMES[] = {
    "阿不都热西提阿布杜克力木",
    "买买提明阿不都卡德尔江",
    "帕尔哈提艾合买提托乎提",
    "乌勒木吉白乙拉图格日勒",
    "Abdurahman Muhammad"
};

static const char* REPEAT_NAMES[] = { "张伟", "李静", "王建国", "刘洋", "陈梅" };

static const char* SYMPTOMS[] = { "反复咳嗽", "急性腹痛", "术后恢复中", "血压波动", "乏力畏寒" };
static const char* DOCTOR_SAY[] = { "注意休息，避免劳累", "饮食清淡，忌油腻", "定期复查", "加强营养" };

/* 辅助函数：全诊疗流水记录（使用模拟时间） */
static void add_record_to_patient(Patient* p, const char* type, const char* doc, const char* content) {
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

void generate_demo_data() {
    srand(12345);

    Doctor* doc_pool[50]; int total_docs = 0;
    Doctor* d_temp = g_doctor_head;
    while (d_temp) {
        if (d_temp->is_active) {
            doc_pool[total_docs++] = d_temp;
        }
        d_temp = d_temp->next;
    }

    Medicine* med_pool[50]; int total_meds = 0;
    Medicine* m_temp = g_medicine_head;
    while (m_temp) {
        med_pool[total_meds++] = m_temp;
        m_temp = m_temp->next;
    }

    Patient* p_curr = g_patient_head;
    while (p_curr) {
        Patient* next = p_curr->next;
        MedicalRecord* mr = p_curr->record_head;
        while (mr) {
            MedicalRecord* mr_next = mr->next;
            free(mr);
            mr = mr_next;
        }
        free(p_curr);
        p_curr = next;
    }
    g_patient_head = NULL;

    Ward* w_ptr = g_ward_head;
    while (w_ptr) {
        w_ptr->occupied_beds = 0;
        w_ptr = w_ptr->next;
    }

    Medicine* med_ptr = g_medicine_head;
    while (med_ptr) {
        med_ptr->stock = 1000;
        med_ptr->total_sold = 0;
        med_ptr = med_ptr->next;
    }

    printf(">>> 正在生成130位仿真患者，并进行合规化及随机化分配...\n");

    int max_id = 2026000;

    for (int i = 0; i < 130; i++) {
        Patient* p = (Patient*)malloc(sizeof(Patient));
        if (!p) break;
        memset(p, 0, sizeof(Patient));

        p->id = max_id + 1 + i;

        int is_male = rand() % 2;
        strcpy(p->gender, is_male ? "男" : "女");
        int birth_year = 1940 + (rand() % 85);
        sprintf(p->birth_date, "%d-%02d-%02d", birth_year, 1 + rand() % 12, 1 + rand() % 28);
        int age = 2026 - birth_year;

        int name_rand = rand() % 100;
        if (name_rand < 15) {
            strcpy(p->name, REPEAT_NAMES[rand() % 5]);
            if (strcmp(p->name, "李静") == 0 || strcmp(p->name, "陈梅") == 0) { strcpy(p->gender, "女"); is_male = 0; }
            else { strcpy(p->gender, "男"); is_male = 1; }
        }
        else if (name_rand < 25) {
            strcpy(p->name, FOREIGN_NAMES[rand() % 5]);
        }
        else if (name_rand < 35) {
            strcpy(p->name, LONG_MINZU_NAMES[rand() % 5]);
        }
        else {
            char temp_name[30] = { 0 };
            strcpy(temp_name, SURNAMES[rand() % 12]);
            if (is_male) strcat(temp_name, GIVEN_NAMES_M[rand() % 10]);
            else strcat(temp_name, GIVEN_NAMES_F[rand() % 10]);
            strcpy(p->name, temp_name);
        }
        sprintf(p->phone, "138%08d", i);

        if (age < 18) {
            p->dept_id = (rand() % 2 == 0) ? 1 : 4;
        }
        else {
            if (is_male) { p->dept_id = 1 + (rand() % 3); }
            else { int valid_depts[] = { 1, 2, 3, 5 }; p->dept_id = valid_depts[rand() % 4]; }
        }

        Doctor* available_docs[10]; int avail_count = 0;
        for (int j = 0; j < total_docs; j++) {
            if (doc_pool[j]->dept_id == p->dept_id) {
                available_docs[avail_count++] = doc_pool[j];
            }
        }
        if (avail_count > 0) {
            Doctor* selected_doc = available_docs[rand() % avail_count];
            p->doctor_id = selected_doc->id;
            p->fee_treat = selected_doc->consultation_fee;
        }

        if (i < 30) {
            p->status = 3;
            p->need_inpatient = 1;
            int target_type = (i % 3) + 1;

            Ward* avail_wards[50]; int w_count = 0;
            Ward* w_curr = g_ward_head;
            while (w_curr != NULL) {
                if (w_curr->type == target_type && w_curr->occupied_beds < w_curr->total_beds) {
                    if (w_curr->dept_id == p->dept_id || w_curr->dept_id == 0) {
                        avail_wards[w_count++] = w_curr;
                    }
                }
                w_curr = w_curr->next;
            }

            if (w_count > 0) {
                Ward* selected_ward = avail_wards[rand() % w_count];
                p->room_no = selected_ward->room_no;
                p->ward_type = selected_ward->type;
                p->fee_ward = selected_ward->price_per_day * 2.0f;
                selected_ward->occupied_beds++;
                get_current_date_str(p->admit_date);
            }
            else {
                // 没有可用病房，状态回退
                p->status = 4;  // 改为"就诊结束"
                p->need_inpatient = 0;
            }
        }
        else {
            p->status = rand() % 5;
            if (p->status == 3) p->status = 4;
            p->checked_in = (p->status == 0 && rand() % 2) ? 1 : 0;  // 待诊患者随机签到
        }

        add_record_to_patient(p, "挂号", "系统中心", "预约成功");

        if (p->status >= 1) {
            p->fee_check = (float)(40 + rand() % 100);
            p->fee_treat += 50.0f;
            p->fee_medicine = 0.0f;
            char all_pres[300] = { 0 };

            Medicine* valid_med_pool[50]; int valid_med_count = 0;
            for (int j = 0; j < total_meds; j++) {
                if (med_pool[j]->allowed_depts[0] == 1 || med_pool[j]->allowed_depts[p->dept_id] == 1) {
                    valid_med_pool[valid_med_count++] = med_pool[j];
                }
            }

            if (valid_med_count > 0) {
                int med_kinds = 1 + rand() % 3;
                for (int m = 0; m < med_kinds; m++) {
                    Medicine* rand_med = valid_med_pool[rand() % valid_med_count];
                    int qty = 1 + (rand() % 10);
                    p->fee_medicine += rand_med->price * qty;

                    char temp_format[50];
                    sprintf(temp_format, "%s*%d", rand_med->common_name, qty);
                    if (strlen(all_pres) > 0) strcat(all_pres, ",");
                    strcat(all_pres, temp_format);

                    if (p->status >= 2 && rand_med->stock >= qty) {
                        rand_med->stock -= qty;
                        rand_med->total_sold += qty;
                    }
                }

                if (p->status >= 3 || p->status == 4) {
                    char temp_issued[350] = { 0 };
                    sprintf(temp_issued, "[已发]%s", all_pres);
                    strcpy(p->prescription, temp_issued);
                }
                else {
                    strcpy(p->prescription, all_pres);
                }

                sprintf(p->instructions, "主诉：%s。按时服药。%s", SYMPTOMS[rand() % 5], DOCTOR_SAY[rand() % 4]);

                if (p->status >= 2) {
                    p->is_paid = 1;
                    char inv[200];
                    float total = p->fee_medicine + p->fee_treat + p->fee_check + p->fee_ward;
                    sprintf(inv, "诊疗:%.2f 检查:%.2f 药费:%.2f 床位:%.2f | 本次合计:%.2f",
                        p->fee_treat, p->fee_check, p->fee_medicine, p->fee_ward, total);

                    add_record_to_patient(p, "缴费发票", "收费处", inv);

                    if (p->status >= 3 || p->status == 4) {
                        add_record_to_patient(p, "发药", "药房", "核对已领药");
                    }
                }
            }
        }

        if (strlen(p->instructions) == 0) strcpy(p->instructions, "请在候诊区等候。");

        p->next = g_patient_head;
        g_patient_head = p;
    }

    printf("\n>>> [仿真数据注入成功] <<<\n");
    printf("- 已加入国际友人及极限长度少数民族名。\n");
    printf("- 医生挂号已实现同科室随机均衡负载。\n");
    printf("- 病房分配已打乱，随机入住。\n");
    printf("- 发票数据及患者流转状态已完全激活修复。\n");
    press_enter_to_continue();
}