#include "config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <fcntl.h>

#ifndef _WIN32
#define _POSIX_PTHREAD_SEMANTICS
#include <pwd.h>
#endif

#define LIBSSH_STATIC
#include <libssh/priv.h>

#include "misc.c"
#include "torture.h"
#include "error.c"

#ifdef _WIN32
#include <netioapi.h>
#else
#include <net/if.h>
#endif

#define TORTURE_TEST_DIR "/usr/local/bin/truc/much/.."
#define TORTURE_IPV6_LOCAL_LINK "fe80::98e1:82ff:fe8d:28b3%%%s"

const char template[] = "temp_dir_XXXXXX";

static int setup(void **state)
{
    ssh_session session = ssh_new();
    *state = session;

    return 0;
}

static int teardown(void **state)
{
    ssh_free(*state);

    return 0;
}

static void torture_get_user_home_dir(void **state) {
#ifndef _WIN32
    struct passwd *pwd = getpwuid(getuid());
#endif /* _WIN32 */
    char *user;

    (void) state;

    user = ssh_get_user_home_dir();
    assert_non_null(user);
#ifndef _WIN32
    assert_string_equal(user, pwd->pw_dir);
#endif /* _WIN32 */

    SAFE_FREE(user);
}

static void torture_basename(void **state) {
    char *path;

    (void) state;

    path=ssh_basename(TORTURE_TEST_DIR "/test");
    assert_non_null(path);
    assert_string_equal(path, "test");
    SAFE_FREE(path);
    path=ssh_basename(TORTURE_TEST_DIR "/test/");
    assert_non_null(path);
    assert_string_equal(path, "test");
    SAFE_FREE(path);
}

static void torture_dirname(void **state) {
    char *path;

    (void) state;

    path=ssh_dirname(TORTURE_TEST_DIR "/test");
    assert_non_null(path);
    assert_string_equal(path, TORTURE_TEST_DIR );
    SAFE_FREE(path);
    path=ssh_dirname(TORTURE_TEST_DIR "/test/");
    assert_non_null(path);
    assert_string_equal(path, TORTURE_TEST_DIR);
    SAFE_FREE(path);
}

static void torture_ntohll(void **state) {
    uint64_t value = 0x0123456789abcdef;
    uint32_t sample = 1;
    unsigned char *ptr = (unsigned char *) &sample;
    uint64_t check;

    (void) state;

    if (ptr[0] == 1){
        /* we're in little endian */
        check = 0xefcdab8967452301;
    } else {
        /* big endian */
        check = value;
    }
    value = ntohll(value);
    assert_true(value == check);
}

#ifdef _WIN32

static void torture_path_expand_tilde_win(void **state) {
    char *d;

    (void) state;

    d = ssh_path_expand_tilde("~\\.ssh");
    assert_non_null(d);
    print_message("Expanded path: %s\n", d);
    free(d);

    d = ssh_path_expand_tilde("/guru/meditation");
    assert_string_equal(d, "/guru/meditation");
    free(d);
}

#else /* _WIN32 */

static void torture_path_expand_tilde_unix(void **state) {
    char h[256] = {0};
    char *d = NULL;
    char *user = NULL;
    char *home = NULL;
    struct passwd *pw = NULL;

    (void) state;

    pw = getpwuid(getuid());
    assert_non_null(pw);

    user = pw->pw_name;
    assert_non_null(user);
    home = pw->pw_dir;
    assert_non_null(home);

    snprintf(h, 256 - 1, "%s/.ssh", home);

    d = ssh_path_expand_tilde("~/.ssh");
    assert_non_null(d);
    assert_string_equal(d, h);
    free(d);

    d = ssh_path_expand_tilde("/guru/meditation");
    assert_non_null(d);
    assert_string_equal(d, "/guru/meditation");
    free(d);

    snprintf(h, 256 - 1, "~%s/.ssh", user);
    d = ssh_path_expand_tilde(h);
    assert_non_null(d);

    snprintf(h, 256 - 1, "%s/.ssh", home);
    assert_string_equal(d, h);
    free(d);
}

#endif /* _WIN32 */

static void torture_path_expand_escape(void **state) {
    ssh_session session = *state;
    const char *s = "%d/%h/%p/by/%r";
    char *e;

    session->opts.sshdir = strdup("guru");
    session->opts.host = strdup("meditation");
    session->opts.port = 0;
    session->opts.username = strdup("root");

    e = ssh_path_expand_escape(session, s);
    assert_non_null(e);
    assert_string_equal(e, "guru/meditation/22/by/root");
    ssh_string_free_char(e);

    session->opts.port = 222;

    e = ssh_path_expand_escape(session, s);
    assert_non_null(e);
    assert_string_equal(e, "guru/meditation/222/by/root");
    ssh_string_free_char(e);
}

