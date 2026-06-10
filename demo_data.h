/*
 * Copyright (c) 2026, HIS Team.
 * 文件名称：demo_data.h
 * 文件摘要：演示数据模块头文件
 * 修改记录：【升级6.0】
 * 1. 保持原有接口不变。
 * 2. 演示数据生成函数改用模拟时间，头文件无需修改。
 */

#pragma once
#ifndef DEMO_DATA_H
#define DEMO_DATA_H

#include "hospital_system.h"

#ifdef __cplusplus
extern "C" {
#endif

	/* 启动自动化演示数据注入（生成130位仿真患者） */
	void generate_demo_data(void);

#ifdef __cplusplus
}
#endif

#endif // DEMO_DATA_H