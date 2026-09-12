import { z } from 'zod';

export const robotStatusSchema = z.object({
	type: z.literal('robot_status'),
	protocol: z.enum(['RB1', 'RB2', 'RB3', 'RB4']).nullable(),
	port: z.string(),
	baudrate: z.number().int(),
	state: z.enum(['disabled', 'connecting', 'streaming', 'disconnected']),
	last_frame_age_ms: z.number().int().nonnegative(),
	parse_errors: z.number().int().nonnegative(),
	sequence: z.number().int().nonnegative(),
	motor_code: z.number().int(),
	motor_status: z.string(),
	motor_can_alive: z.boolean(),
	arm_code: z.number().int(),
	arm_status: z.string(),
	arm_can_alive: z.boolean(),
	arm_axis_1_pwm: z.number().int().min(0).max(4095).nullable(),
	arm_axis_2_pwm: z.number().int().min(0).max(4095).nullable(),
	arm_axis_3_pwm: z.number().int().min(0).max(4095).nullable(),
	arm_pump_on: z.boolean().nullable(),
	battery_millivolts: z.number().int().nonnegative(),
	battery_volts: z.number().nonnegative(),
	battery_adc: z.number().int().nonnegative(),
	last_command: z.string().nullable(),
	last_command_at: z.string().nullable()
});

export const robotCommandSchema = z.object({
	channel: z.enum(['motor', 'arm', 'all']),
	code: z.number().int().min(0).max(14)
});

export type RobotStatus = z.infer<typeof robotStatusSchema>;
export type RobotCommand = z.infer<typeof robotCommandSchema>;

export const INITIAL_ROBOT_STATUS: RobotStatus = {
	type: 'robot_status',
	protocol: null,
	port: '/dev/ttyACM0',
	baudrate: 115200,
	state: 'connecting',
	last_frame_age_ms: 0,
	parse_errors: 0,
	sequence: 0,
	motor_code: -1,
	motor_status: 'NO_DATA',
	motor_can_alive: false,
	arm_code: -1,
	arm_status: 'NO_DATA',
	arm_can_alive: false,
	arm_axis_1_pwm: null,
	arm_axis_2_pwm: null,
	arm_axis_3_pwm: null,
	arm_pump_on: null,
	battery_millivolts: 0,
	battery_volts: 0,
	battery_adc: 0,
	last_command: null,
	last_command_at: null
};
