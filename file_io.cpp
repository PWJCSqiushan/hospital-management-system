/*
 * Copyright (c) 2026, HIS Team.
 * 文件名称：file_io.cpp
 * 修改摘要：【升级6.0 新增】
 * 1. 实现全部数据的文件保存与加载功能。
 * 2. 数据文件格式：自定义二进制顺序存储，确保跨平台读写一致性。
 * 3. 支持链表嵌套结构的完整恢复。
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file_io.h"
#include "hospital_system.h"

#define DATA_FILE_NAME "his_data.dat"
#define DATA_FILE_MAGIC_V6 "HIS6.0"
#define DATA_FILE_MAGIC_V7 "HIS7.0"

static int g_data_file_version = 7;

 // 文件头结构体，用于校验平台兼容性
typedef struct {
    char magic[6];
    int  int_size;
    int  float_size;
    int  pointer_size;
    int  endian_check;
} FileHeader;

 /* ==================== 内部辅助函数声明 ==================== */

static int save_doctors(FILE* fp);
static int save_patients(FILE* fp);
static int save_medicines(FILE* fp);
static int save_wards(FILE* fp);
static int save_appointments(FILE* fp);
static int save_medical_records(FILE* fp, MedicalRecord* head);

static int load_doctors(FILE* fp);
static int load_patients(FILE* fp);
static int load_medicines(FILE* fp);
static int load_wards(FILE* fp);
static int load_appointments(FILE* fp);
static MedicalRecord* load_medical_records(FILE* fp);

static void free_all_data(void);
static int get_default_medicine_category(int medicine_id);

/* ==================== 公有接口实现 ==================== */

int save_all_data(void) {
    FILE* fp = fopen(DATA_FILE_NAME, "wb");
    if (fp == NULL) { return 0; }

    FileHeader header;
    memcpy(header.magic, DATA_FILE_MAGIC_V7, 6);
    header.int_size = sizeof(int);
    header.float_size = sizeof(float);
    header.pointer_size = sizeof(void*);
    header.endian_check = 0x12345678;
    fwrite(&header, sizeof(FileHeader), 1, fp);

    fwrite(g_current_date, 1, 11, fp);

    int success = 1;
    if (!save_doctors(fp)) success = 0;
    if (!save_patients(fp)) success = 0;
    if (!save_medicines(fp)) success = 0;
    if (!save_wards(fp)) success = 0;
    if (!save_appointments(fp)) success = 0;

    fclose(fp);
    return success;
}

int load_all_data(void) {
    FILE* fp = fopen(DATA_FILE_NAME, "rb");
    if (fp == NULL) {
        return 0;
    }

    FileHeader header;
    if (fread(&header, sizeof(FileHeader), 1, fp) != 1) {
        fclose(fp);
        return 0;
    }

    if (memcmp(header.magic, DATA_FILE_MAGIC_V7, 6) == 0) {
        g_data_file_version = 7;
    }
    else if (memcmp(header.magic, DATA_FILE_MAGIC_V6, 6) == 0) {
        g_data_file_version = 6;
    }
    else {
        fclose(fp);
        return 0;
    }
    if (header.int_size != sizeof(int) || header.float_size != sizeof(float)) {
        printf("[错误] 存档文件与当前平台不兼容，无法加载！\n");
        fclose(fp);
        return 0;
    }

    free_all_data();

    fread(g_current_date, 1, 11, fp);
    g_current_date[10] = '\0';

    int success = 1;
    if (!load_doctors(fp)) success = 0;
    if (!load_patients(fp)) success = 0;
    if (!load_medicines(fp)) success = 0;
    if (!load_wards(fp)) success = 0;
    if (!load_appointments(fp)) success = 0;

    fclose(fp);
    g_data_file_version = 7;
    return success;
}

/* ==================== 链表释放 ==================== */

static void free_all_data(void) {
    Doctor* d = g_doctor_head;
    while (d) {
        Doctor* next = d->next;
        free(d);
        d = next;
    }
    g_doctor_head = NULL;

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

    Medicine* m = g_medicine_head;
    while (m) {
        Medicine* next = m->next;
        free(m);
        m = next;
    }
    g_medicine_head = NULL;

    Ward* w = g_ward_head;
    while (w) {
        Ward* next = w->next;
        free(w);
        w = next;
    }
    g_ward_head = NULL;

    Appointment* a = g_appointment_head;
    while (a) {
        Appointment* next = a->next;
        free(a);
        a = next;
    }
    g_appointment_head = NULL;
}

