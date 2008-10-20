/*
 * config.c - the platform-independent parts of the PuTTY
 * configuration box.
 */

#include <assert.h>
#include <stdlib.h>

#include "putty.h"
#include "dialog.h"
#include "storage.h"

#define PRINTER_DISABLED_STRING "���� (����Ʈ ��� �� ��)"

#define HOST_BOX_TITLE "Host Name (or IP address)"
#define PORT_BOX_TITLE "Port"

/*
 * Convenience function: determine whether this binary supports a
 * given backend.
 */
static int have_backend(int protocol)
{
    struct backend_list *p = backends;
    for (p = backends; p->name; p++) {
	if (p->protocol == protocol)
	    return 1;
    }
    return 0;
}

static void config_host_handler(union control *ctrl, void *dlg,
				void *data, int event)
{
    Config *cfg = (Config *)data;

    /*
     * This function works just like the standard edit box handler,
     * only it has to choose the control's label and text from two
     * different places depending on the protocol.
     */
    if (event == EVENT_REFRESH) {
	if (cfg->protocol == PROT_SERIAL) {
	    /*
	     * This label text is carefully chosen to contain an n,
	     * since that's the shortcut for the host name control.
	     */
	    dlg_label_change(ctrl, dlg, "Serial line");
	    dlg_editbox_set(ctrl, dlg, cfg->serline);
	} else {
	    dlg_label_change(ctrl, dlg, HOST_BOX_TITLE);
	    dlg_editbox_set(ctrl, dlg, cfg->host);
	}
    } else if (event == EVENT_VALCHANGE) {
	if (cfg->protocol == PROT_SERIAL)
	    dlg_editbox_get(ctrl, dlg, cfg->serline, lenof(cfg->serline));
	else
	    dlg_editbox_get(ctrl, dlg, cfg->host, lenof(cfg->host));
    }
}

static void config_port_handler(union control *ctrl, void *dlg,
				void *data, int event)
{
    Config *cfg = (Config *)data;
    char buf[80];

    /*
     * This function works just like the standard edit box handler,
     * only it has to choose the control's label and text from two
     * different places depending on the protocol.
     */
    if (event == EVENT_REFRESH) {
	if (cfg->protocol == PROT_SERIAL) {
	    /*
	     * This label text is carefully chosen to contain a p,
	     * since that's the shortcut for the port control.
	     */
	    dlg_label_change(ctrl, dlg, "Speed");
	    sprintf(buf, "%d", cfg->serspeed);
	} else {
	    dlg_label_change(ctrl, dlg, PORT_BOX_TITLE);
	    sprintf(buf, "%d", cfg->port);
	}
	dlg_editbox_set(ctrl, dlg, buf);
    } else if (event == EVENT_VALCHANGE) {
	dlg_editbox_get(ctrl, dlg, buf, lenof(buf));
	if (cfg->protocol == PROT_SERIAL)
	    cfg->serspeed = atoi(buf);
	else
	    cfg->port = atoi(buf);
    }
}

struct hostport {
    union control *host, *port;
};

/*
 * We export this function so that platform-specific config
 * routines can use it to conveniently identify the protocol radio
 * buttons in order to add to them.
 */
void config_protocolbuttons_handler(union control *ctrl, void *dlg,
				    void *data, int event)
{
    int button, defport;
    Config *cfg = (Config *)data;
    struct hostport *hp = (struct hostport *)ctrl->radio.context.p;

    /*
     * This function works just like the standard radio-button
     * handler, except that it also has to change the setting of
     * the port box, and refresh both host and port boxes when. We
     * expect the context parameter to point at a hostport
     * structure giving the `union control's for both.
     */
    if (event == EVENT_REFRESH) {
	for (button = 0; button < ctrl->radio.nbuttons; button++)
	    if (cfg->protocol == ctrl->radio.buttondata[button].i)
		break;
	/* We expected that `break' to happen, in all circumstances. */
	assert(button < ctrl->radio.nbuttons);
	dlg_radiobutton_set(ctrl, dlg, button);
    } else if (event == EVENT_VALCHANGE) {
	int oldproto = cfg->protocol;
	button = dlg_radiobutton_get(ctrl, dlg);
	assert(button >= 0 && button < ctrl->radio.nbuttons);
	cfg->protocol = ctrl->radio.buttondata[button].i;
	if (oldproto != cfg->protocol) {
	    defport = -1;
	    switch (cfg->protocol) {
	      case PROT_SSH: defport = 22; break;
	      case PROT_TELNET: defport = 23; break;
	      case PROT_RLOGIN: defport = 513; break;
	    }
	    if (defport > 0 && cfg->port != defport) {
		cfg->port = defport;
	    }
	}
	dlg_refresh(hp->host, dlg);
	dlg_refresh(hp->port, dlg);
    }
}

static void loggingbuttons_handler(union control *ctrl, void *dlg,
				   void *data, int event)
{
    int button;
    Config *cfg = (Config *)data;
    /* This function works just like the standard radio-button handler,
     * but it has to fall back to "no logging" in situations where the
     * configured logging type isn't applicable.
     */
    if (event == EVENT_REFRESH) {
        for (button = 0; button < ctrl->radio.nbuttons; button++)
            if (cfg->logtype == ctrl->radio.buttondata[button].i)
	        break;
    
    /* We fell off the end, so we lack the configured logging type */
    if (button == ctrl->radio.nbuttons) {
        button=0;
        cfg->logtype=LGTYP_NONE;
    }
    dlg_radiobutton_set(ctrl, dlg, button);
    } else if (event == EVENT_VALCHANGE) {
        button = dlg_radiobutton_get(ctrl, dlg);
        assert(button >= 0 && button < ctrl->radio.nbuttons);
        cfg->logtype = ctrl->radio.buttondata[button].i;
    }
}

static void numeric_keypad_handler(union control *ctrl, void *dlg,
				   void *data, int event)
{
    int button;
    Config *cfg = (Config *)data;
    /*
     * This function works much like the standard radio button
     * handler, but it has to handle two fields in Config.
     */
    if (event == EVENT_REFRESH) {
	if (cfg->nethack_keypad)
	    button = 2;
	else if (cfg->app_keypad)
	    button = 1;
	else
	    button = 0;
	assert(button < ctrl->radio.nbuttons);
	dlg_radiobutton_set(ctrl, dlg, button);
    } else if (event == EVENT_VALCHANGE) {
	button = dlg_radiobutton_get(ctrl, dlg);
	assert(button >= 0 && button < ctrl->radio.nbuttons);
	if (button == 2) {
	    cfg->app_keypad = FALSE;
	    cfg->nethack_keypad = TRUE;
	} else {
	    cfg->app_keypad = (button != 0);
	    cfg->nethack_keypad = FALSE;
	}
    }
}

static void cipherlist_handler(union control *ctrl, void *dlg,
			       void *data, int event)
{
    Config *cfg = (Config *)data;
    if (event == EVENT_REFRESH) {
	int i;

	static const struct { char *s; int c; } ciphers[] = {
	    { "3DES",			CIPHER_3DES },
	    { "Blowfish",		CIPHER_BLOWFISH },
	    { "DES",			CIPHER_DES },
	    { "AES (SSH2 ��)",	CIPHER_AES },
	    { "Arc4 (SSH2 ��)",	CIPHER_ARCFOUR },
	    { "-- ���ϴ� ������ --",	CIPHER_WARN }
	};

	/* Set up the "selected ciphers" box. */
	/* (cipherlist assumed to contain all ciphers) */
	dlg_update_start(ctrl, dlg);
	dlg_listbox_clear(ctrl, dlg);
	for (i = 0; i < CIPHER_MAX; i++) {
	    int c = cfg->ssh_cipherlist[i];
	    int j;
	    char *cstr = NULL;
	    for (j = 0; j < (sizeof ciphers) / (sizeof ciphers[0]); j++) {
		if (ciphers[j].c == c) {
		    cstr = ciphers[j].s;
		    break;
		}
	    }
	    dlg_listbox_addwithid(ctrl, dlg, cstr, c);
	}
	dlg_update_done(ctrl, dlg);

    } else if (event == EVENT_VALCHANGE) {
	int i;

	/* Update array to match the list box. */
	for (i=0; i < CIPHER_MAX; i++)
	    cfg->ssh_cipherlist[i] = dlg_listbox_getid(ctrl, dlg, i);

    }
}

static void kexlist_handler(union control *ctrl, void *dlg,
			    void *data, int event)
{
    Config *cfg = (Config *)data;
    if (event == EVENT_REFRESH) {
	int i;

	static const struct { char *s; int k; } kexes[] = {
	    { "����-�︸ �׷� 1",		KEX_DHGROUP1 },
	    { "����-�︸ �׷� 14",	KEX_DHGROUP14 },
	    { "����-�︸ �׷� ��ȯ",	KEX_DHGEX },
	    { "-- ���ϴ� ������ --",		KEX_WARN }
	};

	/* Set up the "kex preference" box. */
	/* (kexlist assumed to contain all algorithms) */
	dlg_update_start(ctrl, dlg);
	dlg_listbox_clear(ctrl, dlg);
	for (i = 0; i < KEX_MAX; i++) {
	    int k = cfg->ssh_kexlist[i];
	    int j;
	    char *kstr = NULL;
	    for (j = 0; j < (sizeof kexes) / (sizeof kexes[0]); j++) {
		if (kexes[j].k == k) {
		    kstr = kexes[j].s;
		    break;
		}
	    }
	    dlg_listbox_addwithid(ctrl, dlg, kstr, k);
	}
	dlg_update_done(ctrl, dlg);

    } else if (event == EVENT_VALCHANGE) {
	int i;

	/* Update array to match the list box. */
	for (i=0; i < KEX_MAX; i++)
	    cfg->ssh_kexlist[i] = dlg_listbox_getid(ctrl, dlg, i);

    }
}

static void printerbox_handler(union control *ctrl, void *dlg,
			       void *data, int event)
{
    Config *cfg = (Config *)data;
    if (event == EVENT_REFRESH) {
	int nprinters, i;
	printer_enum *pe;

	dlg_update_start(ctrl, dlg);
	/*
	 * Some backends may wish to disable the drop-down list on
	 * this edit box. Be prepared for this.
	 */
	if (ctrl->editbox.has_list) {
	    dlg_listbox_clear(ctrl, dlg);
	    dlg_listbox_add(ctrl, dlg, PRINTER_DISABLED_STRING);
	    pe = printer_start_enum(&nprinters);
	    for (i = 0; i < nprinters; i++)
		dlg_listbox_add(ctrl, dlg, printer_get_name(pe, i));
	    printer_finish_enum(pe);
	}
	dlg_editbox_set(ctrl, dlg,
			(*cfg->printer ? cfg->printer :
			 PRINTER_DISABLED_STRING));
	dlg_update_done(ctrl, dlg);
    } else if (event == EVENT_VALCHANGE) {
	dlg_editbox_get(ctrl, dlg, cfg->printer, sizeof(cfg->printer));
	if (!strcmp(cfg->printer, PRINTER_DISABLED_STRING))
	    *cfg->printer = '\0';
    }
}

static void codepage_handler(union control *ctrl, void *dlg,
			     void *data, int event)
{
    Config *cfg = (Config *)data;
    if (event == EVENT_REFRESH) {
	int i;
	const char *cp;
	dlg_update_start(ctrl, dlg);
	strcpy(cfg->line_codepage,
	       cp_name(decode_codepage(cfg->line_codepage)));
	dlg_listbox_clear(ctrl, dlg);
	for (i = 0; (cp = cp_enumerate(i)) != NULL; i++)
	    dlg_listbox_add(ctrl, dlg, cp);
	dlg_editbox_set(ctrl, dlg, cfg->line_codepage);
	dlg_update_done(ctrl, dlg);
    } else if (event == EVENT_VALCHANGE) {
	dlg_editbox_get(ctrl, dlg, cfg->line_codepage,
			sizeof(cfg->line_codepage));
	strcpy(cfg->line_codepage,
	       cp_name(decode_codepage(cfg->line_codepage)));
    }
}

