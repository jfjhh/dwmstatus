/* made by profil 2011-12-29.
 * edited by jfjhh 2015-1-23.
 **
 ** Compile with:
 ** gcc -Wall -pedantic -std=c99 -lX11 status.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <X11/Xlib.h>

static Display *dpy;

void setstatus(char *str) {
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

float getfreq(char *file) {
	FILE *fd;
	char *freq;
	float ret;

	freq = malloc(10);
	fd = fopen(file, "r");
	if(fd == NULL) {
		fprintf(stderr, "Cannot open '%s' for reading.\n", file);
		exit(1);
	}

	fgets(freq, 10, fd);
	fclose(fd);

	ret = atof(freq)/1000000;
	free(freq);
	return ret;
}

char *getcpu(void) {
	FILE *fd;
	char *buf, *buf_part, *scaling_cur_freq;
	size_t buf_len, buf_part_len, scaling_cur_freq_len;
	int min, max, cur;
	float *freqs;

	fd = fopen("/sys/devices/system/cpu/online", "r");
	if (fd == NULL) {
		fprintf(stderr, "Error getting the number of cpus online.\n");
		exit(1);
	}

	fscanf(fd, "%d-%d", &min, &max);
	if (max >= 100) {
		fprintf(stderr, "%d cores!? Too many!\n", max);
	}

	if ((freqs = (float *) malloc(sizeof(float) * (max + 1))) == NULL) {
		fprintf(stderr, "Cannot allocate memory for cpu frequencies.\n");
		exit(1);
	}

	// 6 chars for each core, plus the terminating '\0' byte.
	buf_part_len = 7;
	if ((buf_part = (char *) malloc(sizeof(char) * buf_part_len)) == NULL) {
		fprintf(stderr, "Cannot allocate memory for buf_part.\n");
		exit(1);
	}
	memset(buf_part, '\0', buf_part_len);

	// buf_part_len chars for each core; take 2 chars to not have ", " at end.
	buf_len = (((max + 1) * (buf_part_len - 1)) - 2 ) + 1;
	if ((buf = (char *) malloc(sizeof(char) * buf_len)) == NULL) {
		fprintf(stderr, "Cannot allocate memory for buf.\n");
		exit(1);
	}
	memset(buf, '\0', buf_len);

	for (cur = min; cur <= max; cur++) {
		scaling_cur_freq_len =
			strlen("/sys/devices/system/cpu/cpu/cpufreq/scaling_cur_freq") +
			1 + ((cur >= 10) ? 2 : 1);
		scaling_cur_freq = (char *) malloc(sizeof(char) * scaling_cur_freq_len);

		snprintf(scaling_cur_freq, scaling_cur_freq_len,
				"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cur);

		freqs[cur] = getfreq(scaling_cur_freq);

		if (cur != max)
			snprintf(buf_part, buf_part_len, "%0.2f, ", freqs[cur]);
		else
			snprintf(buf_part, buf_part_len - 2, "%0.2f", freqs[cur]);

		strncat(buf, buf_part, buf_len + buf_part_len - 1);
	}

	free(freqs);
	free(buf_part);
	return buf;
}

char *getdatetime(void) {
	char *buf;
	time_t result;
	struct tm *resulttm;

	if((buf = malloc(sizeof(char)*65)) == NULL) {
		fprintf(stderr, "Cannot allocate memory for buf.\n");
		exit(1);
	}
	result = time(NULL);
	resulttm = localtime(&result);
	if(resulttm == NULL) {
		fprintf(stderr, "Error getting localtime.\n");
		exit(1);
	}
	if(!strftime(buf, sizeof(char)*65-1, "%a %b %d %H:%M:%S", resulttm)) {
		fprintf(stderr, "strftime is 0.\n");
		exit(1);
	}

	return buf;
}

float getbattery(void) {
	FILE *fd;
	int charge_now, charge_full, energy_now, energy_full, voltage_now;

	fd = fopen("/sys/class/power_supply/BAT0/energy_now", "r");
	if (fd != NULL) {
		fscanf(fd, "%d", &energy_now);
		fclose(fd);

		fd = fopen("/sys/class/power_supply/BAT0/energy_full", "r");
		if(fd == NULL) {
			fprintf(stderr, "Error opening energy_full.\n");
			return -1;
		}
		fscanf(fd, "%d", &energy_full);
		fclose(fd);

		fd = fopen("/sys/class/power_supply/BAT0/voltage_now", "r");
		if(fd == NULL) {
			fprintf(stderr, "Error opening voltage_now.\n");
			return -1;
		}
		fscanf(fd, "%d", &voltage_now);
		fclose(fd);

		return ((float)energy_now * 1000 / (float)voltage_now) * 100 /
			((float)energy_full * 1000 / (float)voltage_now);

	} else {
		fd = fopen("/sys/class/power_supply/BAT1/charge_now", "r");
		if(fd == NULL) {
			fprintf(stderr, "Error opening charge_now.\n");
			return -1;
		}
		fscanf(fd, "%d", &charge_now);
		fclose(fd);


		fd = fopen("/sys/class/power_supply/BAT1/charge_full", "r");
		if(fd == NULL) {
			fprintf(stderr, "Error opening charge_full.\n");
			return -1;
		}
		fscanf(fd, "%d", &charge_full);
		fclose(fd);

		return (((float) charge_now / (float) charge_full) * 100.0);
	}
}

float ramusage(void) {
	FILE *fd;
	int total, free;
	free = total = -1;

	fd = fopen("/proc/meminfo", "r");
	if(fd == NULL) {
		fprintf(stderr, "Error opening charge_full.\n");
		return -1;
	}
	fscanf(fd, "MemTotal: %d kB\n", &total);
	fscanf(fd, "MemFree: %d kB\n", &free);
	fclose(fd);

	return (100.0 - (((float) free / (float) total) * 100.0));
}

int main(void) {
	char *status;
	float bat1, ram;
	char *datetime, *cpu_str;


	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "Cannot open display.\n");
		return 1;
	}

	if((status = malloc(200)) == NULL)
		exit(1);

	for (;;sleep(1)) {
		cpu_str = getcpu();
		ram = ramusage();
		datetime = getdatetime();
		bat1 = getbattery();
		snprintf(status, 200, "[ %s | %0.1f%% ]  [ %0.1f%% | %s ]",
				cpu_str, ram, bat1, datetime);

		free(cpu_str);
		free(datetime);
		setstatus(status);
	}

	free(status);
	XCloseDisplay(dpy);

	return 0;
}