static int get_default_medicine_category(int medicine_id) {
    switch (medicine_id) {
    case 5001:
    case 5002:
    case 5003:
    case 5008:
    case 5009:
    case 5015:
    case 5020:
        return 0;
    case 5004:
    case 5005:
    case 5006:
    case 5007:
    case 5010:
    case 5011:
    case 5013:
    case 5018:
    case 5019:
        return 1;
    case 5012:
    case 5014:
    case 5016:
    case 5017:
        return 2;
    default:
        return 2;
    }
}

/* ==================== 保存函数实现 ==================== */

static int save_doctors(FILE* fp) {
    int count = 0;
    Doctor* d = g_doctor_head;
    while (d) { count++; d = d->next; }
    fwrite(&count, sizeof(int), 1, fp);

    d = g_doctor_head;
    while (d) {
        fwrite(&d->id, sizeof(int), 1, fp);
        fwrite(d->name, 1, 30, fp);
        fwrite(d->password, 1, 20, fp);
        fwrite(&d->dept_id, sizeof(int), 1, fp);
        fwrite(&d->title_level, sizeof(int), 1, fp);
        fwrite(&d->consultation_fee, sizeof(float), 1, fp);
        fwrite(&d->is_active, sizeof(int), 1, fp);
        d = d->next;
    }
    return 1;
}

static int save_medical_records(FILE* fp, MedicalRecord* head) {
    int count = 0;
    MedicalRecord* mr = head;
    while (mr) { count++; mr = mr->next; }
    fwrite(&count, sizeof(int), 1, fp);

    mr = head;
    while (mr) {
        fwrite(mr->type, 1, 20, fp);
        fwrite(mr->date, 1, 20, fp);
        fwrite(mr->doctor_name, 1, 30, fp);
        fwrite(mr->content, 1, 200, fp);
        mr = mr->next;
    }
    return 1;
}

static int save_patients(FILE* fp) {
    int count = 0;
    Patient* p = g_patient_head;
    while (p) { count++; p = p->next; }
    fwrite(&count, sizeof(int), 1, fp);

    p = g_patient_head;
    while (p) {
        fwrite(&p->id, sizeof(int), 1, fp);
        fwrite(p->name, 1, 30, fp);
        fwrite(p->gender, 1, 10, fp);
        fwrite(p->birth_date, 1, 12, fp);
        fwrite(p->phone, 1, 20, fp);
        fwrite(&p->dept_id, sizeof(int), 1, fp);
        fwrite(&p->doctor_id, sizeof(int), 1, fp);
        fwrite(&p->status, sizeof(int), 1, fp);
        fwrite(p->prescription, 1, 300, fp);
        fwrite(&p->need_inpatient, sizeof(int), 1, fp);
        fwrite(&p->ward_type, sizeof(int), 1, fp);
        fwrite(&p->room_no, sizeof(int), 1, fp);
        fwrite(&p->fee_medicine, sizeof(float), 1, fp);
        fwrite(&p->fee_check, sizeof(float), 1, fp);
        fwrite(&p->fee_treat, sizeof(float), 1, fp);
        fwrite(&p->fee_ward, sizeof(float), 1, fp);
        fwrite(&p->is_paid, sizeof(int), 1, fp);
        fwrite(p->instructions, 1, 200, fp);
        fwrite(p->admit_date, 1, 12, fp);
        fwrite(p->discharge_date, 1, 12, fp);

        save_medical_records(fp, p->record_head);

        p = p->next;
    }
    return 1;
}

