/*
 * Copyright (c) 2026, HIS Team.
 * 文件名称：inpatient.h
 * 文件摘要：住院管理模块头文件
 * 修改记录：【升级6.0 修正】
 * 1. 将函数声明放入 extern "C" 块，解决链接规范冲突。
 */

#pragma once
#ifndef INPATIENT_H
#define INPATIENT_H

#include "hospital_system.h"

#ifdef __cplusplus
extern "C" {
#endif

	/* 初始化病房数据 */
	void init_wards_data(void);

	/* 患者自助办理住院 */
	void patient_admission_flow(Patient* p);

	/* 患者自助办理出院 */
	void patient_discharge_flow(Patient* p);

	/* 医生直接将急诊患者转入ICU */
	int doctor_admit_to_icu(Patient* p);

#ifdef __cplusplus
}
#endif

#endif // INPATIENT_H