static void sshbug_handler(union control *ctrl, void *dlg,
			   void *data, int event)
{
    if (event == EVENT_REFRESH) {
	dlg_update_start(ctrl, dlg);
	dlg_listbox_clear(ctrl, dlg);
	dlg_listbox_addwithid(ctrl, dlg, "�ڵ�", AUTO);
	dlg_listbox_addwithid(ctrl, dlg, "��", FORCE_OFF);
	dlg_listbox_addwithid(ctrl, dlg, "��", FORCE_ON);
	switch (*(int *)ATOFFSET(data, ctrl->listbox.context.i)) {
	  case AUTO:      dlg_listbox_select(ctrl, dlg, 0); break;
	  case FORCE_OFF: dlg_listbox_select(ctrl, dlg, 1); break;
	  case FORCE_ON:  dlg_listbox_select(ctrl, dlg, 2); break;
	}
	dlg_update_done(ctrl, dlg);
    } else if (event == EVENT_SELCHANGE) {
	int i = dlg_listbox_index(ctrl, dlg);
	if (i < 0)
	    i = AUTO;
	else
	    i = dlg_listbox_getid(ctrl, dlg, i);
	*(int *)ATOFFSET(data, ctrl->listbox.context.i) = i;
    }
}

#define SAVEDSESSION_LEN 2048

struct sessionsaver_data {
    union control *editbox, *listbox, *loadbutton, *savebutton, *delbutton;
    union control *okbutton, *cancelbutton;
    struct sesslist sesslist;
    int midsession;
};

/* 
 * Helper function to load the session selected in the list box, if
 * any, as this is done in more than one place below. Returns 0 for
 * failure.
 */
static int load_selected_session(struct sessionsaver_data *ssd,
				 char *savedsession,
				 void *dlg, Config *cfg, int *maybe_launch)
{
    int i = dlg_listbox_index(ssd->listbox, dlg);
    int isdef;
    if (i < 0) {
	dlg_beep(dlg);
	return 0;
    }
    isdef = !strcmp(ssd->sesslist.sessions[i], "�⺻ ����");
    load_settings(ssd->sesslist.sessions[i], cfg);
    if (!isdef) {
	strncpy(savedsession, ssd->sesslist.sessions[i],
		SAVEDSESSION_LEN);
	savedsession[SAVEDSESSION_LEN-1] = '\0';
	if (maybe_launch)
	    *maybe_launch = TRUE;
    } else {
	savedsession[0] = '\0';
	if (maybe_launch)
	    *maybe_launch = FALSE;
    }
    dlg_refresh(NULL, dlg);
    /* Restore the selection, which might have been clobbered by
     * changing the value of the edit box. */
    dlg_listbox_select(ssd->listbox, dlg, i);
    return 1;
}

static void sessionsaver_handler(union control *ctrl, void *dlg,
				 void *data, int event)
{
    Config *cfg = (Config *)data;
    struct sessionsaver_data *ssd =
	(struct sessionsaver_data *)ctrl->generic.context.p;
    char *savedsession;

    /*
     * The first time we're called in a new dialog, we must
     * allocate space to store the current contents of the saved
     * session edit box (since it must persist even when we switch
     * panels, but is not part of the Config).
     */
    if (!ssd->editbox) {
        savedsession = NULL;
    } else if (!dlg_get_privdata(ssd->editbox, dlg)) {
	savedsession = (char *)
	    dlg_alloc_privdata(ssd->editbox, dlg, SAVEDSESSION_LEN);
	savedsession[0] = '\0';
    } else {
	savedsession = dlg_get_privdata(ssd->editbox, dlg);
    }

    if (event == EVENT_REFRESH) {
	if (ctrl == ssd->editbox) {
	    dlg_editbox_set(ctrl, dlg, savedsession);
	} else if (ctrl == ssd->listbox) {
	    int i;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    for (i = 0; i < ssd->sesslist.nsessions; i++)
		dlg_listbox_add(ctrl, dlg, ssd->sesslist.sessions[i]);
	    dlg_update_done(ctrl, dlg);
	}
    } else if (event == EVENT_VALCHANGE) {
        int top, bottom, halfway, i;
	if (ctrl == ssd->editbox) {
	    dlg_editbox_get(ctrl, dlg, savedsession,
			    SAVEDSESSION_LEN);
	    top = ssd->sesslist.nsessions;
	    bottom = -1;
	    while (top-bottom > 1) {
	        halfway = (top+bottom)/2;
	        i = strcmp(savedsession, ssd->sesslist.sessions[halfway]);
	        if (i <= 0 ) {
		    top = halfway;
	        } else {
		    bottom = halfway;
	        }
	    }
	    if (top == ssd->sesslist.nsessions) {
	        top -= 1;
	    }
	    dlg_listbox_select(ssd->listbox, dlg, top);
	}
    } else if (event == EVENT_ACTION) {
	int mbl = FALSE;
	if (!ssd->midsession &&
	    (ctrl == ssd->listbox ||
	     (ssd->loadbutton && ctrl == ssd->loadbutton))) {
	    /*
	     * The user has double-clicked a session, or hit Load.
	     * We must load the selected session, and then
	     * terminate the configuration dialog _if_ there was a
	     * double-click on the list box _and_ that session
	     * contains a hostname.
	     */
	    if (load_selected_session(ssd, savedsession, dlg, cfg, &mbl) &&
		(mbl && ctrl == ssd->listbox && cfg_launchable(cfg))) {
		dlg_end(dlg, 1);       /* it's all over, and succeeded */
	    }
	} else if (ctrl == ssd->savebutton) {
	    int isdef = !strcmp(savedsession, "�⺻ ����");
	    if (!savedsession[0]) {
		int i = dlg_listbox_index(ssd->listbox, dlg);
		if (i < 0) {
		    dlg_beep(dlg);
		    return;
		}
		isdef = !strcmp(ssd->sesslist.sessions[i], "�⺻ ����");
		if (!isdef) {
		    strncpy(savedsession, ssd->sesslist.sessions[i],
			    SAVEDSESSION_LEN);
		    savedsession[SAVEDSESSION_LEN-1] = '\0';
		} else {
		    savedsession[0] = '\0';
		}
	    }
            {
                char *errmsg = save_settings(savedsession, cfg);
                if (errmsg) {
                    dlg_error_msg(dlg, errmsg);
                    sfree(errmsg);
                }
            }
	    get_sesslist(&ssd->sesslist, FALSE);
	    get_sesslist(&ssd->sesslist, TRUE);
	    dlg_refresh(ssd->editbox, dlg);
	    dlg_refresh(ssd->listbox, dlg);
	} else if (!ssd->midsession &&
		   ssd->delbutton && ctrl == ssd->delbutton) {
	    int i = dlg_listbox_index(ssd->listbox, dlg);
	    if (i <= 0) {
		dlg_beep(dlg);
	    } else {
		del_settings(ssd->sesslist.sessions[i]);
		get_sesslist(&ssd->sesslist, FALSE);
		get_sesslist(&ssd->sesslist, TRUE);
		dlg_refresh(ssd->listbox, dlg);
	    }
	} else if (ctrl == ssd->okbutton) {
            if (ssd->midsession) {
                /* In a mid-session Change Settings, Apply is always OK. */
		dlg_end(dlg, 1);
                return;
            }
	    /*
	     * Annoying special case. If the `Open' button is
	     * pressed while no host name is currently set, _and_
	     * the session list previously had the focus, _and_
	     * there was a session selected in that which had a
	     * valid host name in it, then load it and go.
	     */
	    if (dlg_last_focused(ctrl, dlg) == ssd->listbox &&
		!cfg_launchable(cfg)) {
		Config cfg2;
		int mbl = FALSE;
		if (!load_selected_session(ssd, savedsession, dlg,
					   &cfg2, &mbl)) {
		    dlg_beep(dlg);
		    return;
		}
		/* If at this point we have a valid session, go! */
		if (mbl && cfg_launchable(&cfg2)) {
		    *cfg = cfg2;       /* structure copy */
		    cfg->remote_cmd_ptr = NULL;
		    dlg_end(dlg, 1);
		} else
		    dlg_beep(dlg);
                return;
	    }

	    /*
	     * Otherwise, do the normal thing: if we have a valid
	     * session, get going.
	     */
	    if (cfg_launchable(cfg)) {
		dlg_end(dlg, 1);
	    } else
		dlg_beep(dlg);
	} else if (ctrl == ssd->cancelbutton) {
	    dlg_end(dlg, 0);
	}
    }
}

struct charclass_data {
    union control *listbox, *editbox, *button;
};

static void charclass_handler(union control *ctrl, void *dlg,
			      void *data, int event)
{
    Config *cfg = (Config *)data;
    struct charclass_data *ccd =
	(struct charclass_data *)ctrl->generic.context.p;

    if (event == EVENT_REFRESH) {
	if (ctrl == ccd->listbox) {
	    int i;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    for (i = 0; i < 128; i++) {
		char str[100];
		sprintf(str, "%d\t(0x%02X)\t%c\t%d", i, i,
			(i >= 0x21 && i != 0x7F) ? i : ' ', cfg->wordness[i]);
		dlg_listbox_add(ctrl, dlg, str);
	    }
	    dlg_update_done(ctrl, dlg);
	}
    } else if (event == EVENT_ACTION) {
	if (ctrl == ccd->button) {
	    char str[100];
	    int i, n;
	    dlg_editbox_get(ccd->editbox, dlg, str, sizeof(str));
	    n = atoi(str);
	    for (i = 0; i < 128; i++) {
		if (dlg_listbox_issel(ccd->listbox, dlg, i))
		    cfg->wordness[i] = n;
	    }
	    dlg_refresh(ccd->listbox, dlg);
	}
    }
}

struct colour_data {
    union control *listbox, *redit, *gedit, *bedit, *button;
};

static const char *const colours[] = {
    "�⺻ ���ڻ�", "�⺻ ���ڻ�(����)",
    "�⺻ ����", "�⺻ ����(����)",
    "Ŀ�� ����", "Ŀ�� ����",
    "ANSI ������", "ANSI ������(����)",
    "ANSI ����", "ANSI ����(����)",
    "ANSI ���", "ANSI ���(����)",
    "ANSI ���", "ANSI ���(����)",
    "ANSI �Ķ�", "ANSI �Ķ�(����)",
    "ANSI ����Ÿ", "ANSI ����Ÿ(����)",
    "ANSI �ϴû�", "ANSI �ϴû�(����)",
    "ANSI �Ͼ�", "ANSI �Ͼ�(����)"
};

