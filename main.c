/*
 * Smart Energy Consumption Analyzer
 * PPS Project 21
 *
 * Features:
 *  - Add appliances and record usage sessions
 *  - Calculate session energy (kWh), monthly totals and estimated cost
 *  - Show the top 3 highest-consuming appliances for a month
 *  - Display a text-based monthly comparison chart
 *  - Use structures, dynamically allocated arrays, functions and pointers
 *
 * Compile (GCC): gcc smart_energy_analyzer.c -o energy_analyzer -std=c11 -Wall -Wextra
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define NAME_LEN 100
#define DATE_LEN 11
#define TIME_LEN 6
#define BAR_MAX 50

/* One appliance in the household/business. */
typedef struct {
    char name[NAME_LEN];
    double power_watts;
} Appliance;

/* One recorded usage session. appliance_index links it to the appliance array. */
typedef struct {
    int appliance_index;
    char date[DATE_LEN];       /* YYYY-MM-DD */
    char start_time[TIME_LEN]; /* HH:MM */
    char end_time[TIME_LEN];   /* HH:MM; if earlier than start, session ends next day */
    int days_used;             /* Number of days this schedule was used in that month */
} UsageEntry;

/* ---------- Input and validation helpers ---------- */

static void read_line(const char *prompt, char *buffer, size_t size) {
    size_t length;
    int ch;

    printf("%s", prompt);
    if (fgets(buffer, (int)size, stdin) == NULL) {
        printf("\nInput ended. Exiting.\n");
        exit(EXIT_SUCCESS);
    }

    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    } else {
        /* Discard the rest of an overlong input line. */
        while ((ch = getchar()) != '\n' && ch != EOF) { }
    }
}

static int read_int(const char *prompt, int min_value, int max_value) {
    char line[100];
    char *end;
    long value;

    for (;;) {
        read_line(prompt, line, sizeof(line));
        value = strtol(line, &end, 10);
        while (isspace((unsigned char)*end)) end++;
        if (line[0] != '\0' && *end == '\0' && value >= min_value && value <= max_value) {
            return (int)value;
        }
        printf("Please enter a whole number from %d to %d.\n", min_value, max_value);
    }
}

static double read_double(const char *prompt, double min_value, double max_value) {
    char line[100];
    char *end;
    double value;

    for (;;) {
        read_line(prompt, line, sizeof(line));
        value = strtod(line, &end);
        while (isspace((unsigned char)*end)) end++;
        if (line[0] != '\0' && *end == '\0' && value >= min_value && value <= max_value) {
            return value;
        }
        printf("Please enter a number from %.2f to %.2f.\n", min_value, max_value);
    }
}

static int is_leap_year(int year) {
    return (year % 400 == 0) || (year % 4 == 0 && year % 100 != 0);
}

static int valid_date(const char *date) {
    int year, month, day, days_in_month;
    if (strlen(date) != 10 || date[4] != '-' || date[7] != '-') return 0;
    if (!isdigit((unsigned char)date[0]) || !isdigit((unsigned char)date[1]) ||
        !isdigit((unsigned char)date[2]) || !isdigit((unsigned char)date[3]) ||
        !isdigit((unsigned char)date[5]) || !isdigit((unsigned char)date[6]) ||
        !isdigit((unsigned char)date[8]) || !isdigit((unsigned char)date[9])) return 0;

    year = (date[0]-'0')*1000 + (date[1]-'0')*100 + (date[2]-'0')*10 + date[3]-'0';
    month = (date[5]-'0')*10 + date[6]-'0';
    day = (date[8]-'0')*10 + date[9]-'0';
    if (year < 1900 || month < 1 || month > 12) return 0;

    if (month == 2) days_in_month = is_leap_year(year) ? 29 : 28;
    else if (month == 4 || month == 6 || month == 9 || month == 11) days_in_month = 30;
    else days_in_month = 31;
    return day >= 1 && day <= days_in_month;
}

static int parse_time_minutes(const char *time_text, int *minutes) {
    int hour, minute;
    if (strlen(time_text) != 5 || time_text[2] != ':' ||
        !isdigit((unsigned char)time_text[0]) || !isdigit((unsigned char)time_text[1]) ||
        !isdigit((unsigned char)time_text[3]) || !isdigit((unsigned char)time_text[4])) return 0;
    hour = (time_text[0]-'0')*10 + time_text[1]-'0';
    minute = (time_text[3]-'0')*10 + time_text[4]-'0';
    if (hour > 23 || minute > 59) return 0;
    *minutes = hour * 60 + minute;
    return 1;
}