static void torture_path_expand_known_hosts(void **state) {
    ssh_session session = *state;
    char *tmp;

    session->opts.sshdir = strdup("/home/guru/.ssh");

    tmp = ssh_path_expand_escape(session, "%d/known_hosts");
    assert_non_null(tmp);
    assert_string_equal(tmp, "/home/guru/.ssh/known_hosts");
    free(tmp);
}

static void torture_path_expand_percent(void **state) {
    ssh_session session = *state;
    char *tmp;

    session->opts.sshdir = strdup("/home/guru/.ssh");

    tmp = ssh_path_expand_escape(session, "%d/config%%1");
    assert_non_null(tmp);
    assert_string_equal(tmp, "/home/guru/.ssh/config%1");
    free(tmp);
}

static void torture_timeout_elapsed(void **state){
    struct ssh_timestamp ts;
    (void) state;
    ssh_timestamp_init(&ts);
    usleep(30000);

    assert_true(ssh_timeout_elapsed(&ts,25));
    assert_false(ssh_timeout_elapsed(&ts,30000));
    assert_false(ssh_timeout_elapsed(&ts,300));
    assert_true(ssh_timeout_elapsed(&ts,0));
    assert_false(ssh_timeout_elapsed(&ts,-1));
}

static void torture_timeout_update(void **state){
    struct ssh_timestamp ts;
    (void) state;
    ssh_timestamp_init(&ts);
    usleep(50000);
    assert_int_equal(ssh_timeout_update(&ts,25), 0);
    assert_in_range(ssh_timeout_update(&ts,30000),29000,29960);
    assert_in_range(ssh_timeout_update(&ts,500),1,460);
    assert_int_equal(ssh_timeout_update(&ts,0),0);
    assert_int_equal(ssh_timeout_update(&ts,-1),-1);
}

static void torture_ssh_analyze_banner(void **state) {
    int rc = 0;
    ssh_session session = NULL;
    (void) state;

#define reset_banner_test() \
    do {                           \
        rc = 0;                    \
        ssh_free(session);         \
        session = ssh_new();       \
        assert_non_null(session);  \
    } while (0)

#define assert_banner_rejected(is_server) \
    do {                                                            \
        rc = ssh_analyze_banner(session, is_server);  \
        assert_int_not_equal(0, rc);                                \
    } while (0);

#define assert_client_banner_rejected(banner) \
    do {                                         \
        reset_banner_test();                     \
        session->clientbanner = strdup(banner);  \
        assert_non_null(session->clientbanner);  \
        assert_banner_rejected(1 /*server*/);    \
        SAFE_FREE(session->clientbanner);        \
    } while (0)

#define assert_server_banner_rejected(banner) \
    do {                                         \
        reset_banner_test();                     \
        session->serverbanner = strdup(banner);  \
        assert_non_null(session->serverbanner);  \
        assert_banner_rejected(0 /*client*/);    \
        SAFE_FREE(session->serverbanner);        \
    } while (0)

#define assert_banner_accepted(is_server) \
    do {                                                            \
        rc = ssh_analyze_banner(session, is_server);  \
        assert_int_equal(0, rc);                                    \
    } while (0)

#define assert_client_banner_accepted(banner)          \
    do {                                               \
        reset_banner_test();                           \
        session->clientbanner = strdup(banner);        \
        assert_non_null(session->clientbanner);        \
        assert_banner_accepted(1 /*server*/);          \
        SAFE_FREE(session->clientbanner);              \
    } while (0)

#define assert_server_banner_accepted(banner)          \
    do {                                               \
        reset_banner_test();                           \
        session->serverbanner = strdup(banner);        \
        assert_non_null(session->serverbanner);        \
        assert_banner_accepted(0 /*client*/);          \
        SAFE_FREE(session->serverbanner);              \
    } while (0)

    /* no banner is set */
    reset_banner_test();
    assert_banner_rejected(0 /*client*/);
    reset_banner_test();
    assert_banner_rejected(1 /*server*/);

    /* banner is too short */
    assert_client_banner_rejected("abc");
    assert_server_banner_rejected("abc");

    /* banner doesn't start "SSH-" */
    assert_client_banner_rejected("abc-2.0");
    assert_server_banner_rejected("abc-2.0");

    /* SSH v1 */
    assert_client_banner_rejected("SSH-1.0");
    assert_server_banner_rejected("SSH-1.0");

    /* SSH v1.9 gets counted as both v1 and v2 */
    assert_client_banner_accepted("SSH-1.9");
    assert_server_banner_accepted("SSH-1.9");

    /* SSH v2 */
    assert_client_banner_accepted("SSH-2.0");
    assert_server_banner_accepted("SSH-2.0");

    /* OpenSSH banners: too short to extract major and minor versions */
    assert_client_banner_accepted("SSH-2.0-OpenSSH");
    assert_int_equal(0, session->openssh);
    assert_server_banner_accepted("SSH-2.0-OpenSSH");
    assert_int_equal(0, session->openssh);


    /* OpenSSH banners: big enough to extract major and minor versions */
    assert_client_banner_accepted("SSH-2.0-OpenSSH_5.9p1");
    assert_int_equal(SSH_VERSION_INT(5, 9, 0), session->openssh);
    assert_server_banner_accepted("SSH-2.0-OpenSSH_5.9p1");
    assert_int_equal(SSH_VERSION_INT(5, 9, 0), session->openssh);

    assert_client_banner_accepted("SSH-2.0-OpenSSH_1.99");
    assert_int_equal(SSH_VERSION_INT(1, 99, 0), session->openssh);
    assert_server_banner_accepted("SSH-2.0-OpenSSH_1.99");
    assert_int_equal(SSH_VERSION_INT(1, 99, 0), session->openssh);

    /* OpenSSH banners: major, minor version limits result in zero */
    assert_client_banner_accepted("SSH-2.0-OpenSSH_0.99p1");
    assert_int_equal(0, session->openssh);
    assert_server_banner_accepted("SSH-2.0-OpenSSH_0.99p1");
    assert_int_equal(0, session->openssh);
    assert_client_banner_accepted("SSH-2.0-OpenSSH_1.101p1");
    assert_int_equal(0, session->openssh);
    assert_server_banner_accepted("SSH-2.0-OpenSSH_1.101p1");
    assert_int_equal(0, session->openssh);

    /* OpenSSH banners: bogus major results in zero */
    assert_client_banner_accepted("SSH-2.0-OpenSSH_X.9p1");
    assert_int_equal(0, session->openssh);
    assert_server_banner_accepted("SSH-2.0-OpenSSH_X.9p1");
    assert_int_equal(0, session->openssh);

    /* OpenSSH banners: bogus minor results in zero */
    assert_server_banner_accepted("SSH-2.0-OpenSSH_5.Yp1");
    assert_int_equal(0, session->openssh);
    assert_client_banner_accepted("SSH-2.0-OpenSSH_5.Yp1");
    assert_int_equal(0, session->openssh);

    /* OpenSSH banners: ssh-keyscan(1) */
    assert_client_banner_accepted("SSH-2.0-OpenSSH-keyscan");
    assert_int_equal(0, session->openssh);
    assert_server_banner_accepted("SSH-2.0-OpenSSH-keyscan");
    assert_int_equal(0, session->openssh);

    /* OpenSSH banners: Double digit in major version */
    assert_server_banner_accepted("SSH-2.0-OpenSSH_10.0p1");
    assert_int_equal(SSH_VERSION_INT(10, 0, 0), session->openssh);

    ssh_free(session);
}