static void colour_handler(union control *ctrl, void *dlg,
			    void *data, int event)
{
    Config *cfg = (Config *)data;
    struct colour_data *cd =
	(struct colour_data *)ctrl->generic.context.p;
    int update = FALSE, r, g, b;

    if (event == EVENT_REFRESH) {
	if (ctrl == cd->listbox) {
	    int i;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    for (i = 0; i < lenof(colours); i++)
		dlg_listbox_add(ctrl, dlg, colours[i]);
	    dlg_update_done(ctrl, dlg);
	    dlg_editbox_set(cd->redit, dlg, "");
	    dlg_editbox_set(cd->gedit, dlg, "");
	    dlg_editbox_set(cd->bedit, dlg, "");
	}
    } else if (event == EVENT_SELCHANGE) {
	if (ctrl == cd->listbox) {
	    /* The user has selected a colour. Update the RGB text. */
	    int i = dlg_listbox_index(ctrl, dlg);
	    if (i < 0) {
		dlg_beep(dlg);
		return;
	    }
	    r = cfg->colours[i][0];
	    g = cfg->colours[i][1];
	    b = cfg->colours[i][2];
	    update = TRUE;
	}
    } else if (event == EVENT_VALCHANGE) {
	if (ctrl == cd->redit || ctrl == cd->gedit || ctrl == cd->bedit) {
	    /* The user has changed the colour using the edit boxes. */
	    char buf[80];
	    int i, cval;

	    dlg_editbox_get(ctrl, dlg, buf, lenof(buf));
	    cval = atoi(buf);
	    if (cval > 255) cval = 255;
	    if (cval < 0)   cval = 0;

	    i = dlg_listbox_index(cd->listbox, dlg);
	    if (i >= 0) {
		if (ctrl == cd->redit)
		    cfg->colours[i][0] = cval;
		else if (ctrl == cd->gedit)
		    cfg->colours[i][1] = cval;
		else if (ctrl == cd->bedit)
		    cfg->colours[i][2] = cval;
	    }
	}
    } else if (event == EVENT_ACTION) {
	if (ctrl == cd->button) {
	    int i = dlg_listbox_index(cd->listbox, dlg);
	    if (i < 0) {
		dlg_beep(dlg);
		return;
	    }
	    /*
	     * Start a colour selector, which will send us an
	     * EVENT_CALLBACK when it's finished and allow us to
	     * pick up the results.
	     */
	    dlg_coloursel_start(ctrl, dlg,
				cfg->colours[i][0],
				cfg->colours[i][1],
				cfg->colours[i][2]);
	}
    } else if (event == EVENT_CALLBACK) {
	if (ctrl == cd->button) {
	    int i = dlg_listbox_index(cd->listbox, dlg);
	    /*
	     * Collect the results of the colour selector. Will
	     * return nonzero on success, or zero if the colour
	     * selector did nothing (user hit Cancel, for example).
	     */
	    if (dlg_coloursel_results(ctrl, dlg, &r, &g, &b)) {
		cfg->colours[i][0] = r;
		cfg->colours[i][1] = g;
		cfg->colours[i][2] = b;
		update = TRUE;
	    }
	}
    }

    if (update) {
	char buf[40];
	sprintf(buf, "%d", r); dlg_editbox_set(cd->redit, dlg, buf);
	sprintf(buf, "%d", g); dlg_editbox_set(cd->gedit, dlg, buf);
	sprintf(buf, "%d", b); dlg_editbox_set(cd->bedit, dlg, buf);
    }
}

struct ttymodes_data {
    union control *modelist, *valradio, *valbox;
    union control *addbutton, *rembutton, *listbox;
};

static void ttymodes_handler(union control *ctrl, void *dlg,
			     void *data, int event)
{
    Config *cfg = (Config *)data;
    struct ttymodes_data *td =
	(struct ttymodes_data *)ctrl->generic.context.p;

    if (event == EVENT_REFRESH) {
	if (ctrl == td->listbox) {
	    char *p = cfg->ttymodes;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    while (*p) {
		int tabpos = strchr(p, '\t') - p;
		char *disp = dupprintf("%.*s\t%s", tabpos, p,
				       (p[tabpos+1] == 'A') ? "�ڵ�" :
				       p+tabpos+2);
		dlg_listbox_add(ctrl, dlg, disp);
		p += strlen(p) + 1;
		sfree(disp);
	    }
	    dlg_update_done(ctrl, dlg);
	} else if (ctrl == td->modelist) {
	    int i;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    for (i = 0; ttymodes[i]; i++)
		dlg_listbox_add(ctrl, dlg, ttymodes[i]);
	    dlg_listbox_select(ctrl, dlg, 0); /* *shrug* */
	    dlg_update_done(ctrl, dlg);
	} else if (ctrl == td->valradio) {
	    dlg_radiobutton_set(ctrl, dlg, 0);
	}
    } else if (event == EVENT_ACTION) {
	if (ctrl == td->addbutton) {
	    int ind = dlg_listbox_index(td->modelist, dlg);
	    if (ind >= 0) {
		char type = dlg_radiobutton_get(td->valradio, dlg) ? 'V' : 'A';
		int slen, left;
		char *p, str[lenof(cfg->ttymodes)];
		/* Construct new entry */
		memset(str, 0, lenof(str));
		strncpy(str, ttymodes[ind], lenof(str)-3);
		slen = strlen(str);
		str[slen] = '\t';
		str[slen+1] = type;
		slen += 2;
		if (type == 'V') {
		    dlg_editbox_get(td->valbox, dlg, str+slen, lenof(str)-slen);
		}
		/* Find end of list, deleting any existing instance */
		p = cfg->ttymodes;
		left = lenof(cfg->ttymodes);
		while (*p) {
		    int t = strchr(p, '\t') - p;
		    if (t == strlen(ttymodes[ind]) &&
			strncmp(p, ttymodes[ind], t) == 0) {
			memmove(p, p+strlen(p)+1, left - (strlen(p)+1));
			continue;
		    }
		    left -= strlen(p) + 1;
		    p    += strlen(p) + 1;
		}
		/* Append new entry */
		memset(p, 0, left);
		strncpy(p, str, left - 2);
		dlg_refresh(td->listbox, dlg);
	    } else
		dlg_beep(dlg);
	} else if (ctrl == td->rembutton) {
	    char *p = cfg->ttymodes;
	    int i = 0, len = lenof(cfg->ttymodes);
	    while (*p) {
		int multisel = dlg_listbox_index(td->listbox, dlg) < 0;
		if (dlg_listbox_issel(td->listbox, dlg, i)) {
		    if (!multisel) {
			/* Populate controls with entry we're about to
			 * delete, for ease of editing.
			 * (If multiple entries were selected, don't
			 * touch the controls.) */
			char *val = strchr(p, '\t');
			if (val) {
			    int ind = 0;
			    val++;
			    while (ttymodes[ind]) {
				if (strlen(ttymodes[ind]) == val-p-1 &&
				    !strncmp(ttymodes[ind], p, val-p-1))
				    break;
				ind++;
			    }
			    dlg_listbox_select(td->modelist, dlg, ind);
			    dlg_radiobutton_set(td->valradio, dlg,
						(*val == 'V'));
			    dlg_editbox_set(td->valbox, dlg, val+1);
			}
		    }
		    memmove(p, p+strlen(p)+1, len - (strlen(p)+1));
		    i++;
		    continue;
		}
		len -= strlen(p) + 1;
		p   += strlen(p) + 1;
		i++;
	    }
	    memset(p, 0, lenof(cfg->ttymodes) - len);
	    dlg_refresh(td->listbox, dlg);
	}
    }
}

struct environ_data {
    union control *varbox, *valbox, *addbutton, *rembutton, *listbox;
};

static void environ_handler(union control *ctrl, void *dlg,
			    void *data, int event)
{
    Config *cfg = (Config *)data;
    struct environ_data *ed =
	(struct environ_data *)ctrl->generic.context.p;

    if (event == EVENT_REFRESH) {
	if (ctrl == ed->listbox) {
	    char *p = cfg->environmt;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    while (*p) {
		dlg_listbox_add(ctrl, dlg, p);
		p += strlen(p) + 1;
	    }
	    dlg_update_done(ctrl, dlg);
	}
    } else if (event == EVENT_ACTION) {
	if (ctrl == ed->addbutton) {
	    char str[sizeof(cfg->environmt)];
	    char *p;
	    dlg_editbox_get(ed->varbox, dlg, str, sizeof(str)-1);
	    if (!*str) {
		dlg_beep(dlg);
		return;
	    }
	    p = str + strlen(str);
	    *p++ = '\t';
	    dlg_editbox_get(ed->valbox, dlg, p, sizeof(str)-1 - (p - str));
	    if (!*p) {
		dlg_beep(dlg);
		return;
	    }
	    p = cfg->environmt;
	    while (*p) {
		while (*p)
		    p++;
		p++;
	    }
	    if ((p - cfg->environmt) + strlen(str) + 2 <
		sizeof(cfg->environmt)) {
		strcpy(p, str);
		p[strlen(str) + 1] = '\0';
		dlg_listbox_add(ed->listbox, dlg, str);
		dlg_editbox_set(ed->varbox, dlg, "");
		dlg_editbox_set(ed->valbox, dlg, "");
	    } else {
		dlg_error_msg(dlg, "ȯ�� ������ �ʹ� Ů�ϴ�");
	    }
	} else if (ctrl == ed->rembutton) {
	    int i = dlg_listbox_index(ed->listbox, dlg);
	    if (i < 0) {
		dlg_beep(dlg);
	    } else {
		char *p, *q, *str;

		dlg_listbox_del(ed->listbox, dlg, i);
		p = cfg->environmt;
		while (i > 0) {
		    if (!*p)
			goto disaster;
		    while (*p)
			p++;
		    p++;
		    i--;
		}
		q = p;
		if (!*p)
		    goto disaster;
		/* Populate controls with the entry we're about to delete
		 * for ease of editing */
		str = p;
		p = strchr(p, '\t');
		if (!p)
		    goto disaster;
		*p = '\0';
		dlg_editbox_set(ed->varbox, dlg, str);
		p++;
		str = p;
		dlg_editbox_set(ed->valbox, dlg, str);
		p = strchr(p, '\0');
		if (!p)
		    goto disaster;
		p++;
		while (*p) {
		    while (*p)
			*q++ = *p++;
		    *q++ = *p++;
		}
		*q = '\0';
		disaster:;
	    }
	}
    }
}

struct portfwd_data {
    union control *addbutton, *rembutton, *listbox;
    union control *sourcebox, *destbox, *direction;
#ifndef NO_IPV6
    union control *addressfamily;
#endif
};

static void portfwd_handler(union control *ctrl, void *dlg,
			    void *data, int event)
{
    Config *cfg = (Config *)data;
    struct portfwd_data *pfd =
	(struct portfwd_data *)ctrl->generic.context.p;

    if (event == EVENT_REFRESH) {
	if (ctrl == pfd->listbox) {
	    char *p = cfg->portfwd;
	    dlg_update_start(ctrl, dlg);
	    dlg_listbox_clear(ctrl, dlg);
	    while (*p) {
		dlg_listbox_add(ctrl, dlg, p);
		p += strlen(p) + 1;
	    }
	    dlg_update_done(ctrl, dlg);
	} else if (ctrl == pfd->direction) {
	    /*
	     * Default is Local.
	     */
	    dlg_radiobutton_set(ctrl, dlg, 0);
#ifndef NO_IPV6
	} else if (ctrl == pfd->addressfamily) {
	    dlg_radiobutton_set(ctrl, dlg, 0);
#endif
	}
    } else if (event == EVENT_ACTION) {
	if (ctrl == pfd->addbutton) {
	    char str[sizeof(cfg->portfwd)];
	    char *p;
	    int i, type;
	    int whichbutton;

	    i = 0;
#ifndef NO_IPV6
	    whichbutton = dlg_radiobutton_get(pfd->addressfamily, dlg);
	    if (whichbutton == 1)
		str[i++] = '4';
	    else if (whichbutton == 2)
		str[i++] = '6';
#endif

	    whichbutton = dlg_radiobutton_get(pfd->direction, dlg);
	    if (whichbutton == 0)
		type = 'L';
	    else if (whichbutton == 1)
		type = 'R';
	    else
		type = 'D';
	    str[i++] = type;

	    dlg_editbox_get(pfd->sourcebox, dlg, str+i, sizeof(str) - i);
	    if (!str[i]) {
		dlg_error_msg(dlg, "���� ��Ʈ ��ȣ�� �����ž� �մϴ�");
		return;
	    }
	    p = str + strlen(str);
	    if (type != 'D') {
		*p++ = '\t';
		dlg_editbox_get(pfd->destbox, dlg, p,
				sizeof(str) - (p - str));
		if (!*p || !strchr(p, ':')) {
		    dlg_error_msg(dlg,
				  "��� �ּҴ� ���� �������� �����ž� �մϴ�:\n\"ȣ��Ʈ.�̸�:��Ʈ\"");
		    return;
		}
	    } else
		*p = '\0';
	    p = cfg->portfwd;
	    while (*p) {
		while (*p)
		    p++;
		p++;
	    }
	    if ((p - cfg->portfwd) + strlen(str) + 2 <=
		sizeof(cfg->portfwd)) {
		strcpy(p, str);
		p[strlen(str) + 1] = '\0';
		dlg_listbox_add(pfd->listbox, dlg, str);
		dlg_editbox_set(pfd->sourcebox, dlg, "");
		dlg_editbox_set(pfd->destbox, dlg, "");
	    } else {
		dlg_error_msg(dlg, "�������� �ʹ� �����ϴ�");
	    }
	} else if (ctrl == pfd->rembutton) {
	    int i = dlg_listbox_index(pfd->listbox, dlg);
	    if (i < 0)
		dlg_beep(dlg);
	    else {
		char *p, *q, *src, *dst;
		char dir;

		dlg_listbox_del(pfd->listbox, dlg, i);
		p = cfg->portfwd;
		while (i > 0) {
		    if (!*p)
			goto disaster2;
		    while (*p)
			p++;
		    p++;
		    i--;
		}
		q = p;
		if (!*p)
		    goto disaster2;
		/* Populate the controls with the entry we're about to
		 * delete, for ease of editing. */
		{
		    static const char *const afs = "A46";
		    char *afp = strchr(afs, *p);
		    int idx = afp ? afp-afs : 0;
		    if (afp)
			p++;
#ifndef NO_IPV6
		    dlg_radiobutton_set(pfd->addressfamily, dlg, idx);
#endif
		}
		{
		    static const char *const dirs = "LRD";
		    dir = *p;
		    dlg_radiobutton_set(pfd->direction, dlg,
					strchr(dirs, dir) - dirs);
		}
		p++;
		if (dir != 'D') {
		    src = p;
		    p = strchr(p, '\t');
		    if (!p)
			goto disaster2;
		    *p = '\0';
		    p++;
		    dst = p;
		} else {
		    src = p;
		    dst = "";
		}
		p = strchr(p, '\0');
		if (!p)
		    goto disaster2;
		dlg_editbox_set(pfd->sourcebox, dlg, src);
		dlg_editbox_set(pfd->destbox, dlg, dst);
		p++;
		while (*p) {
		    while (*p)
			*q++ = *p++;
		    *q++ = *p++;
		}
		*q = '\0';
		disaster2:;
	    }
	}
    }
}

