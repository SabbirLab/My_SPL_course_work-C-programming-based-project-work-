// # We'll create three files for the user:
// # 1) uiu_ums.c  -> full C source
// # 2) Makefile   -> to compile easily on Linux/macOS (MinGW similar)
// # 3) README_UIU_UMS.txt -> quick start and usage

// code = r'''
/*
 * UIU University Management System (UMS) - Single File C Project
 * Author: ChatGPT (for Sabbir Ahmed)
 * Target: GCC/Clang (C11). No external libraries.
 *
 * FEATURES
 * - User roles: ADMIN, FACULTY, STUDENT (simple login system).
 * - Entities: Students, Faculty, Courses, Enrollments (grades).
 * - Admin: CRUD students/faculty/courses, enroll, assign grades, reports.
 * - Faculty: View courses, class rosters, enter/update grades.
 * - Student: View profile, enrollments, transcript, GPA.
 * - Storage: Binary files (simple, portable), created on first run with demo data.
 *
 * SECURITY NOTE
 * - This is a teaching project. Passwords are lightly obfuscated (XOR + salt).
 *   Do NOT use in production. Replace with a proper crypto hash (e.g., bcrypt)
 *   if allowed in your environment.
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* ======== CONFIG ======== */
#define MAX_NAME 64
#define MAX_DEPT 32
#define MAX_EMAIL 64
#define MAX_ID 16
#define MAX_CODE 16
#define MAX_TITLE 64
#define MAX_TERM 16
#define MAX_USER 32
#define MAX_PASS 32

#define FILE_STUD "students.dat"
#define FILE_FAC "faculty.dat"
#define FILE_COURSE "courses.dat"
#define FILE_ENR "enrollments.dat"
#define FILE_USER "users.dat"

/* ======== TYPES ======== */
typedef enum
{
    ROLE_ADMIN = 1,
    ROLE_FACULTY = 2,
    ROLE_STUDENT = 3
} Role;

typedef struct
{
    char id[MAX_ID]; // e.g., 02124100034 or UIU-EEE-001
    char name[MAX_NAME];
    char dept[MAX_DEPT]; // e.g., EEE, CSE
    int batch;           // e.g., 231, 241
    char email[MAX_EMAIL];
} Student;

typedef struct
{
    char id[MAX_ID]; // e.g., FAC-EEE-001
    char name[MAX_NAME];
    char dept[MAX_DEPT];
    char email[MAX_EMAIL];
} Faculty;

typedef struct
{
    char code[MAX_CODE]; // e.g., EEE-2101
    char title[MAX_TITLE];
    float credit; // e.g., 3.0
    char dept[MAX_DEPT];
    char instructorId[MAX_ID]; // optional (who teaches)
} Course;

typedef struct
{
    char studentId[MAX_ID];
    char courseCode[MAX_CODE];
    char term[MAX_TERM]; // e.g., Spring-2025
    char grade[3];       // e.g., A, A-, B+, F
} Enrollment;

typedef struct
{
    char username[MAX_USER];
    Role role;
    char refId[MAX_ID];               // link to Student/Faculty ID (empty for admin)
    unsigned char pass_obf[MAX_PASS]; // obfuscated password (fixed 32 bytes)
} User;

/* ======== UTILS ======== */
static const unsigned char SALT[8] = {0x55, 0x2A, 0x11, 0xC3, 0x7E, 0x90, 0x04, 0xD1};

void obfuscate(const char *plain, unsigned char out[MAX_PASS])
{
    // Simple XOR with rotating SALT into a fixed 32-byte buffer
    size_t n = strlen(plain);
    for (int i = 0; i < MAX_PASS; i++)
    {
        unsigned char p = (i < (int)n) ? (unsigned char)plain[i] : 0;
        out[i] = p ^ SALT[i % 8];
    }
}

int verify_pass(const unsigned char obf[MAX_PASS], const char *try_pass)
{
    unsigned char tmp[MAX_PASS];
    obfuscate(try_pass, tmp);
    return memcmp(obf, tmp, MAX_PASS) == 0;
}

void pause_enter()
{
    printf("\nPress ENTER to continue...");
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
    {
    }
    getchar();
}

void trim_newline(char *s)
{
    size_t n = strlen(s);
    if (n && s[n - 1] == '\n')
        s[n - 1] = 0;
}

