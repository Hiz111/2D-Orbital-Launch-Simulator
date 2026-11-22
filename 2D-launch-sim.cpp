#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

#define PI 3.14159265358979323846
#define g0 9.81
#define R_E 6378000.0
#define ATMO_LIMIT 85000.0

struct base { int stage_count; double del_t; };
struct stage { double m_dry, m_fuel, t_burn, thr, C_d, crs_sct; };
struct highlights {
	double m_alt, downrange, t_fl;
	bool in_orbit, escaped;
	double apoapsis, periapsis, eccentricity, orbital_period;
};
struct command {
	double cmd_thr_time;
	double cmd_thr_pct;
	double cmd_thr_pwr;
	double cmd_ship_angle;
	double consumed_burn;
	string decouple;
};

struct base sim;
vector<stage> stages;
vector<vector<command>> cmds;
struct highlights hl;

bool is_keyword(const char* buf, const char* kw) {
	size_t n = strlen(kw);
	if (strlen(buf) != n) return false;
	for (size_t i = 0; i < n; i++) {
		if (toupper((unsigned char)buf[i]) != toupper((unsigned char)kw[i])) return false;
	}
	return true;
}

bool parse_double(const char* buf, double* out) {
	char* endptr = NULL;
	*out = strtod(buf, &endptr);
	return (endptr != buf && *endptr == '\0' && isfinite(*out));
}

bool parse_int(const char* buf, int* out) {
	char* endptr = NULL;
	long val = strtol(buf, &endptr, 10);
	if (endptr == buf || *endptr != '\0') return false;
	*out = (int)val;
	return true;
}

void evaluate_orbit(double x, double y, double vx, double vy, struct highlights* fnl) {
	const double mu = g0 * R_E * R_E;
	double r = sqrt(x * x + y * y);
	double v2 = vx * vx + vy * vy;
	double eps = 0.5 * v2 - mu / r;
	double h = fabs(x * vy - y * vx);

	fnl->in_orbit = false;
	fnl->escaped = false;
	fnl->apoapsis = 0.0;
	fnl->periapsis = 0.0;
	fnl->eccentricity = 0.0;
	fnl->orbital_period = 0.0;

	if (eps >= 0.0) {
		double v_rad = (r > 0.0) ? ((x * vx + y * vy) / r) : 0.0;
		if ((r - R_E) > ATMO_LIMIT && v_rad > 0.0) {
			fnl->escaped = true;
		}
		return;
	}

	double a = -mu / (2.0 * eps);
	double disc = 1.0 + (2.0 * eps * h * h) / (mu * mu);
	double e = (disc > 0.0) ? sqrt(disc) : 0.0;

	double r_p = a * (1.0 - e);
	double r_a = a * (1.0 + e);

	fnl->periapsis = r_p - R_E;
	fnl->apoapsis = r_a - R_E;
	fnl->eccentricity = e;
	fnl->orbital_period = 2.0 * PI * sqrt((a * a * a) / mu);

	if (fnl->periapsis > ATMO_LIMIT) {
		fnl->in_orbit = true;
	}
}

int init_input();
int cmd_input(const vector<stage>& stg);
int calculations(struct base sim, const vector<stage>& stg, const vector<vector<command>>& cmd, FILE* result, struct highlights* fnl);