void setup_config_box(struct controlbox *b, int midsession,
		      int protocol, int protcfginfo)
{
    struct controlset *s;
    struct sessionsaver_data *ssd;
    struct charclass_data *ccd;
    struct colour_data *cd;
    struct ttymodes_data *td;
    struct environ_data *ed;
    struct portfwd_data *pfd;
    union control *c;
    char *str;

    ssd = (struct sessionsaver_data *)
	ctrl_alloc(b, sizeof(struct sessionsaver_data));
    memset(ssd, 0, sizeof(*ssd));
    ssd->midsession = midsession;

    /*
     * The standard panel that appears at the bottom of all panels:
     * Open, Cancel, Apply etc.
     */
    s = ctrl_getset(b, "", "", "");
    ctrl_columns(s, 5, 20, 20, 20, 20, 20);
    ssd->okbutton = ctrl_pushbutton(s,
				    (midsession ? "���� (A)" : "���� (O)"),
				    (char)(midsession ? 'a' : 'o'),
				    HELPCTX(no_help),
				    sessionsaver_handler, P(ssd));
    ssd->okbutton->button.isdefault = TRUE;
    ssd->okbutton->generic.column = 3;
    ssd->cancelbutton = ctrl_pushbutton(s, "��� (C)", 'c', HELPCTX(no_help),
					sessionsaver_handler, P(ssd));
    ssd->cancelbutton->button.iscancel = TRUE;
    ssd->cancelbutton->generic.column = 4;
    /* We carefully don't close the 5-column part, so that platform-
     * specific add-ons can put extra buttons alongside Open and Cancel. */

    /*
     * The Session panel.
     */
    str = dupprintf("%s ���� �⺻ �ɼ�", appname);
    ctrl_settitle(b, "����", str);
    sfree(str);

    if (!midsession) {
	struct hostport *hp = (struct hostport *)
	    ctrl_alloc(b, sizeof(struct hostport));

	s = ctrl_getset(b, "����", "hostport",
			"���� ��� ����");
	ctrl_columns(s, 2, 75, 25);
	c = ctrl_editbox(s, HOST_BOX_TITLE, 'n', 100,
			 HELPCTX(session_hostname),
			 config_host_handler, I(0), I(0));
	c->generic.column = 0;
	hp->host = c;
	c = ctrl_editbox(s, PORT_BOX_TITLE, 'p', 100,
			 HELPCTX(session_hostname),
			 config_port_handler, I(0), I(0));
	c->generic.column = 1;
	hp->port = c;
	ctrl_columns(s, 1, 100);

	if (!have_backend(PROT_SSH)) {
	    ctrl_radiobuttons(s, "���� ����:", NO_SHORTCUT, 3,
			      HELPCTX(session_hostname),
			      config_protocolbuttons_handler, P(hp),
			      "��¥ (R)", 'r', I(PROT_RAW),
			      "Telnet", 't', I(PROT_TELNET),
			      "Rlogin", 'i', I(PROT_RLOGIN),
			      NULL);
	} else {
	    ctrl_radiobuttons(s, "���� ����:", NO_SHORTCUT, 4,
			      HELPCTX(session_hostname),
			      config_protocolbuttons_handler, P(hp),
			      "��¥ (R)", 'r', I(PROT_RAW),
			      "Telnet", 't', I(PROT_TELNET),
			      "Rlogin", 'i', I(PROT_RLOGIN),
			      "SSH", 's', I(PROT_SSH),
			      NULL);
	}
    }

    /*
     * The Load/Save panel is available even in mid-session.
     */
    s = ctrl_getset(b, "����", "savedsessions",
		    midsession ? "���� ���� ���� ����" :
		    "����� ������ �ҷ�����, ����, ����");
    ctrl_columns(s, 2, 75, 25);
    get_sesslist(&ssd->sesslist, TRUE);
    ssd->editbox = ctrl_editbox(s, "����� ���� (E)", 'e', 100,
				HELPCTX(session_saved),
				sessionsaver_handler, P(ssd), P(NULL));
    ssd->editbox->generic.column = 0;
    /* Reset columns so that the buttons are alongside the list, rather
     * than alongside that edit box. */
    ctrl_columns(s, 1, 100);
    ctrl_columns(s, 2, 75, 25);
    ssd->listbox = ctrl_listbox(s, NULL, NO_SHORTCUT,
				HELPCTX(session_saved),
				sessionsaver_handler, P(ssd));
    ssd->listbox->generic.column = 0;
    ssd->listbox->listbox.height = 7;
    if (!midsession) {
	ssd->loadbutton = ctrl_pushbutton(s, "�ҷ���(L)", 'l',
					  HELPCTX(session_saved),
					  sessionsaver_handler, P(ssd));
	ssd->loadbutton->generic.column = 1;
    } else {
	/* We can't offer the Load button mid-session, as it would allow the
	 * user to load and subsequently save settings they can't see. (And
	 * also change otherwise immutable settings underfoot; that probably
	 * shouldn't be a problem, but.) */
	ssd->loadbutton = NULL;
    }
    /* "Save" button is permitted mid-session. */
    ssd->savebutton = ctrl_pushbutton(s, "����(V)", 'v',
				      HELPCTX(session_saved),
				      sessionsaver_handler, P(ssd));
    ssd->savebutton->generic.column = 1;
    if (!midsession) {
	ssd->delbutton = ctrl_pushbutton(s, "����(D)", 'd',
					 HELPCTX(session_saved),
					 sessionsaver_handler, P(ssd));
	ssd->delbutton->generic.column = 1;
    } else {
	/* Disable the Delete button mid-session too, for UI consistency. */
	ssd->delbutton = NULL;
    }
    ctrl_columns(s, 1, 100);

    s = ctrl_getset(b, "����", "otheropts", NULL);
    c = ctrl_radiobuttons(s, "����ÿ� â�� ���� (W):", 'w', 4,
			  HELPCTX(session_coe),
			  dlg_stdradiobutton_handler,
			  I(offsetof(Config, close_on_exit)),
			  "�׻�", I(FORCE_ON),
			  "�� ����", I(FORCE_OFF),
			  "������ ������ ����", I(AUTO), NULL);

    /*
     * The Session/Logging panel.
     */
    ctrl_settitle(b, "����/�α�", "���� �α� ���� �ɼ��� �����մϴ�");

    s = ctrl_getset(b, "����/�α�", "main", NULL);
    /*
     * The logging buttons change depending on whether SSH packet
     * logging can sensibly be available.
     */
    {
	char *sshlogname, *sshrawlogname;
	if ((midsession && protocol == PROT_SSH) ||
	    (!midsession && have_backend(PROT_SSH))) {
	    sshlogname = "SSH ��Ŷ";
	    sshrawlogname = "SSH packets and raw data";
        } else {
	    sshlogname = NULL;	       /* this will disable both buttons */
	    sshrawlogname = NULL;      /* this will just placate optimisers */
        }
	ctrl_radiobuttons(s, "���� �α�:", NO_SHORTCUT, 2,
			  HELPCTX(logging_main),
			  loggingbuttons_handler,
			  I(offsetof(Config, logtype)),
			  "������� ���� (T)", 't', I(LGTYP_NONE),
			  "��� ������ ���ڸ� (P)", 'p', I(LGTYP_ASCII),
			  "��� ���� ��� (L)", 'l', I(LGTYP_DEBUG),
			  sshlogname, 's', I(LGTYP_PACKETS),
			  sshrawlogname, 'r', I(LGTYP_SSHRAW),
			  NULL);
    }
    ctrl_filesel(s, "�α� ���� �̸� (F):", 'f',
		 NULL, TRUE, "���� �α� ���� �̸� ����",
		 HELPCTX(logging_filename),
		 dlg_stdfilesel_handler, I(offsetof(Config, logfilename)));
    ctrl_text(s, "(�α� ���� �̸��� ��¥�� &Y, &M, &D, �ð��� &T, ȣ��Ʈ �̸����δ� &H�� �� �� �ֽ��ϴ�)",
	      HELPCTX(logging_filename));
    ctrl_radiobuttons(s, "�α� ������ �̹� ���� �� (E):", 'e', 1,
		      HELPCTX(logging_exists),
		      dlg_stdradiobutton_handler, I(offsetof(Config,logxfovr)),
		      "�� ���� ��", I(LGXF_OVR),
		      "�� ���� ���� �߰���", I(LGXF_APN),
		      "�Ź� ����ڿ��� ���� ��", I(LGXF_ASK), NULL);
    ctrl_checkbox(s, "�α� ������ ���� �Ѹ�", 'u',
		 HELPCTX(logging_flush),
		 dlg_stdcheckbox_handler, I(offsetof(Config,logflush)));

    if ((midsession && protocol == PROT_SSH) ||
	(!midsession && have_backend(PROT_SSH))) {
	s = ctrl_getset(b, "����/�α�", "ssh",
			"SSH ��Ŷ �α� �ɼ��� �����մϴ�");
	ctrl_checkbox(s, "�н����� ó�� ���̴� �� ���� (K)", 'k',
		      HELPCTX(logging_ssh_omit_password),
		      dlg_stdcheckbox_handler, I(offsetof(Config,logomitpass)));
	ctrl_checkbox(s, "���� ������ ���� (D)", 'd',
		      HELPCTX(logging_ssh_omit_data),
		      dlg_stdcheckbox_handler, I(offsetof(Config,logomitdata)));
    }

    /*
     * The Terminal panel.
     */
    ctrl_settitle(b, "�͹̳�", "�͹̳� ���ķ��̼� ���� �ɼ��� �����մϴ�");

    s = ctrl_getset(b, "�͹̳�", "general", "�پ��� �͹̳� �ɼ�");
    ctrl_checkbox(s, "�⺻���� �ڵ� �ٳѱ� (W)", 'w',
		  HELPCTX(terminal_autowrap),
		  dlg_stdcheckbox_handler, I(offsetof(Config,wrap_mode)));
    ctrl_checkbox(s, "DEC �� ��带 �⺻���� ��", 'd',
		  HELPCTX(terminal_decom),
		  dlg_stdcheckbox_handler, I(offsetof(Config,dec_om)));
    ctrl_checkbox(s, "CR�� ��� LF�� �߰ߵǸ� �ٳѱ� ����", 'r',
		  HELPCTX(terminal_lfhascr),
		  dlg_stdcheckbox_handler, I(offsetof(Config,lfhascr)));
    ctrl_checkbox(s, "�������� ȭ�� ���� (E)", 'e',
		  HELPCTX(terminal_bce),
		  dlg_stdcheckbox_handler, I(offsetof(Config,bce)));
    ctrl_checkbox(s, "���� ������ ��� (N)", 'n',
		  HELPCTX(terminal_blink),
		  dlg_stdcheckbox_handler, I(offsetof(Config,blinktext)));
    ctrl_editbox(s, "^E�� ���� ���� (S):", 's', 100,
		 HELPCTX(terminal_answerback),
		 dlg_stdeditbox_handler, I(offsetof(Config,answerback)),
		 I(sizeof(((Config *)0)->answerback)));

    s = ctrl_getset(b, "�͹̳�", "ldisc", "�� ���� �ɼ�");
    ctrl_radiobuttons(s, "�ڱ� ���� (L):", 'l', 3,
		      HELPCTX(terminal_localecho),
		      dlg_stdradiobutton_handler,I(offsetof(Config,localecho)),
		      "�ڵ�", I(AUTO),
		      "�׻� ��", I(FORCE_ON),
		      "�׻� ��", I(FORCE_OFF), NULL);
    ctrl_radiobuttons(s, "��ü �� ���� (T):", 't', 3,
		      HELPCTX(terminal_localedit),
		      dlg_stdradiobutton_handler,I(offsetof(Config,localedit)),
		      "�ڵ�", I(AUTO),
		      "�׻� ��", I(FORCE_ON),
		      "�׻� ��", I(FORCE_OFF), NULL);

    s = ctrl_getset(b, "�͹̳�", "printing", "���� ���� ����Ʈ");
    ctrl_combobox(s, "ANSI ������ ����� ���� ������ (P):", 'p', 100,
		  HELPCTX(terminal_printing),
		  printerbox_handler, P(NULL), P(NULL));

    /*
     * The Terminal/Keyboard panel.
     */
    ctrl_settitle(b, "�͹̳�/Ű����",
		  "Ű ȿ�� ���� �ɼ��� �����մϴ�");

    s = ctrl_getset(b, "�͹̳�/Ű����", "mappings",
		    "Ű �Է� ���� ��� ����:");
    ctrl_radiobuttons(s, "�齺���̽� Ű (B)", 'b', 2,
		      HELPCTX(keyboard_backspace),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, bksp_is_delete)),
		      "Ctrl-H", I(0), "Ctrl-? (127)", I(1), NULL);
    ctrl_radiobuttons(s, "Home�� EndŰ", 'e', 2,
		      HELPCTX(keyboard_homeend),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, rxvt_homeend)),
		      "ǥ��", I(0), "rxvt", I(1), NULL);
    ctrl_radiobuttons(s, "���Ű�� Ű�е� (F)", 'f', 3,
		      HELPCTX(keyboard_funkeys),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, funky_type)),
		      "ESC[n~", I(0), "������", I(1), "Xterm R6", I(2),
		      "VT400", I(3), "VT100+", I(4), "SCO", I(5), 
                      "����", I(6), NULL);

    s = ctrl_getset(b, "�͹̳�/Ű����", "appkeypad",
		    "���ø����̼� Ű�е� ����:");
    ctrl_radiobuttons(s, "Ŀ�� Ű �⺻ ���� (R):", 'r', 3,
		      HELPCTX(keyboard_appcursor),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, app_cursor)),
		      "�Ϲ�", I(0), "���α׷�", I(1), NULL);
    ctrl_radiobuttons(s, "���� Ű�е� �⺻ ���� (N):", 'n', 3,
		      HELPCTX(keyboard_appkeypad),
		      numeric_keypad_handler, P(NULL),
		      "�Ϲ�", I(0), "���α׷�", I(1), "����", I(2),
		      NULL);

    /*
     * The Terminal/Bell panel.
     */
    ctrl_settitle(b, "�͹̳�/��",
		  "�͹̳� �� ���� �ɼ��� �����մϴ�");

    s = ctrl_getset(b, "�͹̳�/��", "style", "�� ��Ÿ��");
    ctrl_radiobuttons(s, "�� �︲ ȿ�� (B):", 'b', 1,
		      HELPCTX(bell_style),
		      dlg_stdradiobutton_handler, I(offsetof(Config, beep)),
		      "���� (�� ��� �� ��) ", I(BELL_DISABLED),
		      "�⺻ �ý��� ���Ҹ�", I(BELL_DEFAULT),
		      "�ð� �� (â ��½��)", I(BELL_VISUAL), NULL);

    s = ctrl_getset(b, "�͹̳�/��", "overload",
		    "�Ͻ����� ���Ҹ� ������ ����");
    ctrl_checkbox(s, "���Ҹ��� �ò����� �� �ӽ÷� ������ (D)", 'd',
		  HELPCTX(bell_overload),
		  dlg_stdcheckbox_handler, I(offsetof(Config,bellovl)));
    ctrl_editbox(s, "�ּ� �� ������ ���� ����", 'm', 20,
		 HELPCTX(bell_overload),
		 dlg_stdeditbox_handler, I(offsetof(Config,bellovl_n)), I(-1));
    ctrl_editbox(s, "���� �ð�(T) (��)", 't', 20,
		 HELPCTX(bell_overload),
		 dlg_stdeditbox_handler, I(offsetof(Config,bellovl_t)),
		 I(-TICKSPERSEC));
    ctrl_text(s, "���� �ð� �����ϸ� ���Ҹ� �ٽ� ����",
	      HELPCTX(bell_overload));
    ctrl_editbox(s, "���� �ð� (S)", 's', 20,
		 HELPCTX(bell_overload),
		 dlg_stdeditbox_handler, I(offsetof(Config,bellovl_s)),
		 I(-TICKSPERSEC));

    /*
     * The Terminal/Features panel.
     */
    ctrl_settitle(b, "�͹̳�/���",
		  "�ΰ����� �͹̳� ��ɵ��� �Ѱų� ���ϴ�.");

    s = ctrl_getset(b, "�͹̳�/���", "main", NULL);
    ctrl_checkbox(s, "���ø����̼� Ŀ�� Ű ��� ��� �� �� (U)", 'u',
		  HELPCTX(features_application),
		  dlg_stdcheckbox_handler, I(offsetof(Config,no_applic_c)));
    ctrl_checkbox(s, "���ø����̼� Ű�е� ��� ��� �� �� (K)", 'k',
		  HELPCTX(features_application),
		  dlg_stdcheckbox_handler, I(offsetof(Config,no_applic_k)));
    ctrl_checkbox(s, "xterm ��Ÿ�� ���콺 ��� �� ��", 'x',
		  HELPCTX(features_mouse),
		  dlg_stdcheckbox_handler, I(offsetof(Config,no_mouse_rep)));
    ctrl_checkbox(s, "���� â ũ�� ���� ��� �� �� (S)", 's',
		  HELPCTX(features_resize),
		  dlg_stdcheckbox_handler,
		  I(offsetof(Config,no_remote_resize)));
    ctrl_checkbox(s, "��ü �͹̳� ��ũ�� ���� ��� ��� �� �� (W)", 'w',
		  HELPCTX(features_altscreen),
		  dlg_stdcheckbox_handler, I(offsetof(Config,no_alt_screen)));
    ctrl_checkbox(s, "���� â ���� ���� ��� �� �� (T)", 't',
		  HELPCTX(features_retitle),
		  dlg_stdcheckbox_handler,
		  I(offsetof(Config,no_remote_wintitle)));
    ctrl_radiobuttons(s, "���ݿ����� â ���� �˾Ƴ��� ���� (����) (Q)", 'q', 3,
		      HELPCTX(features_qtitle),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config,remote_qtitle_action)),
		      "�������� ����", I(TITLE_NONE),
		      "�� ���ڿ�", I(TITLE_EMPTY),
		      "â ����", I(TITLE_REAL), NULL);
    ctrl_checkbox(s, "������ ������ ^? ������ ��� �� �� (B)",'b',
		  HELPCTX(features_dbackspace),
		  dlg_stdcheckbox_handler, I(offsetof(Config,no_dbackspace)));
    ctrl_checkbox(s, "���� ���� �� ���� ��� �� �� (R)",
		  'r', HELPCTX(features_charset), dlg_stdcheckbox_handler,
		  I(offsetof(Config,no_remote_charset)));
    ctrl_checkbox(s, "�ƶ��� ����� ������� ���� (L)",
		  'l', HELPCTX(features_arabicshaping), dlg_stdcheckbox_handler,
		  I(offsetof(Config, arabicshaping)));
    ctrl_checkbox(s, "�����ʿ��� �������� �۾��� ��� �� �� (D)",
		  'd', HELPCTX(features_bidi), dlg_stdcheckbox_handler,
		  I(offsetof(Config, bidi)));

    /*
     * The Window panel.
     */
    str = dupprintf("%s�� â ���� �ɼ��� �����մϴ�", appname);
    ctrl_settitle(b, "â", str);
    sfree(str);

    s = ctrl_getset(b, "â", "size", "â ũ�� ����");
    ctrl_columns(s, 2, 50, 50);
    c = ctrl_editbox(s, "���� (M)", 'm', 100,
		     HELPCTX(window_size),
		     dlg_stdeditbox_handler, I(offsetof(Config,width)), I(-1));
    c->generic.column = 0;
    c = ctrl_editbox(s, "�� (R)", 'r', 100,
		     HELPCTX(window_size),
		     dlg_stdeditbox_handler, I(offsetof(Config,height)),I(-1));
    c->generic.column = 1;
    ctrl_columns(s, 1, 100);

    s = ctrl_getset(b, "â", "scrollback",
		    "â ���� ���� �÷����� ����");
    ctrl_editbox(s, "����ȭ�� ���� �� �� (S)", 's', 50,
		 HELPCTX(window_scrollback),
		 dlg_stdeditbox_handler, I(offsetof(Config,savelines)), I(-1));
    ctrl_checkbox(s, "��ũ�� �� ���� (D)", 'd',
		  HELPCTX(window_scrollback),
		  dlg_stdcheckbox_handler, I(offsetof(Config,scrollbar)));
    ctrl_checkbox(s, "Ű �Է��� ������ ����ȭ�� ���⸦ �ʱ�ȭ (K)", 'k',
		  HELPCTX(window_scrollback),
		  dlg_stdcheckbox_handler, I(offsetof(Config,scroll_on_key)));
    ctrl_checkbox(s, "����� ������ ����ȭ�� ���⸦ �ʱ�ȭ (P)", 'p',
		  HELPCTX(window_scrollback),
		  dlg_stdcheckbox_handler, I(offsetof(Config,scroll_on_disp)));
    ctrl_checkbox(s, "������ ���� ����ȭ������ �о (E)", 'e',
		  HELPCTX(window_erased),
		  dlg_stdcheckbox_handler,
		  I(offsetof(Config,erase_to_scrollback)));

    /*
     * The Window/Appearance panel.
     */
    str = dupprintf("%s â ����� �����մϴ�.", appname);
    ctrl_settitle(b, "â/���", str);
    sfree(str);

    s = ctrl_getset(b, "â/���", "cursor",
		    "Ŀ�� ���� ����");
    ctrl_radiobuttons(s, "Ŀ�� ���:", NO_SHORTCUT, 3,
		      HELPCTX(appearance_cursor),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, cursor_type)),
		      "�簢�� (B)", 'l', I(0),
		      "���� (U)", 'u', I(1),
		      "������ (V)", 'v', I(2), NULL);
    ctrl_checkbox(s, "Ŀ�� ������ (B)", 'b',
		  HELPCTX(appearance_cursor),
		  dlg_stdcheckbox_handler, I(offsetof(Config,blink_cur)));

    s = ctrl_getset(b, "â/���", "font",
		    "�۲� ����");
    ctrl_fontsel(s, "�͹̳� �⺻ �۲� (N)", 'n',
		 HELPCTX(appearance_font),
		 dlg_stdfontsel_handler, I(offsetof(Config, font)));
    ctrl_checkbox(s, "���� �����ڵ� �۲� ��� (F)", 'f',
                 HELPCTX(no_help),
                 dlg_stdcheckbox_handler, I(offsetof(Config, use_font_unicode)));
    ctrl_fontsel(s, "�͹̳� �����ڵ� �۲� (S)", '\0',
                 HELPCTX(no_help),
                 dlg_stdfontsel_handler, I(offsetof(Config, font_unicode)));
    ctrl_editbox(s, "�����ڵ� �۲� Y ��ǥ �̼� ���� (px)", '\0', 20,
                 HELPCTX(no_help),
                 dlg_stdeditbox_handler, I(offsetof(Config, font_unicode_adj)), I(-1));

    s = ctrl_getset(b, "â/���", "mouse",
		    "���콺 ������ ���� ����");
    ctrl_checkbox(s, "�Է� �߿��� ���콺 Ŀ�� ���� (P)", 'p',
		  HELPCTX(appearance_hidemouse),
		  dlg_stdcheckbox_handler, I(offsetof(Config,hide_mouseptr)));

    s = ctrl_getset(b, "â/���", "border",
		    "âƲ ����");
    ctrl_editbox(s, "������ âƲ ����:", 'e', 20,
		 HELPCTX(appearance_border),
		 dlg_stdeditbox_handler,
		 I(offsetof(Config,window_border)), I(-1));

    /*
     * The Window/Behaviour panel.
     */
    str = dupprintf("%s â Ư���� �����մϴ�.", appname);
    ctrl_settitle(b, "â/Ư��", str);
    sfree(str);

    s = ctrl_getset(b, "â/Ư��", "title",
		    "â ���� ����");
    ctrl_editbox(s, "â ���� (T):", 't', 100,
		 HELPCTX(appearance_title),
		 dlg_stdeditbox_handler, I(offsetof(Config,wintitle)),
		 I(sizeof(((Config *)0)->wintitle)));
    ctrl_checkbox(s, "â�� ������ ������ �и� (I)", 'i',
		  HELPCTX(appearance_title),
		  dlg_stdcheckbox_handler,
		  I(CHECKBOX_INVERT | offsetof(Config,win_name_always)));

    s = ctrl_getset(b, "â/Ư��", "main", NULL);
    ctrl_checkbox(s, "â �ݱ� ���� ��� (W)", 'w',
		  HELPCTX(behaviour_closewarn),
		  dlg_stdcheckbox_handler, I(offsetof(Config,warn_on_close)));

    /*
     * The Window/Translation panel.
     */
    ctrl_settitle(b, "â/��ȯ",
		  "���� �� ��ȯ ���� �ɼ��� �����մϴ�");

    s = ctrl_getset(b, "â/��ȯ", "trans",
		    "���� �������� ���� �� ��ȯ");
    ctrl_combobox(s, "������ �����͸� �� ���ڼ����� ���� (R):",
		  'r', 100, HELPCTX(translation_codepage),
		  codepage_handler, P(NULL), P(NULL));

    s = ctrl_getset(b, "â/��ȯ", "tweaks", NULL);
    ctrl_checkbox(s, "�ʺ� �������� ���� ������ ���ڸ� �а� ǥ�� (W)", 'w',
		  HELPCTX(translation_cjk_ambig_wide),
		  dlg_stdcheckbox_handler, I(offsetof(Config,cjk_ambig_wide)));

    str = dupprintf("%s�� ���׸� ���ڸ� �ٷ�� ���", appname);
    s = ctrl_getset(b, "â/��ȯ", "linedraw", str);
    sfree(str);
    ctrl_radiobuttons(s, "���׸� ���� �ٷ��:", NO_SHORTCUT,1,
		      HELPCTX(translation_linedraw),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, vtmode)),
		      "�����ڵ� ������ ��� (U)",'u',I(VT_UNICODE),
		      "������ ���� ���׸� ���� (+, - �� |) (P)",'p',I(VT_POORMAN),
		      NULL);
    ctrl_checkbox(s, "VT100 ���׸� ���ڸ� lqqqk�� �Ѹ� (D)",'d',
		  HELPCTX(selection_linedraw),
		  dlg_stdcheckbox_handler, I(offsetof(Config,rawcnp)));

    /*
     * The Window/Selection panel.
     */
    ctrl_settitle(b, "â/����", "����, �ٿ��ֱ� ���� �ɼ��� �����մϴ�");
	
    s = ctrl_getset(b, "â/����", "mouse",
		    "���콺 ����");
    ctrl_checkbox(s, "����Ʈ�� ���ø����̼� ���콺����� ���� (P)", 'p',
		  HELPCTX(selection_shiftdrag),
		  dlg_stdcheckbox_handler, I(offsetof(Config,mouse_override)));
    ctrl_radiobuttons(s,
		      "�⺻ ���� ���� ��� (Alt+�巡�״� �ٸ�):",
		      NO_SHORTCUT, 2,
		      HELPCTX(selection_rect),
		      dlg_stdradiobutton_handler,
		      I(offsetof(Config, rect_select)),
		      "�Ϲ�", 'n', I(0),
		      "�簢 �� (R)", 'r', I(1), NULL);

    s = ctrl_getset(b, "â/����", "charclass",
		    "Ŭ���ؼ� �� �ܾ �����ϱ� ����");
    ccd = (struct charclass_data *)
	ctrl_alloc(b, sizeof(struct charclass_data));
    ccd->listbox = ctrl_listbox(s, "���� ���� (E):", 'e',
				HELPCTX(selection_charclasses),
				charclass_handler, P(ccd));
    ccd->listbox->listbox.multisel = 1;
    ccd->listbox->listbox.ncols = 4;
    ccd->listbox->listbox.percentages = snewn(4, int);
    ccd->listbox->listbox.percentages[0] = 15;
    ccd->listbox->listbox.percentages[1] = 25;
    ccd->listbox->listbox.percentages[2] = 20;
    ccd->listbox->listbox.percentages[3] = 40;
    ctrl_columns(s, 2, 67, 33);
    ccd->editbox = ctrl_editbox(s, "Ŭ���� (T)", 't', 50,
				HELPCTX(selection_charclasses),
				charclass_handler, P(ccd), P(NULL));
    ccd->editbox->generic.column = 0;
    ccd->button = ctrl_pushbutton(s, "�ٲ� (S)", 's',
				  HELPCTX(selection_charclasses),
				  charclass_handler, P(ccd));
    ccd->button->generic.column = 1;
    ctrl_columns(s, 1, 100);

    /*
     * The Window/Colours panel.
     */
    ctrl_settitle(b, "â/����", "���� ��� �ɼ��� �����մϴ�");

    s = ctrl_getset(b, "â/����", "general",
		    "������� �Ϲ� �ɼ�");
    ctrl_checkbox(s, "ANSI ���� ���� ���", 'i',
		  HELPCTX(colours_ansi),
		  dlg_stdcheckbox_handler, I(offsetof(Config,ansi_colour)));
    ctrl_checkbox(s, "xterm 256�� ��� ���", '2',
		  HELPCTX(colours_xterm256), dlg_stdcheckbox_handler,
		  I(offsetof(Config,xterm_256_colour)));
    ctrl_checkbox(s, "���� �۾��� �ٸ� ����� ǥ�� (B)", 'b',
		  HELPCTX(colours_bold),
		  dlg_stdcheckbox_handler, I(offsetof(Config,bold_colour)));

    str = dupprintf("���� �̼� ����", appname);
    s = ctrl_getset(b, "â/����", "adjust", str);
    sfree(str);
    ctrl_text(s, "��Ͽ��� ������ ���� ���� ��ư�� ���� ǥ������ �ٲټ���.",
	      HELPCTX(colours_config));
    ctrl_columns(s, 2, 67, 33);
    cd = (struct colour_data *)ctrl_alloc(b, sizeof(struct colour_data));
    cd->listbox = ctrl_listbox(s, "������ ���� ���� (U):", 'u',
			       HELPCTX(colours_config), colour_handler, P(cd));
    cd->listbox->generic.column = 0;
    cd->listbox->listbox.height = 7;
    c = ctrl_text(s, "RGB ��:", HELPCTX(colours_config));
    c->generic.column = 1;
    cd->redit = ctrl_editbox(s, "����", 'r', 50, HELPCTX(colours_config),
			     colour_handler, P(cd), P(NULL));
    cd->redit->generic.column = 1;
    cd->gedit = ctrl_editbox(s, "���", 'n', 50, HELPCTX(colours_config),
			     colour_handler, P(cd), P(NULL));
    cd->gedit->generic.column = 1;
    cd->bedit = ctrl_editbox(s, "�Ķ�", 'e', 50, HELPCTX(colours_config),
			     colour_handler, P(cd), P(NULL));
    cd->bedit->generic.column = 1;
    cd->button = ctrl_pushbutton(s, "���� (M)", 'm', HELPCTX(colours_config),
				 colour_handler, P(cd));
    cd->button->generic.column = 1;
    ctrl_columns(s, 1, 100);

    /*
     * The Connection panel. This doesn't show up if we're in a
     * non-network utility such as pterm. We tell this by being
     * passed a protocol < 0.
     */
    if (protocol >= 0) {
	ctrl_settitle(b, "����", "���� ���� �ɼ��� �����մϴ�");

	s = ctrl_getset(b, "����", "keepalive",
			"���� ������ ���� ����Ŷ ������");
	ctrl_editbox(s, "���� ���� ���� (�ʴ���, 0�� ��)", 'k', 20,
		     HELPCTX(connection_keepalive),
		     dlg_stdeditbox_handler, I(offsetof(Config,ping_interval)),
		     I(-1));

	if (!midsession) {
	    s = ctrl_getset(b, "����", "tcp",
			    "������ TCP ���� �ɼ�");
	    ctrl_checkbox(s, "���̱� �˰��� ��� �� �� (N)",
			  'n', HELPCTX(connection_nodelay),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,tcp_nodelay)));
	    ctrl_checkbox(s, "TCP �������� ��� �� (SO_KEEPALIVE �ɼ�)",
			  'p', HELPCTX(connection_tcpkeepalive),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,tcp_keepalives)));