static void read_valid_date(const char *prompt, char *date) {
    for (;;) {
        read_line(prompt, date, DATE_LEN);
        if (valid_date(date)) return;
        printf("Invalid date. Use a real date in YYYY-MM-DD format (example: 2026-11-18).\n");
    }
}

static void read_valid_time(const char *prompt, char *time_text) {
    int ignored;
    for (;;) {
        read_line(prompt, time_text, TIME_LEN);
        if (parse_time_minutes(time_text, &ignored)) return;
        printf("Invalid time. Use 24-hour HH:MM format (example: 09:30 or 18:45).\n");
    }
}

static void read_month(char *month) {
    char line[100];
    for (;;) {
        read_line("Enter month (YYYY-MM): ", line, sizeof(line));
        if (strlen(line) == 7 && line[4] == '-' &&
            isdigit((unsigned char)line[0]) && isdigit((unsigned char)line[1]) &&
            isdigit((unsigned char)line[2]) && isdigit((unsigned char)line[3]) &&
            isdigit((unsigned char)line[5]) && isdigit((unsigned char)line[6])) {
            int m = (line[5]-'0')*10 + line[6]-'0';
            int y = (line[0]-'0')*1000 + (line[1]-'0')*100 + (line[2]-'0')*10 + line[3]-'0';
            if (m >= 1 && m <= 12 && y >= 1900) {
                strcpy(month, line);
                return;
            }
        }
        printf("Invalid month. Use YYYY-MM, for example 2026-11.\n");
    }
}

static int same_month(const char *date, const char *month) {
    return strncmp(date, month, 7) == 0;
}

static double session_hours(const UsageEntry *entry) {
    int start_minutes, end_minutes;
    parse_time_minutes(entry->start_time, &start_minutes);
    parse_time_minutes(entry->end_time, &end_minutes);
    if (end_minutes <= start_minutes) end_minutes += 24 * 60;
    return (double)(end_minutes - start_minutes) / 60.0;
}

static double entry_energy_kwh(const Appliance *appliance, const UsageEntry *entry) {
    return (appliance->power_watts / 1000.0) * session_hours(entry) * entry->days_used;
}

/* ---------- Appliance and usage operations ---------- */

static void add_appliance(Appliance **appliances, int *count, int *capacity) {
    char name[NAME_LEN];
    double watts;
    Appliance *temporary;
    int i;

    if (*count == *capacity) {
        int new_capacity = (*capacity == 0) ? 4 : (*capacity * 2);
        temporary = (Appliance *)realloc(*appliances, (size_t)new_capacity * sizeof(Appliance));
        if (temporary == NULL) {
            printf("Memory allocation failed. Appliance was not added.\n");
            return;
        }
        *appliances = temporary;
        *capacity = new_capacity;
    }

    read_line("Enter appliance name (do not use |): ", name, sizeof(name));
    if (name[0] == '\0' || strchr(name, '|') != NULL) {
        printf("Name cannot be empty or contain the | character.\n");
        return;
    }
    for (i = 0; i < *count; i++) {
        if (strcmp((*appliances)[i].name, name) == 0) {
            printf("That appliance is already in the list.\n");
            return;
        }
    }

    watts = read_double("Enter power rating in watts (1-100000): ", 1.0, 100000.0);
    strcpy((*appliances)[*count].name, name);
    (*appliances)[*count].power_watts = watts;
    (*count)++;
    printf("Appliance added successfully.\n");
}

static void list_appliances(const Appliance *appliances, int count) {
    int i;
    if (count == 0) {
        printf("No appliances added yet.\n");
        return;
    }
    printf("\n%-5s %-30s %12s\n", "No.", "Appliance", "Power (W)");
    printf("---------------------------------------------------\n");
    for (i = 0; i < count; i++)
        printf("%-5d %-30s %12.2f\n", i + 1, appliances[i].name, appliances[i].power_watts);
}