void read_line(const char *prompt, char *buf, size_t cap)
{
    printf("%s", prompt);
    if (fgets(buf, (int)cap, stdin))
    {
        trim_newline(buf);
    }
    else
    {
        buf[0] = 0;
        clearerr(stdin);
    }
}

int read_int(const char *prompt)
{
    char line[64];
    read_line(prompt, line, sizeof(line));
    return atoi(line);
}

float read_float(const char *prompt)
{
    char line[64];
    read_line(prompt, line, sizeof(line));
    return (float)atof(line);
}

void upper(char *s)
{
    for (; *s; ++s)
        *s = (char)toupper((unsigned char)*s);
}

/* ======== FILE HELPERS ======== */
#define OPEN_BIN_APPEND(path, fp) FILE *fp = fopen(path, "ab")
#define OPEN_BIN_READ(path, fp) FILE *fp = fopen(path, "rb")
#define OPEN_BIN_WRITE(path, fp) FILE *fp = fopen(path, "wb")

/* Count records of size recSize in file */
long file_count_records(const char *path, size_t recSize)
{
    OPEN_BIN_READ(path, fp);
    if (!fp)
        return 0;
    fseek(fp, 0, SEEK_END);
    long bytes = ftell(fp);
    fclose(fp);
    return (bytes < 0) ? 0 : (bytes / (long)recSize);
}

/* Generic find first match by equality */
typedef int (*rec_pred)(const void *rec, const void *key);

long file_find_first(const char *path, size_t recSize, rec_pred pred, const void *key, void *out)
{
    OPEN_BIN_READ(path, fp);
    if (!fp)
        return -1;
    long idx = 0;
    unsigned char *buf = (unsigned char *)malloc(recSize);
    while (fread(buf, recSize, 1, fp) == 1)
    {
        if (pred(buf, key))
        {
            if (out)
                memcpy(out, buf, recSize);
            free(buf);
            fclose(fp);
            return idx;
        }
        idx++;
    }
    free(buf);
    fclose(fp);
    return -1;
}

int file_read_at(const char *path, size_t recSize, long index, void *out)
{
    OPEN_BIN_READ(path, fp);
    if (!fp)
        return 0;
    if (fseek(fp, index * recSize, SEEK_SET) != 0)
    {
        fclose(fp);
        return 0;
    }
    int ok = fread(out, recSize, 1, fp) == 1;
    fclose(fp);
    return ok;
}

int file_write_at(const char *path, size_t recSize, long index, const void *rec)
{
    OPEN_BIN_READ(path, rfp);
    if (!rfp)
        return 0;
    fclose(rfp);
    FILE *fp = fopen(path, "rb+");
    if (!fp)
        return 0;
    if (fseek(fp, index * recSize, SEEK_SET) != 0)
    {
        fclose(fp);
        return 0;
    }
    int ok = fwrite(rec, recSize, 1, fp) == 1;
    fflush(fp);
    fclose(fp);
    return ok;
}

int file_append(const char *path, size_t recSize, const void *rec)
{
    OPEN_BIN_APPEND(path, fp);
    if (!fp)
        return 0;
    int ok = fwrite(rec, recSize, 1, fp) == 1;
    fflush(fp);
    fclose(fp);
    return ok;
}

/* ======== PREDICATES ======== */
int pred_student_by_id(const void *rec, const void *key)
{
    const Student *s = (const Student *)rec;
    const char *id = (const char *)key;
    return strcmp(s->id, id) == 0;
}
int pred_faculty_by_id(const void *rec, const void *key)
{
    const Faculty *s = (const Faculty *)rec;
    const char *id = (const char *)key;
    return strcmp(s->id, id) == 0;
}
int pred_course_by_code(const void *rec, const void *key)
{
    const Course *s = (const Course *)rec;
    const char *code = (const char *)key;
    return strcmp(s->code, code) == 0;
}
int pred_user_by_username(const void *rec, const void *key)
{
    const User *u = (const User *)rec;
    const char *uname = (const char *)key;
    return strcmp(u->username, uname) == 0;
}
typedef struct
{
    char sid[MAX_ID];
    char code[MAX_CODE];
    char term[MAX_TERM];
} EnrKey;
int pred_enr_by_key(const void *rec, const void *key)
{
    const Enrollment *e = (const Enrollment *)rec;
    const EnrKey *k = (const EnrKey *)key;
    return strcmp(e->studentId, k->sid) == 0 && strcmp(e->courseCode, k->code) == 0 && strcmp(e->term, k->term) == 0;
}

