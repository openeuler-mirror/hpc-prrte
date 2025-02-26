/*
 * Copyright (c) 2024      Huawei Technologies Co., Ltd.
 *                         All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 * 
 * These symbols are in a file by themselves to provide nice linker
 * semantics. Since linkers generally pull in symbols by object
 * files, keeping these symbols as the only symbols in this file
 * prevents utility programs such as "ompi_info" from having to import
 * entire components just to query their version and parameters.
 */

#include "src/mca/prteinstalldirs/prteinstalldirs.h"
#include "src/mca/plm/plm_types.h"
#include "prte_config.h"
#include "src/runtime/prte_globals.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "constants.h"
#include "types.h"
#include "src/util/name_fns.h"
#include "src/util/pmix_show_help.h"
#include "src/threads/pmix_threads.h"
#include "src/runtime/prte_wait.h"
#include "src/runtime/prte_quit.h"
#include "src/mca/errmgr/errmgr.h"
#include "src/mca/rmaps/base/base.h"
#include "src/mca/state/state.h"

#include "src/prted/prted.h"

#include "src/mca/plm/plm.h"
#include "src/mca/plm/base/plm_private.h"
#include "plm_donau.h"

/*
 * Local functions
 */
static int plm_donau_init(void);
static int plm_donau_launch_job(prte_job_t *jdata);
static int plm_donau_terminate_prteds(void);
static int plm_donau_signal_job(pmix_nspace_t jobid, int32_t signal);
static int plm_donau_finalize(void);
static int plm_donau_start_proc(int argc, char **argv, char *prefix);

/*
 * Global variable
 */
prte_plm_base_module_1_0_0_t prte_plm_donau_module = {
    .init = plm_donau_init,
    .set_hnp_name = prte_plm_base_set_hnp_name,
    .spawn = plm_donau_launch_job,
    .terminate_job = prte_plm_base_prted_terminate_job,
    .terminate_orteds = plm_donau_terminate_prteds,
    .terminate_procs = prte_plm_base_prted_kill_local_procs,
    .signal_job = plm_donau_signal_job,
    .finalize = plm_donau_finalize
};

/*
 * Local variables
 */

typedef struct Node {
    char name[DONAU_MAX_NODENAME_LENGTH];
    int len;
    char pre[DONAU_MAX_NODENAME_LENGTH];
    int pre_len;
    int num;
} nod;

typedef enum {
    SIMP_SUCCESS = 0,
    SIMP_OUT_OF_RESOURCE,
    SIMP_NULL
} simp_state;

nod node[DONAU_MAX_NODELIST_LENGTH];

static pid_t primary_drun_pid = 0;
static bool primary_pid_set = false;
static void launch_daemons(int fd, short args, void *cbdata);
static int cmp(const void *a, const void *b);
static void get_pre(char *s, char *result);
static int get_id_num(char *s);
static simp_state donau_nodelist_simp(char *node_list, char *nodelist_result);

/*
 * Init the module
 */
static int plm_donau_init(void)
{
    int rc;
    prte_job_t *jdata;
    if (PRTE_SUCCESS != (rc = prte_plm_base_comm_start())) {
        PRTE_ERROR_LOG(rc);
        return rc;
    }
    jdata = prte_get_job_data_object(PRTE_PROC_MY_NAME->nspace);
    if (prte_get_attribute(&jdata->attributes, PRTE_JOB_DO_NOT_LAUNCH, NULL, PMIX_BOOL)) {
        prte_plm_globals.daemon_nodes_assigned_at_launch = true;
    } else {
        prte_plm_globals.daemon_nodes_assigned_at_launch = false;
    }

    /* point to our launch command */
    rc = prte_state.add_job_state(PRTE_JOB_STATE_LAUNCH_DAEMONS, launch_daemons);
    if (PRTE_SUCCESS != rc) {
        PRTE_ERROR_LOG(rc);
        return rc;
    }

    return rc;
}

/* When working in this function, ALWAYS jump to "cleanup" if
 * you encounter an error so that orterun will be woken up and
 * the job can cleanly terminate
 */
static int plm_donau_launch_job(prte_job_t *jdata)
{
    if (PRTE_FLAG_TEST(jdata, PRTE_JOB_FLAG_RESTART)) {
        /* this is a restart situation - skip to the mapping stage */
        PRTE_ACTIVATE_JOB_STATE(jdata, PRTE_JOB_STATE_MAP);
    } else {
        /* new job - set it up */
        PRTE_ACTIVATE_JOB_STATE(jdata, PRTE_JOB_STATE_INIT);
    }
    return PRTE_SUCCESS;
}

