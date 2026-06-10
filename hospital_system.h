/*
 * Copyright (c) 2026, HIS Team.
 * 文件名称：hospital_system.h
 * 文件摘要：全院公共数据结构定义，整合了挂号、看诊、药房、住院与预约数据结构。
 * 修改记录：【升级6.0】
 * 1. 新增预约结构体 Appointment，支持分时段预约。
 * 2. Doctor 结构体增加 is_active 字段，支持在职/离职状态管理。
 * 3. Patient 结构体增加 admit_date 和 discharge_date 字段，记录住院周期。
 * 4. 新增全局预约链表头声明。
 * 5. 修复链接规范冲突：所有函数声明放入 extern "C" 块。
 */

#pragma once
#ifndef HOSPITAL_SYSTEM_H
#define HOSPITAL_SYSTEM_H
#include <time.h>

 /*
  * 医疗记录结构：MedicalRecord
  * 作用：单向链表，挂载在每个患者结构体下，记录患者生命周期内的关键事件。
  */
typedef struct MedicalRecord {
    char type[20];          /* 记录的动作类型：如"挂号"、"看诊"、"发药"等 */
    char date[20];          /* 记录发生的时间，格式：YYYY-MM-DD HH:MM */
    char doctor_name[30];   /* 触发该记录的责任人姓名（医生或药房管理员） */
    char content[200];      /* 详细内容：诊疗建议、检查结果、或开具的药品列表 */
    struct MedicalRecord* next; /* 指向下一条记录的指针 */
} MedicalRecord;

/*
 * 医生结构体：Doctor
 * 作用：单向链表，存储全院医生的基础信息和业务属性。
 */
typedef struct Doctor {
    int id;                 /* 医生工号，用于登录校验，如 1001 */
    char name[30];          /* 医生姓名 */
    char password[20];      /* 登录密码 */
    //int title_level;        /* 职称级别：1:普通医师, 2:副主任医师, 3:主任医师 */
    int dept_id;            /* 所属科室代码：1:急诊, 2:内科, 3:外科, 4:儿科, 5:妇科 */
    int title_level;        /* 医生职级：1:主任, 2:副主任, 3:普通 */
    float consultation_fee; /* 该医生专属的挂号/诊疗费级别 */
    int is_active;          /* 【6.0新增】在职状态：1:在职, 0:已离职 */
    struct Doctor* next;    /* 指向下一位医生的指针 */
} Doctor;

/*
 * 患者结构体：Patient
 * 作用：系统核心数据总线（单向链表）。每个节点代表一次就诊周期。
 */
typedef struct Patient {
    int id;                 /* 系统顺序生成的唯一就诊ID，从 2026001 开始 */
    char name[30];          /* 患者姓名 */
    char gender[10];        /* 患者性别（男/女/其他） */
    char birth_date[12];    /* 出生日期，格式：YYYY-MM-DD */
    char phone[20];         /* 11位手机号 */

    int dept_id;            /* 本次就诊挂号的科室ID */
    int doctor_id;          /* 系统自动分配的接诊医生工号 */

    /*
     * 核心状态机状态流转：
     * 0: 挂号完毕，等待医生接诊
     * 1: 医生看诊完毕，产生费用，等待前往收费处缴费
     * 2: 收费处缴费完毕，等待前往药房取药 或 等待办理住院
     * 3: 已办理住院，占用床位中
     * 4: 就诊全部结束 / 出院
     */
    int status;

    /*
     * 处方字符串，容量扩至300字节。
     * 医生连续开具多种药品时，药品名会以逗号或分号拼接存储于此。
     */
    char prescription[300];

    /*
     * 住院需求标记（由医生端判定填写）：
     * 0: 不需要住院
     * 1: 需要常规住院（患者端后续可自助选普通/VIP）
     * 2: 紧急情况，直接转入ICU（仅限急诊医生操作）
     */
    int need_inpatient;

    /* 以下字段为住院实体分配结果 */
    int ward_type;          /* 实际分配的病房类型：0:无, 1:普通, 2:VIP, 3:ICU */
    int room_no;            /* 实际分配的房间号/床位号 */

    /* 费用统计矩阵（实时累加） */
    float fee_medicine;     /* 药费累计总额 */
    float fee_check;        /* 附加检查费累计 */
    float fee_treat;        /* 诊疗费累计（包含挂号费） */
    float fee_ward;         /* 住院床位费累计 */

    /*
     * 缴费防逃费锁：
     * 0: 未结清当前费用（药房拒绝发药，住院拒绝出院）
     * 1: 已结清当前全部费用
     */
    int is_paid;

    char instructions[200]; /* 医生填写的医嘱说明文字 */

    /* 【6.0新增】住院时间记录 */
    char admit_date[12];    /* 入院日期，格式：YYYY-MM-DD */
    char discharge_date[12];/* 出院日期，格式：YYYY-MM-DD */
    /* 【7.0新增】医保类型：0=无医保, 1=居民医保, 2=职工医保 */
    int insurance_type;

    /* 【7.0新增】签到状态：0=未签到, 1=已签到 */
    int checked_in;

    struct MedicalRecord* record_head; /* 挂载该患者的历史病历/操作记录链表头 */
    struct Patient* next;   /* 指向下一位患者的指针 */
} Patient;