/* ======== DOMAIN LOGIC ======== */
float grade_to_points(const char *g)
{
    // UIU-like 4.0 scale (adjust if your dept uses different)
    // Supports A, A-, B+, B, B-, C+, C, C-, D, F
    if (strcmp(g, "A") == 0)
        return 4.00f;
    if (strcmp(g, "A-") == 0)
        return 3.70f;
    if (strcmp(g, "B+") == 0)
        return 3.30f;
    if (strcmp(g, "B") == 0)
        return 3.00f;
    if (strcmp(g, "B-") == 0)
        return 2.70f;
    if (strcmp(g, "C+") == 0)
        return 2.30f;
    if (strcmp(g, "C") == 0)
        return 2.00f;
    if (strcmp(g, "C-") == 0)
        return 1.70f;
    if (strcmp(g, "D") == 0)
        return 1.00f;
    if (strcmp(g, "F") == 0)
        return 0.00f;
    return -1.0f; // invalid
}

void print_student(const Student *s)
{
    printf("ID: %s | Name: %s | Dept: %s | Batch: %d | Email: %s\n", s->id, s->name, s->dept, s->batch, s->email);
}

void print_faculty(const Faculty *f)
{
    printf("ID: %s | Name: %s | Dept: %s | Email: %s\n", f->id, f->name, f->dept, f->email);
}

void print_course(const Course *c)
{
    printf("Code: %s | Title: %s | Credit: %.1f | Dept: %s | Instructor: %s\n",
           c->code, c->title, c->credit, c->dept, c->instructorId);
}

void print_enr(const Enrollment *e)
{
    printf("Student: %s | Course: %s | Term: %s | Grade: %s\n", e->studentId, e->courseCode, e->term, e->grade);
}

/* ======== CRUD ======== */
void add_student()
{
    Student s = {0};
    read_line("Student ID: ", s.id, sizeof(s.id));
    if (file_find_first(FILE_STUD, sizeof(Student), pred_student_by_id, s.id, NULL) >= 0)
    {
        printf("Student with this ID already exists.\n");
        return;
    }
    read_line("Name: ", s.name, sizeof(s.name));
    read_line("Dept (EEE/CSE...): ", s.dept, sizeof(s.dept));
    s.batch = read_int("Batch (e.g., 241): ");
    read_line("Email: ", s.email, sizeof(s.email));
    if (file_append(FILE_STUD, sizeof(Student), &s))
        printf("Student added.\n");
    else
        printf("Error writing student file.\n");
}

void edit_student()
{
    char id[MAX_ID];
    read_line("Enter Student ID to edit: ", id, sizeof(id));
    Student s;
    long idx = file_find_first(FILE_STUD, sizeof(Student), pred_student_by_id, id, &s);
    if (idx < 0)
    {
        printf("Not found.\n");
        return;
    }
    print_student(&s);
    printf("Leave blank to keep existing.\n");
    char buf[128];
    read_line("New name: ", buf, sizeof(buf));
    if (strlen(buf))
        strncpy(s.name, buf, MAX_NAME);
    read_line("New dept: ", buf, sizeof(buf));
    if (strlen(buf))
        strncpy(s.dept, buf, MAX_DEPT);
    read_line("New email: ", buf, sizeof(buf));
    if (strlen(buf))
        strncpy(s.email, buf, MAX_EMAIL);
    buf[0] = 0;
    read_line("New batch (empty to keep): ", buf, sizeof(buf));
    if (strlen(buf))
        s.batch = atoi(buf);
    if (file_write_at(FILE_STUD, sizeof(Student), idx, &s))
        printf("Updated.\n");
    else
        printf("Write error.\n");
}

void list_students()
{
    OPEN_BIN_READ(FILE_STUD, fp);
    if (!fp)
    {
        printf("No students yet.\n");
        return;
    }
    Student s;
    printf("\n-- Students --\n");
    while (fread(&s, sizeof(Student), 1, fp) == 1)
        print_student(&s);
    fclose(fp);
}

void add_faculty()
{
    Faculty f = {0};
    read_line("Faculty ID: ", f.id, sizeof(f.id));
    if (file_find_first(FILE_FAC, sizeof(Faculty), pred_faculty_by_id, f.id, NULL) >= 0)
    {
        printf("Faculty exists.\n");
        return;
    }
    read_line("Name: ", f.name, sizeof(f.name));
    read_line("Dept: ", f.dept, sizeof(f.dept));
    read_line("Email: ", f.email, sizeof(f.email));
    if (file_append(FILE_FAC, sizeof(Faculty), &f))
        printf("Faculty added.\n");
    else
        printf("Write error.\n");
}