static void launch_daemons(int fd, short args, void *cbdata)
{
    prte_app_context_t *app = NULL;
    prte_node_t *node = NULL;
    int32_t nnode;
    prte_job_map_t *map = NULL;
    char *param = NULL;
    char **argv = NULL;
    int argc;
    int rc;
    char *tmp = NULL;
    char **env = NULL;
    char *nodelist_flat = NULL;
    char *nodelist_simp = NULL;
    char **nodelist_argv = NULL;
    char *name_string = NULL;
    char **custom_strings = NULL;
    int num_args;
    char *cur_prefix = NULL;
    int proc_vpid_index;
    bool failed_launch = true;
    prte_job_t *daemons = NULL;
    prte_state_caddy_t *state = (prte_state_caddy_t*)cbdata;

    PMIX_ACQUIRE_OBJECT(state);

    PMIX_OUTPUT_VERBOSE((1, prte_plm_base_framework.framework_output,
                         "%s plm:donau: LAUNCH DAEMONS CALLED",
                         PRTE_NAME_PRINT(PRTE_PROC_MY_NAME)));

    /* start by setting up the virtual machine */
    daemons = prte_get_job_data_object(PRTE_PROC_MY_NAME->nspace);
    if (NULL == daemons) {
        PRTE_ERROR_LOG(PRTE_ERR_NOT_FOUND);
        goto cleanup;
    }
    if (PRTE_SUCCESS != (rc = prte_plm_base_setup_virtual_machine(state->jdata))) {
        PRTE_ERROR_LOG(rc);
        goto cleanup;
    }
    /* if we don't want to launch, then don't attempt to
     * launch the daemons - the user really wants to just
     * look at the proposed process map
     */
    if (prte_get_attribute(&daemons->attributes, PRTE_JOB_DO_NOT_LAUNCH, NULL, PMIX_BOOL)) {
        state->jdata->state = PRTE_JOB_STATE_DAEMONS_LAUNCHED;
        PRTE_ACTIVATE_JOB_STATE(state->jdata, PRTE_JOB_STATE_DAEMONS_REPORTED);
        PMIX_RELEASE(state);
        return;
    }

    // OPAL_OUTPUT_VERBOSE((1, orte_plm_base_framework.framework_output,
    //                      "%s plm:donau: launching vm",
    //                      ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    /* get the map for the job */
    if (NULL == (map = daemons->map)) {
        PRTE_ERROR_LOG(PRTE_ERR_NOT_FOUND);
        rc = PRTE_ERR_NOT_FOUND;
        goto cleanup;
    }

    if (0 == map->num_new_daemons) {
        /* set the state to indicate the daemons reported - this
         * will trigger the daemons_reported event and cause the
         * job to move the following step
         */
        PMIX_OUTPUT_VERBOSE((1, prte_plm_base_framework.framework_output,
                             "%s plm:donau: no new daemons to launch",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME)));
        state->jdata->state = PRTE_JOB_STATE_DAEMONS_LAUNCHED;
        PRTE_ACTIVATE_JOB_STATE(state->jdata, PRTE_JOB_STATE_DAEMONS_REPORTED);
        PMIX_RELEASE(state);
        return;
    }

    /*
     * start building argv array
     */
    argv = NULL;
    argc = 0;

    /*
     * DONAU drun OPTIONS
     */
    
    /* add the drun command */
    pmix_argv_append(&argc, &argv, donau_launch_exec);

    /* Append user defined arguments to drun */
    if (NULL != prte_mca_plm_donau_component.custom_args) {
        custom_strings = PMIX_ARGV_SPLIT_COMPAT(prte_mca_plm_donau_component.custom_args, ' ');
        num_args = PMIX_ARGV_COUNT_COMPAT(custom_strings);
        for (int i = 0; i < num_args; ++i) {
            pmix_argv_append(&argc, &argv, custom_strings[i]);
        }
        PMIX_ARGV_FREE_COMPAT(custom_strings);
    }
    /* create nodelist */
    nodelist_argv = NULL;

    /* get the hnp node and send to donau */
    prte_node_t *hnp_node = (prte_node_t*)pmix_pointer_array_get_item(prte_node_pool, 0);
    pmix_argv_append_nosize(&nodelist_argv, hnp_node->name);

    int node_size = map->nodes->size;
    for (nnode = 0; nnode < node_size; nnode++) {
        if (NULL == (node = (prte_node_t*)pmix_pointer_array_get_item(map->nodes, nnode))) {
            continue;
        }
        /* if the daemon already exists on this node, then
         * don't include it
         */
        if (PRTE_FLAG_TEST(node, PRTE_NODE_FLAG_DAEMON_LAUNCHED)) {
            continue;
        }

        /* otherwise, add it to the list of nodes upon which
         * we need to launch a daemon
         */
        PMIX_ARGV_APPEND_NOSIZE_COMPAT(&nodelist_argv, node->name);
    }
    if (0 == PMIX_ARGV_COUNT_COMPAT(nodelist_argv)) {
        pmix_show_help("help-plm-donau.txt", "no-hosts-in-list", true);
        rc = PRTE_ERR_FAILED_TO_START;
        goto cleanup;
    }
    nodelist_flat = PMIX_ARGV_JOIN_COMPAT(nodelist_argv, ',');
    PMIX_ARGV_FREE_COMPAT(nodelist_argv);

    /* simplify nodelist for donau */

    nodelist_simp = (char *)malloc(DONAU_MAX_NODELIST_LENGTH);
    memset(nodelist_simp, 0, sizeof(nodelist_simp));
    simp_state error_num = donau_nodelist_simp(nodelist_flat, nodelist_simp);
    if (error_num == SIMP_OUT_OF_RESOURCE) {
        PRTE_ERROR_LOG(PRTE_ERR_OUT_OF_RESOURCE);
    } else if (error_num == SIMP_NULL) {
        PRTE_ERROR_LOG(PRTE_ERR_NOT_AVAILABLE);
    }
    if (error_num != SIMP_SUCCESS) {
        free(nodelist_simp);
        free(nodelist_flat);
        goto cleanup;
    }

    pmix_asprintf(&tmp, "-nl");
    pmix_argv_append(&argc, &argv, tmp);
    free(tmp);
    pmix_asprintf(&tmp, "%s", nodelist_simp);
    pmix_argv_append(&argc, &argv, tmp);
    free(tmp);
    pmix_asprintf(&tmp, "-ao");
    pmix_argv_append(&argc, &argv, tmp);
    free(tmp);
    pmix_asprintf(&tmp, "%s", hnp_node->name);
    pmix_argv_append(&argc, &argv, tmp);
    free(tmp);
    pmix_asprintf(&tmp, "-rpn");
    pmix_argv_append(&argc, &argv, tmp);
    free(tmp);
    pmix_asprintf(&tmp, "1");
    pmix_argv_append(&argc, &argv, tmp);
    free(tmp);

    PMIX_OUTPUT_VERBOSE((2, prte_plm_base_framework.framework_output,
                         "%s plm:donau: launching on nodes %s",
                         PRTE_NAME_PRINT(PRTE_PROC_MY_NAME), nodelist_simp));
    free(nodelist_simp);
    free(nodelist_flat);
    /*
     * ORTED OPTIONS
     */

    /* add the daemon command (as specified by user) */
    prte_plm_base_setup_prted_cmd(&argc, &argv);
    /* add basic orted command line options, including debug flags */
    prte_plm_base_prted_append_basic_args(&argc, &argv,
                                          "donau", &proc_vpid_index);
    /* tell the new daemons the base of the name list so they can compute
     * their own name on the other end
     */
    rc = prte_util_convert_vpid_to_string(&name_string, map->daemon_vpid_start);
    if (PRTE_SUCCESS != rc) {
        pmix_output(0, "plm_donau: unable to get daemon vpid as string");
        goto cleanup;
    }

    free(argv[proc_vpid_index]);
    argv[proc_vpid_index] = strdup(name_string);
    free(name_string);

    char *param1 = NULL;
    prte_oob_base_get_addr(&param1);
    if (param1 == NULL) {
        pmix_output(0, "plm_donau: unable to get param1 from orte_oob_base_get_addr");
        goto cleanup;
    }
    pmix_argv_append(&argc, &argv, "--prtemca");
    pmix_argv_append(&argc, &argv, "prte_parent_uri");
    pmix_argv_append(&argc, &argv, param1);
    free(param1);
    /* Copy the prefix-directory specified in the
     * corresponding add_context. If there are multiple,
     * different prefix's in the app context, complain (i.e., only
     * allow one --prefix option for the entire donau run -- we
     * don't support different --prefix'es for different nodes in
     * the DONAU plm)
     */
    cur_prefix = NULL;
    for (nnode = 0; nnode < state->jdata->apps->size; nnode++) {
        char *app_prefix_dir = NULL;
        if (NULL == (app = (prte_app_context_t*)pmix_pointer_array_get_item(state->jdata->apps, nnode))) {
            continue;
        }
        app_prefix_dir = NULL;
        prte_get_attribute(&app->attributes, PRTE_APP_PREFIX_DIR, (void**)&app_prefix_dir, PMIX_STRING);
        /* Check for already set cur_prefix_dir -- if different,
           complain */
        if (NULL != app_prefix_dir) {
            if (NULL != cur_prefix &&
                0 != strcmp(cur_prefix, app_prefix_dir)) {
                pmix_show_help("help-plm-donau.txt", "multiple-prefixes",
                           true, cur_prefix, app_prefix_dir);
                goto cleanup;
            }

            /* If not yet set, copy it; if set, then it's the
             * same way
             */
            if (NULL == cur_prefix) {
                cur_prefix = strdup(app_prefix_dir);
                PMIX_OUTPUT_VERBOSE((1, prte_plm_base_framework.framework_output,
                                     "%s plm:donau: Set prefix:%s",
                                     PRTE_NAME_PRINT(PRTE_PROC_MY_NAME),
                                     cur_prefix));
            }
            free(app_prefix_dir);
        }
    }

    /* protect the args in case someone has a script wrapper around drun */
    prte_plm_base_wrap_args(argv);

    if (0 < pmix_output_get_verbosity(prte_plm_base_framework.framework_output)) {
        param = PMIX_ARGV_JOIN_COMPAT(argv, ' ');
        pmix_output(prte_plm_base_framework.framework_output,
                    "%s plm:donau: final top-level argv:\n\t%s",
                    PRTE_NAME_PRINT(PRTE_PROC_MY_NAME),
                    (NULL == param) ? "NULL" : param);
        if (NULL != param) {
            free(param);
        }
    }

    /* exec the daemon(s) */
    if (PRTE_SUCCESS != (rc = plm_donau_start_proc(argc, argv, cur_prefix))) {
        PRTE_ERROR_LOG(rc);
        goto cleanup;
    }

    /* indicate that the daemons for this job were launched */
    state->jdata->state = PRTE_JOB_STATE_DAEMONS_LAUNCHED;
    daemons->state = PRTE_JOB_STATE_DAEMONS_LAUNCHED;

    /* flag that launch was successful, so far as we currently know */
    failed_launch = false;