static void add_usage_entry(const Appliance *appliances, int appliance_count,
                            UsageEntry **entries, int *entry_count, int *entry_capacity) {
    UsageEntry new_entry;
    UsageEntry *temporary;
    int start_minutes, end_minutes;

    if (appliance_count == 0) {
        printf("Add an appliance first.\n");
        return;
    }
    list_appliances(appliances, appliance_count);
    new_entry.appliance_index = read_int("Choose appliance number: ", 1, appliance_count) - 1;
    read_valid_date("Enter usage date (YYYY-MM-DD): ", new_entry.date);
    read_valid_time("Enter start time (HH:MM): ", new_entry.start_time);
    read_valid_time("Enter end time (HH:MM): ", new_entry.end_time);
    parse_time_minutes(new_entry.start_time, &start_minutes);
    parse_time_minutes(new_entry.end_time, &end_minutes);
    if (start_minutes == end_minutes) {
        printf("Start and end times cannot be identical (that would mean zero or 24 hours). Entry cancelled.\n");
        return;
    }
    new_entry.days_used = read_int("How many days was this schedule used in that month (1-31)? ", 1, 31);

    if (*entry_count == *entry_capacity) {
        int new_capacity = (*entry_capacity == 0) ? 8 : (*entry_capacity * 2);
        temporary = (UsageEntry *)realloc(*entries, (size_t)new_capacity * sizeof(UsageEntry));
        if (temporary == NULL) {
            printf("Memory allocation failed. Usage entry was not added.\n");
            return;
        }
        *entries = temporary;
        *entry_capacity = new_capacity;
    }
    (*entries)[*entry_count] = new_entry;
    (*entry_count)++;
    printf("Usage entry added. Energy for this entry: %.3f kWh\n",
           entry_energy_kwh(&appliances[new_entry.appliance_index], &new_entry));
}

/* ---------- Reports ---------- */

typedef struct {
    int appliance_index;
    double energy;
} ApplianceTotal;

static void monthly_report(const Appliance *appliances, int appliance_count,
                           const UsageEntry *entries, int entry_count, double tariff) {
    char month[8];
    ApplianceTotal *totals;
    double grand_total = 0.0;
    int i, j, found;

    if (appliance_count == 0 || entry_count == 0) {
        printf("Add appliances and usage entries before creating a report.\n");
        return;
    }
    read_month(month);
    totals = (ApplianceTotal *)calloc((size_t)appliance_count, sizeof(ApplianceTotal));
    if (totals == NULL) {
        printf("Not enough memory to create the report.\n");
        return;
    }
    for (i = 0; i < appliance_count; i++) totals[i].appliance_index = i;

    for (i = 0; i < entry_count; i++) {
        if (same_month(entries[i].date, month)) {
            int index = entries[i].appliance_index;
            double energy = entry_energy_kwh(&appliances[index], &entries[i]);
            totals[index].energy += energy;
            grand_total += energy;
        }
    }

    printf("\n========== MONTHLY ENERGY REPORT: %s ==========\n", month);
    printf("%-30s %14s %16s\n", "Appliance", "Energy (kWh)", "Cost");
    printf("---------------------------------------------------------------\n");
    for (i = 0; i < appliance_count; i++) {
        if (totals[i].energy > 0.0)
            printf("%-30s %14.3f %15.2f\n", appliances[i].name,
                   totals[i].energy, totals[i].energy * tariff);
    }
    printf("---------------------------------------------------------------\n");
    printf("%-30s %14.3f %15.2f\n", "TOTAL", grand_total, grand_total * tariff);
    printf("Tariff used: %.2f currency units per kWh\n", tariff);

    /* Sort appliance totals in descending order to find the top 3. */
    for (i = 0; i < appliance_count - 1; i++) {
        for (j = i + 1; j < appliance_count; j++) {
            if (totals[j].energy > totals[i].energy) {
                ApplianceTotal temp = totals[i];
                totals[i] = totals[j];
                totals[j] = temp;
            }
        }
    }

    printf("\nTop 3 highest-consuming appliances:\n");
    found = 0;
    for (i = 0; i < appliance_count && found < 3; i++) {
        if (totals[i].energy > 0.0) {
            found++;
            printf("%d. %s - %.3f kWh\n", found,
                   appliances[totals[i].appliance_index].name, totals[i].energy);
        }
    }
    if (found == 0) printf("No usage entries were recorded for this month.\n");

    printf("\nSuggestions to reduce consumption:\n");
    if (found == 0) {
        printf("- Add usage records for this month to receive useful suggestions.\n");
    } else {
        int suggestions = 0;
        for (i = 0; i < appliance_count && suggestions < 3; i++) {
            if (totals[i].energy > 0.0) {
                printf("- Review the usage time of %s; reducing its daily running time can lower consumption.\n",
                       appliances[totals[i].appliance_index].name);
                suggestions++;
            }
        }
        printf("- Switch off appliances when they are not needed and prefer energy-efficient models.\n");
    }
    free(totals);
}