static void torture_ssh_dir_writeable(UNUSED_PARAM(void **state))
{
    char *tmp_dir = NULL;
    int rc = 0;
    FILE *file = NULL;
    char buffer[256];

    tmp_dir = torture_make_temp_dir(template);
    assert_non_null(tmp_dir);

    rc = ssh_dir_writeable(tmp_dir);
    assert_int_equal(rc, 1);

    /* Create a file */
    snprintf(buffer, sizeof(buffer), "%s/a", tmp_dir);

    file = fopen(buffer, "w");
    assert_non_null(file);

    fprintf(file, "Hello world!\n");
    fclose(file);

    /* Negative test for checking a normal file */
    rc = ssh_dir_writeable(buffer);
    assert_int_equal(rc, 0);

    /* Negative test for non existent file */
    snprintf(buffer, sizeof(buffer), "%s/b", tmp_dir);
    rc = ssh_dir_writeable(buffer);
    assert_int_equal(rc, 0);

#ifndef _WIN32
    /* Negative test for directory without write permission */
    rc = ssh_mkdir(buffer, 0400);
    assert_return_code(rc, errno);

    rc = ssh_dir_writeable(buffer);
    assert_int_equal(rc, 0);
#endif

    torture_rmdirs(tmp_dir);

    SAFE_FREE(tmp_dir);
}

