/*
 * Copyright (c) 2026, HIS Team.
 * 文件名称：doctor.h
 * 文件摘要：医生模块头文件
 * 修改记录：【升级6.0 修正】
 * 1. 将所有函数声明放入 extern "C" 块，解决链接规范冲突。
 * 2. 补充缺失的全局函数声明。
 */

#pragma once
#ifndef DOCTOR_H
#define DOCTOR_H

#include "hospital_system.h"

#ifdef __cplusplus
extern "C" {
#endif

	/* ==================== 医生模块主入口 ==================== */
	void doctor_function(void);

	/* ==================== 工具函数（跨模块调用） ==================== */
	const char* get_dept_name(int dept_id);
	const char* get_title_name(int title_level);
	Doctor* get_doctor_by_id(int doc_id);
	int CalculateAge(const char* birthDate);
	int SecondaryConfirm(const char* actionDesc);
	int ChangeDoctorPassword(Doctor* currDoc);
	void ViewAllPatientsReadonly(void);
	void ShowCurrentDate(void);

#ifdef __cplusplus
}
#endif

#endif // DOCTOR_H