void list_faculty()
{
    OPEN_BIN_READ(FILE_FAC, fp);
    if (!fp)
    {
        printf("No faculty yet.\n");
        return;
    }
    Faculty f;
    printf("\n-- Faculty --\n");
    while (fread(&f, sizeof(Faculty), 1, fp) == 1)
        print_faculty(&f);
    fclose(fp);
}

void add_course()
{
    Course c = {0};
    read_line("Course code (e.g., EEE-2101): ", c.code, sizeof(c.code));
    if (file_find_first(FILE_COURSE, sizeof(Course), pred_course_by_code, c.code, NULL) >= 0)
    {
        printf("Course exists.\n");
        return;
    }
    read_line("Title: ", c.title, sizeof(c.title));
    c.credit = read_float("Credit (e.g., 3): ");
    read_line("Dept: ", c.dept, sizeof(c.dept));
    read_line("Instructor ID (optional, blank to skip): ", c.instructorId, sizeof(c.instructorId));
    if (file_append(FILE_COURSE, sizeof(Course), &c))
        printf("Course added.\n");
    else
        printf("Write error.\n");
}

void assign_instructor()
{
    char code[MAX_CODE], fid[MAX_ID];
    read_line("Course code: ", code, sizeof(code));
    Course c;
    long idx = file_find_first(FILE_COURSE, sizeof(Course), pred_course_by_code, code, &c);
    if (idx < 0)
    {
        printf("Course not found.\n");
        return;
    }
    read_line("Faculty ID: ", fid, sizeof(fid));
    if (file_find_first(FILE_FAC, sizeof(Faculty), pred_faculty_by_id, fid, NULL) < 0)
    {
        printf("Faculty not found.\n");
        return;
    }
    strncpy(c.instructorId, fid, MAX_ID);
    if (file_write_at(FILE_COURSE, sizeof(Course), idx, &c))
        printf("Instructor assigned.\n");
    else
        printf("Write error.\n");
}

void list_courses()
{
    OPEN_BIN_READ(FILE_COURSE, fp);
    if (!fp)
    {
        printf("No courses yet.\n");
        return;
    }
    Course c;
    printf("\n-- Courses --\n");
    while (fread(&c, sizeof(Course), 1, fp) == 1)
        print_course(&c);
    fclose(fp);
}

void enroll_student()
{
    Enrollment e = {0};
    read_line("Student ID: ", e.studentId, sizeof(e.studentId));
    if (file_find_first(FILE_STUD, sizeof(Student), pred_student_by_id, e.studentId, NULL) < 0)
    {
        printf("Student not found.\n");
        return;
    }
    read_line("Course code: ", e.courseCode, sizeof(e.courseCode));
    Course c;
    if (file_find_first(FILE_COURSE, sizeof(Course), pred_course_by_code, e.courseCode, &c) < 0)
    {
        printf("Course not found.\n");
        return;
    }
    read_line("Term (e.g., Fall-2025): ", e.term, sizeof(e.term));
    strcpy(e.grade, "NA");
    EnrKey key;
    strncpy(key.sid, e.studentId, MAX_ID);
    strncpy(key.code, e.courseCode, MAX_CODE);
    strncpy(key.term, e.term, MAX_TERM);
    if (file_find_first(FILE_ENR, sizeof(Enrollment), pred_enr_by_key, &key, NULL) >= 0)
    {
        printf("Already enrolled.\n");
        return;
    }
    if (file_append(FILE_ENR, sizeof(Enrollment), &e))
        printf("Enrollment added.\n");
    else
        printf("Write error.\n");
}

void set_grade()
{
    Enrollment e;
    char sid[MAX_ID], code[MAX_CODE], term[MAX_TERM], g[3];
    read_line("Student ID: ", sid, sizeof(sid));
    read_line("Course code: ", code, sizeof(code));
    read_line("Term: ", term, sizeof(term));
    EnrKey key;
    strncpy(key.sid, sid, MAX_ID);
    strncpy(key.code, code, MAX_CODE);
    strncpy(key.term, term, MAX_TERM);
    long idx = file_find_first(FILE_ENR, sizeof(Enrollment), pred_enr_by_key, &key, &e);
    if (idx < 0)
    {
        printf("Enrollment not found.\n");
        return;
    }
    read_line("Grade (A, A-, B+, ... , F): ", g, sizeof(g));
    upper(g);
    if (grade_to_points(g) < 0 && strcmp(g, "NA") != 0)
    {
        printf("Invalid grade.\n");
        return;
    }
    strncpy(e.grade, g, 2);
    e.grade[2] = 0;
    if (file_write_at(FILE_ENR, sizeof(Enrollment), idx, &e))
        printf("Grade updated.\n");
    else
        printf("Write error.\n");
}