static void torture_ssh_mkdirs(UNUSED_PARAM(void **state))
{
    char *tmp_dir = NULL;
    char *cwd = NULL;
    char buffer[256];

    ssize_t count = 0;

    int rc;

    /* Get current working directory */
    cwd = torture_get_current_working_dir();
    assert_non_null(cwd);

    /* Create a base disposable directory */
    tmp_dir = torture_make_temp_dir(template);
    assert_non_null(tmp_dir);

    /* Create a single directory */
    count = snprintf(buffer, sizeof(buffer), "%s/a", tmp_dir);
    assert_return_code(count, errno);

    rc = ssh_mkdirs(buffer, 0700);
    assert_return_code(rc, errno);

    rc = ssh_dir_writeable(buffer);
    assert_int_equal(rc, 1);

    /* Create directories recursively */
    count = snprintf(buffer, sizeof(buffer), "%s/b/c/d", tmp_dir);
    assert_return_code(count, errno);

    rc = ssh_mkdirs(buffer, 0700);
    assert_return_code(rc, errno);

    rc = ssh_dir_writeable(buffer);
    assert_int_equal(rc, 1);

    /* Change directory */
    rc = torture_change_dir(tmp_dir);
    assert_return_code(rc, errno);

    /* Create single local directory */
    rc = ssh_mkdirs("e", 0700);
    assert_return_code(rc, errno);

    rc = ssh_dir_writeable("e");
    assert_int_equal(rc, 1);

    /* Create local directories recursively */
    rc = ssh_mkdirs("f/g/h", 0700);
    assert_return_code(rc, errno);

    rc = ssh_dir_writeable("f/g/h");
    assert_int_equal(rc, 1);

    /* Negative test for creating "." directory */
    rc = ssh_mkdirs(".", 0700);
    assert_int_equal(rc, -1);
    assert_int_equal(errno, EINVAL);

    /* Negative test for creating "/" directory */
    rc = ssh_mkdirs("/", 0700);
    assert_int_equal(rc, -1);
    assert_int_equal(errno, EINVAL);

    /* Negative test for creating "" directory */
    rc = ssh_mkdirs("", 0700);
    assert_int_equal(rc, -1);
    assert_int_equal(errno, EINVAL);

    /* Negative test for creating NULL directory */
    rc = ssh_mkdirs(NULL, 0700);
    assert_int_equal(rc, -1);
    assert_int_equal(errno, EINVAL);

    /* Negative test for creating existing directory */
    rc = ssh_mkdirs("a", 0700);
    assert_int_equal(rc, -1);
    assert_int_equal(errno, EEXIST);

    /* Return to original directory */
    rc = torture_change_dir(cwd);
    assert_return_code(rc, errno);

    /* Cleanup */
    torture_rmdirs(tmp_dir);

    SAFE_FREE(tmp_dir);
    SAFE_FREE(cwd);
}

static void torture_ssh_quote_file_name(UNUSED_PARAM(void **state))
{
    char buffer[2048];
    int rc;

    /* Only ordinary chars */
    rc = ssh_quote_file_name("a b", buffer, 2048);
    assert_int_equal(rc, 5);
    assert_string_equal(buffer, "'a b'");

    /* Single quote in file name */
    rc = ssh_quote_file_name("a'b", buffer, 2048);
    assert_int_equal(rc, 9);
    assert_string_equal(buffer, "'a'\"'\"'b'");

    /* Exclamation in file name */
    rc = ssh_quote_file_name("a!b", buffer, 2048);
    assert_int_equal(rc, 8);
    assert_string_equal(buffer, "'a'\\!'b'");

    /* All together */
    rc = ssh_quote_file_name("'a!b'", buffer, 2048);
    assert_int_equal(rc, 14);
    assert_string_equal(buffer, "\"'\"'a'\\!'b'\"'\"");

    rc = ssh_quote_file_name("a'!b", buffer, 2048);
    assert_int_equal(rc, 11);
    assert_string_equal(buffer, "'a'\"'\"\\!'b'");

    rc = ssh_quote_file_name("a'$b", buffer, 2048);
    assert_int_equal(rc, 10);
    assert_string_equal(buffer, "'a'\"'\"'$b'");

    rc = ssh_quote_file_name("a'`b", buffer, 2048);
    assert_int_equal(rc, 10);
    assert_string_equal(buffer, "'a'\"'\"'`b'");


    rc = ssh_quote_file_name(" ", buffer, 2048);
    assert_int_equal(rc, 3);
    assert_string_equal(buffer, "' '");

    rc = ssh_quote_file_name("  ", buffer, 2048);
    assert_int_equal(rc, 4);
    assert_string_equal(buffer, "'  '");


    rc = ssh_quote_file_name("\r", buffer, 2048);
    assert_int_equal(rc, 3);
    assert_string_equal(buffer, "'\r'");

    rc = ssh_quote_file_name("\n", buffer, 2048);
    assert_int_equal(rc, 3);
    assert_string_equal(buffer, "'\n'");

    rc = ssh_quote_file_name("\r\n", buffer, 2048);
    assert_int_equal(rc, 4);
    assert_string_equal(buffer, "'\r\n'");


    rc = ssh_quote_file_name("\\r", buffer, 2048);
    assert_int_equal(rc, 4);
    assert_string_equal(buffer, "'\\r'");

    rc = ssh_quote_file_name("\\n", buffer, 2048);
    assert_int_equal(rc, 4);
    assert_string_equal(buffer, "'\\n'");

    rc = ssh_quote_file_name("\\r\\n", buffer, 2048);
    assert_int_equal(rc, 6);
    assert_string_equal(buffer, "'\\r\\n'");


    rc = ssh_quote_file_name("\t", buffer, 2048);
    assert_int_equal(rc, 3);
    assert_string_equal(buffer, "'\t'");

    rc = ssh_quote_file_name("\v", buffer, 2048);
    assert_int_equal(rc, 3);
    assert_string_equal(buffer, "'\v'");

    rc = ssh_quote_file_name("\t\v", buffer, 2048);
    assert_int_equal(rc, 4);
    assert_string_equal(buffer, "'\t\v'");


    rc = ssh_quote_file_name("'", buffer, 2048);
    assert_int_equal(rc, 3);
    assert_string_equal(buffer, "\"'\"");

    rc = ssh_quote_file_name("''", buffer, 2048);
    assert_int_equal(rc, 4);
    assert_string_equal(buffer, "\"''\"");


    rc = ssh_quote_file_name("\"", buffer, 2048);
    assert_int_equal(rc, 3);
    assert_string_equal(buffer, "'\"'");

    rc = ssh_quote_file_name("\"\"", buffer, 2048);
    assert_int_equal(rc, 4);
    assert_string_equal(buffer, "'\"\"'");

    rc = ssh_quote_file_name("'\"", buffer, 2048);
    assert_int_equal(rc, 6);
    assert_string_equal(buffer, "\"'\"'\"'");

    rc = ssh_quote_file_name("\"'", buffer, 2048);
    assert_int_equal(rc, 6);
    assert_string_equal(buffer, "'\"'\"'\"");


    /* Worst case */
    rc = ssh_quote_file_name("a'b'", buffer, 3 * 4 + 1);
    assert_int_equal(rc, 12);
    assert_string_equal(buffer, "'a'\"'\"'b'\"'\"");

    /* Negative tests */

    /* NULL params */
    rc = ssh_quote_file_name(NULL, buffer, 3 * 4 + 1);
    assert_int_equal(rc, SSH_ERROR);

    /* NULL params */
    rc = ssh_quote_file_name("a b", NULL, 3 * 4 + 1);
    assert_int_equal(rc, SSH_ERROR);

    /* Small buffer size */
    rc = ssh_quote_file_name("a b", buffer, 0);
    assert_int_equal(rc, SSH_ERROR);

    /* Worst case and small buffer size */
    rc = ssh_quote_file_name("a'b'", buffer, 3 * 4);
    assert_int_equal(rc, SSH_ERROR);
}