static int save_medicines(FILE* fp) {
    int count = 0;
    Medicine* m = g_medicine_head;
    while (m) { count++; m = m->next; }
    fwrite(&count, sizeof(int), 1, fp);

    m = g_medicine_head;
    while (m) {
        fwrite(&m->id, sizeof(int), 1, fp);
        fwrite(m->common_name, 1, 50, fp);
        fwrite(m->trade_name, 1, 50, fp);
        fwrite(m->alias, 1, 50, fp);
        fwrite(m->allowed_depts, sizeof(int), 6, fp);
        fwrite(&m->stock, sizeof(int), 1, fp);
        fwrite(&m->price, sizeof(float), 1, fp);
        fwrite(&m->purchase_price, sizeof(float), 1, fp);
        fwrite(&m->category, sizeof(int), 1, fp);
        fwrite(&m->total_sold, sizeof(int), 1, fp);
        m = m->next;
    }
    return 1;
}

static int save_wards(FILE* fp) {
    int count = 0;
    Ward* w = g_ward_head;
    while (w) { count++; w = w->next; }
    fwrite(&count, sizeof(int), 1, fp);

    w = g_ward_head;
    while (w) {
        fwrite(&w->room_no, sizeof(int), 1, fp);
        fwrite(&w->type, sizeof(int), 1, fp);
        fwrite(&w->dept_id, sizeof(int), 1, fp);
        fwrite(&w->total_beds, sizeof(int), 1, fp);
        fwrite(&w->occupied_beds, sizeof(int), 1, fp);
        fwrite(&w->price_per_day, sizeof(float), 1, fp);
        w = w->next;
    }
    return 1;
}

static int save_appointments(FILE* fp) {
    int count = 0;
    Appointment* a = g_appointment_head;
    while (a) { count++; a = a->next; }
    fwrite(&count, sizeof(int), 1, fp);

    a = g_appointment_head;
    while (a) {
        fwrite(&a->patient_id, sizeof(int), 1, fp);
        fwrite(&a->doctor_id, sizeof(int), 1, fp);
        fwrite(a->date, 1, 12, fp);
        fwrite(&a->period, sizeof(int), 1, fp);
        a = a->next;
    }
    return 1;
}

/* ==================== 加载函数实现 ==================== */

static int load_doctors(FILE* fp) {
    int count;
    if (fread(&count, sizeof(int), 1, fp) != 1) return 0;

    Doctor* tail = NULL;
    for (int i = 0; i < count; i++) {
        Doctor* d = (Doctor*)malloc(sizeof(Doctor));
        if (!d) return 0;
        memset(d, 0, sizeof(Doctor));

        fread(&d->id, sizeof(int), 1, fp);
        fread(d->name, 1, 30, fp);
        fread(d->password, 1, 20, fp);
        fread(&d->dept_id, sizeof(int), 1, fp);
        fread(&d->title_level, sizeof(int), 1, fp);
        fread(&d->consultation_fee, sizeof(float), 1, fp);
        fread(&d->is_active, sizeof(int), 1, fp);

        d->next = NULL;
        if (tail == NULL) {
            g_doctor_head = d;
            tail = d;
        }
        else {
            tail->next = d;
            tail = d;
        }
    }
    return 1;
}

static MedicalRecord* load_medical_records(FILE* fp) {
    int count;
    if (fread(&count, sizeof(int), 1, fp) != 1) return NULL;

    MedicalRecord* head = NULL;
    MedicalRecord* tail = NULL;
    for (int i = 0; i < count; i++) {
        MedicalRecord* mr = (MedicalRecord*)malloc(sizeof(MedicalRecord));
        if (!mr) return NULL;
        memset(mr, 0, sizeof(MedicalRecord));

        fread(mr->type, 1, 20, fp);
        fread(mr->date, 1, 20, fp);
        fread(mr->doctor_name, 1, 30, fp);
        fread(mr->content, 1, 200, fp);
        mr->next = NULL;

        if (head == NULL) {
            head = mr;
            tail = mr;
        }
        else {
            tail->next = mr;
            tail = mr;
        }
    }
    return head;
}