static void monthly_comparison(const Appliance *appliances, int appliance_count,
                               const UsageEntry *entries, int entry_count) {
    double *month_totals;
    char **months;
    int month_count = 0, i, j;

    if (entry_count == 0 || appliance_count == 0) {
        printf("No usage data available.\n");
        return;
    }
    month_totals = (double *)calloc((size_t)entry_count, sizeof(double));
    months = (char **)calloc((size_t)entry_count, sizeof(char *));
    if (month_totals == NULL || months == NULL) {
        printf("Not enough memory to create comparison.\n");
        free(month_totals);
        free(months);
        return;
    }

    for (i = 0; i < entry_count; i++) {
        char current_month[8];
        strncpy(current_month, entries[i].date, 7);
        current_month[7] = '\0';
        for (j = 0; j < month_count; j++)
            if (strcmp(months[j], current_month) == 0) break;
        if (j == month_count) {
            months[month_count] = (char *)malloc(8);
            if (months[month_count] == NULL) {
                printf("Not enough memory to create comparison.\n");
                for (j = 0; j < month_count; j++) free(months[j]);
                free(months);
                free(month_totals);
                return;
            }
            strcpy(months[month_count], current_month);
            month_totals[month_count] = 0.0;
            month_count++;
        }
        month_totals[j] += entry_energy_kwh(&appliances[entries[i].appliance_index], &entries[i]);
    }

    /* Sort months chronologically (YYYY-MM sorts correctly as text). */
    for (i = 0; i < month_count - 1; i++) {
        for (j = i + 1; j < month_count; j++) {
            if (strcmp(months[i], months[j]) > 0) {
                char *month_temp = months[i];
                double energy_temp = month_totals[i];
                months[i] = months[j];
                month_totals[i] = month_totals[j];
                months[j] = month_temp;
                month_totals[j] = energy_temp;
            }
        }
    }

    printf("\n========== MONTHLY CONSUMPTION COMPARISON ==========\n");
    printf("\nEach # represents approximately 1 kWh (maximum bar length %d).\n", BAR_MAX);
    for (i = 0; i < month_count; i++) {
        int hashes = (int)(month_totals[i] + 0.5);
        if (hashes > BAR_MAX) hashes = BAR_MAX;
        printf("%s | %8.3f kWh | ", months[i], month_totals[i]);
        for (j = 0; j < hashes; j++) putchar('#');
        if (month_totals[i] > BAR_MAX) printf(" ...");
        putchar('\n');
    }

    for (i = 0; i < month_count; i++) free(months[i]);
    free(months);
    free(month_totals);
}

static void change_tariff(double *tariff) {
    *tariff = read_double("Enter electricity tariff per kWh: ", 0.0, 100000.0);
    printf("Tariff updated to %.2f per kWh.\n", *tariff);
}

/* ---------- Main menu ---------- */

int main(void) {
    Appliance *appliances = NULL;
    UsageEntry *entries = NULL;
    int appliance_count = 0, appliance_capacity = 0;
    int entry_count = 0, entry_capacity = 0;
    double tariff = 8.0; /* Default only; change this to match your electricity bill. */
    int choice;

    printf("===============================================\n");
    printf("       SMART ENERGY CONSUMPTION ANALYZER       \n");
    printf("===============================================\n");
    printf("Default tariff: %.2f per kWh (you can change it).\n", tariff);

    do {
        printf("\n---------------- MAIN MENU ----------------\n");
        printf("1. Add appliance\n");
        printf("2. Add daily usage entry\n");
        printf("3. View all appliances\n");
        printf("4. Generate monthly report (totals, cost, top 3, suggestions)\n");
        printf("5. Show monthly comparison chart\n");
        printf("6. Set electricity tariff\n");
        printf("0. Exit\n");
        printf("-------------------------------------------\n");
        choice = read_int("Enter your choice: ", 0, 6);

        switch (choice) {
            case 1:
                add_appliance(&appliances, &appliance_count, &appliance_capacity);
                break;
            case 2:
                add_usage_entry(appliances, appliance_count, &entries, &entry_count, &entry_capacity);
                break;
            case 3:
                list_appliances(appliances, appliance_count);
                break;
            case 4:
                monthly_report(appliances, appliance_count, entries, entry_count, tariff);
                break;
            case 5:
                monthly_comparison(appliances, appliance_count, entries, entry_count);
                break;
            case 6:
                change_tariff(&tariff);
                break;
            case 0:
                printf("Thank you for using Smart Energy Consumption Analyzer.\n");
                break;
        }
    } while (choice != 0);

    free(appliances);
    free(entries);
    return 0;
}