static void torture_ssh_newline_vis(UNUSED_PARAM(void **state))
{
    int rc;
    char buffer[1024];

    rc = ssh_newline_vis("\n", buffer, 1024);
    assert_int_equal(rc, 2);
    assert_string_equal(buffer, "\\n");

    rc = ssh_newline_vis("\n\n\n\n", buffer, 1024);
    assert_int_equal(rc, 8);
    assert_string_equal(buffer, "\\n\\n\\n\\n");

    rc = ssh_newline_vis("a\nb\n", buffer, 1024);
    assert_int_equal(rc, 6);
    assert_string_equal(buffer, "a\\nb\\n");
}

static void torture_ssh_strreplace(void **state)
{
    char test_string1[] = "this;is;a;test";
    char test_string2[] = "test;is;a;this";
    char test_string3[] = "this;test;is;a";
    char *replaced_string = NULL;

    (void) state;

    /* pattern and replacement are of the same size */
    replaced_string = ssh_strreplace(test_string1, "test", "kiwi");
    assert_string_equal(replaced_string, "this;is;a;kiwi");
    free(replaced_string);

    replaced_string = ssh_strreplace(test_string2, "test", "kiwi");
    assert_string_equal(replaced_string, "kiwi;is;a;this");
    free(replaced_string);

    replaced_string = ssh_strreplace(test_string3, "test", "kiwi");
    assert_string_equal(replaced_string, "this;kiwi;is;a");
    free(replaced_string);

    /* replacement is greater than pattern */
    replaced_string = ssh_strreplace(test_string1, "test", "an;apple");
    assert_string_equal(replaced_string, "this;is;a;an;apple");
    free(replaced_string);

    replaced_string = ssh_strreplace(test_string2, "test", "an;apple");
    assert_string_equal(replaced_string, "an;apple;is;a;this");
    free(replaced_string);

    replaced_string = ssh_strreplace(test_string3, "test", "an;apple");
    assert_string_equal(replaced_string, "this;an;apple;is;a");
    free(replaced_string);

    /* replacement is less than pattern */
    replaced_string = ssh_strreplace(test_string1, "test", "an");
    assert_string_equal(replaced_string, "this;is;a;an");
    free(replaced_string);

    replaced_string = ssh_strreplace(test_string2, "test", "an");
    assert_string_equal(replaced_string, "an;is;a;this");
    free(replaced_string);

    replaced_string = ssh_strreplace(test_string3, "test", "an");
    assert_string_equal(replaced_string, "this;an;is;a");
    free(replaced_string);

    /* pattern not found in teststring */
    replaced_string = ssh_strreplace(test_string1, "banana", "an");
    assert_string_equal(replaced_string, test_string1);
    free(replaced_string);

    /* pattern is NULL */
    replaced_string = ssh_strreplace(test_string1, NULL , "an");
    assert_string_equal(replaced_string, test_string1);
    free(replaced_string);

    /* replacement is NULL */
    replaced_string = ssh_strreplace(test_string1, "test", NULL);
    assert_string_equal(replaced_string, test_string1);
    free(replaced_string);

    /* src is NULL */
    replaced_string = ssh_strreplace(NULL, "test", "kiwi");
    assert_null(replaced_string);
}