int main() {
	FILE* datasheet = NULL;
	if (fopen_s(&datasheet, "fl_data.txt", "w") != 0 || datasheet == NULL) {
		printf("Error opening file!\n");
		return 1;
	}

	fprintf(datasheet, "%10.9s\t%20.19s\t%20.19s\t%20.19s\t%20.19s\t%15.14s\t%15.14s\t%15.14s\n",
		"Time (s)", "Velocity (ms-1)", "Altitude (m)", "Mass (kg)", "Air Density (kgm-3)", "X Pos", "Y Pos", "Earth Arc (rad)");

	if (init_input() != 0) {
		printf("Input Error!\n");
		fclose(datasheet);
		return 1;
	}

	if (cmd_input(stages) != 0) {
		printf("Command Input Error!\n");
		fclose(datasheet);
		return 1;
	}

	int status = calculations(sim, stages, cmds, datasheet, &hl);

	if (status == 1) {
		printf("Calculation Error!\n");
	}
	else if (status == 2) {
		printf("Firing Stage Error!\n");
	}
	else if (status == 3) {
		printf("Launch Altitude Invalid!\n");
	}
	else if (status == 4) {
		printf("Climb Altitude Calculation Error!\n");
	}
	else if (status == 5) {
		printf("Glide Altitude Calculation Error!\n");
	}

	printf("\n================ FLIGHT SUMMARY ================\n");
	if (hl.in_orbit) {
		printf("Status:            ORBIT ACHIEVED\n");
		printf("Apoapsis (m):      %.2lf\n", hl.apoapsis);
		printf("Periapsis (m):     %.2lf\n", hl.periapsis);
		printf("Eccentricity:      %.5lf\n", hl.eccentricity);
		printf("Orbital Period (s):%.2lf\n", hl.orbital_period);
		printf("Burn/Sim Time (s): %.2lf\n", hl.t_fl);
	}
	else if (hl.escaped) {
		printf("Status:            ESCAPE TRAJECTORY ACHIEVED\n");
		printf("Max Altitude (m):  %.2lf\n", hl.m_alt);
		printf("Burn/Sim Time (s): %.2lf\n", hl.t_fl);
	}
	else {
		printf("Status:            SUBORBITAL / TOUCHDOWN\n");
		printf("Apogee (m):        %.2lf\n", hl.m_alt);
		printf("Down Range (m):    %.2lf\n", hl.downrange);
		printf("Flight Time (s):   %.2lf\n", hl.t_fl);
	}

	fclose(datasheet);
	return 0;
}

int init_input() {
	char buf[64];

	while (true) {
		printf("Number of stages: ");
		if (scanf_s("%63s", buf, (unsigned)sizeof(buf)) != 1) return 1;
		if (parse_int(buf, &sim.stage_count) && sim.stage_count >= 1) break;
		printf("Invalid: Enter an integer >= 1.\n");
	}

	while (true) {
		printf("Time step (s) [or UNDO]: ");
		if (scanf_s("%63s", buf, (unsigned)sizeof(buf)) != 1) return 1;
		if (is_keyword(buf, "UNDO")) return init_input();
		if (parse_double(buf, &sim.del_t) && sim.del_t > 0.0) break;
		printf("Invalid: Time step must be > 0.\n");
	}

	stages.assign(sim.stage_count, stage{});

	int i = 0;
	int field = 0; // 0: m_dry, 1: m_fuel, 2: t_burn, 3: thr, 4: C_d, 5: crs_sct
	while (i < sim.stage_count) {
		if (field == 0) {
			printf("\n--- Stage %d ---\n", i + 1);
		}

		if (field == 0) printf("Stage %d dry mass (kg) [or UNDO]: ", i + 1);
		else if (field == 1) printf("Stage %d fuel mass (kg) [or UNDO]: ", i + 1);
		else if (field == 2) printf("Stage %d burn time (s) [or UNDO]: ", i + 1);
		else if (field == 3) printf("Stage %d max thrust (N) [or UNDO]: ", i + 1);
		else if (field == 4) printf("Stage %d drag coefficient (subsonic ref) [or UNDO]: ", i + 1);
		else if (field == 5) printf("Stage %d cross section (m^2) [or UNDO]: ", i + 1);

		if (scanf_s("%63s", buf, (unsigned)sizeof(buf)) != 1) return 1;

		if (is_keyword(buf, "UNDO")) {
			if (field > 0) {
				field--;
			}
			else if (i > 0) {
				i--;
				field = 5;
			}
			else {
				return init_input();
			}
			continue;
		}

		double val = 0.0;
		if (!parse_double(buf, &val)) {
			printf("Invalid number.\n");
			continue;
		}

		if (field == 0) {
			if (val < 0.0) { printf("Invalid: Must be >= 0.\n"); continue; }
			stages[i].m_dry = val;
			field++;
		}
		else if (field == 1) {
			if (val < 0.0) { printf("Invalid: Must be >= 0.\n"); continue; }
			stages[i].m_fuel = val;
			field++;
		}
		else if (field == 2) {
			if (val <= 0.0) { printf("Invalid: Must be > 0.\n"); continue; }
			stages[i].t_burn = val;
			field++;
		}
		else if (field == 3) {
			if (val <= 0.0) { printf("Invalid: Must be > 0.\n"); continue; }
			stages[i].thr = val;
			field++;
		}
		else if (field == 4) {
			if (val < 0.0) { printf("Invalid: Must be >= 0.\n"); continue; }
			stages[i].C_d = val;
			field++;
		}
		else if (field == 5) {
			if (val < 0.0) { printf("Invalid: Must be >= 0.\n"); continue; }
			stages[i].crs_sct = val;
			field = 0;
			i++;
		}
	}
	return 0;
}