#ifndef NO_IPV6
	    s = ctrl_getset(b, "����", "ipversion",
			  "���ͳ� �������� ����");
	    ctrl_radiobuttons(s, NULL, NO_SHORTCUT, 3,
			  HELPCTX(connection_ipversion),
			  dlg_stdradiobutton_handler,
			  I(offsetof(Config, addressfamily)),
			  "�ڵ�", 'u', I(ADDRTYPE_UNSPEC),
			  "IPv4", '4', I(ADDRTYPE_IPV4),
			  "IPv6", '6', I(ADDRTYPE_IPV6),
			  NULL);
#endif
	}

	/*
	 * A sub-panel Connection/Data, containing options that
	 * decide on data to send to the server.
	 */
	if (!midsession) {
	    ctrl_settitle(b, "����/������", "������ ���� ������");

	    s = ctrl_getset(b, "����/������", "login",
			    "�α��� ����");
	    ctrl_editbox(s, "�ڵ� �α��� ����� (U)", 'u', 50,
			 HELPCTX(connection_username),
			 dlg_stdeditbox_handler, I(offsetof(Config,username)),
			 I(sizeof(((Config *)0)->username)));

	    s = ctrl_getset(b, "����/������", "term",
			    "�͹̳� ����");
	    ctrl_editbox(s, "�͹̳� Ÿ�� ���ڿ� (T)", 't', 50,
			 HELPCTX(connection_termtype),
			 dlg_stdeditbox_handler, I(offsetof(Config,termtype)),
			 I(sizeof(((Config *)0)->termtype)));
	    ctrl_editbox(s, "�͹̳� �ӵ� (S)", 's', 50,
			 HELPCTX(connection_termspeed),
			 dlg_stdeditbox_handler, I(offsetof(Config,termspeed)),
			 I(sizeof(((Config *)0)->termspeed)));

	    s = ctrl_getset(b, "����/������", "env",
			    "ȯ�� ����");
	    ctrl_columns(s, 2, 80, 20);
	    ed = (struct environ_data *)
		ctrl_alloc(b, sizeof(struct environ_data));
	    ed->varbox = ctrl_editbox(s, "����(V)", 'v', 60,
				      HELPCTX(telnet_environ),
				      environ_handler, P(ed), P(NULL));
	    ed->varbox->generic.column = 0;
	    ed->valbox = ctrl_editbox(s, "��(L)", 'l', 60,
				      HELPCTX(telnet_environ),
				      environ_handler, P(ed), P(NULL));
	    ed->valbox->generic.column = 0;
	    ed->addbutton = ctrl_pushbutton(s, "�߰� (D)", 'd',
					    HELPCTX(telnet_environ),
					    environ_handler, P(ed));
	    ed->addbutton->generic.column = 1;
	    ed->rembutton = ctrl_pushbutton(s, "���� (R)", 'r',
					    HELPCTX(telnet_environ),
					    environ_handler, P(ed));
	    ed->rembutton->generic.column = 1;
	    ctrl_columns(s, 1, 100);
	    ed->listbox = ctrl_listbox(s, NULL, NO_SHORTCUT,
				       HELPCTX(telnet_environ),
				       environ_handler, P(ed));
	    ed->listbox->listbox.height = 3;
	    ed->listbox->listbox.ncols = 2;
	    ed->listbox->listbox.percentages = snewn(2, int);
	    ed->listbox->listbox.percentages[0] = 30;
	    ed->listbox->listbox.percentages[1] = 70;
	}

    }

    if (!midsession) {
	/*
	 * The Connection/Proxy panel.
	 */
	ctrl_settitle(b, "����/������",
		      "������ ���� �ɼ��� �����մϴ�");

	s = ctrl_getset(b, "����/������", "basics", NULL);
	ctrl_radiobuttons(s, "������ ���� (T):", 't', 3,
			  HELPCTX(proxy_type),
			  dlg_stdradiobutton_handler,
			  I(offsetof(Config, proxy_type)),
			  "����", I(PROXY_NONE),
			  "SOCKS 4", I(PROXY_SOCKS4),
			  "SOCKS 5", I(PROXY_SOCKS5),
			  "HTTP", I(PROXY_HTTP),
			  "Telnet", I(PROXY_TELNET),
			  NULL);
	ctrl_columns(s, 2, 80, 20);
	c = ctrl_editbox(s, "������ ȣ��Ʈ (Y)", 'y', 100,
			 HELPCTX(proxy_main),
			 dlg_stdeditbox_handler,
			 I(offsetof(Config,proxy_host)),
			 I(sizeof(((Config *)0)->proxy_host)));
	c->generic.column = 0;
	c = ctrl_editbox(s, "��Ʈ (P)", 'p', 100,
			 HELPCTX(proxy_main),
			 dlg_stdeditbox_handler,
			 I(offsetof(Config,proxy_port)),
			 I(-1));
	c->generic.column = 1;
	ctrl_columns(s, 1, 100);
	ctrl_editbox(s, "������ ȣ��Ʈ/IP (E)", 'e', 100,
		     HELPCTX(proxy_exclude),
		     dlg_stdeditbox_handler,
		     I(offsetof(Config,proxy_exclude_list)),
		     I(sizeof(((Config *)0)->proxy_exclude_list)));
	ctrl_checkbox(s, "���� ȣ��Ʈ ������ �����ÿ��� ����� (X)", 'x',
		      HELPCTX(proxy_exclude),
		      dlg_stdcheckbox_handler,
		      I(offsetof(Config,even_proxy_localhost)));
	ctrl_radiobuttons(s, "������ ���ӿ� ���� DNS ������ ã��:", 'd', 3,
			  HELPCTX(proxy_dns),
			  dlg_stdradiobutton_handler,
			  I(offsetof(Config, proxy_dns)),
			  "�ƴϿ�", I(FORCE_OFF),
			  "�ڵ�", I(AUTO),
			  "��", I(FORCE_ON), NULL);
	ctrl_editbox(s, "����ڸ� (U)", 'u', 60,
		     HELPCTX(proxy_auth),
		     dlg_stdeditbox_handler,
		     I(offsetof(Config,proxy_username)),
		     I(sizeof(((Config *)0)->proxy_username)));
	c = ctrl_editbox(s, "��ȣ (W)", 'w', 60,
			 HELPCTX(proxy_auth),
			 dlg_stdeditbox_handler,
			 I(offsetof(Config,proxy_password)),
			 I(sizeof(((Config *)0)->proxy_password)));
	c->editbox.password = 1;
	ctrl_editbox(s, "�ڳ� ��� (M)", 'm', 100,
		     HELPCTX(proxy_command),
		     dlg_stdeditbox_handler,
		     I(offsetof(Config,proxy_telnet_command)),
		     I(sizeof(((Config *)0)->proxy_telnet_command)));
    }

    /*
     * The Telnet panel exists in the base config box, and in a
     * mid-session reconfig box _if_ we're using Telnet.
     */
    if (!midsession || protocol == PROT_TELNET) {
	/*
	 * The Connection/Telnet panel.
	 */
	ctrl_settitle(b, "����/�ڳ�",
		      "�ڳ� ���� ���� �ɼ��� �����մϴ�");

	s = ctrl_getset(b, "����/�ڳ�", "protocol",
			"�ڳ� �������� ����");

	if (!midsession) {
	    ctrl_radiobuttons(s, "OLD_ENVIRON ��ȣ�� �ٷ��:",
			      NO_SHORTCUT, 2,
			      HELPCTX(telnet_oldenviron),
			      dlg_stdradiobutton_handler,
			      I(offsetof(Config, rfc_environ)),
			      "BSD (���)", 'b', I(0),
			      "RFC 1408 (�� ����)", 'f', I(1), NULL);
	    ctrl_radiobuttons(s, "�ڳ� ���� ��� (T):", 't', 2,
			      HELPCTX(telnet_passive),
			      dlg_stdradiobutton_handler,
			      I(offsetof(Config, passive_telnet)),
			      "����", I(1), "�ɵ�", I(0), NULL);
	}
	ctrl_checkbox(s, "Ű����� �ڳ� Ư�� ��� ���� (K)", 'k',
		      HELPCTX(telnet_specialkeys),
		      dlg_stdcheckbox_handler,
		      I(offsetof(Config,telnet_keyboard)));
	ctrl_checkbox(s, "CR��� �ڳ� New Line ���ڸ� ����",
		      'm', HELPCTX(telnet_newline),
		      dlg_stdcheckbox_handler,
		      I(offsetof(Config,telnet_newline)));
    }

    if (!midsession) {

	/*
	 * The Connection/Rlogin panel.
	 */
	ctrl_settitle(b, "����/rlogin",
		      "Rlogin ���� ���� �ɼ��� �����մϴ�");

	s = ctrl_getset(b, "����/rlogin", "data",
			"������ ���� ������");
	ctrl_editbox(s, "����� �� (L):", 'l', 50,
		     HELPCTX(rlogin_localuser),
		     dlg_stdeditbox_handler, I(offsetof(Config,localusername)),
		     I(sizeof(((Config *)0)->localusername)));

    }

    /*
     * All the SSH stuff is omitted in PuTTYtel, or in a reconfig
     * when we're not doing SSH.
     */

    if (have_backend(PROT_SSH) && (!midsession || protocol == PROT_SSH)) {

	/*
	 * The Connection/SSH panel.
	 */
	ctrl_settitle(b, "����/SSH",
		      "SSH ���� �ɼ��� �����մϴ�");

	if (midsession && protcfginfo == 1) {
	    s = ctrl_getset(b, "����/SSH", "disclaimer", NULL);
	    ctrl_text(s, "�� ȭ�鿡 �ִ� ������ ���� �߿��� ������� �ʽ��ϴ�. ���� ������ �ٸ� ������ ǥ���ϱ� ���ؼ� ������ ���� ���Դϴ�.", HELPCTX(no_help));
	}

	if (!midsession) {

	    s = ctrl_getset(b, "����/SSH", "data",
			    "������ ���� ������");
	    ctrl_editbox(s, "���� ��� (R):", 'r', 100,
			 HELPCTX(ssh_command),
			 dlg_stdeditbox_handler, I(offsetof(Config,remote_cmd)),
			 I(sizeof(((Config *)0)->remote_cmd)));

	    s = ctrl_getset(b, "����/SSH", "protocol", "�������� �ɼ�");
	    ctrl_checkbox(s, "���̳� ��ɾ� ������ ���� ���� (N)", 'n',
			  HELPCTX(ssh_noshell),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,ssh_no_shell)));
	}

	if (!midsession || protcfginfo != 1) {
	    s = ctrl_getset(b, "����/SSH", "protocol", "�������� �ɼ�");

	    ctrl_checkbox(s, "���� ��� (E)", 'e',
			  HELPCTX(ssh_compress),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,compression)));
	}

	if (!midsession) {
	    s = ctrl_getset(b, "����/SSH", "protocol", "�������� �ɼ�");

	    ctrl_radiobuttons(s, "���� �õ��� SSH �������� ����:", NO_SHORTCUT, 4,
			      HELPCTX(ssh_protocol),
			      dlg_stdradiobutton_handler,
			      I(offsetof(Config, sshprot)),
			      "SSH1��", 'l', I(0),
			      "SSH1", '1', I(1),
			      "SSH2", '2', I(2),
			      "SSH2��", 'y', I(3), NULL);
	}

	if (!midsession || protcfginfo != 1) {
	    s = ctrl_getset(b, "����/SSH", "encryption", "��ȣȭ �ɼ�");
	    c = ctrl_draglist(s, "��ȣ �˰��� ���� ��å (S):", 's',
			      HELPCTX(ssh_ciphers),
			      cipherlist_handler, P(NULL));
	    c->listbox.height = 6;

	    ctrl_checkbox(s, "SSH 2���� ����-DES ���� ȣȯ�� ��� (I)", 'i',
			  HELPCTX(ssh_ciphers),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,ssh2_des_cbc)));
	}

	/*
	 * The Connection/SSH/Kex panel. (Owing to repeat key
	 * exchange, this is all meaningful in mid-session _if_
	 * we're using SSH-2 or haven't decided yet.)
	 */
	if (protcfginfo != 1) {
	    ctrl_settitle(b, "����/SSH/Ű��ȯ",
			  "SSH Ű ��ȯ �ɼ��� �����մϴ�");

	    s = ctrl_getset(b, "����/SSH/Ű��ȯ", "main",
			    "Ű ��ȯ �˰��� �ɼ�");
	    c = ctrl_draglist(s, "��ȣ �˰��� ���� ��å (S):", 's',
			      HELPCTX(ssh_kexlist),
			      kexlist_handler, P(NULL));
	    c->listbox.height = 5;

	    s = ctrl_getset(b, "����/SSH/Ű��ȯ", "repeat",
			    "Ű �米ȯ ���� �ɼ��� �����մϴ�");

	    ctrl_editbox(s, "�ð�(��) ���� Ű �米ȯ (0: ������) (T)", 't', 20,
			 HELPCTX(ssh_kex_repeat),
			 dlg_stdeditbox_handler,
			 I(offsetof(Config,ssh_rekey_time)),
			 I(-1));
	    ctrl_editbox(s, "������ ũ�� ���� Ű �米ȯ (0: ������) (K)", 'x', 20,
			 HELPCTX(ssh_kex_repeat),
			 dlg_stdeditbox_handler,
			 I(offsetof(Config,ssh_rekey_data)),
			 I(16));
	    ctrl_text(s, "(1�ް��� 1M, 1�Ⱑ�� 1G ���� �Է��ϼ���)",
		      HELPCTX(ssh_kex_repeat));
	}

	if (!midsession) {

	    /*
	     * The Connection/SSH/Auth panel.
	     */
	    ctrl_settitle(b, "����/SSH/����",
			  "SSH ���� ���� �ɼ��� �����մϴ�");

	    s = ctrl_getset(b, "����/SSH/����", "main", NULL);
	    ctrl_checkbox(s, "������ ��� �ǳ� �� (SSH-2) (B)", 'b',
			  HELPCTX(ssh_auth_bypass),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,ssh_no_userauth)));

	    s = ctrl_getset(b, "����/SSH/����", "methods",
			    "���� ���");
	    ctrl_checkbox(s, "Pageant�� �̿��� ������ �õ� (P)", 'p',
			  HELPCTX(ssh_auth_pageant),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,tryagent)));
	    ctrl_checkbox(s, "SSH1���� TIS�� CryptoCard ������ �õ� (M)", 'm',
			  HELPCTX(ssh_auth_tis),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,try_tis_auth)));
	    ctrl_checkbox(s, "SSH2���� \"Ű����� ���� �Է�\"������ �õ� (I)",
			  'i', HELPCTX(ssh_auth_ki),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,try_ki_auth)));

	    s = ctrl_getset(b, "����/SSH/����", "params",
			    "���� ����");
	    ctrl_checkbox(s, "������Ʈ ������ ��� (F)", 'f',
			  HELPCTX(ssh_auth_agentfwd),
			  dlg_stdcheckbox_handler, I(offsetof(Config,agentfwd)));
	    ctrl_checkbox(s, "SSH2���� ����� �̸� ������ �õ��� (U)", 'u',
			  HELPCTX(ssh_auth_changeuser),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,change_username)));
	    ctrl_filesel(s, "���� ����Ű ���� (K):", 'k',
			 FILTER_KEY_FILES, FALSE, "���Ű ���� ����",
			 HELPCTX(ssh_auth_privkey),
			 dlg_stdfilesel_handler, I(offsetof(Config, keyfile)));
	}

	if (!midsession) {
	    /*
	     * The Connection/SSH/TTY panel.
	     */
	    ctrl_settitle(b, "����/SSH/TTY", "���� �͹̳� ����");

	    s = ctrl_getset(b, "����/SSH/TTY", "sshtty", NULL);
	    ctrl_checkbox(s, "����(pseudo)-�͹̳��� �Ҵ����� ����", 'p',
			  HELPCTX(ssh_nopty),
			  dlg_stdcheckbox_handler,
			  I(offsetof(Config,nopty)));

	    s = ctrl_getset(b, "����/SSH/TTY", "ttymodes",
			    "�͹̳� ���");
	    td = (struct ttymodes_data *)
		ctrl_alloc(b, sizeof(struct ttymodes_data));
	    ctrl_columns(s, 2, 75, 25);
	    c = ctrl_text(s, "������ �͹̳� ���", HELPCTX(ssh_ttymodes));
	    c->generic.column = 0;
	    td->rembutton = ctrl_pushbutton(s, "���� (R)", 'r',
					    HELPCTX(ssh_ttymodes),
					    ttymodes_handler, P(td));
	    td->rembutton->generic.column = 1;
	    td->rembutton->generic.tabdelay = 1;
	    ctrl_columns(s, 1, 100);
	    td->listbox = ctrl_listbox(s, NULL, NO_SHORTCUT,
				       HELPCTX(ssh_ttymodes),
				       ttymodes_handler, P(td));
	    td->listbox->listbox.multisel = 1;
	    td->listbox->listbox.height = 4;
	    td->listbox->listbox.ncols = 2;
	    td->listbox->listbox.percentages = snewn(2, int);
	    td->listbox->listbox.percentages[0] = 40;
	    td->listbox->listbox.percentages[1] = 60;
	    ctrl_tabdelay(s, td->rembutton);
	    ctrl_columns(s, 2, 75, 25);
	    td->modelist = ctrl_droplist(s, "���:", 'm', 67,
					 HELPCTX(ssh_ttymodes),
					 ttymodes_handler, P(td));
	    td->modelist->generic.column = 0;
	    td->addbutton = ctrl_pushbutton(s, "�߰� (D)", 'd',
					    HELPCTX(ssh_ttymodes),
					    ttymodes_handler, P(td));
	    td->addbutton->generic.column = 1;
	    td->addbutton->generic.tabdelay = 1;
	    ctrl_columns(s, 1, 100);	    /* column break */
	    /* Bit of a hack to get the value radio buttons and
	     * edit-box on the same row. */
	    ctrl_columns(s, 3, 25, 50, 25);
	    c = ctrl_text(s, "��:", HELPCTX(ssh_ttymodes));
	    c->generic.column = 0;
	    td->valradio = ctrl_radiobuttons(s, NULL, NO_SHORTCUT, 2,
					     HELPCTX(ssh_ttymodes),
					     ttymodes_handler, P(td),
					     "�ڵ�", NO_SHORTCUT, P(NULL),
					     "�Է�:", NO_SHORTCUT, P(NULL),
					     NULL);
	    td->valradio->generic.column = 1;
	    td->valbox = ctrl_editbox(s, NULL, NO_SHORTCUT, 100,
				      HELPCTX(ssh_ttymodes),
				      ttymodes_handler, P(td), P(NULL));
	    td->valbox->generic.column = 2;
	    ctrl_tabdelay(s, td->addbutton);

	}

	if (!midsession) {
	    /*
	     * The Connection/SSH/X11 panel.
	     */
	    ctrl_settitle(b, "����/SSH/X11",
			  "SSH X11 ������ ���� �ɼ��� �����մϴ�");

	    s = ctrl_getset(b, "����/SSH/X11", "x11", "X11 ������");
	    ctrl_checkbox(s, "X11 ������ ��� (E)", 'e',
			  HELPCTX(ssh_tunnels_x11),
			  dlg_stdcheckbox_handler,I(offsetof(Config,x11_forward)));
	    ctrl_editbox(s, "X ���÷��� ��ġ", 'x', 50,
			 HELPCTX(ssh_tunnels_x11),
			 dlg_stdeditbox_handler, I(offsetof(Config,x11_display)),
			 I(sizeof(((Config *)0)->x11_display)));
	    ctrl_radiobuttons(s, "���� X11 ���� �������� (U)", 'u', 2,
			      HELPCTX(ssh_tunnels_x11auth),
			      dlg_stdradiobutton_handler,
			      I(offsetof(Config, x11_auth)),
			      "MIT-Magic-Cookie-1", I(X11_MIT),
			      "XDM-Authorization-1", I(X11_XDM), NULL);
	}

	/*
	 * The Tunnels panel _is_ still available in mid-session.
	 */
	ctrl_settitle(b, "����/SSH/�ͳθ�",
		      "SSH ��Ʈ ������ �ɼ��� �����մϴ�");

	s = ctrl_getset(b, "����/SSH/�ͳθ�", "portfwd",
			"��Ʈ ������");
	ctrl_checkbox(s, "�ٸ� ȣ��Ʈ���� �츮�� ��Ʈ���� ���� (T)",'t',
		      HELPCTX(ssh_tunnels_portfwd_localhost),
		      dlg_stdcheckbox_handler,
		      I(offsetof(Config,lport_acceptall)));
	ctrl_checkbox(s, "���� ��Ʈ�� ���� ������� (SSH2��) (P)", 'p',
		      HELPCTX(ssh_tunnels_portfwd_localhost),
		      dlg_stdcheckbox_handler,
		      I(offsetof(Config,rport_acceptall)));

	ctrl_columns(s, 3, 55, 20, 25);
	c = ctrl_text(s, "������ ��Ʈ:", HELPCTX(ssh_tunnels_portfwd));
	c->generic.column = COLUMN_FIELD(0,2);
	/* You want to select from the list, _then_ hit Remove. So tab order
	 * should be that way round. */
	pfd = (struct portfwd_data *)ctrl_alloc(b,sizeof(struct portfwd_data));
	pfd->rembutton = ctrl_pushbutton(s, "���� (R)", 'r',
					 HELPCTX(ssh_tunnels_portfwd),
					 portfwd_handler, P(pfd));
	pfd->rembutton->generic.column = 2;
	pfd->rembutton->generic.tabdelay = 1;
	pfd->listbox = ctrl_listbox(s, NULL, NO_SHORTCUT,
				    HELPCTX(ssh_tunnels_portfwd),
				    portfwd_handler, P(pfd));
	pfd->listbox->listbox.height = 3;
	pfd->listbox->listbox.ncols = 2;
	pfd->listbox->listbox.percentages = snewn(2, int);
	pfd->listbox->listbox.percentages[0] = 20;
	pfd->listbox->listbox.percentages[1] = 80;
	ctrl_tabdelay(s, pfd->rembutton);
	ctrl_text(s, "������� ��Ʈ �߰�:", HELPCTX(ssh_tunnels_portfwd));
	/* You want to enter source, destination and type, _then_ hit Add.
	 * Again, we adjust the tab order to reflect this. */
	pfd->addbutton = ctrl_pushbutton(s, "�߰� (D)", 'd',
					 HELPCTX(ssh_tunnels_portfwd),
					 portfwd_handler, P(pfd));
	pfd->addbutton->generic.column = 2;
	pfd->addbutton->generic.tabdelay = 1;
	pfd->sourcebox = ctrl_editbox(s, "�� ��Ʈ (S)", 's', 40,
				      HELPCTX(ssh_tunnels_portfwd),
				      portfwd_handler, P(pfd), P(NULL));
	pfd->sourcebox->generic.column = 0;
	pfd->destbox = ctrl_editbox(s, "��� (I)", 'i', 67,
				    HELPCTX(ssh_tunnels_portfwd),
				    portfwd_handler, P(pfd), P(NULL));
	pfd->direction = ctrl_radiobuttons(s, NULL, NO_SHORTCUT, 3,
					   HELPCTX(ssh_tunnels_portfwd),
					   portfwd_handler, P(pfd),
					   "���� (L)", 'l', P(NULL),
					   "���� (M)", 'm', P(NULL),
					   "���� (Y)", 'y', P(NULL),
					   NULL);