static void torture_ssh_strerror(void **state)
{
    char buf[1024];
    size_t bufflen = sizeof(buf);
    char *out = NULL;

    (void) state;

    out = ssh_strerror(ENOENT, buf, 1); /* too short */
    assert_string_equal(out, "\0");

    out = ssh_strerror(256, buf, bufflen); /* unknown error code */
    /* This error is always different:
     * Freebd: "Unknown error: 256"
     * MinGW/Win: "Unknown error"
     * Linux/glibc: "Unknown error 256"
     * Alpine/musl: "No error information"
     */
    assert_non_null(out);

    out = ssh_strerror(ENOMEM, buf, bufflen);
    /* This actually differs too for glibc/musl:
     * musl: "Out of memory"
     * everything else: "Cannot allocate memory"
     */
    assert_non_null(out);
}

static void torture_ssh_readn(void **state)
{
    char *write_buf = NULL, *read_buf = NULL, *file_path = NULL;
    size_t data_len = 10 * 1024 * 1024;
    size_t read_buf_size = data_len + 1024;
    size_t i, total_bytes_written = 0;

    const char *file_template = "libssh_torture_ssh_readn_test_XXXXXX";

    off_t off;
    ssize_t bytes_read, bytes_written;
    int fd, rc, flags;

    (void)state;

    write_buf = malloc(data_len);
    assert_non_null(write_buf);

    /* Fill the write buffer with random data */
    for (i = 0; i < data_len; ++i) {
        rc = rand();
        write_buf[i] = (char)(rc & 0xff);
    }

    /*
     * The read buffer's size is intentionally kept larger than data_len.
     *
     * This is done so that we are able to test the scenario when the user
     * requests ssh_readn() to read more bytes than the number of bytes present
     * in the file.
     *
     * If the read buffer's size is kept same as data_len and if the test
     * requests ssh_readn() to read more than data_len bytes starting from file
     * offset 0, it will lead to a valgrind failure as in this case, ssh_readn()
     * will pass an unallocated memory address in the last call it makes to
     * read().
     */
    read_buf = malloc(read_buf_size);
    assert_non_null(read_buf);

    file_path = torture_create_temp_file(file_template);
    assert_non_null(file_path);

    /* Open a file for reading and writing */
    flags = O_RDWR;
#ifdef _WIN32
    flags |= O_BINARY;
#endif

    fd = open(file_path, flags, 0);
    assert_int_not_equal(fd, -1);

    /* Write the data present in the write buffer to the file */
    do {
        bytes_written = write(fd,
                              write_buf + total_bytes_written,
                              data_len - total_bytes_written);

        if (bytes_written == -1 && errno == EINTR) {
            continue;
        }

        assert_int_not_equal(bytes_written, -1);
        total_bytes_written += bytes_written;
    } while (total_bytes_written < data_len);

    /* Seek to the start of the file */
    off = lseek(fd, 0, SEEK_SET);
    assert_int_not_equal(off, -1);

    bytes_read = ssh_readn(fd, read_buf, data_len);
    assert_int_equal(bytes_read, data_len);

    /*
     * Ensure that the data stored in the read buffer is same as the data
     * present in the file and not some garbage.
     */
    assert_memory_equal(read_buf, write_buf, data_len);

    /*
     * Ensure that the file offset is on EOF and requesting to read more leads
     * to 0 bytes getting read.
     */
    off = lseek(fd, 0, SEEK_CUR);
    assert_int_equal(off, data_len);

    bytes_read = ssh_readn(fd, read_buf, data_len);
    assert_int_equal(bytes_read, 0);

    /* Try to read more bytes than what are present in the file */
    off = lseek(fd, 0, SEEK_SET);
    assert_int_not_equal(off, -1);

    bytes_read = ssh_readn(fd, read_buf, read_buf_size);
    assert_int_equal(bytes_read, data_len);

    /*
     * Ensure that the data stored in the read buffer is same as the data
     * present in the file and not some garbage.
     */
    assert_memory_equal(read_buf, write_buf, data_len);

    /* Negative tests start */
    bytes_read = ssh_readn(-2, read_buf, data_len);
    assert_int_equal(bytes_read, -1);

    bytes_read = ssh_readn(fd, NULL, data_len);
    assert_int_equal(bytes_read, -1);

    bytes_read = ssh_readn(fd, read_buf, 0);
    assert_int_equal(bytes_read, -1);

    /* Clean up */
    rc = close(fd);
    assert_int_equal(rc, 0);

    rc = unlink(file_path);
    assert_int_equal(rc, 0);

    free(file_path);
    free(read_buf);
    free(write_buf);
}