static int load_patients(FILE* fp) {
    int count;
    if (fread(&count, sizeof(int), 1, fp) != 1) return 0;

    Patient* tail = NULL;
    for (int i = 0; i < count; i++) {
        Patient* p = (Patient*)malloc(sizeof(Patient));
        if (!p) return 0;
        memset(p, 0, sizeof(Patient));

        fread(&p->id, sizeof(int), 1, fp);
        fread(p->name, 1, 30, fp);
        fread(p->gender, 1, 10, fp);
        fread(p->birth_date, 1, 12, fp);
        fread(p->phone, 1, 20, fp);
        fread(&p->dept_id, sizeof(int), 1, fp);
        fread(&p->doctor_id, sizeof(int), 1, fp);
        fread(&p->status, sizeof(int), 1, fp);
        fread(p->prescription, 1, 300, fp);
        fread(&p->need_inpatient, sizeof(int), 1, fp);
        fread(&p->ward_type, sizeof(int), 1, fp);
        fread(&p->room_no, sizeof(int), 1, fp);
        fread(&p->fee_medicine, sizeof(float), 1, fp);
        fread(&p->fee_check, sizeof(float), 1, fp);
        fread(&p->fee_treat, sizeof(float), 1, fp);
        fread(&p->fee_ward, sizeof(float), 1, fp);
        fread(&p->is_paid, sizeof(int), 1, fp);
        fread(p->instructions, 1, 200, fp);
        fread(p->admit_date, 1, 12, fp);
        fread(p->discharge_date, 1, 12, fp);

        p->record_head = load_medical_records(fp);

        p->next = NULL;
        if (tail == NULL) {
            g_patient_head = p;
            tail = p;
        }
        else {
            tail->next = p;
            tail = p;
        }
    }
    return 1;
}

static int load_medicines(FILE* fp) {
    int count;
    if (fread(&count, sizeof(int), 1, fp) != 1) return 0;

    Medicine* tail = NULL;
    for (int i = 0; i < count; i++) {
        Medicine* m = (Medicine*)malloc(sizeof(Medicine));
        if (!m) return 0;
        memset(m, 0, sizeof(Medicine));

        fread(&m->id, sizeof(int), 1, fp);
        fread(m->common_name, 1, 50, fp);
        fread(m->trade_name, 1, 50, fp);
        fread(m->alias, 1, 50, fp);
        fread(m->allowed_depts, sizeof(int), 6, fp);
        fread(&m->stock, sizeof(int), 1, fp);
        fread(&m->price, sizeof(float), 1, fp);
        fread(&m->purchase_price, sizeof(float), 1, fp);
        if (g_data_file_version >= 7) {
            fread(&m->category, sizeof(int), 1, fp);
        }
        else {
            m->category = get_default_medicine_category(m->id);
        }
        fread(&m->total_sold, sizeof(int), 1, fp);

        m->next = NULL;
        if (tail == NULL) {
            g_medicine_head = m;
            tail = m;
        }
        else {
            tail->next = m;
            tail = m;
        }
    }
    return 1;
}

static int load_wards(FILE* fp) {
    int count;
    if (fread(&count, sizeof(int), 1, fp) != 1) return 0;

    Ward* tail = NULL;
    for (int i = 0; i < count; i++) {
        Ward* w = (Ward*)malloc(sizeof(Ward));
        if (!w) return 0;
        memset(w, 0, sizeof(Ward));

        fread(&w->room_no, sizeof(int), 1, fp);
        fread(&w->type, sizeof(int), 1, fp);
        fread(&w->dept_id, sizeof(int), 1, fp);
        fread(&w->total_beds, sizeof(int), 1, fp);
        fread(&w->occupied_beds, sizeof(int), 1, fp);
        fread(&w->price_per_day, sizeof(float), 1, fp);

        w->next = NULL;
        if (tail == NULL) {
            g_ward_head = w;
            tail = w;
        }
        else {
            tail->next = w;
            tail = w;
        }
    }
    return 1;
}

static int load_appointments(FILE* fp) {
    int count;
    if (fread(&count, sizeof(int), 1, fp) != 1) return 0;

    Appointment* tail = NULL;
    for (int i = 0; i < count; i++) {
        Appointment* a = (Appointment*)malloc(sizeof(Appointment));
        if (!a) return 0;
        memset(a, 0, sizeof(Appointment));

        fread(&a->patient_id, sizeof(int), 1, fp);
        fread(&a->doctor_id, sizeof(int), 1, fp);
        fread(a->date, 1, 12, fp);
        fread(&a->period, sizeof(int), 1, fp);

        a->next = NULL;
        if (tail == NULL) {
            g_appointment_head = a;
            tail = a;
        }
        else {
            tail->next = a;
            tail = a;
        }
    }
    return 1;
}