cleanup:
    if (NULL != argv) {
        PMIX_ARGV_FREE_COMPAT(argv);
    }
    if (NULL != cur_prefix) {
        free(cur_prefix);
    }
    /* check for failed launch - if so, force terminate */
    if (failed_launch) {
        PRTE_ACTIVATE_JOB_STATE(state->jdata, PRTE_JOB_STATE_FAILED_TO_LAUNCH);
    }
    /* cleanup the caddy */
    PMIX_RELEASE(state);
}

/*
 * Terminate the orteds for a given job
 */
static int plm_donau_terminate_prteds(void)
{
    int rc = PRTE_SUCCESS;
    prte_job_t *jdata;

    /* check to see if the primary pid is set. If not, this indicates
     * that we never launched any additional daemons, so we cannot
     * not wait for a waitpid to fire and tell us it's okay to
     * exit. Instead, we simply trigger an exit for ourselves.
     */
    if (primary_pid_set) {
        if (PRTE_SUCCESS != (rc = prte_plm_base_prted_exit(PRTE_DAEMON_EXIT_CMD))) {
            PRTE_ERROR_LOG(rc);
        }
    } else {
        PMIX_OUTPUT_VERBOSE((1, prte_plm_base_framework.framework_output,
                             "%s plm:donau: primary daemons complete!",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME)));
        jdata = prte_get_job_data_object(PRTE_PROC_MY_NAME->nspace);
        /* need to set the $terminated value to avoid an incorrect error msg */
        jdata->num_terminated = jdata->num_procs;
        PRTE_ACTIVATE_JOB_STATE(jdata, PRTE_JOB_STATE_DAEMONS_TERMINATED);
    }

    return rc;
}