static void torture_ssh_writen(void **state)
{
    char *write_buf = NULL, *read_buf = NULL, *file_path = NULL;
    const char *file_template = "libssh_torture_ssh_writen_test_XXXXXX";

    size_t data_len = 10 * 1024 * 1024;
    size_t i, total_bytes_read = 0;
    ssize_t bytes_written, bytes_read;
    off_t off;
    int rc, fd, flags;

    (void)state;

    write_buf = malloc(data_len);
    assert_non_null(write_buf);

    /* Fill the write buffer with random data */
    for (i = 0; i < data_len; ++i) {
        rc = rand();
        write_buf[i] = (char)(rc & 0xff);
    }

    read_buf = malloc(data_len);
    assert_non_null(read_buf);

    file_path = torture_create_temp_file(file_template);
    assert_non_null(file_path);

    /* Open a file for reading and writing */
    flags = O_RDWR;
#ifdef _WIN32
    flags |= O_BINARY;
#endif

    fd = open(file_path, flags, 0);
    assert_int_not_equal(fd, -1);

    /* Write the data present in the write buffer to the file */
    bytes_written = ssh_writen(fd, write_buf, data_len);
    assert_int_equal(bytes_written, data_len);

    /*
     * Ensure that the file offset is incremented by the number of bytes
     * written.
     */
    off = lseek(fd, 0, SEEK_CUR);
    assert_int_equal(off, data_len);

    /*
     * Ensure that the data present in the write buffer has been written to the
     * file and not some garbage.
     */
    off = lseek(fd, 0, SEEK_SET);
    assert_int_not_equal(off, -1);

    do {
        bytes_read = read(fd,
                          read_buf + total_bytes_read,
                          data_len - total_bytes_read);

        if (bytes_read == -1 && errno == EINTR) {
            continue;
        }

        assert_int_not_equal(bytes_read, -1);
        assert_int_not_equal(bytes_read, 0);

        total_bytes_read += bytes_read;
    } while (total_bytes_read < data_len);

    assert_memory_equal(write_buf, read_buf, data_len);

    /* Negative tests start */
    bytes_written = ssh_writen(-3, write_buf, data_len);
    assert_int_equal(bytes_written, -1);

    bytes_written = ssh_writen(fd, NULL, data_len);
    assert_int_equal(bytes_written, -1);

    bytes_written = ssh_writen(fd, write_buf, 0);
    assert_int_equal(bytes_written, -1);

    /* Clean up */
    rc = close(fd);
    assert_int_equal(rc, 0);

    rc = unlink(file_path);
    assert_int_equal(rc, 0);

    free(file_path);
    free(read_buf);
    free(write_buf);
}

static void torture_ssh_check_hostname_syntax(void **state)
{
    int rc;
    (void)state;

    rc = ssh_check_hostname_syntax("duckduckgo.com");
    assert_int_equal(rc, SSH_OK);
    rc = ssh_check_hostname_syntax("www.libssh.org");
    assert_int_equal(rc, SSH_OK);
    rc = ssh_check_hostname_syntax("Some-Thing.com");
    assert_int_equal(rc, SSH_OK);
    rc = ssh_check_hostname_syntax("amazon.a23456789012345678901234567890123456789012345678901234567890123");
    assert_int_equal(rc, SSH_OK);
    rc = ssh_check_hostname_syntax("amazon.a23456789012345678901234567890123456789012345678901234567890123.a23456789012345678901234567890123456789012345678901234567890123.ok");
    assert_int_equal(rc, SSH_OK);
    rc = ssh_check_hostname_syntax("amazon.a23456789012345678901234567890123456789012345678901234567890123.a23456789012345678901234567890123456789012345678901234567890123.a23456789012345678901234567890123456789012345678901234567890123");
    assert_int_equal(rc, SSH_OK);
    rc = ssh_check_hostname_syntax("lavabo-inter.innocentes-manus-meas");
    assert_int_equal(rc, SSH_OK);
    rc = ssh_check_hostname_syntax("localhost");
    assert_int_equal(rc, SSH_OK);
    rc = ssh_check_hostname_syntax("a");
    assert_int_equal(rc, SSH_OK);
    rc = ssh_check_hostname_syntax("a-0.b-b");
    assert_int_equal(rc, SSH_OK);
    rc = ssh_check_hostname_syntax("libssh.");
    assert_int_equal(rc, SSH_OK);

    rc = ssh_check_hostname_syntax(NULL);
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("/");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("@");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("[");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("`");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("{");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("&");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("|");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("\"");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("`");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax(" ");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("*the+giant&\"rooks\".c0m");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("!www.libssh.org");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("--.--");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("libssh.a234567890123456789012345678901234567890123456789012345678901234");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("libssh.a234567890123456789012345678901234567890123456789012345678901234.a234567890123456789012345678901234567890123456789012345678901234");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("libssh-");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("fe80::9656:d028:8652:66b6");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax(".");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_hostname_syntax("..");
    assert_int_equal(rc, SSH_ERROR);
}