void transcript_for_student(const char *sid)
{
    // Print courses, terms, credits, grades, and compute CGPA
    OPEN_BIN_READ(FILE_ENR, fp);
    if (!fp)
    {
        printf("No enrollments.\n");
        return;
    }
    Course c;
    Enrollment e;
    float totalCred = 0.0f, totalPts = 0.0f;
    printf("\n-- Transcript for %s --\n", sid);
    while (fread(&e, sizeof(Enrollment), 1, fp) == 1)
    {
        if (strcmp(e.studentId, sid) == 0)
        {
            // lookup course
            if (file_find_first(FILE_COURSE, sizeof(Course), pred_course_by_code, e.courseCode, &c) >= 0)
            {
                float pts = grade_to_points(e.grade);
                printf("%-8s | %-10s | %4.1f cr | Grade: %-2s", c.code, e.term, c.credit, e.grade);
                if (pts >= 0)
                {
                    totalCred += c.credit;
                    totalPts += (pts * c.credit);
                    printf(" | GP: %.2f", pts);
                }
                printf("\n");
            }
        }
    }
    fclose(fp);
    if (totalCred > 0)
    {
        printf("CGPA: %.2f (%.1f total credits)\n", totalPts / totalCred, totalCred);
    }
    else
    {
        printf("No graded credits yet.\n");
    }
}

void roster_for_course_term(const char *code, const char *term)
{
    OPEN_BIN_READ(FILE_ENR, fp);
    if (!fp)
    {
        printf("No enrollments.\n");
        return;
    }
    Enrollment e;
    Student s;
    int count = 0;
    printf("\n-- Roster %s (%s) --\n", code, term);
    while (fread(&e, sizeof(Enrollment), 1, fp) == 1)
    {
        if (strcmp(e.courseCode, code) == 0 && strcmp(e.term, term) == 0)
        {
            if (file_find_first(FILE_STUD, sizeof(Student), pred_student_by_id, e.studentId, &s) >= 0)
            {
                printf("%-12s  %-24s  Grade: %-2s\n", s.id, s.name, e.grade);
                count++;
            }
        }
    }
    fclose(fp);
    if (!count)
        printf("No students enrolled.\n");
}

void gpa_leaderboard(const char *term)
{
    // naive: compute term GPA for each student enrolled in that term
    OPEN_BIN_READ(FILE_ENR, fp);
    if (!fp)
    {
        printf("No enrollments.\n");
        return;
    }
    typedef struct
    {
        char sid[MAX_ID];
        float pts;
        float cred;
    } Acc;
    Acc accs[2048];
    int n = 0;
    Enrollment e;
    Course c;
    while (fread(&e, sizeof(Enrollment), 1, fp) == 1)
    {
        if (strcmp(e.term, term) == 0)
        {
            if (file_find_first(FILE_COURSE, sizeof(Course), pred_course_by_code, e.courseCode, &c) >= 0)
            {
                float gp = grade_to_points(e.grade);
                if (gp < 0)
                    continue; // ungraded
                int found = -1;
                for (int i = 0; i < n; i++)
                    if (strcmp(accs[i].sid, e.studentId) == 0)
                    {
                        found = i;
                        break;
                    }
                if (found < 0)
                {
                    strncpy(accs[n].sid, e.studentId, MAX_ID);
                    accs[n].pts = 0;
                    accs[n].cred = 0;
                    found = n;
                    n++;
                }
                accs[found].pts += gp * c.credit;
                accs[found].cred += c.credit;
            }
        }
    }
    fclose(fp);
    // simple bubble sort by GPA desc
    for (int i = 0; i < n; i++)
        for (int j = 0; j + 1 < n; j++)
        {
            float g1 = (accs[j].cred > 0) ? accs[j].pts / accs[j].cred : 0;
            float g2 = (accs[j + 1].cred > 0) ? accs[j + 1].pts / accs[j + 1].cred : 0;
            if (g2 > g1)
            {
                Acc t = accs[j];
                accs[j] = accs[j + 1];
                accs[j + 1] = t;
            }
        }
    printf("\n-- Term GPA Leaderboard: %s --\n", term);
    for (int i = 0; i < n; i++)
    {
        float gpa = (accs[i].cred > 0) ? accs[i].pts / accs[i].cred : 0;
        Student s;
        if (file_find_first(FILE_STUD, sizeof(Student), pred_student_by_id, accs[i].sid, &s) >= 0)
        {
            printf("%2d) %-12s %-24s GPA: %.2f (%.1f cr)\n", i + 1, s.id, s.name, gpa, accs[i].cred);
        }
        else
        {
            printf("%2d) %-12s GPA: %.2f (%.1f cr)\n", i + 1, accs[i].sid, gpa, accs[i].cred);
        }
    }
}