/*
 * Signal all the processes in the child drun by sending the signal directly to it
 */
static int plm_donau_signal_job(pmix_nspace_t jobid, int32_t signal)
{
    int rc = PRTE_SUCCESS;

    /* order them to pass this signal to their local procs */
    if (PRTE_SUCCESS != (rc = prte_plm_base_prted_signal_local_procs(jobid, signal))) {
        PRTE_ERROR_LOG(rc);
    }

    return rc;
}

static int plm_donau_finalize(void)
{
    int rc;

    /* cleanup any pending recvs */
    if (PRTE_SUCCESS != (rc = prte_plm_base_comm_stop())) {
        PRTE_ERROR_LOG(rc);
    }

    return PRTE_SUCCESS;
}

static void drun_wait_cb(int sd, short fd, void *cbdata)
{
    prte_wait_tracker_t *t2 = (prte_wait_tracker_t*)cbdata;
    prte_proc_t *proc = t2->child;
    prte_job_t *jdata;

    jdata = prte_get_job_data_object(PRTE_PROC_MY_NAME->nspace);

    /* abort only if the status returned is non-zero - i.e., if
     * the orteds exited with an error
     */
    if (0 != proc->exit_code) {
        /* an orted must have died unexpectedly - report
         * that the daemon has failed so we exit
         */
        PMIX_OUTPUT_VERBOSE((1, prte_plm_base_framework.framework_output,
                             "%s plm:donau: drun returned non-zero exit status (%d) from launching the per-node daemon",
                             PRTE_NAME_PRINT(PRTE_PROC_MY_NAME),
                             proc->exit_code));
        PRTE_ACTIVATE_JOB_STATE(jdata, PRTE_JOB_STATE_DAEMONS_TERMINATED);
    } else {
        /* otherwise, check to see if this is the primary pid */
        if (primary_drun_pid == proc->pid) {
            /* in this case, we just want to fire the proper trigger so
             * mpirun can exit
             */
            PMIX_OUTPUT_VERBOSE((1, prte_plm_base_framework.framework_output,
                                 "%s plm:donau: primary daemons complete!",
                                 PRTE_NAME_PRINT(PRTE_PROC_MY_NAME)));
            /* need to set the #terminated value to avoid an incorrect error msg */
            jdata->num_terminated = jdata->num_procs;
            PRTE_ACTIVATE_JOB_STATE(jdata, PRTE_JOB_STATE_DAEMONS_TERMINATED);
        }
    }

    /* done with this dummy */
    PMIX_RELEASE(t2);
}