static void torture_ssh_check_username_syntax(void **state) {
    int rc;
    (void)state;

    rc = ssh_check_username_syntax("username");
    assert_int_equal(rc, SSH_OK);
    rc = ssh_check_username_syntax("Alice");
    assert_int_equal(rc, SSH_OK);
    rc = ssh_check_username_syntax("Alice and Bob");
    assert_int_equal(rc, SSH_OK);
    rc = ssh_check_username_syntax("n4me?");
    assert_int_equal(rc, SSH_OK);

    rc = ssh_check_username_syntax("alice&bob");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_username_syntax("backslash\\");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_username_syntax("&var|()us\"<ha`r{}'");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_username_syntax(" -");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_username_syntax("me and -");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_username_syntax("los -santos");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_username_syntax("- who?");
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_username_syntax(NULL);
    assert_int_equal(rc, SSH_ERROR);
    rc = ssh_check_username_syntax("");
    assert_int_equal(rc, SSH_ERROR);
}

static void torture_ssh_is_ipaddr(void **state) {
    int rc;
    char *interf = malloc(64);
    char *test_interf = malloc(128);
    (void)state;

    assert_non_null(interf);
    assert_non_null(test_interf);
    rc = ssh_is_ipaddr("201.255.3.69");
    assert_int_equal(rc, 1);
    rc = ssh_is_ipaddr("::1");
    assert_int_equal(rc, 1);
    rc = ssh_is_ipaddr("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
    assert_int_equal(rc, 1);
    if_indextoname(1, interf);
    assert_non_null(interf);
    rc = sprintf(test_interf, TORTURE_IPV6_LOCAL_LINK, interf);
    /* the "%%s" is not written */
    assert_int_equal(rc, strlen(interf) + strlen(TORTURE_IPV6_LOCAL_LINK) - 3);
    rc = ssh_is_ipaddr(test_interf);
    assert_int_equal(rc, 1);
    free(interf);
    free(test_interf);

    rc = ssh_is_ipaddr("..");
    assert_int_equal(rc, 0);
    rc = ssh_is_ipaddr(":::");
    assert_int_equal(rc, 0);
    rc = ssh_is_ipaddr("1.1.1.1.1");
    assert_int_equal(rc, 0);
    rc = ssh_is_ipaddr("1.1");
    assert_int_equal(rc, 0);
    rc = ssh_is_ipaddr("caesar");
    assert_int_equal(rc, 0);
    rc = ssh_is_ipaddr("::xa:1");
    assert_int_equal(rc, 0);
}

int torture_run_tests(void) {
    int rc;
    struct CMUnitTest tests[] = {
        cmocka_unit_test(torture_get_user_home_dir),
        cmocka_unit_test(torture_basename),
        cmocka_unit_test(torture_dirname),
        cmocka_unit_test(torture_ntohll),
#ifdef _WIN32
        cmocka_unit_test(torture_path_expand_tilde_win),
#else
        cmocka_unit_test(torture_path_expand_tilde_unix),
#endif
        cmocka_unit_test_setup_teardown(torture_path_expand_escape, setup, teardown),
        cmocka_unit_test_setup_teardown(torture_path_expand_known_hosts, setup, teardown),
        cmocka_unit_test_setup_teardown(torture_path_expand_percent, setup, teardown),
        cmocka_unit_test(torture_timeout_elapsed),
        cmocka_unit_test(torture_timeout_update),
        cmocka_unit_test(torture_ssh_analyze_banner),
        cmocka_unit_test(torture_ssh_dir_writeable),
        cmocka_unit_test(torture_ssh_newline_vis),
        cmocka_unit_test(torture_ssh_mkdirs),
        cmocka_unit_test(torture_ssh_quote_file_name),
        cmocka_unit_test(torture_ssh_strreplace),
        cmocka_unit_test(torture_ssh_strerror),
        cmocka_unit_test(torture_ssh_readn),
        cmocka_unit_test(torture_ssh_writen),
        cmocka_unit_test(torture_ssh_check_hostname_syntax),
        cmocka_unit_test(torture_ssh_check_username_syntax),
        cmocka_unit_test(torture_ssh_is_ipaddr),
    };

    ssh_init();
    torture_filter_tests(tests);
    rc = cmocka_run_group_tests(tests, NULL, NULL);
    ssh_finalize();
    return rc;
}