int cmd_input(const vector<stage>& stg) {
	cmds.assign(sim.stage_count, vector<command>());
	vector<double> rem_burn(sim.stage_count);
	for (int k = 0; k < sim.stage_count; k++) {
		rem_burn[k] = stg[k].t_burn;
	}

	char buf[64];
	int i = 0;
	int step = 0; // 0: duration, 1: throttle %, 2: ship angle, 3: decouple/end
	command cur{};
	bool stage_banner_shown = false;

	while (i < sim.stage_count) {
		if (!stage_banner_shown) {
			printf("\n=== Stage %d Commands (Max Thrust: %.2lf N, Total Burn Time: %.2lf s) ===\n",
				i + 1, stg[i].thr, stg[i].t_burn);
			stage_banner_shown = true;
		}

		int c_idx = (int)cmds[i].size();

		if (step == 0) {
			printf("\n-- Stage %d, Command %d (Remaining Burn Time: %.2lf s | Max Thrust: %.2lf N) --\n",
				i + 1, c_idx + 1, rem_burn[i], stg[i].thr);

			if (i == sim.stage_count - 1) {
				printf("Command duration cmd_thr_time (s) [or UNDO / DONE]: ");
			}
			else {
				printf("Command duration cmd_thr_time (s) [or UNDO]: ");
			}

			if (scanf_s("%63s", buf, (unsigned)sizeof(buf)) != 1) return 1;

			if (is_keyword(buf, "UNDO")) {
				if (!cmds[i].empty()) {
					cur = cmds[i].back();
					cmds[i].pop_back();
					rem_burn[i] += cur.consumed_burn;
					step = 3;
				}
				else if (i > 0) {
					i--;
					stage_banner_shown = false;
					cur = cmds[i].back();
					cmds[i].pop_back();
					cur.decouple = "N";
					rem_burn[i] += cur.consumed_burn;
					step = 3;
				}
				else {
					printf("Nothing to undo (already at Stage 1, Command 1).\n");
				}
				continue;
			}

			if (i == sim.stage_count - 1 && is_keyword(buf, "DONE")) {
				i++;
				stage_banner_shown = false;
				step = 0;
				continue;
			}

			double dur = 0.0;
			if (!parse_double(buf, &dur)) {
				printf("Invalid input. Enter a number or UNDO.\n");
				continue;
			}

			double step_ratio = dur / sim.del_t;
			bool divisible = fabs(step_ratio - round(step_ratio)) < 1e-6;

			if (dur <= 0.0) {
				printf("Invalid: cmd_thr_time must be > 0.\n");
			}
			else if (!divisible) {
				printf("Invalid: cmd_thr_time must be divisible by time step (%.4lf s).\n", sim.del_t);
			}
			else {
				cur.cmd_thr_time = dur;
				step = 1;
			}
		}
		else if (step == 1) {
			double max_thr_pct = (rem_burn[i] <= 0.0) ? 0.0 : (rem_burn[i] / cur.cmd_thr_time);
			if (max_thr_pct > 1.0) max_thr_pct = 1.0;

			printf("Command throttle setting (0.0 to %.3lf) [or UNDO]: ", max_thr_pct);
			if (scanf_s("%63s", buf, (unsigned)sizeof(buf)) != 1) return 1;

			if (is_keyword(buf, "UNDO")) {
				step = 0;
				continue;
			}

			double thr_pct = 0.0;
			if (!parse_double(buf, &thr_pct)) {
				printf("Invalid input. Enter a number between 0 and %.3lf or UNDO.\n", max_thr_pct);
				continue;
			}

			// Allow 0.001 rounding tolerance so typing the displayed 3-decimal max is always accepted
			if (thr_pct < 0.0 || thr_pct > max_thr_pct + 0.001 + 1e-6) {
				printf("Invalid: Throttle must be between 0.000 and %.3lf.\n", max_thr_pct);
				continue;
			}

			double consumed = cur.cmd_thr_time * thr_pct;
			if (consumed > rem_burn[i]) {
				consumed = rem_burn[i];
				thr_pct = (cur.cmd_thr_time > 0.0) ? (rem_burn[i] / cur.cmd_thr_time) : 0.0;
			}

			cur.cmd_thr_pct = thr_pct;
			cur.cmd_thr_pwr = thr_pct * stg[i].thr;
			cur.consumed_burn = consumed;
			rem_burn[i] -= consumed;
			if (rem_burn[i] < 1e-9) rem_burn[i] = 0.0;

			step = 2;
		}
		else if (step == 2) {
			printf("Command ship angle (deg, 90 = straight up) [or UNDO]: ");
			if (scanf_s("%63s", buf, (unsigned)sizeof(buf)) != 1) return 1;

			if (is_keyword(buf, "UNDO")) {
				rem_burn[i] += cur.consumed_burn;
				step = 1;
				continue;
			}

			double ang = 0.0;
			if (!parse_double(buf, &ang)) {
				printf("Invalid input. Enter an angle in degrees or UNDO.\n");
				continue;
			}

			cur.cmd_ship_angle = ang;
			step = 3;
		}
		else if (step == 3) {
			if (i == sim.stage_count - 1) {
				printf("Finish Stage %d commands after this command? (Y/N) [or UNDO]: ", i + 1);
			}
			else {
				printf("Decouple Stage %d after this command? (Y/N) [or UNDO]: ", i + 1);
			}

			if (scanf_s("%63s", buf, (unsigned)sizeof(buf)) != 1) return 1;

			if (is_keyword(buf, "UNDO")) {
				step = 2;
				continue;
			}

			if (is_keyword(buf, "Y") || is_keyword(buf, "YES")) {
				cur.decouple = "Y";
				cmds[i].push_back(cur);
				i++;
				stage_banner_shown = false;
				step = 0;
			}
			else if (is_keyword(buf, "N") || is_keyword(buf, "NO")) {
				cur.decouple = "N";
				cmds[i].push_back(cur);
				step = 0;
			}
			else {
				printf("Invalid input. Please enter Y, N, or UNDO.\n");
			}
		}
	}

	return 0;
}