/* ======== USERS / AUTH ======== */
void add_user(const char *username, Role role, const char *refId, const char *pass)
{
    if (file_find_first(FILE_USER, sizeof(User), pred_user_by_username, username, NULL) >= 0)
        return;
    User u = {0};
    strncpy(u.username, username, MAX_USER);
    u.role = role;
    if (refId)
        strncpy(u.refId, refId, MAX_ID);
    obfuscate(pass, u.pass_obf);
    file_append(FILE_USER, sizeof(User), &u);
}

void bootstrap_if_empty()
{
    if (file_count_records(FILE_USER, sizeof(User)) == 0)
    {
        // demo data
        Student s1 = {"02124100034", "Sabbir Ahmed", "EEE", 241, "allexsabbir117@gmail.com"};
        Student s2 = {"02124100001", "Afsana Mim", "CSE", 231, "mim@example.com"};
        Faculty f1 = {"FAC-EEE-001", "Dr. Rezwan Khan", "EEE", "rezwan.khan@uiu.ac.bd"};
        Faculty f2 = {"FAC-CSE-002", "Dr. John Doe", "CSE", "john.doe@uiu.ac.bd"};
        Course c1 = {"EEE-2101", "Circuits I", 3.0f, "EEE", "FAC-EEE-001"};
        Course c2 = {"CSE-1101", "Intro to Programming", 3.0f, "CSE", "FAC-CSE-002"};

        file_append(FILE_STUD, sizeof(Student), &s1);
        file_append(FILE_STUD, sizeof(Student), &s2);
        file_append(FILE_FAC, sizeof(Faculty), &f1);
        file_append(FILE_FAC, sizeof(Faculty), &f2);
        file_append(FILE_COURSE, sizeof(Course), &c1);
        file_append(FILE_COURSE, sizeof(Course), &c2);

        Enrollment e1 = {"02124100034", "EEE-2101", "Fall-2025", "A"};
        Enrollment e2 = {"02124100034", "CSE-1101", "Fall-2025", "B+"};
        Enrollment e3 = {"02124100001", "CSE-1101", "Fall-2025", "A-"};
        file_append(FILE_ENR, sizeof(Enrollment), &e1);
        file_append(FILE_ENR, sizeof(Enrollment), &e2);
        file_append(FILE_ENR, sizeof(Enrollment), &e3);

        // Users
        add_user("admin", ROLE_ADMIN, "", "admin123");
        add_user("rezwan", ROLE_FACULTY, "FAC-EEE-001", "teacher123");
        add_user("john", ROLE_FACULTY, "FAC-CSE-002", "teacher123");
        add_user("sabbir", ROLE_STUDENT, "02124100034", "student123");
        add_user("mim", ROLE_STUDENT, "02124100001", "student123");

        printf("Initialized with demo data.\nDefault logins -> admin/admin123, rezwan/teacher123, sabbir/student123\n\n");
    }
}

typedef struct
{
    User user;
    int logged;
} Session;

Session login()
{
    Session s;
    memset(&s, 0, sizeof(s));
    char uname[MAX_USER], pass[MAX_PASS];
    read_line("Username: ", uname, sizeof(uname));
    read_line("Password: ", pass, sizeof(pass));
    User u;
    if (file_find_first(FILE_USER, sizeof(User), pred_user_by_username, uname, &u) >= 0)
    {
        if (verify_pass(u.pass_obf, pass))
        {
            s.user = u;
            s.logged = 1;
            return s;
        }
    }
    printf("Invalid credentials.\n");
    return s;
}

