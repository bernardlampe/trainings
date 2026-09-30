// security_models/model_deputy.c --- model-level lab companion (user-space build)
//
// A user-space model of the three gates a privileged service ("deputy") is
// supposed to apply before acting for a client: an attested subject identity
// (the DAC arm), a MAC label check (the SELinux arm), and a single resolved
// object used by both the check and the operation. It compiles in user space
// for source-level demonstration; the auditing reasoning in the lab applies
// unchanged to the real in-kernel (inode_permission + the LSM hook) and
// Android (Binder + SELinux) versions.
//
// Lab exercises live in lab.tex.

#include <stdio.h>
#include <string.h>

#define MAX_LABEL 32
#define MAX_PATH  64
#define NFSO       5

/* one object in the modeled filesystem */
struct object {
    char path[MAX_PATH];
    char label[MAX_LABEL];
    char symlink[MAX_PATH];   /* non-empty: path resolves to this instead */
    unsigned int owner;
    unsigned int mode;        /* 0600-style permission bits */
    char contents[64];
};

static struct object fs[NFSO] = {
    { "/data/data/com.app/x",   "app_data_file",    "",                    10001, 0600, "app secret"    },
    { "/data/data/com.other/x", "app_data_file",    "",                    10002, 0600, "other secret"  },
    /* 0666 rather than the real 0644: keeps the DAC arm out of the way so the
       demo isolates the MAC arm */
    { "/system/etc/secret",    "system_data_file", "",                        0, 0666, "system secret" },
    { "/data/local/tmp/x",     "app_data_file",    "",                    10001, 0666, "scratch"       },
    { "/data/local/tmp/link",  "app_data_file",    "/system/etc/secret",   10001, 0777, ""              },
};

/* the modeled policy: which domain may read/write which target label */
struct rule {
    const char *domain;
    const char *target;
    int may_read;
    int may_write;
};

static const struct rule policy[] = {
    { "untrusted_app", "app_data_file",    1, 1 },
    { "untrusted_app", "system_data_file", 0, 0 },
    { "system_server", "system_data_file", 1, 1 },
};
#define NPOLICY (sizeof(policy) / sizeof(policy[0]))

/* kernel-attested subject: the uid and the SELinux domain are set by the
   kernel (credentials on the task; the domain by the exec-time transition),
   never by the client. */
struct subject {
    unsigned int uid;
    const char *domain;
};

/* the request as it arrives over the modeled IPC channel (Binder parcel) */
struct request {
    unsigned int uid;         /* client-claimed */
    const char *domain;       /* client-claimed */
    const char *path;
    int write;
};

/* path resolution, following one symlink hop (models lookup + follow_link) */
static struct object *lookup(const char *path, int *followed)
{
    for (unsigned int i = 0; i < NFSO; i++) {
        if (strcmp(fs[i].path, path))
            continue;
        if (fs[i].symlink[0]) {
            if (followed)
                *followed = 1;
            return lookup(fs[i].symlink, NULL);
        }
        return &fs[i];
    }
    return NULL;
}

/* the DAC arm, modeled on inode_permission: owner/group/other bits, and the
   root bypass of CAP_DAC_OVERRIDE */
static int dac_allows(const struct subject *s, const struct object *o, int write)
{
    unsigned int bit = write ? 0200 : 0400;

    if (s->uid == 0)
        return 1;
    if (s->uid == o->owner)
        return (o->mode & bit) != 0;
    return (o->mode & (bit >> 6)) != 0;
}

/* the MAC arm, modeled on the SELinux inode hook. BUG 1: it returns on the
   first rule whose *domain* matches and never consults the target's label, so
   every target label inherits the first rule's verdict. */
static int mac_allows(const struct subject *s, const struct object *o, int write)
{
    (void)o;   /* the target label is the part of the tuple never used */

    for (unsigned int i = 0; i < NPOLICY; i++) {
        if (strcmp(policy[i].domain, s->domain))
            continue;
        return write ? policy[i].may_write : policy[i].may_read;
    }
    return 0;
}

/* the check phase: resolve, then apply DAC and MAC */
static int deputy_check(const struct subject *attested, const struct request *req,
                        int *followed)
{
    struct object *o;

    if (followed)
        *followed = 0;
    o = lookup(req->path, followed);
    if (!o)
        return -1;

    /* BUG 2: the subject is built from the client-claimed request fields
       instead of the attested identity. What does the real deputy read? */
    (void)attested;
    struct subject s = { req->uid, req->domain };

    if (!dac_allows(&s, o, req->write))
        return -2;
    if (!mac_allows(&s, o, req->write))
        return -3;
    return 0;
}

/* the use phase: resolve again and act. Returns the label actually acted on,
   or NULL if the path did not resolve. BUG 3: this second resolution means
   the object checked need not be the object used. */
static const char *deputy_use(const struct request *req)
{
    struct object *o = lookup(req->path, NULL);
    if (!o)
        return NULL;

    if (req->write)
        strcpy(o->contents, "by deputy");
    return o->label;
}

/* the window between check and use: a concurrent actor replaces the path */
static void swap_to_symlink(const char *path, const char *target)
{
    for (unsigned int i = 0; i < NFSO; i++)
        if (!strcmp(fs[i].path, path))
            strncpy(fs[i].symlink, target, MAX_PATH - 1);
}

/* ---------------- user-space main: drives the three bug paths ------------ */
int main(void)
{
    struct subject app = { 10001, "untrusted_app" };
    int followed;

    /* honest client: check and use the same object; expected 0 */
    struct request ok = { 10001, "untrusted_app", "/data/data/com.app/x", 1 };
    int c = deputy_check(&app, &ok, &followed);
    const char *used = deputy_use(&ok);
    printf("ok:          check=%d followed=%d\n"
           "        use=%s wrote=\"%s\"\n",
           c, followed, used, fs[0].contents);

    /* BUG 2: the client claims uid 0; expected -2 (DAC denies) */
    struct request forged = { 0, "untrusted_app", "/data/data/com.other/x", 1 };
    c = deputy_check(&app, &forged, NULL);
    used = c == 0 ? deputy_use(&forged) : NULL;
    printf("forged uid:  check=%d\n"
           "        use=%s wrote=\"%s\"\n",
           c, used ? used : "denied", fs[1].contents);

    /* BUG 1: untrusted_app has no write to the target's label; expected -3 */
    struct request wrong = { 10001, "untrusted_app", "/system/etc/secret", 1 };
    c = deputy_check(&app, &wrong, NULL);
    used = c == 0 ? deputy_use(&wrong) : NULL;
    printf("wrong label: check=%d\n"
           "        use=%s wrote=\"%s\"\n",
           c, used ? used : "denied", fs[2].contents);

    /* BUG 3: the check resolves the real file, then the path is swapped;
       expected the use to act on the object the check approved */
    struct request swap = { 10001, "untrusted_app", "/data/local/tmp/x", 1 };
    followed = 0;
    c = deputy_check(&app, &swap, &followed);
    swap_to_symlink("/data/local/tmp/x", "/system/etc/secret");
    used = deputy_use(&swap);
    printf("swap:        check=%d followed=%d\n"
           "        use=%s wrote=\"%s\"\n",
           c, followed, used ? used : "denied", fs[2].contents);

    return 0;
}
