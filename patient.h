/*
 * Copyright (c) 2026, HIS Team.
 * 文件名称：patient.h
 * 文件摘要：患者自助服务模块头文件
 * 修改记录：【升级6.0 修正】
 * 1. 将所有函数声明放入 extern "C" 块，解决链接规范冲突。
 */

#pragma once
#ifndef PATIENT_H
#define PATIENT_H

#include "hospital_system.h"

#ifdef __cplusplus
extern "C" {
#endif

	/* 患者模块主入口 */
	void patient_function(void);

	/* 内部业务函数 */
	void register_patient_flow(void);          /* 门诊挂号 */
	void view_my_registration_slip(void);      /* 打印挂号单 */
	void view_medical_advice(void);            /* 查看医嘱 */
	void appointment_flow(void);               /* 预约挂号 */

	/* 辅助函数 */
	const char* get_status_desc(int status);   /* 获取状态描述 */

#ifdef __cplusplus
}
#endif

#endif // PATIENT_H