/* ======== MENUS ======== */
void menu_admin();
void menu_faculty(const User *u);
void menu_student(const User *u);

void menu_admin()
{
    while (1)
    {
        printf("\n==== ADMIN MENU ====\n");
        printf("1. Add Student\n");
        printf("2. Edit Student\n");
        printf("3. List Students\n");
        printf("4. Add Faculty\n");
        printf("5. List Faculty\n");
        printf("6. Add Course\n");
        printf("7. Assign Instructor to Course\n");
        printf("8. List Courses\n");
        printf("9. Enroll Student in Course\n");
        printf("10. Set/Update Grade\n");
        printf("11. Transcript (by Student ID)\n");
        printf("12. Course Roster (code+term)\n");
        printf("13. Term GPA Leaderboard\n");
        printf("0. Logout\n");
        int ch = read_int("Choose: ");
        if (ch == 0)
            break;
        switch (ch)
        {
        case 1:
            add_student();
            break;
        case 2:
            edit_student();
            break;
        case 3:
            list_students();
            break;
        case 4:
            add_faculty();
            break;
        case 5:
            list_faculty();
            break;
        case 6:
            add_course();
            break;
        case 7:
            assign_instructor();
            break;
        case 8:
            list_courses();
            break;
        case 9:
            enroll_student();
            break;
        case 10:
            set_grade();
            break;
        case 11:
        {
            char sid[MAX_ID];
            read_line("Student ID: ", sid, sizeof(sid));
            transcript_for_student(sid);
        }
        break;
        case 12:
        {
            char code[MAX_CODE], term[MAX_TERM];
            read_line("Course code: ", code, sizeof(code));
            read_line("Term: ", term, sizeof(term));
            roster_for_course_term(code, term);
        }
        break;
        case 13:
        {
            char term[MAX_TERM];
            read_line("Term: ", term, sizeof(term));
            gpa_leaderboard(term);
        }
        break;
        default:
            printf("Invalid.\n");
        }
    }
}

void menu_faculty(const User *u)
{
    // faculty id in u->refId
    while (1)
    {
        printf("\n==== FACULTY MENU ====\n");
        printf("1. List My Courses\n");
        printf("2. View Roster for a Course+Term\n");
        printf("3. Enter/Update Grade\n");
        printf("0. Logout\n");
        int ch = read_int("Choose: ");
        if (ch == 0)
            break;
        if (ch == 1)
        {
            OPEN_BIN_READ(FILE_COURSE, fp);
            if (!fp)
            {
                printf("No courses.\n");
                continue;
            }
            Course c;
            int any = 0;
            while (fread(&c, sizeof(Course), 1, fp) == 1)
            {
                if (strcmp(c.instructorId, u->refId) == 0)
                {
                    print_course(&c);
                    any = 1;
                }
            }
            fclose(fp);
            if (!any)
                printf("No assigned courses.\n");
        }
        else if (ch == 2)
        {
            char code[MAX_CODE], term[MAX_TERM];
            read_line("Course code: ", code, sizeof(code));
            read_line("Term: ", term, sizeof(term));
            // Validate the course belongs to faculty
            Course c;
            if (file_find_first(FILE_COURSE, sizeof(Course), pred_course_by_code, code, &c) < 0 || strcmp(c.instructorId, u->refId) != 0)
            {
                printf("You are not the instructor of this course.\n");
                continue;
            }
            roster_for_course_term(code, term);
        }
        else if (ch == 3)
        {
            char code[MAX_CODE], term[MAX_TERM], sid[MAX_ID];
            read_line("Course code: ", code, sizeof(code));
            read_line("Term: ", term, sizeof(term));
            Course c;
            if (file_find_first(FILE_COURSE, sizeof(Course), pred_course_by_code, code, &c) < 0 || strcmp(c.instructorId, u->refId) != 0)
            {
                printf("You are not the instructor of this course.\n");
                continue;
            }
            read_line("Student ID: ", sid, sizeof(sid));
            Enrollment e;
            EnrKey key;
            strncpy(key.sid, sid, MAX_ID);
            strncpy(key.code, code, MAX_CODE);
            strncpy(key.term, term, MAX_TERM);
            long idx = file_find_first(FILE_ENR, sizeof(Enrollment), pred_enr_by_key, &key, &e);
            if (idx < 0)
            {
                printf("Enrollment not found.\n");
                continue;
            }
            char g[3];
            read_line("Grade (A, A-, B+, ..., F): ", g, sizeof(g));
            upper(g);
            if (grade_to_points(g) < 0 && strcmp(g, "NA") != 0)
            {
                printf("Invalid grade.\n");
                continue;
            }
            strncpy(e.grade, g, 2);
            e.grade[2] = 0;
            if (file_write_at(FILE_ENR, sizeof(Enrollment), idx, &e))
                printf("Grade saved.\n");
            else
                printf("Write error.\n");
        }
        else
        {
            printf("Invalid.\n");
        }
    }
}