static int plm_donau_start_proc(int argc, char **argv, char *prefix)
{
    int fd;
    int drun_pid;
    char *exec_argv = pmix_path_findv(argv[0], 0, environ, NULL);
    prte_proc_t *dummy;

    if (NULL == exec_argv) {
        pmix_show_help("help-plm-donau.txt", "no-drun", true);
        return PRTE_ERR_SILENT;
    }

    drun_pid = fork();
    if (-1 == drun_pid) {
        PRTE_ERROR_LOG(PRTE_ERR_SYS_LIMITS_CHILDREN);
        free(exec_argv);
        return PRTE_ERR_SYS_LIMITS_CHILDREN;
    }
    /* if this is the primary launch - i.e., not a comm_spawn of a
     * child job - then save the pid
     */
    if (0 < drun_pid && !primary_pid_set) {
        primary_drun_pid = drun_pid;
        primary_pid_set = true;
    }

    /* setup a dummy proc object to track the drun */
    dummy = PMIX_NEW(prte_proc_t);
    dummy->pid = drun_pid;
    /* be sure to mark it as alive so we don't instantly fire */
    PRTE_FLAG_SET(dummy, PRTE_PROC_FLAG_ALIVE);
    /* setup the waitpid so we can find out if drun succeeds! */
    prte_wait_cb(dummy, drun_wait_cb, NULL);

    if (0 == drun_pid) { /* child */
        char *bin_base = NULL;
        char *lib_base = NULL;

        /* Figure out the basenames for the libdir and bindir. There
         * is a lengthy comment about this in plm_rsh_module.c
         * explaining all the rationale for how / why we're doing
         * this.
         */
        lib_base = pmix_basename(prte_install_dirs.libdir);
        bin_base = pmix_basename(prte_install_dirs.bindir);

        /* If we have a prefix, then modify the PATH and
         * LD_LIBRARY_PATH environment variables.
         */
        if (NULL != prefix) {
            char *oldenv = NULL;
            char *newenv = NULL;

            /* Reset PATH */
            oldenv = getenv("PATH");
            if (NULL != oldenv) {
                pmix_asprintf(&newenv, "%s/%s:%s", prefix, bin_base, oldenv);
            } else {
                pmix_asprintf(&newenv, "%s/%s", prefix, bin_base);
            }
            setenv("PATH", newenv, true);
            PMIX_OUTPUT_VERBOSE((1, prte_plm_base_framework.framework_output,
                                 "%s plm:donau: reset PATH: %s",
                                 PRTE_NAME_PRINT(PRTE_PROC_MY_NAME),
                                 newenv));
            free(newenv);

            /* Reset LD_LIBRARY_PATH */
            oldenv = getenv("LD_LIBRARY_PATH");
            if (NULL != oldenv) {
                pmix_asprintf(&newenv, "%s/%s:%s", prefix, lib_base, oldenv);
            } else {
                pmix_asprintf(&newenv, "%s/%s", prefix, lib_base);
            }
            setenv("LD_LIBRARY_PATH", newenv, true);
            PMIX_OUTPUT_VERBOSE((1, prte_plm_base_framework.framework_output,
                                 "%s plm:donau: reset LD_LIBRARY_PATH: %s",
                                 PRTE_NAME_PRINT(PRTE_PROC_MY_NAME),
                                 newenv));
            free(newenv);
        }

        fd = open("/dev/null", O_CREAT|O_RDWR|O_TRUNC, 0666);
        if (fd >= 0) {
            dup2(fd, 0);
            /* When not in debug mode and --debug-daemons was not passed,
             * tie stdout/stderr to dev null so we don't see messages from orted
             * EXCEPT if the user has requested that we leave sessions attached
             */
            if (0 > pmix_output_get_verbosity(prte_plm_base_framework.framework_output) &&
                !prte_debug_daemons_flag && !prte_leave_session_attached) {
                dup2(fd, 1);
                dup2(fd, 2);
            }

            /* Don't leave the extra fd to /dev/null open */
            if (fd > 2) {
                close(fd);
            }
        }

        /* get the drun process out of orterun's process group so that
         * signals sent from the shell (like those resulting from
         * cntl-c) don't get sent to drun
         */
        setpgid(0, 0);
        execvp(exec_argv, argv);

        pmix_output(0, "plm:donau:start_proc: exec failed");
        /* don't return - need to exit - returning would be bad -
         * we're not in the calling process anymore
         */
        exit(1);
    } else { /* parent */
        /* just in case, make sure that the drun process is not in our
         * process group anymore. Stevens says always do this on both
         * sides of the fork...
         */
        setpgid(drun_pid, drun_pid);

        free(exec_argv);
    }

    return PRTE_SUCCESS;
}