#ifndef NO_IPV6
	pfd->addressfamily =
	    ctrl_radiobuttons(s, NULL, NO_SHORTCUT, 3,
			      HELPCTX(ssh_tunnels_portfwd_ipversion),
			      portfwd_handler, P(pfd),
			      "�ڵ�", 'u', I(ADDRTYPE_UNSPEC),
			      "IPv4", '4', I(ADDRTYPE_IPV4),
			      "IPv6", '6', I(ADDRTYPE_IPV6),
			      NULL);
#endif
	ctrl_tabdelay(s, pfd->addbutton);
	ctrl_columns(s, 1, 100);

	if (!midsession) {
	    /*
	     * The Connection/SSH/Bugs panel.
	     */
	    ctrl_settitle(b, "����/SSH/����",
			  "SSH ���� ���� ���ذ���");

	    s = ctrl_getset(b, "����/SSH/����", "main",
			    "SSH ������ �˷��� ���� ���ذ���");
	    ctrl_droplist(s, "SSH1 ���� �޽��� ���� (I)", 'i', 20,
			  HELPCTX(ssh_bugs_ignore1),
			  sshbug_handler, I(offsetof(Config,sshbug_ignore1)));
	    ctrl_droplist(s, "SSH1 �н����� ������ �ź�", 's', 20,
			  HELPCTX(ssh_bugs_plainpw1),
			  sshbug_handler, I(offsetof(Config,sshbug_plainpw1)));
	    ctrl_droplist(s, "SSH1 RSA ���� ���� (R)", 'r', 20,
			  HELPCTX(ssh_bugs_rsa1),
			  sshbug_handler, I(offsetof(Config,sshbug_rsa1)));
	    ctrl_droplist(s, "SSH2 HMAC Ű�� �߸� ��� (M)", 'm', 20,
			  HELPCTX(ssh_bugs_hmac2),
			  sshbug_handler, I(offsetof(Config,sshbug_hmac2)));
	    ctrl_droplist(s, "SSH2 ��ȣȭ Ű�� �߸� ����� (E)", 'e', 20,
			  HELPCTX(ssh_bugs_derivekey2),
			  sshbug_handler, I(offsetof(Config,sshbug_derivekey2)));
	    ctrl_droplist(s, "SSH2 RSA �ñ׳��翡 �е� �䱸 (P)", 'p', 20,
			  HELPCTX(ssh_bugs_rsapad2),
			  sshbug_handler, I(offsetof(Config,sshbug_rsapad2)));
	    ctrl_droplist(s, "���� ID�� PK ������ ���� (N)", 'n', 20,
			  HELPCTX(ssh_bugs_pksessid2),
			  sshbug_handler, I(offsetof(Config,sshbug_pksessid2)));
	    ctrl_droplist(s, "�߸��� SSH2 Ű �米ȯ ��� (K)", 'k', 20,
			  HELPCTX(ssh_bugs_rekey2),
			  sshbug_handler, I(offsetof(Config,sshbug_rekey2)));
	}
    }
}