int calculations(struct base sim, const vector<stage>& stg, const vector<vector<command>>& cmd, FILE* result, struct highlights* fnl) {
	double m_tot, v, v_x, v_y, rho, F_x, F_y, F_D, a_x, a_y;
	double x0, y0, x, y, H_prev, arc_prev, H, H_init, arc, T, P, radian, dH, dH_prev;
	double g_local, a_sound, mach, C_d_eff;

	double* rem_fuel = new double[sim.stage_count];
	bool* decoupled = new bool[sim.stage_count];
	m_tot = 0.0;
	for (int k = 0; k < sim.stage_count; k++) {
		rem_fuel[k] = stg[k].m_fuel;
		decoupled[k] = false;
		m_tot += stg[k].m_dry + rem_fuel[k];
	}

	v = 0.0;
	v_x = 0.0;
	v_y = 0.0;
	x = 0.0;
	x0 = 0.0;
	y = R_E;
	y0 = R_E;
	arc = 0.0;
	arc_prev = 0.0;
	H_init = sqrt((x0 * x0) + (y0 * y0));
	H_prev = H_init - R_E;
	H = sqrt((x * x) + (y * y)) - R_E;
	dH_prev = 0.0;
	dH = 0.0;

	fnl->m_alt = 0.0;
	fnl->downrange = 0.0;
	fnl->t_fl = 0.0;
	fnl->in_orbit = false;
	fnl->escaped = false;

	if (H < 11000) {
		T = 15.04 - 0.00649 * H;
		P = 101.29 * pow((T + 273.1) / 288, 5.256);
		rho = P / (0.2869 * (T + 273.1));
	}
	else if (H >= 11000 && H < 25000) {
		T = -56.46;
		P = 22.65 * exp(1.73 - 0.000157 * H);
		rho = P / (0.2869 * (T + 273.1));
	}
	else if (H >= 25000 && H < ATMO_LIMIT) {
		T = -131.21 + 0.00299 * H;
		P = 2.488 * pow((T + 273.1) / 216.6, -11.388);
		rho = P / (0.2869 * (T + 273.1));
	}
	else if (H >= ATMO_LIMIT) {
		rho = 0;
	}
	else {
		delete[] rem_fuel;
		delete[] decoupled;
		return 3;
	}

	double time = 0.0;
	bool grounded = false;
	fprintf(result, "%10.2lf\t%20.1lf\t%20.1lf\t%20.1lf\t%20.3lf\t%15.1lf\t%15.1lf\t%15.1lf\n",
		time, v, H, m_tot, rho, x, y, arc);

	// Powered / scheduled command phase
	for (int s = 0; s < sim.stage_count && !grounded; s++) {
		int n_cmds_s = (int)cmd[s].size();
		for (int c = 0; c < n_cmds_s && !grounded; c++) {
			int n_steps = (int)round(cmd[s][c].cmd_thr_time / sim.del_t);
			radian = cmd[s][c].cmd_ship_angle * PI / 180.0;

			double dt_burn_per_step = sim.del_t * cmd[s][c].cmd_thr_pct;
			double dm_fuel_per_step = stg[s].m_fuel * (dt_burn_per_step / stg[s].t_burn);

			for (int step = 0; step < n_steps; step++) {
				g_local = g0 * pow((double)R_E / (R_E + H), 2.0);

				if (H < 11000) {
					T = 15.04 - 0.00649 * H;
					P = 101.29 * pow((T + 273.1) / 288, 5.256);
					rho = P / (0.2869 * (T + 273.1));
				}
				else if (H >= 11000 && H < 25000) {
					T = -56.46;
					P = 22.65 * exp(1.73 - 0.000157 * H);
					rho = P / (0.2869 * (T + 273.1));
				}
				else if (H >= 25000 && H < ATMO_LIMIT) {
					T = -131.21 + 0.00299 * H;
					P = 2.488 * pow((T + 273.1) / 216.6, -11.388);
					rho = P / (0.2869 * (T + 273.1));
				}
				else if (H >= ATMO_LIMIT) {
					T = -86.2;
					rho = 0;
				}
				else {
					delete[] rem_fuel;
					delete[] decoupled;
					return 4;
				}

				a_sound = sqrt(1.4 * 287.05 * (T + 273.15));
				mach = (a_sound > 0.0) ? (v / a_sound) : 0.0;

				if (mach < 0.7) {
					C_d_eff = stg[s].C_d * 1.0;
				}
				else if (mach < 1.2) {
					C_d_eff = stg[s].C_d * 1.8;
				}
				else if (mach <= 5.0) {
					C_d_eff = stg[s].C_d * 1.3;
				}
				else {
					C_d_eff = stg[s].C_d * 1.1;
				}

				m_tot = 0.0;
				for (int k = 0; k < sim.stage_count; k++) {
					if (!decoupled[k]) {
						m_tot += stg[k].m_dry + rem_fuel[k];
					}
				}
				if (m_tot <= 0.0) {
					delete[] rem_fuel;
					delete[] decoupled;
					return 1;
				}

				F_D = 0.5 * rho * v * v * C_d_eff * stg[s].crs_sct;
				double drag_x = (v > 0.0) ? (-F_D * (v_x / v)) : 0.0;
				double drag_y = (v > 0.0) ? (-F_D * (v_y / v)) : 0.0;

				double thr_dir_x = cos(radian - arc);
				double thr_dir_y = sin(radian - arc);

				F_x = cmd[s][c].cmd_thr_pwr * thr_dir_x + drag_x - m_tot * g_local * sin(arc);
				F_y = cmd[s][c].cmd_thr_pwr * thr_dir_y + drag_y - m_tot * g_local * cos(arc);

				a_x = F_x / m_tot;
				a_y = F_y / m_tot;
				v_x = v_x + a_x * sim.del_t;
				v_y = v_y + a_y * sim.del_t;
				v = sqrt(pow(v_x, 2) + pow(v_y, 2));
				x = x + v_x * sim.del_t;
				y = y + v_y * sim.del_t;

				rem_fuel[s] -= dm_fuel_per_step;
				if (rem_fuel[s] < 0.0) rem_fuel[s] = 0.0;

				time += sim.del_t;
				dH_prev = H - H_prev;
				H_prev = H;
				H = sqrt((x * x) + (y * y)) - R_E;
				dH = H - H_prev;
				arc_prev = arc;
				arc = atan2(x, y);

				if (H > fnl->m_alt) {
					fnl->m_alt = H;
				}

				m_tot = 0.0;
				for (int k = 0; k < sim.stage_count; k++) {
					if (!decoupled[k]) {
						m_tot += stg[k].m_dry + rem_fuel[k];
					}
				}

				if (H >= 0.0) {
					fprintf(result, "%10.2lf\t%20.1lf\t%20.1lf\t%20.1lf\t%20.3lf\t%15.1lf\t%15.1lf\t%15.1lf\n",
						time, v, H, m_tot, rho, x, y, arc);
				}
				else {
					if (time <= sim.del_t + 1e-9) {
						delete[] rem_fuel;
						delete[] decoupled;
						return 1;
					}
					double frac = (H_prev / (H_prev - H));
					fnl->downrange = (arc_prev + ((arc - arc_prev) * frac)) * R_E;
					fnl->t_fl = time - sim.del_t + frac * sim.del_t;
					grounded = true;
					break;
				}
			}

			// Eject lower stage if decoupled (keep final stage intact so payload/spacecraft remains active)
			if (cmd[s][c].decouple == "Y" && s < sim.stage_count - 1) {
				decoupled[s] = true;
				rem_fuel[s] = 0.0;
			}
			fprintf(result, "Command %d Completed", c + 1);
			for (int strip = 0; strip < 100; strip++) {
				fprintf(result, "-");
			}
			fprintf(result, "\n");
		}
		fprintf(result, "Stage %d Completed", s + 1);
		for (int strip = 0; strip < 100; strip++) {
			fprintf(result, "-");
		}
		fprintf(result, "\n");
	}

	// Evaluate orbital status at the end of all scheduled commands
	if (!grounded) {
		evaluate_orbit(x, y, v_x, v_y, fnl);
		if (fnl->in_orbit) {
			fnl->t_fl = time;
			if (fnl->apoapsis > fnl->m_alt) fnl->m_alt = fnl->apoapsis;
			fprintf(result, "ORBIT ACHIEVED! Periapsis: %.2lf m | Apoapsis: %.2lf m | Eccentricity: %.5lf | Period: %.2lf s\n",
				fnl->periapsis, fnl->apoapsis, fnl->eccentricity, fnl->orbital_period);
			delete[] rem_fuel;
			delete[] decoupled;
			return 0;
		}
		if (fnl->escaped) {
			fnl->t_fl = time;
			fprintf(result, "ESCAPE TRAJECTORY ACHIEVED!\n");
			delete[] rem_fuel;
			delete[] decoupled;
			return 0;
		}
	}

	// Unpowered glide/coast phase until touchdown (H < 0) or orbital/escape exit
	int aero_stage = sim.stage_count - 1;
	for (int k = sim.stage_count - 1; k >= 0; k--) {
		if (!decoupled[k]) {
			aero_stage = k;
			break;
		}
	}

	while (H >= 0.0 && !grounded) {
		g_local = g0 * pow((double)R_E / (R_E + H), 2.0);

		if (H < 11000) {
			T = 15.04 - 0.00649 * H;
			P = 101.29 * pow((T + 273.1) / 288, 5.256);
			rho = P / (0.2869 * (T + 273.1));
		}
		else if (H >= 11000 && H < 25000) {
			T = -56.46;
			P = 22.65 * exp(1.73 - 0.000157 * H);
			rho = P / (0.2869 * (T + 273.1));
		}
		else if (H >= 25000 && H < ATMO_LIMIT) {
			T = -131.21 + 0.00299 * H;
			P = 2.488 * pow((T + 273.1) / 216.6, -11.388);
			rho = P / (0.2869 * (T + 273.1));
		}
		else if (H >= ATMO_LIMIT) {
			T = -86.2;
			rho = 0;
		}
		else {
			delete[] rem_fuel;
			delete[] decoupled;
			return 5;
		}

		a_sound = sqrt(1.4 * 287.05 * (T + 273.15));
		mach = (a_sound > 0.0) ? (v / a_sound) : 0.0;

		if (mach < 0.7) {
			C_d_eff = stg[aero_stage].C_d * 1.0;
		}
		else if (mach < 1.2) {
			C_d_eff = stg[aero_stage].C_d * 1.8;
		}
		else if (mach <= 5.0) {
			C_d_eff = stg[aero_stage].C_d * 1.3;
		}
		else {
			C_d_eff = stg[aero_stage].C_d * 1.1;
		}

		m_tot = 0.0;
		for (int k = 0; k < sim.stage_count; k++) {
			if (!decoupled[k]) {
				m_tot += stg[k].m_dry + rem_fuel[k];
			}
		}
		if (m_tot <= 0.0) {
			delete[] rem_fuel;
			delete[] decoupled;
			return 1;
		}

		F_D = 0.5 * rho * v * v * C_d_eff * stg[aero_stage].crs_sct;
		if (v > 0.0) {
			F_x = (-F_D) * (v_x / v) - m_tot * g_local * sin(arc);
			F_y = (-F_D) * (v_y / v) - m_tot * g_local * cos(arc);
		}
		else {
			F_x = 0.0;
			F_y = -m_tot * g_local;
		}

		a_x = F_x / m_tot;
		a_y = F_y / m_tot;
		v_x = v_x + a_x * sim.del_t;
		v_y = v_y + a_y * sim.del_t;
		v = sqrt(pow(v_x, 2) + pow(v_y, 2));
		x = x + v_x * sim.del_t;
		y = y + v_y * sim.del_t;
		time += sim.del_t;

		dH_prev = H - H_prev;
		H_prev = H;
		H = sqrt((x * x) + (y * y)) - R_E;
		dH = H - H_prev;
		arc_prev = arc;
		arc = atan2(x, y);

		if (H > fnl->m_alt) {
			fnl->m_alt = H;
		}

		if (H > 0.0) {
			fprintf(result, "%10.2lf\t%20.1lf\t%20.1lf\t%20.1lf\t%20.3lf\t%15.1lf\t%15.1lf\t%15.1lf\n",
				time, v, H, m_tot, rho, x, y, arc);

			evaluate_orbit(x, y, v_x, v_y, fnl);
			if (fnl->in_orbit) {
				fnl->t_fl = time;
				if (fnl->apoapsis > fnl->m_alt) fnl->m_alt = fnl->apoapsis;
				fprintf(result, "ORBIT ACHIEVED! Periapsis: %.2lf m | Apoapsis: %.2lf m | Eccentricity: %.5lf | Period: %.2lf s\n",
					fnl->periapsis, fnl->apoapsis, fnl->eccentricity, fnl->orbital_period);
				break;
			}
			if (fnl->escaped) {
				fnl->t_fl = time;
				fprintf(result, "ESCAPE TRAJECTORY ACHIEVED!\n");
				break;
			}
		}
		else {
			double frac = (H_prev / (H_prev - H));
			fnl->downrange = (arc_prev + ((arc - arc_prev) * frac)) * R_E;
			fnl->t_fl = time - sim.del_t + frac * sim.del_t;
			grounded = true;
		}
	}

	delete[] rem_fuel;
	delete[] decoupled;
	return 0;
}