/*
 * 药品字典结构体：Medicine
 * 作用：单向链表，作为全院系统的标准药品库，支持多种名称比对及真实库存扣减。
 */
typedef struct Medicine {
    int id;                 /* 药品唯一系统ID，如 5001 */
    char common_name[50];   /* 通用名 (学名，如：乙酰水杨酸) */
    char trade_name[50];    /* 商品名 (如：拜阿司匹林) */
    char alias[50];         /* 别名 (俗名，如：阿司匹林) */
    /*
     * allowed_depts[0] == 1 表示全科通用公共药品。
     * 否则，allowed_depts[1] 到 [5] 对应 1~5 科室是否有权开具 (1:是, 0:否)
     */
    int allowed_depts[6];
    /* 【7.0新增】药品类别：0=甲类, 1=乙类, 2=丙类 */
    int category;
    int stock;              /* 当前实体库存数量，每次发药真实扣减 */
    float price;            /* 药品单价 */
    float purchase_price;   /* 进货单价 */
    int total_sold;         /* 累计发药售出总数 */
    struct Medicine* next;  /* 指向下一类药品的指针 */
} Medicine;

/*
 * 病房与床位结构体：Ward
 * 作用：单向链表，管理全院病房资源池。
 */
typedef struct Ward {
    int room_no;            /* 物理房间号，如 1001 */
    int type;               /* 病房物理类型：1:普通, 2:VIP, 3:ICU */
    int dept_id;            /* 归属科室：0代表全科公共资源，1代表急诊专属等 */
    int total_beds;         /* 该房间的额定总床位数 */
    int occupied_beds;      /* 当前已被患者占用的床位数 */
    float price_per_day;    /* 该类型病房每日基础床位费标准 */
    struct Ward* next;      /* 指向下一间病房的指针 */
} Ward;

/*
 * 【6.0新增】预约挂号结构体：Appointment
 * 作用：单向链表，管理患者对未来日期的预约记录。
 */
typedef struct Appointment {
    int patient_id;         /* 预约的患者ID */
    int doctor_id;          /* 预约的医生ID */
    char date[12];          /* 预约日期，格式：YYYY-MM-DD */
    int period;             /* 时段：0=上午, 1=下午 */
    struct Appointment* next; /* 指向下一条预约记录 */
} Appointment;

/*
* 签到队列结构体：SignInQueue
* 作用：单向链表，按签到时间顺序排列，实现叫号接诊功能
*/
typedef struct SignInQueue {
    int patient_id;              /* 患者ID */
    char patient_name[30];       /* 患者姓名 */
    int dept_id;                 /* 科室ID */
    int doctor_id;               /* 分配的医生ID */
    time_t sign_in_time;         /* 签到时间戳 */
    int is_valid;                /* 签到是否有效：1:有效, 0:过号作废 */
    struct SignInQueue* next;    /* 指向下一个签到记录 */
} SignInQueue;

/* ==================== 全局变量与接口声明 ==================== */

/* 四大核心数据链表的全局表头指针声明（实体定义在 main.cpp 中） */
extern Doctor* g_doctor_head;
extern Patient* g_patient_head;
extern Medicine* g_medicine_head;
extern Ward* g_ward_head;
extern Appointment* g_appointment_head;  /* 【6.0新增】预约链表头 */
extern SignInQueue* g_sign_in_head;

/* 全局模拟时间（定义在 main.cpp 中） */
extern char g_current_date[12];

/* 将所有函数声明放入 extern "C" 块，解决链接规范冲突 */
#ifdef __cplusplus
extern "C" {
#endif

    /* 全局UI与子模块入口函数声明 */
    void display_main_menu(void);
    void clear_screen(void);
    void press_enter_to_continue(void);
    void patient_function(void);
    void doctor_function(void);
    void pharmacist_function(void);
    void admin_function(void);

    /* 全院基础静态数据（医生、药品、病房规则）统一初始化入口 */
    void init_hospital_data(void);

    /* 一键生成演示数据功能接口 */
    void generate_demo_data(void);

    /* 文件持久化接口 */
    int save_all_data(void);
    int load_all_data(void);

    /* 【7.0新增】医保报销计算函数 */
    float calculate_insurance_reimbursement(struct Patient* p, int use_insurance,
        float* out_patient_pay, float* out_insurance_pay);

    /* 【7.0新增】药品类别获取函数 */
    const char* get_medicine_category_name(int category);

    /* 【7.0新增】医保类型获取函数 */
    const char* get_insurance_type_name(int insurance_type);

    /* 科室名称获取函数 */
    const char* get_dept_name(int dept_id);

#ifdef __cplusplus
}
#endif

#endif