// Structure sorting
static int cmp(const void *a, const void *b) {
    nod c = *(nod *)a;
    nod d = *(nod *)b;
    if (strcmp(c.pre, d.pre) != 0) {
        return strcmp(c.pre, d.pre);
    } else {
        return d.num - c.num;
    }
}

// Obtain the node prefix name
static void get_pre(char *s, char *result) {
    int len = strlen(s);
    int prelen = -1;
    memset(result, '\0', sizeof(result));
    for (int i = len - 1; i >= 0; i--) {
        if(s[i] >= '0' && s[i] <= '9'){
            continue;
        } else {
            prelen = i;
            break;
        }
    }
    if (prelen == -1) {
        prelen = len - 1;
    }
    memcpy(result, s, prelen + 1);
    return ;
}

// Obtain the node number
static int get_id_num(char *s) {
    int result = 0;
    int len = strlen(s);
    int last_non_zero = len;
    for (int i = len - 1; i >= 0; i--) {
        if (s[i] < '0' || s[i] > '9') {
            break;
        } else if (s[i] > '0' && s[i] <= '9') {
            last_non_zero = i;
        }
    }
    if (s[len - 1] < '0' || s[len - 1] > '9') {
        // Use -2 to make a plain string at the front of the sort
        return -2;
    } else if (last_non_zero == len && s[len - 1] == '0' &&
                (len - 2 < 0 || (s[len - 2] < '0' || s[len - 2] > '9'))) {
        // Valid number 0
        return 0;
    } else if (last_non_zero != 0 && s[last_non_zero - 1] == '0') {
        // Contain leading zeros
        return -1;
    }
    for (int i = last_non_zero; i < len; i++) {
        result = result * 10 + s[i] - '0';
    }
    return result;
}