void menu_student(const User *u)
{
    // student id in u->refId
    while (1)
    {
        printf("\n==== STUDENT MENU ====\n");
        printf("1. View My Profile\n");
        printf("2. View My Transcript\n");
        printf("3. List Available Courses\n");
        printf("0. Logout\n");
        int ch = read_int("Choose: ");
        if (ch == 0)
            break;
        if (ch == 1)
        {
            Student s;
            if (file_find_first(FILE_STUD, sizeof(Student), pred_student_by_id, u->refId, &s) >= 0)
                print_student(&s);
            else
                printf("Profile not found.\n");
        }
        else if (ch == 2)
        {
            transcript_for_student(u->refId);
        }
        else if (ch == 3)
        {
            list_courses();
        }
        else
        {
            printf("Invalid.\n");
        }
    }
}

/* ======== MAIN ======== */
int main()
{
    printf("UIU University Management System (UMS)\n");
    printf("Storage: binary files in current folder\n");
    bootstrap_if_empty();

    while (1)
    {
        Session s = login();
        if (!s.logged)
        {
            pause_enter();
            continue;
        }
        if (s.user.role == ROLE_ADMIN)
            menu_admin();
        else if (s.user.role == ROLE_FACULTY)
            menu_faculty(&s.user);
        else if (s.user.role == ROLE_STUDENT)
            menu_student(&s.user);
        else
            printf("Unknown role.\n");
        printf("Logged out.\n\n");
    }
    return 0;
}

// '''

// mk = r'''CC = gcc
// CFLAGS = -std=c11 -Wall -Wextra -O2

// all: uiu_ums

// uiu_ums: uiu_ums.c
// 	$(CC) $(CFLAGS) -o uiu_ums uiu_ums.c

// clean:
// 	rm -f uiu_ums *.dat
// '''

// readme = r'''UIU University Management System (C Project)
// =========================================

// Quick Start
// -----------
// 1) Compile (Linux/macOS/WSL/MinGW):
//    make
//    # or: gcc -std=c11 -O2 -o uiu_ums uiu_ums.c

// 2) Run:
//    ./uiu_ums

// 3) First Run Demo Accounts (auto-created):
//    - Admin:   username "admin",   password "admin123"
//    - Faculty: username "rezwan",  password "teacher123" (FAC-EEE-001)
//    - Student: username "sabbir",  password "student123" (02124100034)

// 4) Data files (auto-created in working dir):
//    - students.dat, faculty.dat, courses.dat, enrollments.dat, users.dat

// Main Features
// -------------
// - Admin:
//   * Manage Students/Faculty/Courses (add, edit/list)
//   * Assign instructors to courses
//   * Enroll students
//   * Set grades
//   * Reports: transcript, course roster, term GPA leaderboard

// - Faculty:
//   * List my courses
//   * View roster for a course and term
//   * Enter/update grades for enrolled students

// - Student:
//   * View own profile
//   * View transcript and CGPA
//   * List available courses

// Design Notes
// ------------
// - Storage is in simple binary files to keep the code compact.
// - Passwords are only *obfuscated* (XOR + salt). For real systems, replace with a secure hash.
// - Grading scale uses a standard 4.0 system (A to F, with +/-). Adjust in grade_to_points().

// Customization
// -------------
// - Update demo users and seed data in bootstrap_if_empty().
// - Extend with delete operations, better validation, CSV import/export, PDFs, etc.

// Have fun and good luck with your UIU project!
// '''

// with open('/mnt/data/uiu_ums.c','w') as f:
//     f.write(code)

// with open('/mnt/data/Makefile','w') as f:
//     f.write(mk)

// with open('/mnt/data/README_UIU_UMS.txt','w') as f:
//     f.write(readme)

// print("Created files: /mnt/data/uiu_ums.c, /mnt/data/Makefile, /mnt/data/README_UIU_UMS.txt")