// Simplify node name (Split with ",")
static simp_state donau_nodelist_simp(char *node_list, char *nodelist_result) {
    int temp_num = 0;
    char *temp_s;

    int node_stack[DONAU_MAX_NODELIST_LENGTH];
    int stack_size = 0;

    if (*node_list == '\0' || node_list == NULL) {
        return SIMP_NULL;
    }
    temp_s = strtok(node_list, ",");
    strcpy(node[temp_num].name, temp_s);
    temp_num++;
    while (1) {
        temp_s = strtok(NULL, ",");
        if(temp_s == NULL) {
            break;
        }
        strcpy(node[temp_num].name, temp_s);
        temp_num++;
    }
    for (int i = 0; i < temp_num; i++) {
        node[i].len = strlen(node[i].name);
        node[i].num = get_id_num(node[i].name);
        get_pre(node[i].name, node[i].pre);
        node[i].pre_len = strlen(node[i].pre);
    }

    qsort(node, temp_num, sizeof(node[0]), cmp);

    for (int i = 0; i <= temp_num; i++) {
        char temp_str[DONAU_MAX_NODELIST_LENGTH] = "";
        int str_len = node[i].pre_len;

        if (i < temp_num && stack_size == 0) {
            node_stack[++stack_size] = i;
        } else if (i < temp_num && strcmp(node[i].pre, node[i - 1].pre) == 0) {
            node_stack[++stack_size] = i;
        } else {
            int temp_len = 0;
            int last_str_len = strlen(node[i - 1].pre);
            for (int j = 0; j < last_str_len; j++) {
                temp_str[j] = node[i - 1].name[j];
                temp_len++;
            }
            if (node[node_stack[stack_size]].pre_len == node[node_stack[stack_size]].len) {
                if(strlen(nodelist_result) + strlen(temp_str) >= DONAU_MAX_NODELIST_LENGTH) {
                    return SIMP_OUT_OF_RESOURCE;
                }
                strcat(nodelist_result, temp_str);
                if (i < temp_num || i == temp_num && stack_size > 1) {
                    strcat(nodelist_result, ",");
                }
                stack_size--;
            }
            if (stack_size > 0) {
                temp_str[temp_len++] = '[';
                // Determine whether a character is at the beginning
                int is_beginning = 0;
                // Determine whether a character is at the end
                int is_end = 0;
                while (stack_size > 0) {
                    // Compress if adjacent to the previous number
                    if (stack_size >= 1 && node[node_stack[stack_size]].num >= 0 &&
                        node[node_stack[stack_size]].num == node[node_stack[stack_size - 1]].num - 1) {
                        if (is_beginning == 0){
                            is_beginning = 1;
                        } else {
                            stack_size--;
                            continue;
                        }
                    } else {
                        is_beginning =0;
                    }
                    if (is_end == 1) {
                        temp_str[temp_len++] = '-';
                        is_end = 0;
                    }
                    for (int j = node[node_stack[stack_size]].pre_len; j < node[node_stack[stack_size]].len; j++) {
                        temp_str[temp_len++] = node[node_stack[stack_size]].name[j];
                    }
                    if (stack_size > 1) {
                        if ((node[node_stack[stack_size]].num < 0) ||
                            (node[node_stack[stack_size]].num != node[node_stack[stack_size - 1]].num - 1)) {
                            temp_str[temp_len++] = ',';
                        }
                    }
                    if (is_beginning == 1) {
                        is_end = 1;
                    }

                    stack_size--;
                }
                temp_str[temp_len++] = ']';
                if(strlen(nodelist_result) + strlen(temp_str) >= DONAU_MAX_NODELIST_LENGTH) {
                    return SIMP_OUT_OF_RESOURCE;
                }
                strcat(nodelist_result, temp_str);
                if (i < temp_num) {
                    strcat(nodelist_result, ",");
                }
            }

            node_stack[++stack_size] = i;
        }
    }

    return SIMP_SUCCESS;
}